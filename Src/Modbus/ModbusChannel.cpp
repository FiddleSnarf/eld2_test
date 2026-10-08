/******************************************************************************
 * Copyright (C) АО "Диаконт"
 * Project     : DeviceControl
 * File        : ModbusChannel.cpp
 * Description : Реализация канала коммуникации по протоколу modbus.
******************************************************************************/

#include "ModbusChannel.h"
#include "ModbusUtils.h"
#include "ModbusProtocolLoader.h"
#include <modbus.h>
#include <QMetaObject>
#include <QDebug>

using namespace Modbus;

IModbusChannelPtr Modbus::createChannel()
{
    return std::make_shared<ModbusChannel>();
}

Dk::ICommProtocolPtr Modbus::loadProtocol(const std::string& fileName, const std::string& typeIdent)
{
    ModbusProtocolLoader loader;
    return loader.loadFromFile(fileName, typeIdent);
}


ModbusChannel::ModbusChannel()
    : m_taskQueue("modbus_task_thread", true)
{
    qRegisterMetaType<Dk::AddrType>("Dk::AddrType");
    qRegisterMetaType<Dk::CommDataPtr>("Dk::CommDataPtr");
}

ModbusChannel::~ModbusChannel()
{
    closeConnection();
}

void ModbusChannel::setConnectionSettings(const ConnectionSettings& params)
{
    m_connectionSettings = params;
}

ConnectionSettings ModbusChannel::connectionSettings() const
{
    return m_connectionSettings;
}

bool ModbusChannel::start()
{
    if (m_taskQueue.isWorking())
    {
        toLog(tr("The restart of active modbus connection is prohibited"), true);
        return false;
    }

    m_modbus = ModbusLibFactory::createModbusDevice(m_connectionSettings.connectionString);
    if (!m_modbus)
    {
        toLog(tr("Invalid modbus connection parameters"), true);
        return false;
    }

    // Запускаем процесс подключения в отдельном потоке
    m_taskQueue.run();
    m_taskQueue.postTask([this]() { processConnectTask(); });
    return true;
}

void ModbusChannel::stop()
{
    closeConnection();
    
    if (m_connectionPortOpened)
    {
        m_connectionPortOpened = { false };
        emit signalClosed();
    }
}

bool ModbusChannel::isConnected(Dk::AddrType address) const
{
    if (!m_connectionPortOpened)
        return false;

    auto it = m_devices.find(address);
    if (it != m_devices.end())
        return it->second.hasLink;
        
    return false;
}

bool ModbusChannel::isChannelOpened() const
{
    return m_connectionPortOpened;
}

UniqueIdent ModbusChannel::configAsyncRead(SlaveId slaveId, const AsyncReadParameters& params, ReadRegResHandler callback)
{
    static UniqueIdent generator = 0;

    AsyncReadInfo info;
    info.readRegisters = params.registers;
    info.handler = callback;
    info.readPeriod_msec = params.period_msec;
    info.sendResultViaCommChannelInterface = params.emitResultViaCommInterface;

    if (params.batchRead && params.registers.size() > 1)
        info.batchSize = calculateBatchReadSize(params.registers);

    UniqueIdent read_id;
    {
        auto& dev_params = m_devices[slaveId];
        std::lock_guard<std::mutex >locker(dev_params.readRegistersEntry);
        read_id = ++generator;
        dev_params.asyncReadRegisters[read_id] = std::move(info);
    }
    if (m_connectionPortOpened)
        processAsyncReadTask(slaveId, read_id);

    return read_id;
}

void ModbusChannel::removeReadRegisters(UniqueIdent id)
{
    for (auto& [_, dev_params] : m_devices)
    {
        if (auto it = dev_params.asyncReadRegisters.find(id); it != std::cend(dev_params.asyncReadRegisters))
        {
            std::lock_guard<std::mutex> locker(dev_params.readRegistersEntry);
            dev_params.asyncReadRegisters.erase(it);
            break;
        }
    }
}

bool ModbusChannel::writeRegisterSync(SlaveId slaveId, const ModbusRegister& reg, const QVariant& value)
{
    if (reg.regType == EnRegType::enInputReg || reg.regType == EnRegType::enDiscreteInput)
        return logErrorDesciption(tr("Fail to write to read-only register"));

    if (!m_modbus ||!m_connectionPortOpened)
        return logErrorDesciption(tr("Unable to write register - no connection"));

    int modbuslibRes = -1;

    if (reg.regType == EnRegType::enHoldingReg)
    {
        const auto& rawData = packModbusRaw(reg, value);
        if (rawData.empty())
            return logErrorDesciption(tr("Fail to write register - value conversion error"), reg, slaveId);

        auto message = tr("Write to register {0x%1} value %2 (").arg(int(reg.modbusAddr), 4, 16, QLatin1Char('0')).arg(value.toString());
        for (const auto hrReg : rawData)
            message += QString("0x%1 ").arg(int(hrReg), 4, 16, QLatin1Char('0'));
        message += QString(")");

        toLog(message, false);

        {
            std::lock_guard<std::mutex> locker(m_sentry);
            modbus_set_slave(m_modbus.get(), slaveId);
            modbuslibRes = modbus_write_registers(m_modbus.get(), reg.modbusAddr, rawData.size(), rawData.data());
        }
    }
    else if (reg.regType == EnRegType::enCoil)
    {
        std::lock_guard<std::mutex> locker(m_sentry);
        modbus_set_slave(m_modbus.get(), slaveId);
        modbuslibRes = modbus_write_bit(m_modbus.get(), reg.modbusAddr, value.toInt() == 0 ? 0 : 1);
    }

    if (modbuslibRes == -1)
        return logErrorDesciption(tr("modbuslib reported: fail to write register"), reg, slaveId);

    // считываем, что было реально записано
    m_taskQueue.postDelayedTask(std::chrono::milliseconds(100), [this, slaveId, reg]
    {
        if (auto readResult = readRegisterSync(slaveId, reg); readResult.has_value())
            processReadRequestResult(slaveId, reg, *readResult);
    });

    return true;
}

std::optional<ReadResult> ModbusChannel::readRegisterSync(SlaveId slaveId, const ModbusRegister& reg)
{
    if (!m_modbus || !m_connectionPortOpened)
    {
        logErrorDesciption(tr("Unable to read register - no connection"));
        return {};
    }

    ReadResult result = {};
    auto readSize = getWordsCount(reg);   // 1, 2, 4 words
    if (reg.regType == EnRegType::enHoldingReg || reg.regType == EnRegType::enInputReg)
        readSize = qMin(uint(result.words.size()), readSize);
    else
        readSize = qMin(uint(result.discrets.size()), readSize * 2);        // todo: это требует проверки 

    std::lock_guard<std::mutex> locker(m_sentry);
    modbus_set_slave(m_modbus.get(), slaveId);
    int modbuslibRes = -1;
    switch (reg.regType)
    {
    case EnRegType::enHoldingReg:
        modbuslibRes = modbus_read_registers(m_modbus.get(), reg.modbusAddr, readSize, result.words.data());
        break;
    case EnRegType::enInputReg:
        modbuslibRes = modbus_read_input_registers(m_modbus.get(), reg.modbusAddr, readSize, result.words.data());
        break;
    case EnRegType::enCoil:
        modbuslibRes = modbus_read_bits(m_modbus.get(), reg.modbusAddr, readSize, result.discrets.data());
        break;
    case EnRegType::enDiscreteInput:
        modbuslibRes = modbus_read_input_bits(m_modbus.get(), reg.modbusAddr, readSize, result.discrets.data());
        break;
    default:
        break;
    }

    if (modbuslibRes == -1)
        return {};

    if (reg.swapWords)
        swapWords(result.words, readSize);

    return result;
}

void ModbusChannel::setSlaveInfo(SlaveId slaveId, const std::string& typeIdent, Dk::ICommProtocolPtr protocol)
{
    DeviceParams& dp = m_devices[slaveId];
    
    dp.info = Dk::DeviceInfo
    {
        Dk::DeviceIdent{typeIdent},
        Dk::AddrType{slaveId},
        protocol,
    };

    if (auto modbus_protocol = std::dynamic_pointer_cast<ModbusProtocol>(protocol))
    {
        for (const Dk::CmdIndex& cmd_index : modbus_protocol->commandsList())
        {
            if (auto reg = modbus_protocol->getRegister(cmd_index); reg.has_value())
                dp.allRegs[cmd_index] = *reg;
        }
    }
}

bool ModbusChannel::readRequest(Dk::AddrType addr, Dk::CmdIndex cmd_index)
{
    auto devIt = m_devices.find(addr);
    if (devIt == m_devices.end())
    {
        const auto message = tr("Read request to unregistred device with addr %1").arg(addr);
        return logErrorDesciption(message);
    }
    auto& dev = devIt->second;

    auto regIt = dev.allRegs.find(cmd_index);
    if (regIt == dev.allRegs.end())
    {
        const auto message = tr("Read request to device with addr %1 witn unregistred cmd index %2").arg(addr).arg(cmd_index);
        return logErrorDesciption(message);
    }

    m_taskQueue.postTask([this, slave_id = addr, reg = regIt->second]()
    { 
        if (auto readResult = readRegisterSync(slave_id, reg); readResult.has_value())
            processReadRequestResult(slave_id, reg, *readResult);
        else 
            logErrorDesciption(tr("modbuslib reported: fail to read register"), reg, slave_id);
    });

    return true;
}

bool ModbusChannel::writeRequest(Dk::AddrType addr, Dk::CmdIndex id, const QVariant& value)
{
    auto devIt = m_devices.find(addr);
    if (devIt == m_devices.end())
    {
        const auto message = tr("Write request to unregistred device with addr %1").arg(addr);
        return logErrorDesciption(message);
    }
    auto& dev = devIt->second;

    auto regIt = dev.allRegs.find(id);
    if (regIt == dev.allRegs.end())
    {
        const auto message = tr("Write request to device with addr %1 witn unregistred cmd index %2").arg(addr).arg(id);
        return logErrorDesciption(message);
    }

    m_taskQueue.postTask([this, addr, regIt, value]() { writeRegisterSync(addr, regIt->second, value); });
    return true;
}

bool ModbusChannel::updateState(Dk::AddrType concreteAddress)
{
    if (!m_modbus)
        return false;

    for (auto& [slave_id, device] : m_devices)
    {
        if (concreteAddress != Dk::INVALID_ADDR && slave_id != concreteAddress)
            continue;
        
        const auto id = slave_id;
        m_taskQueue.postTask([this, id, registers = device.allRegs]
        {
            for (const auto& [_, reg] : registers)
            {
                if (auto readResult = readRegisterSync(id, reg); readResult.has_value())
                    processReadRequestResult(id, reg, *readResult);
                else
                    logErrorDesciption(tr("modbuslib reported: fail to read register"), reg, id);
            }
        });
    }

    return true;
}

std::vector<Dk::DeviceInfo> ModbusChannel::linkedDevices() const
{
    std::vector<Dk::DeviceInfo> devices;
    for (const auto& [_, dev] : m_devices)
    {
        if (const auto& info = dev.info; info.has_value())
            devices.push_back(*info);
    }

    return devices;
}

void ModbusChannel::processConnectTask()
{
    for (auto& [slave_id, dev_params] : m_devices)
    {
        dev_params.noResponseCounter =  0 ;
        dev_params.hasLink =  false ;
    }

    m_connectionPortOpened =  false ;

    assert(m_modbus);
    if (!m_modbus)
        return;

    {
        std::lock_guard<std::mutex> locker(m_sentry);
        auto res = modbus_connect(m_modbus.get());

        m_connectionPortOpened = res != -1;
        if (m_connectionPortOpened)
            modbus_flush(m_modbus.get());
    }

    if (m_connectionPortOpened)
    {
        toLog("Connection port opened", false);
        emit signalOpened();

        // Запускаем процесс асинхронного чтения регистров
        for (auto& [slave_id, dev_params] : m_devices)
        {
            for (const auto& [id, _] : dev_params.asyncReadRegisters)
                m_taskQueue.postTask([this, slave_id = slave_id, id = id]() { processAsyncReadTask(slave_id, id); });
        }
    }
    else
    {
        logErrorDesciption(tr("Fail to connect via %1").arg(QString::fromStdString(m_connectionSettings.connectionString)));
        
        // повторяем попытку подключения
        const auto interval = std::chrono::milliseconds(qMax(100u, m_reconnectTimeout));
        m_taskQueue.postDelayedTask(interval, [this]() { processConnectTask(); });
    }
}

void ModbusChannel::checkConnectionStatus(SlaveId slaveId, bool hasResponse)
{
    auto& dev_params = m_devices[slaveId];

    auto updateLinkStatus = [this, slaveId, &dev_params](bool has_link)
        {
            dev_params.noResponseCounter =  0 ;
            if (dev_params.hasLink != has_link)
            {
                dev_params.hasLink = has_link;
                emit signalConnectionStateChanged(slaveId, has_link);

                if (!has_link)
                {

                    // todo: продумать политику выполнения переподключения.
                    // закрывать канал не нужно, если имеется связь хоть с одним устройством

                    // В любом случае переподключение должно быть выполнено в основном потоке
                    //QMetaObject::invokeMethod(this, [this]
                    //{
                    //    DKLG_LOG_DEBUG_EX(m_logger) << L"Try reconnect...";
                    //    stop();
                    //    QTimer::singleShot(m_reconnectTimeout, this, &ModbusChannel::start);
                    //});
                }
            }
        };

    if (hasResponse)
    {
        updateLinkStatus(true);
    }
    else if (++dev_params.noResponseCounter >= 5)
    {
        updateLinkStatus(false);
        toLog(QString("Link error to modbus unit, address = %1").arg(slaveId), true);
    }
}

void ModbusChannel::processAsyncReadTask(SlaveId slaveId, UniqueIdent id)
{
    bool hasAnyResponse = false;

    auto it_dev = m_devices.find(slaveId);
    if (it_dev == m_devices.end())
        return;

    auto& dev_params = it_dev->second;
    std::lock_guard<std::mutex> reg_locker(dev_params.readRegistersEntry);
    auto it_info = dev_params.asyncReadRegisters.find(id);
    if (it_info == dev_params.asyncReadRegisters.end())
        return;

    const auto& info = it_info->second;
    if (readSlaveRegisters(slaveId, info))
        hasAnyResponse = true;

    // добавляем задачу следующей итерации чтения
    const auto delay = std::chrono::milliseconds(info.readPeriod_msec);
    m_taskQueue.postDelayedTask(delay, [this, slaveId, id]
    { 
        processAsyncReadTask(slaveId, id);
    });

    checkConnectionStatus(slaveId, hasAnyResponse);
}

bool ModbusChannel::readSlaveRegisters(uint slaveId, const AsyncReadInfo& info)
{
    if (!m_modbus || !m_connectionPortOpened)
        return false;

    if (info.batchSize > 0)
    {
        // пытаемся читать одним пакетом
        std::vector<uint16_t> buffer(info.batchSize);
        const auto& start_register = info.readRegisters.front();

        {
            std::lock_guard<std::mutex> locker(m_sentry);
            modbus_set_slave(m_modbus.get(), slaveId);
            auto read_count = modbus_read_registers(m_modbus.get(), start_register.modbusAddr, info.batchSize, buffer.data());
            if (read_count != info.batchSize)
                return false;
        }

        int buffer_pos = 0;
        for (const auto& reg : info.readRegisters)
        {
            const auto words_count = getWordsCount(reg);
            assert(buffer_pos + words_count <= buffer.size());

            ReadResult* reg_data = reinterpret_cast<ReadResult*>(buffer.data() + buffer_pos);
            if (words_count > 1 && reg.swapWords)
                swapWords(reg_data->words, words_count);

            if (auto callback = info.handler)
                callback(reg.modbusAddr, *reg_data);

            if (info.sendResultViaCommChannelInterface)
                processReadRequestResult(slaveId, reg, *reg_data);

            buffer_pos += words_count;
        }
    }
    else
    {
        // читаем каждый регистр отдельно
        for (const auto& reg : info.readRegisters)
        {
            auto reg_data = readRegisterSync(slaveId, reg);
            if (!reg_data.has_value())
                return false;

            if (auto callback = info.handler)
                callback(reg.modbusAddr, *reg_data);

            if (info.sendResultViaCommChannelInterface)
                processReadRequestResult(slaveId, reg, *reg_data);
        }
    }

    return true;
}

bool ModbusChannel::logErrorDesciption(const QString& preamble, const ModbusRegister& reg, uint slaveId)
{
    auto message = tr("%1 {addr: %2, register: 0x%3}").arg(preamble).arg(slaveId).arg(reg.modbusAddr, 4, 16, QLatin1Char('0'));
    return logErrorDesciption(message);
}

bool ModbusChannel::logErrorDesciption(const QString & preamble)
{
    auto message = preamble;
    if (const auto err_code = errno)
        message += tr(" - %1").arg(QString(modbus_strerror(errno)).toLower());

    toLog(message, true);
    return false;
}

void ModbusChannel::processReadRequestResult(uint slaveId, const ModbusRegister& reg, ReadResult readResult)
{
    auto data = std::make_shared<Dk::CommData>(reg.modbusAddr, Dk::EnStatus::enMessage);
    data->value = unpackModbusData(reg, readResult);

    QMetaObject::invokeMethod(this, [this, addr = Dk::AddrType{ slaveId }, data]
    {
        emit signalDataReceived(addr, data);
    });
}

void ModbusChannel::closeConnection()
{
    m_taskQueue.stopASAP();
    m_taskQueue.clear();

    // Удаляем контекст взаимодествия с modbuslib
    if (m_modbus && m_connectionPortOpened)
    {
        std::lock_guard<std::mutex> locker(m_sentry);
        modbus_close(m_modbus.get());
    }
}

uint8_t ModbusChannel::calculateBatchReadSize(const std::vector<ModbusRegister>& registers) const
{
    uint8_t batch_size = 0;

    // проверяем, можно ли прочитать данные одним пакетом
    for (auto it = registers.cbegin(); it != registers.cend(); ++it)
    {
        if (it->regType != EnRegType::enHoldingReg)
            return 0;

        auto register_size = getWordsCount(*it);
        if (auto next = std::next(it); next != registers.cend())
        {
            // не допускаем пропусков между регистрами
            if (it->modbusAddr + register_size != next->modbusAddr)
                return 0;
        }

        // todo: следим за переполнением, учитывая, 
        // что по стандарту ограничение на размер пакетного чтения 
        // составляет MODBUS_MAX_READ_REGISTERS     125 (слов) == 250 байт
        batch_size += register_size;
    }

    return batch_size;
}

void ModbusChannel::toLog(const QString& mess, bool err)
{
    if (err)
        qWarning() << mess;
    else
        qDebug() << mess;
}

/******************************************************************************
 * Copyright (C) АО "Диаконт"
 * Project     : DeviceControl
 * File        : ModbusChannel.h
 * Description : Реализация канала коммуникации по протоколу modbus.
******************************************************************************/

#pragma once

#include "IModbusChannel.h"
#include "ModbusProtocol.h"
#include "ModbusLibFactory.h"

#include "TaskRunner/TaskRunnerTimer.h"

#include <mutex>
#include <atomic>
#include <optional>
#include <vector>

namespace Modbus
{
///////////////////////////////////////////////////////////////////////////////

using UniqueIdent = int;

class ModbusChannel :public IModbusChannel
{
    Q_OBJECT

public:
    explicit ModbusChannel();
    virtual ~ModbusChannel();

    // Inherited via ICommunicationChannel
    void setConnectionSettings(const ConnectionSettings& params);
    ConnectionSettings connectionSettings() const;
    bool start();
    void stop();
    bool isConnected(Dk::AddrType address) const;
    bool isChannelOpened() const;
    bool readRequest(Dk::AddrType addr, Dk::CmdIndex id);
    bool writeRequest(Dk::AddrType addr, Dk::CmdIndex id, const QVariant& value);
    bool updateState(Dk::AddrType addr = Dk::INVALID_ADDR);
    std::vector<Dk::DeviceInfo> linkedDevices() const;

    // Inherited via IModbusChannel
    UniqueIdent configAsyncRead(SlaveId slaveId, const AsyncReadParameters& params, ReadRegResHandler callback) final;
    void removeReadRegisters(UniqueIdent id) final;
    bool writeRegisterSync(SlaveId slaveId, const ModbusRegister& reg, const QVariant& value) final;
    std::optional<ReadResult> readRegisterSync(SlaveId slaveId, const ModbusRegister& reg) final;
    void setSlaveInfo(SlaveId slaveId, const std::string& typeIdent, Dk::ICommProtocolPtr protocol) final;

private:

    struct AsyncReadInfo
    {
        std::vector<ModbusRegister> readRegisters;      // Список регистров 
        ReadRegResHandler handler;                      // Обработчик результатов чтения
        uint8_t batchSize = 0;                          // Количество регистров для пакетного чтения
        uint readPeriod_msec = 250;                     // периодичность чтения, мсек
        bool sendResultViaCommChannelInterface = false; // отправлять данные (в сводку) через интерфейс ICommChannel
    };

    struct DeviceParams
    {
        std::optional<Dk::DeviceInfo> info;                // Информация о устройстве
        std::map<Dk::CmdIndex, ModbusRegister> allRegs;    // Все существующие регистры

        std::map<UniqueIdent, AsyncReadInfo> asyncReadRegisters;    // Словарь регистров для периодического чтения.
        mutable std::mutex readRegistersEntry;                      // Мьютекс для защиты списка регистров автоматического чтения.
        std::atomic_uint noResponseCounter = { 0 };                 // Счетчик ошибок чтения регистров.
        std::atomic_bool hasLink = { false };                       // Флаг наличия связи с устройством.
    };

    std::map<SlaveId, DeviceParams> m_devices;
    std::atomic_bool m_connectionPortOpened = { false };
    std::mutex m_sentry;                            /**< Мьютекс для защиты доступа к контексту modbuslib. */
    ModbusPtr m_modbus;                             /**< Контекст устройства libmodbus. */
    ConnectionSettings m_connectionSettings;        /**< Параметры подключения. */
    uint m_reconnectTimeout = 2000;                 /**< Таймаут переподключения, мс. */

    DkTools::TaskRunnerTimer m_taskQueue;           /**< Очередь асинхронных задач. */

private:
    /** \brief Выполнить подключение (в потоке m_taskQueue) */
    void processConnectTask();

    /** \brief Выполнить обновление статуса подключения */
    void checkConnectionStatus(SlaveId slaveId, bool hasResponse);

    /** \brief Выполнить чтение необходимых регистров с интервалом \arg interval_msec */
    void processAsyncReadTask(SlaveId slaveId, UniqueIdent id);

    bool readSlaveRegisters(uint slaveId, const AsyncReadInfo& info);

    bool logErrorDesciption(const QString& preamble);

    bool logErrorDesciption(const QString& preamble, const ModbusRegister& reg, uint slaveId = 0);

    void processReadRequestResult(uint slaveId, const ModbusRegister& reg, ReadResult res);

    void closeConnection();

    /** \brief Рассчитать размер пакета группового чтения регистров.
     *  \return Количество слов в пакете. 
     */
    uint8_t calculateBatchReadSize(const std::vector<ModbusRegister>& registers) const;

    void toLog(const QString& mess, bool err);
};
using ModbusChannelPtr = std::shared_ptr<ModbusChannel>;

///////////////////////////////////////////////////////////////////////////////
} //namespace Modbus

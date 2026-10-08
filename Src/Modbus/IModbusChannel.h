/******************************************************************************
 * Copyright (C) АО "Диаконт"
 * Project     : DeviceControl
 * File        : ModbusChannel.h
 * Description : Реализация канала коммуникации по протоколу modbus.
******************************************************************************/

#pragma once

#include "IModbusCommTypes.h"
#include "ICommProtocol.h"

#include <QObject>
#include <vector>
#include <memory>
#include <optional>

namespace Modbus
{
///////////////////////////////////////////////////////////////////////////////

struct ConnectionSettings
{
    int processTimeout = 100;               /**< Таймаут между обработкой данных (мс). */
    int pingTimeout = 1000;                 /**< Таймаут между отправкой сообщений для проверки соединения с оборудованием (мс). */
    int reconnectTimeout = 2500;            /**< Таймаут на восстановление соединения (мс). */
    int watchdogRxTimeout = 2000;           /**< Интервал отправки пакетов сторожевого таймера (мс). */
    std::string connectionString;           /**< Строка подключения. */
    std::string networkInterface;           /**< Название сетевого интерфейса (для подключения по Etercat), например 'eth0'. */
    std::string defaultConnectionString;    /**< Строка подключения по умолчанию. */
    std::string bootloaderConnectionString; /**< Строка подключения к загрузчику. */
};

using UniqueIdent = int;
using ReadRegResHandler = std::function<void(RegAddress /*modbusAddr*/, const ReadResult& /*value*/)>;
using SlaveId = uint8_t;

struct AsyncReadParameters
{
    std::vector<ModbusRegister> registers;
    uint period_msec = 250;
    bool batchRead = false;                     // считывать данные одним пакетом (реализуемо для Holding Registers).
    bool emitResultViaCommInterface = true;     // испускать сигнал с результатами чтения через ICommunicationChannel интерфейс

    AsyncReadParameters(const ModbusRegister _reg, uint _period = 250) 
        : registers({ _reg }) 
        , period_msec(_period)
    {}
    explicit AsyncReadParameters(const std::vector<ModbusRegister>& _registers = {}, uint _period = 100, bool _batch = false)
        : registers(_registers) 
        , period_msec(_period)
        , batchRead(_batch)
    {}
};

class IModbusChannel : public QObject
{
    Q_OBJECT

public:
    virtual ~IModbusChannel() = default;

    /** \brief Добавить карту регистров для периодического асинхронного чтения.
     *  \param slaveId - адрес устройства.
     *  \param params - параметры чтения.
     *  \param callback - функция обратного вызова для передачи результата чтения.
     *  \return уникальный идентификатор для возможности последующей отписки от чтения, изменения параметров чтения.
     */
    virtual UniqueIdent configAsyncRead(SlaveId slaveId, const AsyncReadParameters& params, ReadRegResHandler callback) = 0;

    /** \brief Отписаться от периодического асинхронного чтения, определяемого идентификатором \arg id. 
     */ 
    virtual void removeReadRegisters(UniqueIdent id) = 0;

    /** \brief Запись регистра в блокирующем режиме.
     */
    virtual bool writeRegisterSync(SlaveId slaveId, const ModbusRegister& reg, const QVariant& value) = 0;

    /** \brief Чтение регистра в блокирующем режиме.
     */
    virtual std::optional<ReadResult> readRegisterSync(SlaveId slaveId, const ModbusRegister& reg) = 0;

    /** \brief Задать информацию о ведемом устройстве.
     *  \param slaveId - адрес устройства.
     *  \param typeIdent - идентификатор типа устройства.
     *  \param protocol - протокол команд (регистров).
     */
    virtual void setSlaveInfo(SlaveId slaveId, const std::string& typeIdent, Dk::ICommProtocolPtr protocol) = 0;

signals:
    /** \brief Сигнал извещает о получении новых данных от оборудования.
     *  \param address - адрес устройства.
     *  \param data - принятые данные.
     */
    void signalDataReceived(Dk::AddrType address, Dk::CommDataPtr data);

    /** \brief Сигнал об изменении состояния подключения
     *  \param address - адрес устройства.
     *  \param state - состояние подключения.
     */
    void signalConnectionStateChanged(Dk::AddrType address, bool state);

    /** \brief Сигнал об открытии канала. */
    void signalOpened();

    /** \brief Сигнал о закрытии канала. */
    void signalClosed();
};
using IModbusChannelPtr = std::shared_ptr<IModbusChannel>;

IModbusChannelPtr createChannel();

/** \brief Загрузить протокол команд (регистров) устройства.
 *  \param fileName - путь к файлу протокола.
 *  \param typeIdent - идентификатор типа устройства.
 *  \return протокол команд (регистров).
 */
Dk::ICommProtocolPtr loadProtocol(const std::string& fileName, const std::string& typeIdent);


///////////////////////////////////////////////////////////////////////////////
} //namespace Modbus

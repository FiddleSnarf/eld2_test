#include "EldCommands.h"
#include "Eld7ModbusDriver.h"
#include <bitset>
#include <QDebug>

using namespace DeviceControl;
using namespace Modbus;

namespace
{
    Modbus::RegAddress getAddrSelectionDI(const Eld7ModbusDriver::EnDiscreteInputNum& di)
    {
        constexpr Modbus::RegAddress startAddr = 0x0401;
        return startAddr + static_cast<uint16_t>(di) * 2;
    }
}


Eld7ModbusDriver::Eld7ModbusDriver(const Parameters& params)
    : m_params(params)
{
    configureAsyncRead();

    connect(m_params.modbusChannel.get(), &Modbus::IModbusChannel::signalConnectionStateChanged, this, [this](Dk::AddrType addr, bool connected)
    {
        if (addr != m_params.modbusAddr)
            return;

        if (connected)
        {
            setStatus(EnDriveStatus::enInitialization);
            if (auto res = initialize(); !res)
                emit setStatus(EnDriveStatus::enInitializationError);
            else
                emit setStatus(EnDriveStatus::enInitializationCompleted);
        }
        else
        {
            resetCache();
        }
    });
}

Eld7ModbusDriver::~Eld7ModbusDriver()
{
    stopMoving();
    setServoOn(false);
    for (auto id : m_asyncReadIdents)
        m_params.modbusChannel->removeReadRegisters(id);
}

ServoDriveConfig Eld7ModbusDriver::getConfiguration() const
{
    return m_params.config;
}

bool Eld7ModbusDriver::hasFeature(EnFeature feature) const
{
    switch (feature)
    {
    case EnFeature::enSpeedLimitationForTorqueControl:
        return true;
    case EnFeature::enPositionSensorIsAbsoluteEncoder:
        return m_params.config.positionSensorIsAbsEncoder;
    }

    return false;
}

bool Eld7ModbusDriver::connectionStatus() const
{
    return m_actualStatusCodes.has_value();
}

std::optional<int16_t> Eld7ModbusDriver::currentRotorSpeed() const
{
    return m_actualSpeed;
}

bool Eld7ModbusDriver::setAmperageLimit(int percentOfMaximum)
{
    return writeHoldingRegister(Eld::enTorqueLimit, percentOfMaximum);
}

bool Eld7ModbusDriver::resetPosition()
{
    if (m_params.config.positionSensorIsAbsEncoder)
    {
        // Homing при использовании абсолютного энкодера отличается от обычного
        const auto res = readHoldingRegister(Eld::enAbsoluteEncoderSetting);

        if (!res.has_value())
        {
            toLog(tr("Fail to read actual settings of absolute encoder"), true);
            return false;
        }
            
        if (res.value().words.at(0) != 1)
        {
            toLog(tr("In order to start homing process need to set the value of absolute encoder settings to 1 (Multiturn absolute linear)"), true);
            return false;
        }

        return setServoOn(false) && writeHoldingRegister(Eld::enAbsoluteEncoderSetting, 9); // Выполняем сброс позиции
    }

        // устанавливаем текущую позицию (по энкодеру) нулевой
    return setServoOn(true) && writeHoldingRegister(Eld::EnModbusRegister::enControlRegister, 0x021); // Manually set currently position as 0 (Origin)
}

void Eld7ModbusDriver::resetError()
{
    // Pr0.25
    // 0x1111 - Reset current alarm
    // 0x1122 - Reset record alarm

    writeHoldingRegister(Eld::EnModbusRegister::enAuxiliaryFunction, 0x1111);
}

std::vector<FaultCode> Eld7ModbusDriver::getDriveErrors() const
{
    if (!m_actualFaultCodes.has_value())
        return {};

    // формат представления кода ошибки:
    // младший байт - код ошибки в пределах группы
    // байты 1 и 2 - код группы ошибок

    uint16_t code = *m_actualFaultCodes;
    FaultCode result;
    result.mainFault = code >> 4;
    result.subFault = code & 0x000F;
    return { result };
}

EnDriveStatus Eld7ModbusDriver::actualStatus() const
{
    return m_actualStatus;
}

bool Eld7ModbusDriver::isRunning() const
{
    return m_isRunning;
}

bool Eld7ModbusDriver::startSpeedRegulation(int16_t targetSpeed)
{
    const bool ok =
        setCMode(false)
        && setServoOn(true)
        && writePrMode(Eld::PrMode::enVelocity)
        && writeHoldingRegister(Eld::EnModbusRegister::enPath0_Velocity, targetSpeed)
        && writeHoldingRegister(Eld::EnModbusRegister::enControlRegister, 0x0010);  // Trigger Path0 motion

    if (!ok)
    {
        setServoOn(false);

        toLog(tr("Fail to start speed regulation process - communication error"), true);
        return false;
    }

    setIsDriveRunning(true);
    return true;
}

bool Eld7ModbusDriver::startAmperageRegulation(int32_t targetAmperage_mA, std::optional<int16_t> speedLimit)
{
    // Через path упралять по усилию нельзя
    // Для Pr0.01 в таблице видим что для режима 6 есть 2 столбца 1st и 2nd mode.
    // Переключение между этими режимами выполняется с помощью c-mode
    // В первом режиме управление осуществляется с помощью path (один или несколько шагов в режиме по скорости и позиции),
    // во втором мы можем переключаться между Position/Velocity/Torque в обычном режме работы (без path)
    // Для управления по моменту мы задаем все необхоимые уставки и задаем pr0.22 = 2 (Torque), после
    // этого включаем c-mode (переключаемся в 2nd mode), это служит триггером для начала движения

    if (!m_params.config.maxAmperage.has_value())
    {
        toLog(tr("Driver amperage limit not configured"), true);
        return false;
    }

    if (auto res = setServoOn(true); !res)
        return res;

    if (speedLimit.has_value())
    {
        if (auto res = writeHoldingRegister(Eld::enVelLimitInTorqueMode, std::abs(speedLimit.value())); !res)
            return res;
    }

    const uint16_t reverse = targetAmperage_mA > 0 ? 0 : 1;
    const float maxAmparage_mA = 1000.f * m_params.config.maxAmperage.value();
    const uint16_t percentage = std::abs(100.f * targetAmperage_mA / maxAmparage_mA);

    bool ok = 
        writeHoldingRegister(Eld::enTorqueInputInversion, reverse)
        && writeHoldingRegister(Eld::enTorqueCommand, percentage)
        && writeHoldingRegister(Eld::enPRandPST_Switching, static_cast<uint16_t>(Eld::PST_Mode::enPrTorque))
        && setCMode(true);

    if (!ok)
    {
        setServoOn(false);

        toLog(tr("Fail to start torque regulation process - communication error"), true);
        return false;
    }

    setIsDriveRunning(true);
    return true;
}

bool Eld7ModbusDriver::startPositioning(int32_t targetPosition, int16_t speed)
{
    // Ниже комментарий из проекта "Колонны" (https://swdevelop.diakont.com:8446/da99-build-line/id8883_column.git)
    // TODO Выход в абсолютную позицию не работает корректно после перемещения в режиме удержания момента
    // При перемещении по моменту не меняется какой-то внутренний счетчик (При этом в регистре из которого сейчас читается текущее положение
    // значение меняется) и из-за этого выход в позицию в дальнейшем выполняется некорректно. Необходимо разобраться   

    m_targetPosition = targetPosition;

    const bool ok =
        setCMode(false)
        && setServoOn(true)
        && writePrMode(Eld::PrMode::enPositioning)
        && writePositionCode(targetPosition)
        && writeHoldingRegister(Eld::EnModbusRegister::enPath0_Velocity, static_cast<int16_t>(speed))
        && writeHoldingRegister(Eld::EnModbusRegister::enControlRegister, 0x0010);  // Trigger Path0 motion

    if (!ok)
    {
        m_targetPosition.reset();
        setServoOn(false);

        toLog(tr("Fail to start positioning process - communication error"), true);
        return false;
    }

    setIsDriveRunning(true);
    return true;
}

std::optional<int32_t> Eld7ModbusDriver::currentPosition() const
{
    return m_actualPosition;
}

void Eld7ModbusDriver::hardStop()
{
    stopMoving();
    setServoOn(false);
}

void Eld7ModbusDriver::stopMoving()
{
    writePrMode(Eld::PrMode::enEStop);

    // Для режима управления по моменту
    writeHoldingRegister(Eld::enTorqueCommand, 0);

    if (m_targetPosition.has_value())
    {
        m_targetPosition.reset();
        emit signalPositioningInterrupted();
    }

    // флаг EnStatusBit::enRUN не означает zero speed
    setIsDriveRunning(false);
}

bool Eld7ModbusDriver::servoOn()
{
    return setServoOn(true);
}

bool Eld7ModbusDriver::servoOff()
{
    return setServoOn(false);
}

void Eld7ModbusDriver::configureAsyncRead()
{
    auto addRegisters = [this](Modbus::AsyncReadParameters& params, auto callback)
    {
        for (auto& reg : params.registers)
            reg.isSigned = Eld::isSigned(static_cast<Eld::EnModbusRegister>(reg.modbusAddr));

        auto id = m_params.modbusChannel->configAsyncRead(m_params.modbusAddr, params, callback);
        m_asyncReadIdents.push_back(id);
    };

    {
        // формируем последовательность регистров таким образом, чтобы они были прочитаны за однин запрос
        Modbus::AsyncReadParameters async_params;
        async_params.batchRead = true;
        async_params.registers =
        {
            ModbusRegister(Eld::EnModbusRegister::enCurrentAlarm),
            ModbusRegister(Eld::EnModbusRegister::enNotRotatingReason),
            ModbusRegister(Eld::EnModbusRegister::enDriveOperationStatus),
            ModbusRegister(Eld::EnModbusRegister::enActualMotorSpeed),
            ModbusRegister(Eld::EnModbusRegister::enTorquePercent),
            ModbusRegister(Eld::EnModbusRegister::enAmperage),
            //ModbusRegister(Eld::EnModbusRegister::enFilteredMotorSpeed),
            //ModbusRegister(Eld::EnModbusRegister::enDcBusVoltage),
            //ModbusRegister(Eld::EnModbusRegister::enDriveTemperature),
            //ModbusRegister(Eld::EnModbusRegister::enExtAnalog1),
            //ModbusRegister(Eld::EnModbusRegister::enExtAnalog2),
            //ModbusRegister(Eld::EnModbusRegister::enExtAnalog3),
            //ModbusRegister(Eld::EnModbusRegister::enMotorOverloadRatio),
            //ModbusRegister(Eld::EnModbusRegister::enRegenerationRatio),
        };


        addRegisters(async_params, [this](RegAddress addr, const ReadResult& value)
        {
            switch (addr)
            {
            case Eld::EnModbusRegister::enCurrentAlarm:
                handlerRealtimeFault(value);
                break;
            case Eld::EnModbusRegister::enDriveOperationStatus:
                handlerRealtimeStatus(value);
                break;
            case Eld::EnModbusRegister::enAmperage:
                handlerAmperage(value);
                break;
            //case Eld::EnModbusRegister::enFilteredMotorSpeed:
            case Eld::EnModbusRegister::enActualMotorSpeed:
                handlerMeasSpeed(value);
                break;
            };
        });
    }

    {
        // читаем регистр enMotorAbsPosition, в котором учитывается сброс позиции в 0 в процессе хоминга
        Modbus::AsyncReadParameters reg_position

        {
            Modbus::ModbusRegister(Eld::EnModbusRegister::enMotorAbsPosition, EnRegSize::en32bit, EnRegType::enHoldingReg, true)
        };

        addRegisters(reg_position, [this](RegAddress /*addr*/, const ReadResult& value)
        {
            handlerPos(value);
        });
    }
}

bool Eld7ModbusDriver::initialize()
{
    writeHoldingRegister(Eld::EnModbusRegister::enTorqueIntExtSwitch, 2);               // задаем параметры управления по усилию
    writeHoldingRegister(Eld::EnModbusRegister::enPositioningCompleteOutput, 1);        // Параметр 4.32 должен быть == 1, иначе флаг статуса окончания выхода в позицию не работает нормально

    // Выключаем CMode
    auto res = setCMode(false);
    if (!res)
        return res;

    if (!m_params.config.hasBrake)
    {
        // разрешение работы привода
        return setServoOn(true);
    }

    return true;
}

bool Eld7ModbusDriver::setServoOn(bool enable)
{
    const uint16_t data = enable ? 0x83 : 0x03;
    return writeHoldingRegister(getAddrSelectionDI(m_params.servoOnInput), data);
}

bool Eld7ModbusDriver::setCMode(bool enable)
{
    const uint16_t data = enable ? 0x85 : 0x05;
    return writeHoldingRegister(getAddrSelectionDI(m_params.cModeInput), data);
}

bool Eld7ModbusDriver::writePrMode(Eld::PrMode mode)
{
    return writeHoldingRegister(Eld::EnModbusRegister::enPath0_Mode, static_cast<uint16_t>(mode));
}

bool Eld7ModbusDriver::writePositionCode(int32_t code)
{
    const ModbusRegister reg(Eld::EnModbusRegister::enPath0_Position, EnRegSize::en32bit, EnRegType::enHoldingReg, true);
    return writeHoldingRegister(reg, code);
}

bool Eld7ModbusDriver::writeHoldingRegister(const Modbus::ModbusRegister& reg, int16_t code)
{
    return m_params.modbusChannel->writeRegisterSync(m_params.modbusAddr, reg, code);
}

std::optional<ReadResult> Eld7ModbusDriver::readHoldingRegister(RegAddress address)
{
    return m_params.modbusChannel->readRegisterSync(m_params.modbusAddr, address);
}

void Eld7ModbusDriver::handlerRealtimeFault(const ReadResult& value)
{
    m_actualFaultCodes = value.words.at(0);
}

void Eld7ModbusDriver::handlerRealtimeStatus(const ReadResult& value)
{
    if (m_actualStatusCodes.has_value() && *m_actualStatusCodes == value.words.at(0))
        return;

    auto flagState = [status_mask = std::bitset<16>{ value.words.at(0) }](auto bit_pos)
        {
            return status_mask.test(static_cast<size_t>(bit_pos))
                ? FlagState::Hi 
                : FlagState::Lo;
        };

    // флаг выполнения выхода в позицию
    if (m_targetPosition.has_value())
    {
        if (auto positioningState = flagState(Eld::EnStatusBit::enINP); m_positioningActive != positioningState)
        {
            m_positioningActive = positioningState;
            if (positioningState == FlagState::Lo)
            {
                emit signalStartPositioning(m_targetPosition.value());
            }
            else if (positioningState == FlagState::Hi)
            {
                emit signalPositioningCompleted();
                setIsDriveRunning(false);
                m_targetPosition.reset();
            }
        }
    }

    if (!m_params.config.positionSensorIsAbsEncoder)
    {
        if (auto homingState = flagState(Eld::EnStatusBit::enHOME_OK); m_homingState != homingState)
        {
            m_homingState = homingState;
            if (m_homingState == FlagState::Hi)
                emit signalAbsolutePositionDetected();
        }
    }

    if (auto errorState = flagState(Eld::EnStatusBit::enERR); m_errorState != errorState)
    {
        m_errorState = errorState;
        setStatus(m_errorState == FlagState::Hi 
            ? EnDriveStatus::enStatusError 
            : EnDriveStatus::enStatusOk);
    }

    if (auto runningState = flagState(Eld::EnStatusBit::enRUN); m_runningState != runningState)
    {
        m_runningState = runningState;
        if (m_runningState == FlagState::Lo && m_params.config.hasBrake)
        {
            // чтобы не перегревать тормоз, после завершения движения выключаем привод - тормоз наложится автоматически
            setServoOn(false);
        }

        setIsDriveRunning(runningState == FlagState::Hi);
    }

    if (auto readyState = flagState(Eld::EnStatusBit::enRDY); m_readyState != readyState)
    {
        m_readyState = readyState;
        // todo: определить, что означает статус ready
    }
}

void Eld7ModbusDriver::handlerAmperage(const ReadResult& value)
{
    const auto rawAmperage = static_cast<int16_t>(value.words.at(0));
    if (!m_actualAmperage.has_value() || rawAmperage != m_actualAmperage)
    {
        // Выяснилось что реальность не соответствует документации и измеренный ток получаем в %
        m_actualAmperage = rawAmperage;
        const float ampeargePercent = std::abs(rawAmperage / 10.f);

        // TODO Есть теория что нужно умножать на корень из 2х чтобы получить реальные значения [sungurov]
        const float currentAmperage = m_params.config.maxAmperage.value_or(0.f) * ampeargePercent / 100.f;// *std::sqrt(2.);

        emit signalAmperageChanged(currentAmperage);
    }
}

void Eld7ModbusDriver::handlerMeasSpeed(const ReadResult& value)
{
    const auto rpm = static_cast<int16_t>(value.words.at(0));
    if (!m_actualSpeed.has_value() || rpm != m_actualSpeed)
    {
        m_actualSpeed = rpm;
        emit signalSpeedChanged(rpm);
    }
}

void Eld7ModbusDriver::handlerPos(const ReadResult& value)
{
    if (m_homingState != FlagState::Hi)
    {
        if (!m_actualPosition.has_value() && m_params.config.positionSensorIsAbsEncoder)
        {
            // Если драйвер в режиме абсолютного энкодера 
            // TODO по идее надо проверять наличие ошибки 153 для случая первого включения
            const auto res = readHoldingRegister(Eld::enAbsoluteEncoderSetting);
            if (res.has_value() && res.value().words.at(0) == 1)
            {
                m_homingState = FlagState::Hi;
                emit signalAbsolutePositionDetected();
            }
        }
    }

    const auto pos = static_cast<int32_t>(value.dwords.at(0));
    if (!m_actualPosition.has_value() || pos != m_actualPosition)
    {
        m_actualPosition = pos;
        emit signalPositionChanged(pos);
    }
}

void Eld7ModbusDriver::resetCache()
{
    m_actualPosition.reset();
    m_actualSpeed.reset();
    m_actualAmperage.reset();
    m_actualStatusCodes.reset();
    m_actualFaultCodes.reset();

    m_brakeState = FlagState::Unknown;
    m_homingState = FlagState::Unknown;
    m_errorState = FlagState::Unknown;
    m_readyState = FlagState::Unknown;
    m_runningState = FlagState::Unknown;
    m_positioningActive = FlagState::Unknown;
}

void Eld7ModbusDriver::setIsDriveRunning(bool on)
{
    if (m_isRunning == on)
        return;
    m_isRunning = on;

    emit signalRunningStatusChanged(on);
}

void Eld7ModbusDriver::setStatus(EnDriveStatus new_status)
{
    if (m_actualStatus == new_status)
        return;

    m_actualStatus = new_status;
    emit signalDriveStatusChanged(new_status);
}

void Eld7ModbusDriver::toLog(const QString& mess, bool err)
{
    if (err)
        qWarning() << mess;
    else
        qDebug() << mess;
}

#pragma once

#include "DeviceTypes.h"
#include "../Modbus/IModbusChannel.h"
#include <QObject>
#include <map>
#include <optional>
#include <memory>

namespace Eld
{
    enum class PrMode : uint16_t;
}

namespace DeviceControl
{

class Eld7ModbusDriver : public QObject
{
    Q_OBJECT

public:
    enum EnFeature
    {
        enSpeedLimitationForTorqueControl,      // поддерживает ограничение скорости при управлении по усилию
        enPositionSensorIsAbsoluteEncoder,      // датчик положения является абсолютным энкодером
    };

    enum class EnDiscreteInputNum
    {
        DI_1 = 0,
        DI_2 = 1,
        DI_3 = 2,
        DI_4 = 3,
        DI_5 = 4,
        DI_6 = 5,
        DI_7 = 6,
        DI_8 = 7
    };

    struct Parameters
    {
        UniqueIdent boardIdent;
        ServoDriveConfig config;
        uint32_t modbusAddr;
        Modbus::IModbusChannelPtr modbusChannel;

        EnDiscreteInputNum servoOnInput = EnDiscreteInputNum::DI_5;
        EnDiscreteInputNum cModeInput = EnDiscreteInputNum::DI_3;
    };

    explicit Eld7ModbusDriver(const Parameters& params);
    virtual ~Eld7ModbusDriver();

    ServoDriveConfig getConfiguration() const;
    bool hasFeature(EnFeature feature) const;
    bool connectionStatus() const;
    std::optional<int32_t> currentPosition() const;
    std::optional<int16_t> currentRotorSpeed() const;
    bool setAmperageLimit(int percentOfMaximum);
    bool resetPosition();
    bool startPositioning(int32_t targetPosition, int16_t speed);
    bool startSpeedRegulation(raw_t targetSpeed);
    bool startAmperageRegulation(int32_t targetAmperage_mA, std::optional<raw_t> speedLimit);
    void hardStop();
    void stopMoving();
    void resetError();
    bool isRunning() const;
    std::vector<FaultCode> getDriveErrors() const;
    EnDriveStatus actualStatus() const;

    bool servoOn();
    bool servoOff();

signals:
    /** \brief Сигнал испускается при остановке движения привода. */
    void signalStopMoving();

    /** \brief Сигнал испускается при изменении статуса привода. */
    void signalDriveStatusChanged(DeviceControl::EnDriveStatus status);

    /** \brief Сигнал испускается при изменении тока привода.
     */
    void signalAmperageChanged(float ampers);

    /** \brief Сигнал испускается при изменении позиции привода.
      * \arg position - Положение, град.
     */
    void signalPositionChanged(int position);

    /** \brief Сигнал испускается при изменении скорости привода.
      * \arg speed - Скорость, rpm.
     */
    void signalSpeedChanged(int speed);

    /** \brief Сигнал о начале процесса позиционирования.
      * \arg position - Целевое положение, град.
     */
    void signalStartPositioning(int targetPosition);

    /** \brief Сигнал об успешном выполнении процесса позиционирования.
     */
    void signalPositioningCompleted();

    /** \brief Сигнал о прерывании процесса позиционирования.
     */
    void signalPositioningInterrupted();

    /** \brief Сигнал о изменении статуса движения движения.
     */
    void signalRunningStatusChanged(bool isRunning);

    /** \brief Сигнал об определении абсолютного положения.
     */
    void signalAbsolutePositionDetected();

private:
    const Parameters m_params;
    std::optional<int32_t> m_actualPosition;
    std::optional<int16_t> m_actualSpeed;
    std::optional<int16_t> m_actualAmperage;
    std::optional<uint16_t> m_actualStatusCodes;
    std::optional<uint16_t> m_actualFaultCodes;
    bool m_activationStatus = false;

    std::vector<Modbus::UniqueIdent> m_asyncReadIdents;
    std::optional<int32_t> m_targetPosition;

    enum class FlagState
    {
        Lo, Hi, Unknown
    };

    FlagState m_brakeState = FlagState::Unknown;
    FlagState m_homingState = FlagState::Unknown;
    FlagState m_errorState = FlagState::Unknown;
    FlagState m_readyState = FlagState::Unknown;
    FlagState m_runningState = FlagState::Unknown;
    FlagState m_positioningActive = FlagState::Unknown;

    bool m_isRunning = false;
    EnDriveStatus m_actualStatus = EnDriveStatus::enConnectionLost;

private:
    void configureAsyncRead();
    bool initialize();
    bool setServoOn(bool enable);
    bool setCMode(bool enable);

    bool writePrMode(Eld::PrMode mode);
    bool writePositionCode(int32_t code);
    bool writeHoldingRegister(const Modbus::ModbusRegister& reg, int16_t code);
    std::optional<Modbus::ReadResult> readHoldingRegister(Modbus::RegAddress address);

    // =================== handlers ===========================
    void handlerRealtimeFault(const Modbus::ReadResult&);
    void handlerRealtimeStatus(const Modbus::ReadResult&);
    void handlerAmperage(const Modbus::ReadResult&);
    void handlerMeasSpeed(const Modbus::ReadResult&);
    void handlerPos(const Modbus::ReadResult&);

    void resetCache();
    void setIsDriveRunning(bool on);
    void setStatus(EnDriveStatus new_status);

    void toLog(const QString& mess, bool err);
};

} // namespace DeviceControl

#pragma once

#include <cstdint>

namespace Eld
{

enum EnModbusRegister : uint16_t
{
    enAbsoluteEncoderSetting    = 0x001F,
    enEncoderPulsesPerRev       = 0x0017,    // Encoder output pulse count per revolution
    enTorqueLimit               = 0x001B,

    enCurrentAlarm              = 0x0B03,   // Current alarm. 
    enNotRotatingReason         = 0x0B04,   // Motor not rotating cause.
    enDriveOperationStatus      = 0x0B05,   // Driver operation status.
    enActualMotorSpeed          = 0x0B06,   // Motor speed（Before filter）, rpm
    enTorquePercent             = 0x0B07,   // Motor torque, %
    enAmperage                  = 0x0B08,   // Motor current, 0.01(santi)А
    enFilteredMotorSpeed        = 0x0B09,   // Motor speed (After filter), rpm
    enDcBusVoltage              = 0x0B0A,   // DC bus voltage, V
    enDriveTemperature          = 0x0B0B,   // Driver temperature, °C
    enExtAnalog1                = 0x0B0C,   // External analog 1
    enExtAnalog2                = 0x0B0D,   // External analog 2
    enExtAnalog3                = 0x0B0E,   // External analog 3
    enMotorOverloadRatio        = 0x0B0F,   // Over-load ratio, %
    enRegenerationRatio         = 0x0B10,   // Regeneration load ratio, %
    enDiStatus                  = 0x0B11,   // Digital input signal status
    enDoStatus                  = 0x0B12,   // Digital output signal status
    enMotorPosition_CU          = 0x0B14,   // Command position (Command unit), P


    enTorqueIntExtSwitch        = 0x0323,   // Torque internal/external switching
    enTorqueInputInversion      = 0x0329,   // Torque command input inversion
    enVelLimitInTorqueMode      = 0x032B,   // Velocity limit in torque mode
    enTorqueCommand             = 0x032D,   // Torque command
    enPositioningCompleteOutput = 0x0441,   // Positioning complete output setting
    enAuxiliaryFunction    = 0x0033,
    enPRandPST_Switching   = 0x002D,
    enControlRegister      = 0x6002,
    enMotorAbsPosition     = 0x602C,
    enPath0_Mode           = 0x6200,
    enPath0_Position       = 0x6201,
    enPath0_Velocity       = 0x6203,
};

bool isSigned(Eld::EnModbusRegister id)
{
    switch (id)
    {
        case EnModbusRegister::enActualMotorSpeed:
        case EnModbusRegister::enTorquePercent:
        case EnModbusRegister::enAmperage:
        case EnModbusRegister::enFilteredMotorSpeed:
        case EnModbusRegister::enDcBusVoltage:
        case EnModbusRegister::enDriveTemperature:
        case EnModbusRegister::enExtAnalog1:
        case EnModbusRegister::enExtAnalog2:
        case EnModbusRegister::enExtAnalog3:
        case EnModbusRegister::enMotorOverloadRatio:
        case EnModbusRegister::enRegenerationRatio:
        case EnModbusRegister::enMotorPosition_CU:
        case EnModbusRegister::enMotorAbsPosition:
        case EnModbusRegister::enPath0_Position:
        case EnModbusRegister::enPath0_Velocity:
            return true;
    }
    return false;
}

enum class PST_Mode : uint16_t
{
    enPrPositioning = 0x00,     // Позиционирование
    enPrVelocity    = 0x01,     // Скорость
    enPrTorque      = 0x02,     // Момент усилия

};

enum class PrMode : uint16_t
{
    enNull = 0,
    enPositioning,  // Позиционирование
    enVelocity,     // Скорость
    enHoming,       // Homing
    enEStop         // Останов
};

enum EnStatusBit
{
    enRDY       = 0, // RDY Servo is ready
    enRUN       = 1, // RUN Servo is running
    enERR       = 2, // ERR Driver error
    enHOME_OK   = 3, // HOME_OK Homing completed
    enINP       = 4, // INP In position
    enAT_SPEED  = 5, // AT - SPEED Velocity reached
};

}
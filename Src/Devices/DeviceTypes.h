#pragma once

#include <QMetaType>
#include <string>
#include <memory>
#include <cmath>
#include <map>
#include <optional>
#include <vector>
#include <unordered_set>


///////////////////////////////////////////////////////////////////////////////
namespace DeviceControl
{
///////////////////////////////////////////////////////////////////////////////

using raw_t = float;
using clb_t = float;

enum class EnDriveStatus
{
    enConnectionLost,
    enInitialization,
    enInitializationError,
    enInitializationCompleted,
    enStatusOk,
    enStatusError
};

using UniqueIdent = std::string;

/*  \struct ServoDriveConfig
 *  \brief Конфигурация сервопривода.
 */
struct ServoDriveConfig
{
    UniqueIdent ident;
    std::string driverModel;             // Модель сервопривода
    std::optional<float> maxAmperage;    // Максимальный ток, амперы.
    bool hasBrake = false;               // Опция - наличие тормоза на приводе.
    bool positionSensorIsAbsEncoder = false;
};

/** \struct FaultCode
 *  \brief Структура содержит поля кода ошибки.
 */
struct FaultCode
{
    uint mainFault;
    uint subFault;
};


///////////////////////////////////////////////////////////////////////////////
}   //namespace DeviceControl
///////////////////////////////////////////////////////////////////////////////

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

template <class T>
struct Interval
{
    T lowerBound;    /**< минимальное значения. */
    T upperBound;    /**< максимальное значения. */
    T abs_length() const
    {
        return std::abs(upperBound - lowerBound);
    }
    bool contains(T value) const
    {
        return value >= lowerBound && value <= upperBound;
    }

    void normalize()
    {
        if (lowerBound > upperBound)
            std::swap(lowerBound, upperBound);
    }

    T length() const
    {
        return (upperBound - lowerBound);
    }

    T bounded(T value) const
    {
        return
            value < lowerBound
            ? lowerBound
            : value > upperBound
            ? upperBound
            : value;
    }

};

using FloatInterval = Interval<float>;

using raw_t = float;
using clb_t = float;
using RawInterval = Interval<raw_t>;
using ClbInterval = Interval<clb_t>;

enum class EnDriveStatus
{
    enConnectionLost,
    enInitialization,
    enInitializationError,
    enInitializationCompleted,
    enStatusOk,
    enStatusError
};

/*  \enum class EnDiscreteState
 *  \brief Перечисление дискретных состояний
 */
enum class EnDiscreteState
{
    enActive,
    enNonActive,
    enUnknown
};

/*! \enum EnMoveDirection
 *  \brief Перечисление направлений перемещения.
 */
enum class EnMoveDirection
{
    enUnknown   = 0x00,  // Направление не определено.
    enForward   = 0b01,  // Вперед.
    enBackward  = 0b10,  // Назад.
    enAll       = 0b11,  // Во все стороны.
};

/*! \enum EnBlockingState
 *  \brief Перечисление состояний блокировок.
 */
enum class EnBlockingState
{
    enNoBlocking  = 0x00,  // Ни одно направление не заблокировано.
    enForward     = 0b01,  // Заблокировано условное направление "Вперед".
    enBackward    = 0b10,  // Заблокировано условное направление "Назад".
    enFullBlocked = 0b11,  // Заблокированы все направления.
};


using ConnectionId = std::string;
using ModuleId = std::string;
using BoardIdent = std::string;
using UniqueIdent = std::string;
using LockId = int;

using UniqueIdents = std::unordered_set<DeviceControl::UniqueIdent>;

using DiscretsStateMap = std::map<uint8_t, EnDiscreteState>;

enum class EnBoardStatus
{
    enUnknown = -1,
    enConnectionOK,             // Статус подключения к каналу связи - если канал поддерживает данный функционал.
    enPingOK,                   // Получен ответ на команду пинг (если команда определена в протоколе платы).
    enPingError,                // Отсутствует ответ на команду пинг (если команда определена в протоколе платы).
    enConnectionError,          // Обрыв связи (отключение от канала связи либо отсутствие ответа на пинг).
    enStartActualRequest,       // Начат общий запрос к плате.
    enActualRequestCompleted,   // Завершен общий запрос к плате - состояние всех команд протокола известно.
    enStatusOK,                 // Отсутствие ошибок по результатам диагностики.
    enStatusError,              // Диагностическая ошибка.
    enInitializationError,      // Ошибка выполнения инициализации.
    enSTO,                      // Статус safety.
};

/*  \struct LineTransform
 *  \brief Параметры линейного преобразования.
 */
struct LineTransform
{
    float k = 1.0;
    float b = 0.0;
};

using ClbPoint = std::pair<raw_t, clb_t>;

/** \struct SensorMechanics
 *  \brief Описание механики сенсоров.
 */
struct SensorMechanics
{
    UniqueIdent sensorId;                       // Идентификатор датчика, должен быть уникальным в системе.
    std::optional<float> readingsAtAxisZero;    // Показания датчика в нуле.
    std::optional<float> transformFactor;       // Коэффициент преобразования показаний датчика в физическую величину.
    std::vector<ClbPoint> clbPoints;             // Точки калибровочной кривой    
};

struct IndicatorPositioning
{
    std::optional<float> position;
    bool canUsedForParking = false;
};

/*  \struct Positioning
 *  \brief Описание параметров позиционирования.
 */
struct Positioning
{
    std::optional<FloatInterval> bounds;        // Допустимые пределы позиционирования (единицы калибровки датчика положения).
    std::string physicalUnits;                  // Единицы измерения позиции.
    float accuracy = 0.f;                       // Заявленная точность позиционирования (единицы калибровки датчика положения, больше 0).
    std::optional<EnMoveDirection> homingDir;   // Направление движения для автивного хоминга. 
    int homingRelSpeed_percent = 10;            // Относительная скорость движения для автивного хоминга, % от макс. 
    std::optional<float> parkingPosition;       // Положение "парковки" привода (транспортное положение).
    using IndicatorsInfo = std::vector<std::pair<UniqueIdent, IndicatorPositioning>>;
    IndicatorsInfo indicatorPositions;      // Расположение дискретных индикаторов в определенных позициях.
};

/** \struct SensorClbInfo
 *  \brief Описание калибровочных параметров датчиков.
 */
struct SensorClbInfo
{
    UniqueIdent sensorId;                           // Уникальный идентификатор датчика.
    std::optional<RawInterval> rawBounds;           // Пределы изменений показаний датчика (попугаи). 
    std::optional<ClbInterval> clbBounds;           // Пределы изменений калиброванных значений. 
    float accuracy = 0.f;                           // Точность измерений. (попугаи, больше 0). */
    std::optional<LineTransform> linearTransform;   // Параметры линейного преобразования показаний датчика в единицы калибровки.
    std::string physicalParameterName;             // Название физического параметра (длина, скорость, ток и т.п.). 
    std::string physicalUnits;                     // Единицы представления калиброванных значений. 
};

/*  \struct ServoDriveConfig
 *  \brief Конфигурация сервопривода.
 */
struct ServoDriveConfig
{
    UniqueIdent ident;
    std::string driverModel;             // Модель сервопривода 
    std::optional<float> maxAmperage;    // Максимальный ток, амперы. 
    std::optional<float> minSpeed_rpm;   // Минимальная скорость вращения двигателя, об/мин. 
    std::optional<float> maxSpeed_rpm;   // Максимальная скорость вращения двигателя, об/мин. 
    std::optional<float> acceleration;   // Ускорение, единицы сервопривода. 
    std::optional<float> deceleration;   // Замедление, единицы сервопривода. 
    bool hasBrake = false;               // Опция - наличие тормоза на приводе. 
    bool invertedControl = false;        // Опция - инвертировнное управление (для удобства оператора). 
    std::string work_mode;               // Режим работы сервопривода.
    bool positionSensorIsAbsEncoder = false;
};

/*  \struct BoardsGroup
 *  \brief Информация о группе плат, заведенных на одно подключение.
 */
struct BoardsGroup
{
    ConnectionId connection_id;   /**< Идентификатор канала связи. */
    std::string protocol_type;    /**< Тип протокола, одинаковый для всех плат. */
    std::string description;      /**< Описание группы плат. */
};

/*  \struct BoardDescription
 *  \brief Описание параметров одной платы управления оборудованием.
 */
struct BoardDescription
{
    std::optional<BoardsGroup> group_info;  /**< Информация о группе, в которую входит данная плата. */
    BoardIdent ident;                       /**< Идентификатор платы. */
    std::string description;                /**< Описание платы. */
    std::string protocol;                   /**< Путь к протоколу. */
    std::string board_type;                 /**< Тип платы - соответствует уникальному набору команд протокола. */
    int unique_address;                     /**< Уникальный адрес платы. */
};

/** \brief Forward-декларация классов оборудования.
 */
class IPositionSensor;
using IPositionSensorPtr = std::shared_ptr<IPositionSensor>;

class IDiscreteIndicator;
using IDiscreteIndicatorPtr = std::shared_ptr<IDiscreteIndicator>;

class ILimitSwitcher;
using ILimitSwitcherPtr = std::shared_ptr<ILimitSwitcher>;

class IServoManipulation;
using IServoManipulationPtr = std::shared_ptr<IServoManipulation>;

class IDiscreteManipulation;
using IDiscreteManipulationPtr = std::shared_ptr<IDiscreteManipulation>;

class IAbstractSensor;
using IAbstractSensorPtr = std::shared_ptr<IAbstractSensor>;

class IMechanicalModel;
using IMechanicalModelPtr = std::shared_ptr<IMechanicalModel>;

class IHardwareModel;
using IHardwareModelPtr = std::shared_ptr<IHardwareModel>;

class ICommutator;
using ICommutatorPtr = std::shared_ptr<ICommutator>;

class IDeviceModule;
using IDeviceModulePtr = std::shared_ptr<IDeviceModule>;

class ISafetyController;
using ISafetyControllerPtr = std::shared_ptr<ISafetyController>;

class IBlockingController;
using IBlockingControllerPtr = std::shared_ptr<IBlockingController>;

class IBlockingHolder;
using IBlockingHolderPtr = std::shared_ptr<IBlockingHolder>;

class IServoDriver;
using IServoDriverPtr = std::shared_ptr<IServoDriver>;

class IServoTorqueControl;
using IServoTorqueControlPtr = std::shared_ptr<IServoTorqueControl>;

class IServoSpeedControl;
using IServoSpeedControlPtr = std::shared_ptr<IServoSpeedControl>;

class IServoPositionControl;
using IServoPositionControlPtr = std::shared_ptr<IServoPositionControl>;

class IAbsPositionDetector;
using IAbsPositionDetectorPtr = std::shared_ptr<IAbsPositionDetector>;

class ICalibrationConfig;
using ICalibrationConfigPtr = std::shared_ptr<ICalibrationConfig>;

class ILinearRegulator;
using ILinearRegulatorPtr = std::shared_ptr<ILinearRegulator>;

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

Q_DECLARE_METATYPE(DeviceControl::EnBlockingState)
Q_DECLARE_METATYPE(DeviceControl::EnDiscreteState)

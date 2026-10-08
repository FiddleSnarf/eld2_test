/******************************************************************************
 * Copyright ( C ) АО "Диаконт"
 * Project: DkCommLIb
 * File: CommTypes.h
 * Description: Объявление публичных типов данных.
 ******************************************************************************/

#pragma once

#include <QVariant>

#include <map>
#include <memory>
#include <vector>
#include <string>
#include <optional>

//////////////////////////////////////////////////////////////////////////
namespace Dk
{
//////////////////////////////////////////////////////////////////////////

using AddrType = uint32_t;
using CmdIndex = uint32_t;

constexpr AddrType INVALID_ADDR = ~0;

struct ProtocolInfo
{
    std::string type;
    int major_num;
    int minor_num;
};

/** struct PdoProjection
 * \brief Проекция поля PDO на команду протокола.
 */
struct PdoProjection
{
    CmdIndex originId;      /**< Идентификатор исходной команды протокола. */

    using BitLink = std::pair<int, CmdIndex>;
    std::vector<BitLink> bitLinks;      /**< Привязка отдельных битов (для команды с данными, содержащими битовую маску). */
};

/** struct CmdAttributes
 * \brief Список атрибутов команды.
 */
struct CmdAttributes
{
    int size = 0;                   /**< Размер данных.*/
    bool  readOnly = true;          /**< Команда доступна только для чтения.*/
    bool  flash    = false;         /**< Команда сохраняется во флэш память платы.*/
    bool  reboot   = false;         /**< Значение команды применяется только после перезагрузки.*/
    float minValue = -std::numeric_limits<float>::max();    /**< Минимальное значение по протоколу.*/
    float maxValue = std::numeric_limits<float>::max();     /**< Максимальное значение по протоколу.*/
    std::string units;             /**< Единицы измерения.*/
    std::string type_string;       /**< Строковое описание типа команды.*/

    std::string  representationType; /**< Способ отображения значения (hex, bit_mask и т.п.).*/

    std::map<int, std::string> bitMaskMap;         /**< Карта с описанием масок битовых полей.*/
    std::map<int, std::string> constValueMap;      /**< Карта с описанием кодов значений.*/

    /**< Описание полей для данных составного типа (например, автопакета).*/
    using Field = std::pair<std::string, CmdAttributes>;
    std::vector<Field> fields;      

    std::optional<PdoProjection> link;
};


class ICommProtocol;
using ICommProtocolPtr = std::shared_ptr<ICommProtocol>;

/** \brief Идентификатор устройства (платы).
 */
using DeviceIdent = std::string;

/** struct DeviceInfo
 * \brief Список атрибутов команды.
 */
struct DeviceInfo
{
    DeviceIdent ident;         /**< Идентификатор устройства */
    AddrType address;           /**< Адрес устройства */
    ICommProtocolPtr protocol;  /**< Протокол команд устройства */
};

/** enum class EnStatus
 * \brief Перечисление статусов выполнения команды.
 */
enum class EnStatus
{
    enUnknown,      /**< Статус неопределен. */
    enReadOk,       /**< Чтение успешно. */
    enWriteOk,      /**< Запись успешна. */
    enAccepted,     /**< Команда принята. */
    enReadError,    /**< Ошибка чтения. */
    enWriteError,   /**< Ошибка записи. */
    enMessage,      /**< Сообщение. */
    enProcessed,    /**< Команда обработана. */
    enNotProcessed, /**< Команда не обработана. */
};

struct CommData
{
    const CmdIndex index;  /**< Индекс команды. */
    EnStatus cmdStatus;    /**< Статус команды. */
    QVariant value;        /**< Значение. */

    explicit CommData(CmdIndex id, EnStatus s = EnStatus::enUnknown)
        : index(id)
        , cmdStatus(s)
    {}
};


using CommDataPtr = std::shared_ptr<CommData>;

//////////////////////////////////////////////////////////////////////////
} // namespace Dk
//////////////////////////////////////////////////////////////////////////

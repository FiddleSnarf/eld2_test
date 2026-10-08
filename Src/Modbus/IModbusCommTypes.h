/******************************************************************************
 * Copyright (C) АО "Диаконт"
 * Project     : DeviceControl
 * File        : IModbusCommTypes.h
 * Creation    :
 * Description : Файл содержит интерфейс класса для взаимодействия с устройствами по modbus.
 ******************************************************************************/

#pragma once

#include <memory>
#include <functional>
#include <array>
#include <vector>

namespace Modbus
{
///////////////////////////////////////////////////////////////////////////////

using RegAddress = uint16_t;


/** \enum class EnRegSize
 *  \brief Перечисление размеров регистра.
 */
enum class EnRegSize
{
    enUnknown = 0,
    en16bit = 1,
    en32bit = 2,
    en64bit = 4
};

/** \enum class EnRegType
 *  \brief Перечисление типов регистров.
 */
enum class EnRegType
{
    enUnknown,
    enInputReg,         /**< 16-битные входы (Read only). */
    enHoldingReg,       /**< 16-битные выходы (R/W). */
    enDiscreteInput,    /**< Дискретные входы (Read only). */
    enCoil              /**< Дискретные выходы (R/W). */
};

/** \struct ModbusRegister
 *  \brief Стуктура описывает параметры регистра modbus (Возможен составной регистр - из нескольких modbus регистров).
 */
struct ModbusRegister
{
    ModbusRegister() {};
    ModbusRegister(RegAddress _modbusAddr, EnRegSize _size = EnRegSize::en16bit, EnRegType _regType = EnRegType::enHoldingReg, bool _swapWords = false)
        : modbusAddr(_modbusAddr)
        , regSize(_size)
        , regType(_regType)
        , swapWords(_swapWords)
    {
    }

    RegAddress modbusAddr = -1;                 // Адрес стартового регистра modbus.
    EnRegSize regSize = EnRegSize::enUnknown;   // Размер регистра.
    EnRegType regType = EnRegType::enUnknown;   // Тип регистра.
    bool swapWords = false;                     // При записи, чтении данных 2 или 4 слова требуется перестановка слов.
    bool isSigned = false;                      // Знаковое представление значений
};

/** \struct ReadResult
 *  \brief Результат чтения данных регистра.
 */
struct ReadResult
{
    union
    {
        uint64_t qword;
        std::array<uint32_t, 2> dwords;
        std::array<uint16_t, 4> words;
        std::array<uint8_t, 8> discrets;
    };
};

///////////////////////////////////////////////////////////////////////////////
} // namespace Modbus
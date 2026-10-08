#pragma once

#include "IModbusCommTypes.h"

namespace Modbus
{

    /** \brief Функция возвращает размер регистра (количество 16-ти битных слов) */
    uint getWordsCount(const ModbusRegister& reg)
    {
        switch (reg.regSize)
        {
        case EnRegSize::en16bit:
            return 1;
        case EnRegSize::en32bit:
            return 2;
        case EnRegSize::en64bit:
            return 4;
        }

        return 0;
    }

    /** \brief Шаблонная функция для изменения порядка следования элементов (в случае Modbus - слов) массива */
    template <class T>
    void swapWords(T& buffer, size_t size)
    {
        if (size < 2 || std::size(buffer) < size)
            return;

        for (size_t i = 0, j = size - 1; i < j; ++i, --j)
            std::swap(buffer[i], buffer[j]);
    }

    /** \brief Функция для упаковки данных в "сырой" формат modbus */
    std::vector<uint16_t> packModbusRaw(const ModbusRegister& reg, const QVariant& value)
    {
        auto pack = [&reg](const auto v) -> std::vector<uint16_t>
            {
                auto size = getWordsCount(reg);
                std::vector<uint16_t> rawData(size);
                memcpy(rawData.data(), &v, size * 2);

                if (reg.swapWords)
                    swapWords(rawData, size);

                return rawData;
            };

        switch (reg.regSize)
        {
        case EnRegSize::en16bit:
            return reg.isSigned
                ? pack(value.value<int16_t>())
                : pack(value.value<uint16_t>());
        case EnRegSize::en32bit:
            return reg.isSigned
                ? pack(value.toInt())
                : pack(value.toUInt());
        case EnRegSize::en64bit:
            return reg.isSigned
                ? pack(value.toLongLong())
                : pack(value.toULongLong());
        }

        return {};
    }

    /** \brief Функция для распаковки данных из "сырого" формата modbus */
    QVariant unpackModbusData(const ModbusRegister& reg, const ReadResult& readResult)
    {
        switch (reg.regSize)
        {
        case EnRegSize::en16bit:
            return reg.isSigned
                ? QVariant::fromValue(static_cast<int16_t>(readResult.words.at(0)))
                : QVariant::fromValue(readResult.words.at(0));
        case EnRegSize::en32bit:
            return reg.isSigned
                ? QVariant::fromValue(static_cast<int32_t>(readResult.dwords.at(0)))
                : QVariant::fromValue(readResult.dwords.at(0));
        case EnRegSize::en64bit:
            return reg.isSigned
                ? QVariant::fromValue(static_cast<int64_t>(readResult.qword))
                : QVariant::fromValue(readResult.qword);
        }

        return {};
    }
}
/******************************************************************************
 * Copyright (C) АО "Диаконт"
 * Project     : DeviceControl
 * File        : ModbusLibFactory.h
 * Creation    :
 * Description : Фабрик создания modbus-устройств.
 ******************************************************************************/

#pragma once


#include <string>
#include <memory>

/** \using ModbusPtr
 *  \brief Псевдоним указателя на modbus_t.
 */

using modbus_t = struct _modbus;
using ModbusPtr = std::shared_ptr<modbus_t>;

///////////////////////////////////////////////////////////////////////////////


namespace Modbus
{
///////////////////////////////////////////////////////////////////////////////
class ModbusLibFactory
{
public:
    /** \brief Статический метод для создания нужного помощника взаимодействия по modbus
        \ Пример для RTU - COM14: baud=115200 parity=N data=8 stop=1
        \ Пример для TCP - tcp://192.168.0.6:1235
     */
    static ModbusPtr createModbusDevice(const std::string& connectionString);
};

///////////////////////////////////////////////////////////////////////////////
} //namespace Modbus
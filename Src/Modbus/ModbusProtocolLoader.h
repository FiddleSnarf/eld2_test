/******************************************************************************
 * Copyright ( C ) АО "Диаконт"
 * Project: DkModbus
 * File: ModbusProtocolLoader.сpp
 * Description: Реализация загрузки команд протокола из xml-файла.
 ******************************************************************************/

#pragma once

#include "ModbusProtocol.h"
#include <pugixml.hpp>
#include <unordered_map>

//////////////////////////////////////////////////////////////////////////
namespace Modbus
{
//////////////////////////////////////////////////////////////////////////

class ModbusProtocolLoader
{
public:

    std::shared_ptr<ModbusProtocol> loadFromFile(const std::string& fileName, const std::string& cmndGroup = {});

private:
    /** \brief Структура, описывающая тип данных.
     */
    struct TypeInfo
    {
        int size = 0;
        EnRegType type = EnRegType::enHoldingReg;
    };
    std::unordered_map<std::string, TypeInfo> m_types;    /**< Размеры типов данных протокола.*/

private:
    void parseTypes(const pugi::xml_node& typesNode);

    std::vector<ModbusRegCmd> parseNode(const pugi::xml_node & cmdNode, const ModbusRegister& templateReg);

    /** \brief загрузка расширенных параметров команды (константные именованные значения, битовые флаги, поля составной команды)
     */
    void loadCommandExtensions(const pugi::xml_node& cmdExtNode, ModbusRegCmd& cmdAttributes);

    bool loadTypeInfo(const std::string& typeTag, Dk::CmdAttributes* cmdAttributes, ModbusRegister* regInfo = nullptr) const;
};

//////////////////////////////////////////////////////////////////////////
} // namespace PCCom
//////////////////////////////////////////////////////////////////////////

/******************************************************************************
 * Copyright ( C ) АО "Диаконт"
 * Project: DkModbus
 * File: ModbusProtocolLoader.сpp
 * Description: Реализация загрузки команд протокола из xml-файла.
 ******************************************************************************/

#include "ModbusProtocolLoader.h"

#include <limits>
#include <algorithm>
#include <sstream>
#include <optional>
#include <QDebug>

namespace
{
    Modbus::EnRegType parseRegisterType(const std::string& str)
    {
        if (str == "HoldingReg")
            return Modbus::EnRegType::enHoldingReg;

        if (str == "InputReg")
            return Modbus::EnRegType::enInputReg;
        if (str == "DiscreteInput")
            return Modbus::EnRegType::enDiscreteInput;
        if (str == "Coil")
            return Modbus::EnRegType::enCoil;

        return Modbus::EnRegType::enUnknown;
    }
}

using namespace Modbus;
using namespace pugi;

std::shared_ptr<ModbusProtocol> ModbusProtocolLoader::loadFromFile(const std::string& filePath, const std::string& cmndGroup)
{
    xml_document doc;

    if (xml_parse_result res = doc.load_file(filePath.c_str()); !res)
    {
        qWarning() << res.description();
        return {};
    }

    xml_node rootNode = doc.child("modbus");
    if (!rootNode)
    {
        qWarning() << "Missing root tag \'modbus\'.";
        return {};
    }

    Dk::ProtocolInfo protocolInfo;
    protocolInfo.type = "modbus";
    {
        std::stringstream ss(rootNode.attribute("version").as_string());
        char dummy;
        ss >> protocolInfo.major_num >> dummy >> protocolInfo.minor_num;
    }
    
    if (const auto typesNode = rootNode.child("types"))
    {
        m_types.clear();
        parseTypes(typesNode);
    }

    std::vector<ModbusRegCmd> registers;
    for (const auto& groupNode : rootNode.children("cmdgroup"))
    {
        if (!cmndGroup.empty())
        {
            const auto cmdGroupName = groupNode.attribute("name");
            if (cmdGroupName.as_string() != cmndGroup)
                continue;
        }

        // некоторые свойства регистров могут быть заданы на уровне группы команд
        ModbusRegister templateReg;
        if (auto attr = groupNode.attribute("swap_words"))
            templateReg.swapWords = attr.as_bool(false);

        for (const auto& cmdNode : groupNode.children("node"))
        {
            if (const auto& commands = parseNode(cmdNode, templateReg); !commands.empty())
                registers.insert(registers.end(), commands.begin(), commands.end());
        }


    }
    return std::make_shared<Modbus::ModbusProtocol>(protocolInfo, std::move(registers));
}

void ModbusProtocolLoader::parseTypes(const xml_node& typesNode)
{
    for (const auto& child : typesNode.select_nodes("./type[@name and @size and @group]"))
    {
        const auto& type_node = child.node();

        TypeInfo info;
        info.size = type_node.attribute("size").as_int();
        info.type = parseRegisterType(type_node.attribute("group").as_string());

        std::string type_name = type_node.attribute("name").as_string();
        std::transform(type_name.begin(), type_name.end(), type_name.begin(), ::tolower);
        m_types[type_name] = info;
    }
}

std::vector<ModbusRegCmd> ModbusProtocolLoader::parseNode(const pugi::xml_node& cmdNode, const ModbusRegister& templateReg)
{
    // загружаем узлы команд группы

    std::vector<ModbusRegCmd> commands;
    const auto node_id = cmdNode.attribute("value");
    const auto node_name = cmdNode.attribute("name");
    if (node_id.empty() || node_name.empty())
    {
        qWarning() << "At least one attribute of tag \'node\' is missing (value|name)";
        return {};
    }

    auto loadCmdDescr = [this, node_name, id = node_id.as_int(), templateReg](pugi::xml_node cmd_node)
    {
        ModbusRegCmd regCmdDescr;
        const std::string type_str = cmd_node.attribute("type").as_string();
        if (!loadTypeInfo(type_str, &regCmdDescr.cmd, &regCmdDescr.reg))
        {
            qWarning() << QString("Invalid data type %1").arg(type_str);
            return regCmdDescr;
        }

        uint16_t cmnd_id = cmd_node.attribute("operation").as_uint();
        if ((cmnd_id & 0xFF00) == 0)
            cmnd_id = (id << 8) | cmnd_id;

        regCmdDescr.node_name = node_name.as_string();
        regCmdDescr.name = cmd_node.attribute("name").as_string();
        regCmdDescr.reg.swapWords = cmd_node.attribute("swap_words").as_bool(templateReg.swapWords);
        regCmdDescr.reg.modbusAddr = cmnd_id;
        regCmdDescr.reg.isSigned = cmd_node.attribute("signed").as_bool(false);
        regCmdDescr.cmd.units = cmd_node.attribute("units").as_string();
        regCmdDescr.cmd.readOnly = cmd_node.attribute("readonly").as_bool(true);
        regCmdDescr.cmd.flash = cmd_node.attribute("attribute").as_string() == std::string("f");
        regCmdDescr.cmd.representationType = cmd_node.attribute("view_type").as_string();

        if (auto attr = cmd_node.attribute("min"); !attr.empty())
            regCmdDescr.cmd.minValue = attr.as_float();

        if (auto attr = cmd_node.attribute("max"); !attr.empty())
            regCmdDescr.cmd.maxValue = attr.as_float();

        for (const auto& extNode : cmd_node.children())
            loadCommandExtensions(extNode, regCmdDescr);

        return regCmdDescr;
    };

    // загрузка команд узла с валидным аттрибутом "operation"
    for (const auto& child_node : cmdNode.select_nodes("./cmd[@operation and @name and @type]"))
    {
        commands.emplace_back(loadCmdDescr(child_node.node()));
    }

    return commands;
}

void ModbusProtocolLoader::loadCommandExtensions(const pugi::xml_node& extNode, ModbusRegCmd& cmdAttributes)
{
    auto loadInteger = [&extNode](const auto& tag_value) -> std::optional<std::pair<int, std::string>>
    {
        const std::string name = extNode.attribute("name").as_string();
        if (name.empty())
            return {};

        const auto attr_val = extNode.attribute(tag_value);
        if (!attr_val)
            return {};

        auto val = attr_val.as_int(INT_MIN);
        if (val == INT_MIN)
            return {};

        return std::make_pair(val, name);
    };

    const std::string tag{ extNode.name() };
    if (tag == "const_value")
    {
        // константный код значения. пример: <const_value code="0" name="выкл"/>
        if (auto pair = loadInteger("code"); pair.has_value())
            cmdAttributes.cmd.constValueMap.emplace(*pair);
    }
    else if (tag == "bit")
    {
        // битовое поле. пример: <bit mask="0x08" name="Ошибка калибровки"/>
        if (auto pair = loadInteger("mask"); pair.has_value())
            cmdAttributes.cmd.bitMaskMap.emplace(*pair);
    }
    else if (tag == "field")
    {
        const std::string field_name{ extNode.attribute("name").as_string() };
        if (field_name.empty())
            return;


        Dk::CmdAttributes::Field field;
        std::string type = extNode.attribute("field_type").as_string();
        if (!loadTypeInfo(type, &field.second))
            return;

        // поле составного пакета могут также быть битовой маской, иметь именованные константные значения и т.п.
        for (const auto& childNode : extNode.children())
            loadCommandExtensions(extNode, cmdAttributes);

        cmdAttributes.cmd.fields.emplace_back(std::move(field));
    }
}

bool ModbusProtocolLoader::loadTypeInfo(const std::string& typeTag, Dk::CmdAttributes* cmdAttributes, ModbusRegister* regInfo) const
{
    std::string lower_str = typeTag;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    auto itType = m_types.find(lower_str);
    if (itType == std::end(m_types))
        return false;

    cmdAttributes->type_string = itType->first;
    cmdAttributes->size = itType->second.size;

    if (regInfo)
    {
        regInfo->regSize = (EnRegSize)(itType->second.size / 2); // /2  т.к. размер регистра 16 байт
        regInfo->regType = itType->second.type;
    }
    return true;
}


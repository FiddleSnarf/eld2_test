/******************************************************************************
 * Copyright ( C )  ""
 * Project: DeviceControl
 * File: ModbusProtocol.cpp
 * Creation: 28.10.2024,  ..
 * Description:     .
 ******************************************************************************/

#include "ModbusProtocol.h"

using namespace Modbus;

Modbus::ModbusProtocol::ModbusProtocol(const Dk::ProtocolInfo& info, const std::vector<ModbusRegCmd>& registers)
    : m_protocolInfo(info)
{
    for (const auto& reg : registers)
    {
        Dk::CmdIndex index{ reg.reg.modbusAddr };
        m_modbusRegistersCmd[index] = reg;
    }
}

Dk::ProtocolInfo ModbusProtocol::protocolInfo() const
{
    return Dk::ProtocolInfo();
}

std::vector<Dk::CmdIndex> ModbusProtocol::commandsList() const
{
    std::vector<Dk::CmdIndex> result; 
    for (const auto& [id, _] : m_modbusRegistersCmd)
        result.push_back(id);
    
    return result;
}

std::shared_ptr<Dk::CmdAttributes> ModbusProtocol::commandAttributes(Dk::CmdIndex index) const
{
    if (auto it = m_modbusRegistersCmd.find(index); it != m_modbusRegistersCmd.cend())
        return std::make_shared<Dk::CmdAttributes>(it->second.cmd);
    
    return {};
}

std::vector<std::string> ModbusProtocol::getTrace(Dk::CmdIndex index) const
{
    const auto& ref = m_modbusRegistersCmd.find(index);
    if (ref == m_modbusRegistersCmd.cend())
        return {};
    
    std::vector<std::string> res;
    if (!ref->second.node_name.empty())
        res.push_back(ref->second.node_name);
    
    if (!ref->second.name.empty())
        res.push_back(ref->second.name);

    return res;
}

std::optional<ModbusRegister> ModbusProtocol::getRegister(Dk::CmdIndex cmd) const
{
    const auto& ref = m_modbusRegistersCmd.find(cmd);
    if (ref == m_modbusRegistersCmd.cend())
        return {};

    return ref->second.reg;
}

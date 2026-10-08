/******************************************************************************
 * Copyright ( C )  ""
 * Project: DeviceControl
 * File: ModbusProtocol.h
 * Description: Реализация протокола команд устройства Modbus.
 ******************************************************************************/

#pragma once

#include "IModbusCommTypes.h"

#include "ICommProtocol.h"
#include "CommTypes.h"

#include <map>
#include <optional>

namespace Modbus
{


struct ModbusRegCmd
{
    Dk::CmdAttributes cmd;
    ModbusRegister reg;
    std::string node_name;  /**< Наименование узла.*/
    std::string name;       /**< Наименование команды.*/
};


class ModbusProtocol : public Dk::ICommProtocol
{
public:
    explicit ModbusProtocol(const Dk::ProtocolInfo& info, const std::vector<ModbusRegCmd>& registers);

    /** \brief Inherits via ICommProtocol.*/
    Dk::ProtocolInfo protocolInfo() const override;
    std::vector<Dk::CmdIndex> commandsList() const override;
    std::shared_ptr<Dk::CmdAttributes> commandAttributes(Dk::CmdIndex index) const override;
    std::vector<std::string> getTrace(Dk::CmdIndex index) const override;


    std::optional<ModbusRegister> getRegister(Dk::CmdIndex cmd) const;

private:
    Dk::ProtocolInfo m_protocolInfo;
    std::map<Dk::CmdIndex, ModbusRegCmd> m_modbusRegistersCmd;
};


} // namespace Modbus

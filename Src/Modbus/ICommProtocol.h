/******************************************************************************
 * Copyright ( C ) АО "Диаконт"
 * Project: DkCommLib
 * File: ICommProtocol.h
 * Creation: 25.01.2024, Попов И.Б.
 * Description: Интерфейс протокола коммуникации с оборудованием.
 ******************************************************************************/

#pragma once

#include "CommTypes.h"
#include <vector>
#include <string>
#include <memory>

 //////////////////////////////////////////////////////////////////////////
namespace Dk
{
//////////////////////////////////////////////////////////////////////////

class ICommProtocol
{
public:
    virtual ~ICommProtocol() = default;

    /** \brief Вернуть название (тип) протокола.*/
    virtual ProtocolInfo protocolInfo() const = 0;

    /** \brief Вернуть список индексов команд протокола.*/
    virtual std::vector<CmdIndex> commandsList() const = 0;

    /** \brief Вернуть атрибуты команды по индексу \arg index.*/
    virtual std::shared_ptr<CmdAttributes> commandAttributes(CmdIndex index) const = 0;

    /** \brief Вернуть трэйс команды с индексом \arg index. */
    virtual std::vector<std::string> getTrace(CmdIndex index) const = 0;
};
using ICommProtocolPtr = std::shared_ptr<ICommProtocol>;

//////////////////////////////////////////////////////////////////////////
} // namespace Dk
//////////////////////////////////////////////////////////////////////////

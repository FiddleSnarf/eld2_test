/******************************************************************************
*
* Copyright ( C ) АО "Диаконт"
* Project: Kernerl
* File: BaseTypes.h
* Creation: 01.05.2017, Пикулик С.
* Description:
*
*****************************************************************************/
#pragma once

#include <functional>
#include <chrono>

///////////////////////////////////////////////////////////////////////////////
namespace DkTools
{
///////////////////////////////////////////////////////////////////////////////

using Closure = std::function<void()>;

using PostTask = std::function<void(const Closure&)>;

using PostDelayedTask = std::function<void(const std::chrono::milliseconds, const Closure&)>;

///////////////////////////////////////////////////////////////////////////////
} // namespace DkTools
///////////////////////////////////////////////////////////////////////////////

/******************************************************************************
 * Copyright (C) АО "Диаконт"
 * Project     : DeviceControl
 * File        : ModbusHelperFactory.cpp
 * Creation    :
 * Description : Файл содержит объявление класса фабрики объектов взаимодействия с устройствами по modbus.
 ******************************************************************************/

#include "ModbusLibFactory.h"

#include <modbus.h>

#include <QUrl>
#include <QRegularExpression>   
#include <QDebug>

using namespace Modbus;

struct SerialPortInfo
{
    std::string portName;
    uint baud = 9600;
    char parity = 'N';
    uint data = 8;
    uint stop = 1;
};

namespace
{
    bool parseSerial(const QString& connectionStr, SerialPortInfo& info)
    {
        const QString pattern(R"(^(COM\S*|tty\S*)\s*:\s*baud=(\d+)\s*parity=([N|O|E])\s*data=(\d)\s*stop=(\d))");
        QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
        const auto match = re.match(connectionStr.trimmed());
        if (!match.hasMatch())
        {
            qWarning() << "Invalid connection string to serial port: " << re.errorString();
            return false;
        }
        else for (int i = 1; i <= match.lastCapturedIndex(); ++i)
            qDebug() << match.captured(i);

        const QString portPrefix =
        #ifdef Q_OS_LINUX 
            QStringLiteral("/dev/");
        #else
            QStringLiteral("\\\\.\\");
        #endif


        info.portName = (portPrefix + match.captured(1)).toStdString();
        info.baud = match.captured(2).toInt();
        info.parity = match.captured(3).at(0).toLatin1();
        info.data = match.captured(4).toInt();
        info.stop = match.captured(5).toInt();

        return true;
    }
}

ModbusPtr ModbusLibFactory::createModbusDevice(const std::string& connectionString)
{
    const auto to_lower = QString::fromStdString(connectionString);

    if (to_lower.contains(QStringLiteral("com"))
        || to_lower.contains(QStringLiteral("tty")))
    {
        SerialPortInfo info;
        if (parseSerial(to_lower, info))
        {
            auto modbus = modbus_new_rtu(info.portName.c_str(), info.baud, info.parity, info.data, info.stop);
            if (modbus)
            {
                #ifdef QT_DEBUG
                    modbus_set_debug(modbus, true);
                #endif

                // Настраиваем таймаут ожидания ответа
                modbus_set_response_timeout(modbus, 0, 500000); // 500 мс

                //modbus_rtu_set_serial_mode(modbus, MODBUS_RTU_RS485);
                return ModbusPtr(modbus, modbus_free);
            }
        }
    }
    else if (to_lower.contains(QStringLiteral("tcp")))
    {
        const auto url = QUrl::fromUserInput(to_lower);
        if (url.isValid())
        {
            // Задаем номер сокета
            auto modbus = modbus_new_tcp(url.host().toStdString().c_str(), url.port());
            if (modbus)
            {
//#ifdef QT_DEBUG
//                modbus_set_debug(modbus, true);
//#endif

                modbus_set_socket(modbus, 1);
                return ModbusPtr(modbus, modbus_free);
            }
        }
    }

    return nullptr;
}


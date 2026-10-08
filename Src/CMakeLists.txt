cmake_minimum_required(VERSION 3.19)

project(TestEld2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(pugixml CONFIG REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS REQUIRED IMPORTED_TARGET libmodbus)

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
    WIN32

    UI/MainWindow.ui

    main.cpp
    MainWindow.cpp
    MainWindow.h

    Modbus/CommTypes.h
    Modbus/ICommProtocol.h
    Modbus/IModbusChannel.h
    Modbus/IModbusCommTypes.h
    Modbus/ModbusChannel.cpp
    Modbus/ModbusChannel.h
    Modbus/ModbusLibFactory.cpp
    Modbus/ModbusLibFactory.h
    Modbus/ModbusProtocol.cpp
    Modbus/ModbusProtocol.h
    Modbus/ModbusProtocolLoader.cpp
    Modbus/ModbusProtocolLoader.h
    Modbus/ModbusUtils.h

    Modbus/TaskRunner/BaseTypes.h
    Modbus/TaskRunner/TaskRunnerTimer.h

    Devices/Eld7ModbusDriver.cpp
    Devices/Eld7ModbusDriver.h
    Devices/EldCommands.h
    Devices/DeviceTypes.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/UI"
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets PkgConfig::LIBMODBUS pugixml::pugixml)

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(QT_PLUGINS_SRC "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/Qt6/plugins")
    else()
        set(QT_PLUGINS_SRC "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/Qt6/plugins")
    endif()

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${QT_PLUGINS_SRC}/platforms" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms")
endif()

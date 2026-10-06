cmake_minimum_required(VERSION 3.19)

project(MyQtModbusApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Widgets)

find_package(modbus CONFIG REQUIRED)

qt_standard_project_setup()

qt_add_executable(MyQtModbusApp
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_link_libraries(MyQtModbusApp PRIVATE Qt6::Core Qt6::Widgets modbus)

# --- Установка (опционально) ---
include(GNUInstallDirs)
install(TARGETS MyQtModbusApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET MyQtModbusApp
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
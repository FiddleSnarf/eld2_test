cmake_minimum_required(VERSION 3.19)

project(TestEld2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS REQUIRED IMPORTED_TARGET libmodbus)

qt_standard_project_setup()

qt_add_executable(TestEld2
    WIN32
    main.cpp
    #mainwindow.cpp
    #mainwindow.h
    #mainwindow.ui
)

target_link_libraries(TestEld2 PRIVATE Qt6::Core Qt6::Widgets PkgConfig::LIBMODBUS)

if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(QT_PLUGINS_SRC "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/Qt6/plugins")
    else()
        set(QT_PLUGINS_SRC "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/Qt6/plugins")
    endif()

    add_custom_command(TARGET TestEld2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${QT_PLUGINS_SRC}/platforms"
            "$<TARGET_FILE_DIR:TestEld2>/platforms"
        COMMENT "Копирование плагинов Qt (platforms)"
    )
endif()

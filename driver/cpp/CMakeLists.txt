# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# The default driver in C++20 (DUDI 7, the one and only format driver).
cmake_minimum_required(VERSION 3.22)
project(drv_default_cpp LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

find_package(ZLIB REQUIRED)

file(GLOB FORMAT_SOURCES CONFIGURE_DEPENDS src/formats/*.cpp src/codecs/*.cpp)
add_library(drv_default MODULE
    src/Exports.cpp
    src/Reader.cpp
    src/Archive.cpp
    src/Registry.cpp
    ${FORMAT_SOURCES})
target_include_directories(drv_default PRIVATE src ../contract)
target_link_libraries(drv_default PRIVATE ZLIB::ZLIB)
target_compile_options(drv_default PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
# libdrv_default.d6d.so on Linux, drv_default.d6d on Windows (what DriverHost looks for)
if (WIN32)
    set(DRIVER_PREFIX "")
    set(DRIVER_SUFFIX ".d6d")
else ()
    set(DRIVER_PREFIX "lib")
    set(DRIVER_SUFFIX ".d6d.so")
endif ()
set_target_properties(drv_default PROPERTIES
    OUTPUT_NAME drv_default
    PREFIX "${DRIVER_PREFIX}"
    SUFFIX "${DRIVER_SUFFIX}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../build/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../build/plugins")

# Command line test tool: lists a file through a driver with the MD5 of every entry (used by driver/tests; uses dlopen: not built on Windows)
if (NOT WIN32)
add_executable(dudi7cli ../tools/dudi7cli.cpp)
target_include_directories(dudi7cli PRIVATE ../contract)
target_link_libraries(dudi7cli PRIVATE ${CMAKE_DL_LIBS})
target_compile_options(dudi7cli PRIVATE -Wall -Wextra)
set_target_properties(dudi7cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../build/tools")
endif ()

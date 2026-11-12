cmake_minimum_required(VERSION 3.20)

project(dragonunpacker
        VERSION 6.0.0
        DESCRIPTION "Dragon UnPACKer++, Qt user interface"
        LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Wshadow)

# mold and ccache are used when installed, silently skipped otherwise (same approach as Abuse_2025).
find_program(MOLD_LINKER mold)
if (MOLD_LINKER)
    include(CheckLinkerFlag)
    check_linker_flag(CXX "-fuse-ld=mold" LINKER_SUPPORTS_MOLD)
    if (LINKER_SUPPORTS_MOLD)
        add_link_options(-fuse-ld=mold)
    endif ()
endif ()

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif ()

# Distribution packages by default; with the vcpkg toolchain (make.sh --use-vcpkg) the same names resolve to vcpkg's.
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets Multimedia Svg LinguistTools)
# The desktop's light/dark preference (and its changes) comes from the freedesktop settings portal over D-Bus
if (NOT WIN32)
    find_package(Qt6 6.4 QUIET COMPONENTS DBus)
endif ()
find_package(spdlog REQUIRED)
find_package(Microsoft.GSL REQUIRED)

set(CMAKE_AUTOMOC ON)

# The DUDI 7 header is shared with the Pascal drivers: one source of truth.
set(DUDI7_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../driver/contract)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

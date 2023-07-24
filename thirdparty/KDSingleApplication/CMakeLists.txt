# This file is part of KDSingleApplication.
#
# SPDX-FileCopyrightText: 2020-2023 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
#
# SPDX-License-Identifier: MIT
#
# Contact KDAB at <info@kdab.com> for commercial licensing options.
#

# This is the top-level CMakeLists.txt file for the KDSingleApplication project.
#
# Pass the following variables to cmake to control the build:
# (See INSTALL.txt for more information)
#
# -DKDSingleApplication_QT6=[true|false]
#  Build against Qt6 rather than Qt5
#  Default=false (Qt5 will be used even if Qt6 is available)
#
# -DKDSingleApplication_STATIC=[true|false]
#  Build static versions of the libraries
#  Default=false
#
# -DKDSingleApplication_TESTS=[true|false]
#  Build the test harness.
#  Default=false
#
# -DKDSingleApplication_EXAMPLES=[true|false]
#  Build the examples.
#  Default=true
#
# -DKDSingleApplication_DOCS=[true|false]
#  Build the API documentation. Enables the 'docs' build target.
#  Default=false
#
## DO NOT USE IF YOU ARE AN END-USER.  FOR THE DEVELOPERS ONLY!!
# Special CMake Options for Developers
#
# -DKDSingleApplication_DEVELOPER_MODE=[true|false]
#  Configure the build for a developer setup (strict compiler options, etc)
#  Default=false
#

cmake_minimum_required(VERSION 3.12)

project(
        KDSingleApplication
        DESCRIPTION "KDSingleApplication is a helper class for single-instance policy applications written by KDAB"
        HOMEPAGE_URL "https://github.com/KDAB/KDSingleApplication"
        LANGUAGES CXX
)

set(${PROJECT_NAME}_VERSION_MAJOR 1)
set(${PROJECT_NAME}_VERSION_MINOR 0)
set(${PROJECT_NAME}_VERSION_PATCH 0)
set(${PROJECT_NAME}_VERSION
        ${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}.${${PROJECT_NAME}_VERSION_PATCH}
        )
set(PROJECT_VERSION ${${PROJECT_NAME}_VERSION}) #PROJECT_VERSION is needed by some ECM modules
set(${PROJECT_NAME}_SOVERSION "1.0")

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)

include(FeatureSummary)

option(${PROJECT_NAME}_QT6 "Build against Qt 6" OFF)
option(${PROJECT_NAME}_DEVELOPER_MODE "Developer Mode" OFF)
option(${PROJECT_NAME}_TESTS "Build the tests" OFF)
option(${PROJECT_NAME}_EXAMPLES "Build the examples" ON)
option(${PROJECT_NAME}_DOCS "Build the API documentation" OFF)
option(${PROJECT_NAME}_STATIC "Build lib as static" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/ECM/modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake/KDAB/modules")

# Set a default build type if none was specified
set(default_build_type "Release")
if (EXISTS "${CMAKE_SOURCE_DIR}/.git" OR ${PROJECT_NAME}_DEVELOPER_MODE)
    set(default_build_type "Debug")
endif ()
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to ${default_build_type} as none was specified.")
    set(CMAKE_BUILD_TYPE
            "${default_build_type}"
            CACHE STRING "Choose the type of build." FORCE
            )
    # Set the possible values of build type for cmake-gui
    set_property(
            CACHE CMAKE_BUILD_TYPE
            PROPERTY STRINGS
            "Debug"
            "Release"
            "MinSizeRel"
            "RelWithDebInfo"
    )
endif ()

if (${PROJECT_NAME}_QT6)
    find_package(QT NAMES Qt6 REQUIRED)
    set(${PROJECT_NAME}_LIBRARY_QTID "-qt6")
    set(KDSingleApplication_LIBRARY_QTID ${${PROJECT_NAME}_LIBRARY_QTID})
else ()
    find_package(QT NAMES Qt5 REQUIRED)
    set(${PROJECT_NAME}_LIBRARY_QTID "")
    set(KDSingleApplication_LIBRARY_QTID "")
endif ()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Widgets)

if (${PROJECT_NAME}_TESTS)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
endif ()

set(Qt_VERSION_MAJOR ${QT_VERSION_MAJOR}) # KDQtInstallPaths wants "Qt", not "QT"
include(KDQtInstallPaths) #to set QT_INSTALL_FOO variables

add_definitions(
        -DQT_NO_CAST_TO_ASCII
        -DQT_NO_CAST_FROM_ASCII
        -DQT_NO_URL_CAST_FROM_STRING
        -DQT_NO_CAST_FROM_BYTEARRAY
        -DQT_NO_SIGNALS_SLOTS_KEYWORDS
        -DQT_USE_QSTRINGBUILDER
        -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT
        -DQT_NO_KEYWORDS
        -DQT_DISABLE_DEPRECATED_BEFORE=0x051200
        -DQT_NO_FOREACH
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT MSVC)
    # Strict iterators can't be used on Windows with MSVC (for Qt<5.13) as they lead
    # to a link error when application code iterates over a QVector<QPoint> for instance,
    # unless Qt itself was also built with strict iterators.
    # See example at https://bugreports.qt.io/browse/AUTOSUITE-946
    add_definitions(-DQT_STRICT_ITERATORS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wunused-parameter")
endif ()

# Default to hidden visibility for symbols
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

if (${PROJECT_NAME}_DEVELOPER_MODE)
    add_definitions(-DQT_FORCE_ASSERTS)
    if (NOT MSVC)
        add_compile_options(-Wall -Wextra -Werror -Wshadow)
    endif ()
endif ()

# setup default install locations
include(KDInstallLocation)

set(KDSINGLEAPPLICATION_INCLUDEDIR ${INSTALL_INCLUDE_DIR}/kdsingleapplication${KDSingleApplication_LIBRARY_QTID})

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(${PROJECT_NAME}_IS_ROOT_PROJECT TRUE)

    message(STATUS "Building ${PROJECT_NAME} ${${PROJECT_NAME}_VERSION} in ${CMAKE_BUILD_TYPE} mode. "
            "Installing to ${CMAKE_INSTALL_PREFIX}"
            )

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")

    install(FILES LICENSE.txt README.md DESTINATION ${INSTALL_DOC_DIR})
    install(DIRECTORY LICENSES DESTINATION ${INSTALL_DOC_DIR})

    # Generate .pri file for qmake users (except when using the VS generator)
    if (NOT CMAKE_CONFIGURATION_TYPES)
        if (QT_VERSION_MAJOR EQUAL 5 OR (QT_VERSION_MAJOR EQUAL 6 AND Qt6Core_VERSION VERSION_GREATER "6.2"))
            include(ECMGeneratePriFile)
            set(PROJECT_VERSION_STRING ${${PROJECT_NAME}_VERSION})
            ecm_generate_pri_file(
                    BASE_NAME
                    KDSingleApplication
                    LIB_NAME
                    kdsingleapplication${KDSingleApplication_LIBRARY_QTID}
                    FILENAME_VAR
                    pri_filename
                    INCLUDE_INSTALL_DIR
                    ${KDSINGLEAPPLICATION_INCLUDEDIR}
            )
            install(FILES ${pri_filename} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})
        endif ()
    endif ()
else ()
    #Always disable tests, examples, docs when used as a submodule
    set(${PROJECT_NAME}_IS_ROOT_PROJECT FALSE)
    set(${PROJECT_NAME}_TESTS FALSE)
    set(${PROJECT_NAME}_EXAMPLES FALSE)
    set(${PROJECT_NAME}_DOCS FALSE)
endif ()

add_subdirectory(src)

if (${PROJECT_NAME}_TESTS)
    enable_testing()
    add_definitions(-DKDSINGLEAPPLICATION_BINARY_DIR= "${CMAKE_BINARY_DIR}/bin/")
    add_subdirectory(tests)
endif ()
if (${PROJECT_NAME}_EXAMPLES)
    add_subdirectory(examples)
endif ()

if (${PROJECT_NAME}_DOCS)
    add_subdirectory(docs) # needs to go last, in case there are build source files
endif ()

if (${PROJECT_NAME}_IS_ROOT_PROJECT)
    # Add uninstall target (not for submodules since parent projects typically have uninstall too)
    include(ECMUninstallTarget)
endif ()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

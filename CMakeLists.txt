cmake_minimum_required(VERSION 3.25) # Debian 11 Ubuntu 24.04 Fedora 36

option(WITH_FFMPEG_PLAYER "Enable support for FFMPEG player" ON)
option(WITH_EPWING_SUPPORT "Enable epwing support" ON)
option(WITH_ZIM "enable zim support" ON)
option(WITH_TTS "enable QTexttoSpeech support" OFF)

# options for linux packaging
option(USE_SYSTEM_FMT "use system fmt instead of bundled one" OFF)
option(USE_SYSTEM_TOML "use system toml++ instead of bundled one" OFF)

## This should be avoided because of small regressions, as some scripts and icons themes assume the binary name and resources folder to be `goldendict`
option(USE_ALTERNATIVE_NAME "For Linux, change the binary name and resource folder to goldendict-ng to parallel install with the original GD" OFF)

# vcpkg handling code, must be placed before project()
if (WIN32)
    option(WITH_VCPKG_BREAKPAD "build with Breakpad support for VCPKG build only" OFF)
    if (DEFINED CMAKE_TOOLCHAIN_FILE)
        message(STATUS "Using toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
    else ()
        message(STATUS "CMAKE_TOOLCHAIN_FILE is not set. Try fetching cached vcpkg.")
        include(FetchContent)
        FetchContent_Declare(
                vcpkg-export
                URL https://github.com/xiaoyifang/goldendict-ng/releases/download/vcpkg_250123_b588ddc8230705392c53448597f5892966a8db15/goldendict-ng-vcpkg-export.tar.zst
                URL_HASH SHA512=E8286C6C08BEE93F01E63A1BEAAC0FD88157B9F3AD7C2AABC172BD2F33834AF08A07321AB1823AD167DA4F82320DCE0EC56C75BB19A38402831D126068F8888E
        )
        FetchContent_MakeAvailable(vcpkg-export)
        set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "disable existing manifest mode caused by the existrance of vcpkg.json" FORCE)
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_BINARY_DIR}/_deps/vcpkg-export-src/scripts/buildsystems/vcpkg.cmake")
    endif ()
    if (WITH_VCPKG_BREAKPAD)
        list(APPEND VCPKG_MANIFEST_FEATURES "breakpad")
    endif ()
endif ()

include(FeatureSummary)

project(goldendict-ng
        VERSION 25.04.0
        LANGUAGES CXX C)

if (APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 17)
endif ()

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(GOLDENDICT "goldendict") # binary/executable name
if (USE_ALTERNATIVE_NAME)
    set(GOLDENDICT "goldendict-ng")
endif ()
if (APPLE)
    set(GOLDENDICT "GoldenDict-ng")
endif ()

#### Qt

set(GD_QT_COMPONENTS Concurrent Core5Compat LinguistTools Multimedia PrintSupport WebEngineWidgets Widgets Svg Xml)

if (WITH_TTS)
    list(APPEND GD_QT_COMPONENTS TextToSpeech)
endif ()

find_package(Qt6 REQUIRED COMPONENTS ${GD_QT_COMPONENTS})

qt_standard_project_setup()
set(CMAKE_AUTORCC ON) # not included in the qt_standard_project_setup

#### Things required during configuration
block() # generate version.txt
    string(TIMESTAMP build_time UTC)
    find_package(Git)
    if (EXISTS "${CMAKE_SOURCE_DIR}/.git" AND GIT_FOUND)
        execute_process(
                COMMAND ${GIT_EXECUTABLE} -C "${CMAKE_SOURCE_DIR}" rev-parse --short HEAD
                OUTPUT_STRIP_TRAILING_WHITESPACE
                OUTPUT_VARIABLE GIT_HASH)
        file(WRITE "${CMAKE_SOURCE_DIR}/version.txt" "${PROJECT_VERSION}.${GIT_HASH} at ${build_time}")
    else () # not built in a git repo
        file(WRITE "${CMAKE_SOURCE_DIR}/version.txt" "${PROJECT_VERSION} at ${build_time}")
    endif ()
endblock()

if (WIN32)
    # Binaries output dir for windows. The default ${CMAKE_BINARY_DIR} is too messy, use subfolder instead
    # So that we can directly use windeployqt inside the output folder and obtain a redistributable GD
    set(GD_WIN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/${GOLDENDICT})
endif ()

#### Sources Files

# auto discovery of ui files https://cmake.org/cmake/help/v3.26/prop_tgt/AUTOUIC_SEARCH_PATHS.html
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/")

# https://cmake.org/cmake/help/latest/command/file.html#filesystem
# ! Using GLOB_RECURSE is not recommended by cmake's documentation
# CONFIGURE_DEPENDS will trigger file tree recheck in every rebuilds.
file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS src/*.cc src/*.hh src/*.ui src/*.c)

if (APPLE)
    file(GLOB_RECURSE MACOS_SOURCE_FILES CONFIGURE_DEPENDS src/macos/*.mm)

    set(MACOS_APP_ICON ${CMAKE_SOURCE_DIR}/icons/macicon.icns)
    set_source_files_properties(${MACOS_APP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
endif ()

if (WIN32)
    set(WINDOWS_ICON_RC icons/programicon.rc)
endif ()

set(QSINGLEAPP_SOURCE_FILES
        thirdparty/qtsingleapplication/src/qtlocalpeer.cpp
        thirdparty/qtsingleapplication/src/qtlocalpeer.h
        thirdparty/qtsingleapplication/src/qtsingleapplication.cpp
        thirdparty/qtsingleapplication/src/qtsingleapplication.h
)

qt_add_executable(${GOLDENDICT})

target_sources(${GOLDENDICT} PRIVATE
        icons/flags.qrc
        resources.qrc
        src/scripts/scripts.qrc
        src/stylesheets/css.qrc
        ${ALL_SOURCE_FILES}
        ${MACOS_SOURCE_FILES}
        ${MACOS_APP_ICON}
        ${QSINGLEAPP_SOURCE_FILES}
        ${WINDOWS_ICON_RC}
)

if (NOT USE_SYSTEM_FMT)
    target_sources(${GOLDENDICT} PRIVATE thirdparty/fmt/format.cc)
endif ()

### Common parts amount all platforms

# Note: used as c++ string thus need surrounding " "
add_compile_definitions(PROGRAM_VERSION="${PROJECT_VERSION}")

target_link_libraries(${GOLDENDICT} PRIVATE
        Qt6::Xml
        Qt6::Concurrent
        Qt6::Core5Compat
        Qt6::Multimedia
        Qt6::PrintSupport
        Qt6::WebEngineWidgets
        Qt6::Widgets
        Qt6::Svg
        $<$<BOOL:${WITH_TTS}>:Qt6::TextToSpeech>
)

target_include_directories(${GOLDENDICT} PRIVATE
        ${PROJECT_SOURCE_DIR}/thirdparty/qtsingleapplication/src
        ${PROJECT_SOURCE_DIR}/src/
        ${PROJECT_SOURCE_DIR}/src/common
        ${PROJECT_SOURCE_DIR}/src/dict
        ${PROJECT_SOURCE_DIR}/src/dict/utils
        ${PROJECT_SOURCE_DIR}/src/ui
)

if (NOT USE_SYSTEM_TOML)
    target_include_directories(${GOLDENDICT} PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/tomlplusplus)
endif ()

if (NOT USE_SYSTEM_FMT)
    target_include_directories(${GOLDENDICT} PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/fmt/include)
endif ()

#### Compile definitions

# Disable deprecated staffs for Qt<=6.7 (Bump this if ever consider bump Qt version)
target_compile_definitions(${GOLDENDICT} PRIVATE
        QT_DISABLE_DEPRECATED_UP_TO=0x060700
        QT_NO_DEPRECATED_WARNINGS
)

target_compile_definitions(${GOLDENDICT} PUBLIC
        MAKE_QTMULTIMEDIA_PLAYER
        MAKE_CHINESE_CONVERSION_SUPPORT
        $<$<BOOL:${WIN32}>:__WIN32>
        $<$<BOOL:${WITH_FFMPEG_PLAYER}>:MAKE_FFMPEG_PLAYER>
        $<$<BOOL:${WITH_TTS}>:TTS_SUPPORT>
        $<$<BOOL:${WITH_EPWING_SUPPORT}>:EPWING_SUPPORT>
        $<$<BOOL:${WITH_ZIM}>:MAKE_ZIM_SUPPORT>
        $<$<BOOL:${WITH_VCPKG_BREAKPAD}>:USE_BREAKPAD>
)

#### libraries linking && includes for Win or Unix

if (WIN32)
    include(cmake/Deps_Vcpkg.cmake)
else ()
    include(cmake/Deps_Unix.cmake)
endif ()

#### add translations

# include all *ts files under locale
file(GLOB TRANS_FILES "locale/*.ts")

if (WIN32)
    # Put generated files to output dir's locale
    set_source_files_properties(${TRANS_FILES} PROPERTIES OUTPUT_LOCATION "${GD_WIN_OUTPUT_DIR}/locale")
else ()
    set_source_files_properties(${TRANS_FILES} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/locale")
endif ()

qt_add_translations(${GOLDENDICT} TS_FILES ${TRANS_FILES}
        QM_FILES_OUTPUT_VARIABLE qm_files
        LUPDATE_OPTIONS "-no-ui-lines -locations none -no-obsolete")

add_dependencies(${GOLDENDICT} "release_translations")

#### installation or assemble redistribution

if (APPLE)
    include(cmake/Package_macOS.cmake)
elseif (LINUX OR BSD)
    include(cmake/Package_Linux.cmake)
elseif (WIN32)
    include(cmake/Package_Windows.cmake)
endif ()

feature_summary(WHAT ALL DESCRIPTION "Build configuration:")

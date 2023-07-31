cmake_minimum_required(VERSION 3.25) # ubuntu 23.04 Fedora 36

# Qt6.4+ only

option(WITH_FFMPEG_PLAYER "Enable support for FFMPEG player" ON)
option(WITH_EPWING_SUPPORT "Enable epwing support" ON)
option(WITH_XAPIAN "enable Xapian support" ON)
option(WITH_ZIM "enable zim support" ON)

# options for linux packaging
option(USE_SYSTEM_FMT "use system fmt instead of bundled one" OFF)
option(USE_SYSTEM_TOML "use system toml++ instead of bundled one" OFF)

include(FeatureSummary)

project(goldendict-ng
        VERSION 23.07.23.1
        LANGUAGES CXX C)

set(GOLDENDICT "goldendict") # binary/executable name

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#### Importing Libraries

find_package(Qt6 REQUIRED COMPONENTS
        Concurrent
        Core5Compat
        LinguistTools
        Multimedia
        WebEngineWidgets
        Widgets
        Svg
        Xml
        TextToSpeech
        )

#### Compile time files and preprocessor flags

block() # generate version.txt
    string(TIMESTAMP build_time)
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

#### Sources Files

# auto discovery of ui files https://cmake.org/cmake/help/v3.26/prop_tgt/AUTOUIC_SEARCH_PATHS.html
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/")

# https://cmake.org/cmake/help/latest/command/file.html#filesystem
# ! Using GLOB_RECURSE is not recommended by cmake's documentation
# CONFIGURE_DEPENDS will trigger file tree recheck in every rebuilds.
file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS src/*.cc src/*.hh src/*.c)

if (APPLE)
    file(GLOB_RECURSE MACOS_SOURCE_FILES CONFIGURE_DEPENDS src/macos/*.mm)
endif ()

set(QSINGLEAPP_SOURCE_FILES
        thirdparty/qtsingleapplication/src/qtlocalpeer.cpp
        thirdparty/qtsingleapplication/src/qtlocalpeer.h
        thirdparty/qtsingleapplication/src/qtsingleapplication.cpp
        thirdparty/qtsingleapplication/src/qtsingleapplication.h
        )

qt_add_executable(${GOLDENDICT} MANUAL_FINALIZATION)

target_sources(${GOLDENDICT} PRIVATE
        icons/flags.qrc
        resources.qrc
        src/scripts/scripts.qrc
        src/stylesheets/css.qrc
        ${ALL_SOURCE_FILES}
        ${MACOS_SOURCE_FILES}
        ${QSINGLEAPP_SOURCE_FILES})

if (NOT USE_SYSTEM_FMT)
    target_sources(${GOLDENDICT} PRIVATE thirdparty/fmt/format.cc)
endif ()

### Common parts amount all platforms

# Note: used as c++ string thus need surrounding " "
add_compile_definitions(PROGRAM_VERSION="${PROJECT_VERSION}")

if (LINUX)
    # see: config.cc -> getProgramDataDir
    add_compile_definitions(PROGRAM_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/goldendict")
endif ()

target_link_libraries(${GOLDENDICT} PRIVATE
        Qt6::Xml
        Qt6::Concurrent
        Qt6::Core5Compat
        Qt6::Multimedia
        Qt6::WebEngineWidgets
        Qt6::Widgets
        Qt6::Svg
        Qt6::TextToSpeech
        )


target_include_directories(${GOLDENDICT} PRIVATE
        ${PROJECT_SOURCE_DIR}/thirdparty/qtsingleapplication/src
        ${PROJECT_SOURCE_DIR}/src/
        ${PROJECT_SOURCE_DIR}/src/common
        ${PROJECT_SOURCE_DIR}/src/dict
        ${PROJECT_SOURCE_DIR}/src/ui
        )

if (NOT USE_SYSTEM_TOML)
    target_include_directories(${GOLDENDICT} PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/tomlplusplus)
endif ()

if (NOT USE_SYSTEM_FMT)
    target_include_directories(${GOLDENDICT} PRIVATE ${PROJECT_SOURCE_DIR}/thirdparty/fmt/include)
endif ()

#### Compile definitions

target_compile_definitions(${GOLDENDICT} PUBLIC
        CMAKE_USED_HACK  # temporal hack to avoid breaking qmake build
        USE_ICONV
        MAKE_QTMULTIMEDIA_PLAYER
        MAKE_CHINESE_CONVERSION_SUPPORT
        )

if (WITH_FFMPEG_PLAYER)
    target_compile_definitions(${GOLDENDICT} PUBLIC MAKE_FFMPEG_PLAYER)
endif ()


if (NOT WITH_EPWING_SUPPORT)
    target_compile_definitions(${GOLDENDICT} PUBLIC NO_EPWING_SUPPORT)
endif ()


if (WITH_XAPIAN)
    target_compile_definitions(${GOLDENDICT} PUBLIC USE_XAPIAN)
endif ()

if (WITH_ZIM)
    target_compile_definitions(${GOLDENDICT} PUBLIC MAKE_ZIM_SUPPORT)
endif ()

#### libraries linking && includes for Win or Unix

if (WIN32)
    include(CMake_Win.cmake)
else ()
    include(CMake_Unix.cmake)
endif ()

#### add translations

install(TARGETS ${GOLDENDICT}
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/${GOLDENDICT})

# include all *ts files under locale
file(GLOB TRANS_FILES "locale/*.ts")

# Put generated files to output dir's locale
set_source_files_properties(${TRANS_FILES}
        PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/locale")

# a wrapper over qt_add_lupdate and  qt_add_lrelease
qt_add_translations(${GOLDENDICT} TS_FILES ${TRANS_FILES}
        QM_FILES_OUTPUT_VARIABLE qm_files)

#### installation

if (APPLE)
    set_target_properties(${GOLDENDICT} PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
            MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
            MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
            MACOSX_BUNDLE TRUE
            )
endif ()

if (LINUX OR BSD)
    install(FILES ${CMAKE_SOURCE_DIR}/redist/io.github.xiaoyifang.goldendict_ng.desktop DESTINATION share/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/redist/io.github.xiaoyifang.goldendict_ng.metainfo.xml DESTINATION share/metainfo)

    install(FILES ${CMAKE_SOURCE_DIR}/redist/icons/goldendict.png DESTINATION share/pixmaps)

    install(FILES ${qm_files} DESTINATION share/goldendict/locale)
endif ()

qt_finalize_target(${GOLDENDICT})

feature_summary(WHAT ALL DESCRIPTION "Build configuration:")

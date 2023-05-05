cmake_minimum_required(VERSION 3.25) # ubuntu 23.04 Fedora 36

# Experimental Cmake build only supposed to be used by dev
# Qt6.4+ only

option(WITH_FFMPEG_PLAYER "Enable support for FFMPEG player" ON)
option(WITH_EPWING_SUPPORT "Enable epwing support" ON)
option(WITH_XAPIAN "enable Xapian support" ON)

include(FeatureSummary)

project(goldendict-ng
        VERSION 23.05.01
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

# Obtain git commit hash
execute_process(
        COMMAND git rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TIMESTAMP date_for_version_file) # note: this variable is cached for every run, but for user installation, this doesn't matter much
configure_file(version.txt.in ${CMAKE_SOURCE_DIR}/version.txt)

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

target_sources(${GOLDENDICT} PUBLIC
        icons/flags.qrc
        resources.qrc
        src/scripts/scripts.qrc
        src/stylesheets/css.qrc
        ${ALL_SOURCE_FILES}
        ${MACOS_SOURCE_FILES}
        ${QSINGLEAPP_SOURCE_FILES})

### Common parts amount all platforms

# Note: used as c++ string thus need surrounding " "
add_compile_definitions(PROGRAM_VERSION="${PROJECT_VERSION}")

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


target_include_directories(${GOLDENDICT} PUBLIC
        ${PROJECT_SOURCE_DIR}/thirdparty/qtsingleapplication/src
        ${PROJECT_SOURCE_DIR}/src/
        ${PROJECT_SOURCE_DIR}/src/common
        ${PROJECT_SOURCE_DIR}/src/dict
        ${PROJECT_SOURCE_DIR}/src/ui
        ${PROJECT_SOURCE_DIR}/thirdparty/tomlplusplus
        )

#### Compile definitions

target_compile_definitions(${GOLDENDICT} PUBLIC
        CMAKE_USED_HACK  # temporal hack to avoid breaking qmake build
        USE_ICONV
        MAKE_ZIM_SUPPORT
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
    install(FILES ${CMAKE_SOURCE_DIR}/redist/org.goldendict.GoldenDict.desktop DESTINATION share/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/redist/org.goldendict.GoldenDict.metainfo.xml DESTINATION share/metainfo)

    install(FILES ${CMAKE_SOURCE_DIR}/redist/icons/goldendict.png DESTINATION share/pixmaps)

    install(FILES ${qm_files} DESTINATION share/goldendict/locale)
endif ()

qt_finalize_target(${GOLDENDICT})

feature_summary(WHAT ALL DESCRIPTION "Build configuration:")
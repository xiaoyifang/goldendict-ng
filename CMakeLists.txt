cmake_minimum_required(VERSION 3.25) # ubuntu 23.04 Fedora 36

# Experimental Cmake build only supposed to be used by dev
# Qt6.4+ only

option(WITH_FFMPEG_PLAYER "Enable support for FFMPEG player" ON)
option(WITH_EPWING_SUPPORT "Enable epwing support" ON)
option(WITH_XAPIAN "enable Xapian support" ON)

include(FeatureSummary)

project(goldendict
        VERSION 22.11.20
        LANGUAGES CXX C)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (APPLE)
    if (WITH_EPWING_SUPPORT)
        add_subdirectory(thirdparty/eb)
    endif ()
    include_directories(/usr/local/include /opt/homebrew/include)
endif ()

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


if (APPLE)
    find_library(CARBON_LIBRARY Carbon REQUIRED)
endif ()

if (UNIX)
    # Provided by Cmake
    find_package(ZLIB REQUIRED)
    find_package(BZip2 REQUIRED)
    find_package(Iconv REQUIRED)
    if (WITH_XAPIAN)
        find_package(Xapian REQUIRED) # https://github.com/xapian/xapian/tree/master/xapian-core/cmake
    endif ()

    # PkgConfig only packages
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PKGCONFIG_DEPS IMPORTED_TARGET
            hunspell
            lzo2
            opencc
            vorbis # .ogg
            vorbisfile
            liblzma
            libzstd
            )
    if (WITH_FFMPEG_PLAYER)
        pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
                libavcodec
                libavformat
                libavutil
                libswresample
                )
    endif ()
endif ()

if (LINUX)
    find_package(X11 REQUIRED)
    pkg_check_modules(LIBXTST IMPORTED_TARGET xtst)

    if (WITH_EPWING_SUPPORT)
        add_subdirectory(thirdparty/eb EXCLUDE_FROM_ALL)
    endif ()
    # add_library(libeb SHARED IMPORTED)
    # set_target_properties(libeb PROPERTIES IMPORTED_LOCATION /usr/lib/libeb.so)
    # set_target_properties(libeb PROPERTIES INCLUDE_DIRECTORIES /usr/include/)
endif ()

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
        thirdparty/qtsingleapplication/src/qtsinglecoreapplication.cpp
        thirdparty/qtsingleapplication/src/qtsinglecoreapplication.h
        )

qt_add_executable(${CMAKE_PROJECT_NAME}
        MANUAL_FINALIZATION
        icons/flags.qrc
        resources.qrc
        src/scripts/scripts.qrc
        src/stylesheets/css.qrc
        ${ALL_SOURCE_FILES}
        ${MACOS_SOURCE_FILES}
        ${QSINGLEAPP_SOURCE_FILES})


### Common parts amount all platforms

# hack -> this string will be compared with another string, thus having surrounding " "
add_compile_definitions(PROGRAM_VERSION="${PROJECT_VERSION}")

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        Qt6::Xml
        Qt6::Concurrent
        Qt6::Core5Compat
        Qt6::Multimedia
        Qt6::WebEngineWidgets
        Qt6::Widgets
        Qt6::Svg
        Qt6::TextToSpeech
        )

target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
        CMAKE_USED_HACK  # temporal hack to avoid breaking qmake build
        USE_ICONV
        MAKE_ZIM_SUPPORT
        MAKE_QTMULTIMEDIA_PLAYER
        MAKE_CHINESE_CONVERSION_SUPPORT)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}/thirdparty/qtsingleapplication/src
        ${PROJECT_SOURCE_DIR}/src/
        ${PROJECT_SOURCE_DIR}/src/common
        ${PROJECT_SOURCE_DIR}/src/dict
        ${PROJECT_SOURCE_DIR}/src/ui)


if (WITH_FFMPEG_PLAYER)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC MAKE_FFMPEG_PLAYER)
    if (UNIX)
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE PkgConfig::FFMPEG)
    endif ()
endif ()


if (NOT WITH_EPWING_SUPPORT)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC NO_EPWING_SUPPORT)
endif ()


if (WITH_XAPIAN)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC USE_XAPIAN)

    if (UNIX)
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${XAPIAN_LIBRARIES})
    endif ()
endif ()


### Parts where different platforms disagree

if (LINUX)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC HAVE_X11)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE X11 PkgConfig::LIBXTST)

    if (WITH_EPWING_SUPPORT)
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE eb)
    endif ()

endif ()

if (MSVC)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
            __WIN32
            NO_EPWING_SUPPORT
            INCLUDE_LIBRARY_PATH
            )

    target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
            ${CMAKE_SOURCE_DIR}/winlibs/include/
            )

    set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    file(GLOB WINLIBS_FILES "${CMAKE_SOURCE_DIR}/winlibs/lib/msvc/*.lib")
    foreach (A_WIN_LIB ${WINLIBS_FILES})
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${A_WIN_LIB})
    endforeach ()

endif ()


if (APPLE)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${CARBON_LIBRARY})

    if (WITH_EPWING_SUPPORT)
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE eb)
    endif ()
endif ()

if (UNIX)
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
            ${PROJECT_SOURCE_DIR}/thirdparty)

    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
            # pkg-config packages need manually link
            PkgConfig::PKGCONFIG_DEPS
            BZip2::BZip2
            ZLIB::ZLIB #hidden requirement of dsl_details.cc and more?
            Iconv::Iconv
            )
endif ()

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
        )

install(TARGETS ${CMAKE_PROJECT_NAME}
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# include all *ts files under locale
file(GLOB TRANS_FILES "locale/*.ts")

# Put generated files to output dir's locale
set_source_files_properties(${TRANS_FILES}
        PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/locale")

# a wrapper over qt_add_lupdate and  qt_add_lrelease
qt_add_translations(${CMAKE_PROJECT_NAME} TS_FILES ${TRANS_FILES}
        QM_FILES_OUTPUT_VARIABLE qm_files)

qt_finalize_target(${CMAKE_PROJECT_NAME})

if (LINUX)
    install(FILES ${CMAKE_SOURCE_DIR}/redist/org.goldendict.GoldenDict.desktop DESTINATION share/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/redist/org.goldendict.GoldenDict.metainfo.xml DESTINATION share/metainfo)

    install(FILES ${CMAKE_SOURCE_DIR}/redist/icons/goldendict.png DESTINATION share/pixmaps)

    install(FILES ${qm_files} DESTINATION share/goldendict/locale)
endif ()

# Copy .dlls to output dir
if (MSVC)
    file(GLOB DLL_FILES LIST_DIRECTORIES false "${CMAKE_SOURCE_DIR}/winlibs/lib/msvc/*.dll")
    foreach (A_DLL_FILE ${DLL_FILES})
        get_filename_component(TEMP_VAR_HOLDING_DLL_FILENAME ${A_DLL_FILE} NAME)
        configure_file("${A_DLL_FILE}" "${CMAKE_BINARY_DIR}/${TEMP_VAR_HOLDING_DLL_FILENAME}" COPYONLY)
    endforeach ()
endif ()

feature_summary(WHAT ALL DESCRIPTION "Build configuration:")
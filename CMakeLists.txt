cmake_minimum_required(VERSION 3.25) # ubuntu 23.04 Fedora 36

option(WITH_FFMPEG_PLAYER "Enable support for FFMPEG player" ON)
option(WITH_EPWING_SUPPORT "Enable epwing support" ON)
option(WITH_ZIM "enable zim support" ON)
option(WITH_TTS "enable QTexttoSpeech support" ON)

# options for linux packaging
option(USE_SYSTEM_FMT "use system fmt instead of bundled one" OFF)
option(USE_SYSTEM_TOML "use system toml++ instead of bundled one" OFF)

option(USE_CACHED_VCPKG "use a cached vcpkg with binaries prebuilt" OFF)
option(WITH_VCPKG_BREAKPAD "build with Breakpad support for VCPKG build only" OFF)

## Change binary & resources folder to parallel install with original GD.
## This flag should be avoided because it leads to small regressions:
## 1. There are personal scripts assuming the binary name to be "goldendict" -> require everyone to change the name in their script
## 2. There are icon themes that assuming the icon name to be "goldendict" -> invalidate the GD icon when using a icon theme
## 3. There are dictionary packages that install files to "/usr/share/goldendict/content" -> nullify the auto dict discovery
option(USE_ALTERNATIVE_NAME "Force the name goldendict-ng " OFF)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}") # to put staff in the ./cmake folder


# vcpkg handling code, must be placed before project()

if (WIN32)
    if (NOT USE_CACHED_VCPKG)
        message(FATAL_ERROR "\
GoldenDict-ng uses vcpkg for windows build.
CMAKE_TOOLCHAIN_FILE needs to be set.
Google `CMAKE_TOOLCHAIN_FILE vcpkg` will show how.
`USE_CACHED_VCPKG` can be used to fetch a cached version with binaries prebuilt.
            ")
    else ()
        include(FetchContent)
        message(INFO "Fetching vcpkg exported caches")
        FetchContent_Declare(
                vcpkg-export
                URL https://github.com/shenlebantongying/goldendict-ng/releases/download/vcpkg_240711_3d72d8c930e1b6a1b2432b262c61af7d3287dcd0/goldendict-ng-vcpkg-exports.tar.zst
                URL_HASH SHA512=F48A16A10CE3D05F74EEA1BCD2E7B7B2411CD984DE831FC802EC0EA03CCBF6BC173690CA40A889F40F4D26A201C73834141B44E8133902076D1C7A954F785280
        )
        FetchContent_MakeAvailable(vcpkg-export)
        set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "disable existing manifest mode caused by the existrance of vcpkg.json" FORCE)
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_BINARY_DIR}/_deps/vcpkg-export-src/goldendict-ng-vcpkg-exports/scripts/buildsystems/vcpkg.cmake")
    endif ()
endif ()


if (WITH_VCPKG_BREAKPAD)
    list(APPEND VCPKG_MANIFEST_FEATURES "breakpad")
endif ()

include(FeatureSummary)

project(goldendict-ng
        VERSION 24.05.13
        LANGUAGES CXX C)

set(GOLDENDICT "goldendict") # binary/executable name
if (USE_ALTERNATIVE_NAME )
    set(GOLDENDICT "goldendict-ng")
endif ()
if (APPLE)
    set(GOLDENDICT "GoldenDict-ng")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#### Qt

set(GD_QT_COMPONENTS Concurrent Core5Compat LinguistTools Multimedia PrintSupport WebEngineWidgets Widgets Svg Xml)

if (WITH_TTS)
    list(APPEND GD_QT_COMPONENTS TextToSpeech)
endif ()

find_package(Qt6 REQUIRED COMPONENTS ${GD_QT_COMPONENTS})

qt_standard_project_setup() # availiable after find_package(Qt6 .... Core
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
file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS src/*.cc src/*.hh src/*.c)

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
        )

if (WITH_TTS)
    target_link_libraries(${GOLDENDICT} PRIVATE Qt6::TextToSpeech)
endif ()

target_include_directories(${GOLDENDICT} PRIVATE
        ${PROJECT_SOURCE_DIR}/thirdparty/qtsingleapplication/src
        ${PROJECT_SOURCE_DIR}/src/
        ${PROJECT_SOURCE_DIR}/src/common
        ${PROJECT_SOURCE_DIR}/src/dict
        ${PROJECT_SOURCE_DIR}/src/ui
        )

if (WIN32)
    target_include_directories(${GOLDENDICT} PRIVATE ${PROJECT_SOURCE_DIR}/src/windows)
endif ()

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

if (WIN32)
    target_compile_definitions(${GOLDENDICT} PUBLIC
            __WIN32
            INCLUDE_LIBRARY_PATH
    )
endif ()

if (WITH_FFMPEG_PLAYER)
    target_compile_definitions(${GOLDENDICT} PUBLIC MAKE_FFMPEG_PLAYER)
endif ()

if(NOT WITH_TTS)
    target_compile_definitions(${GOLDENDICT} PUBLIC NO_TTS_SUPPORT)
endif()


if (NOT WITH_EPWING_SUPPORT)
    target_compile_definitions(${GOLDENDICT} PUBLIC NO_EPWING_SUPPORT)
endif ()

if (WITH_ZIM)
    target_compile_definitions(${GOLDENDICT} PUBLIC MAKE_ZIM_SUPPORT)
endif ()

if (WITH_VCPKG_BREAKPAD)
    target_compile_definitions(${GOLDENDICT} PUBLIC USE_BREAKPAD)
endif ()

#### libraries linking && includes for Win or Unix

if (WIN32)
    include(Deps_Vcpkg)
else ()
    include(Deps_Unix)
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
        QM_FILES_OUTPUT_VARIABLE qm_files)
add_dependencies(${GOLDENDICT} "release_translations")

#### installation or assemble redistribution

if (APPLE)
    set(PLIST_FILE "${CMAKE_BINARY_DIR}/info_generated.plist")
    configure_file("${CMAKE_SOURCE_DIR}/redist/mac_info_plist_template_cmake.plist" "${PLIST_FILE}" @ONLY)

    set_target_properties(${GOLDENDICT} PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST "${PLIST_FILE}"
    )

    set(Assembling_Dir "${CMAKE_BINARY_DIR}/redist")
    set(App_Name "${GOLDENDICT}.app")
    set(Redistributable_APP "${Assembling_Dir}/${App_Name}")
    
    qt_generate_deploy_script(
            TARGET ${GOLDENDICT}
            OUTPUT_SCRIPT deploy_script
            CONTENT "qt_deploy_runtime_dependencies(
                        EXECUTABLE \"${Redistributable_APP}\"
                        GENERATE_QT_CONF
                        NO_APP_STORE_COMPLIANCE
                )"
    )

    install(TARGETS ${GOLDENDICT} BUNDLE DESTINATION "${Assembling_Dir}")
    install(FILES ${qm_files} DESTINATION "${Redistributable_APP}/Contents/MacOS/locale")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/opencc" DESTINATION "${Redistributable_APP}/Contents/MacOS")

    install(SCRIPT ${deploy_script})

    install(CODE "execute_process(COMMAND codesign --force --deep -s - ${Redistributable_APP})")

    find_program(CREATE-DMG "create-dmg")
    if (CREATE-DMG)
        install(CODE "
        execute_process(COMMAND ${CREATE-DMG} \
            --skip-jenkins \
            --format \"ULMO\"
            --volname ${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}-${CMAKE_SYSTEM_PROCESSOR} \
            --volicon ${CMAKE_SOURCE_DIR}/icons/macicon.icns \
            --icon \"${App_Name}\" 100 100
            --app-drop-link 300 100 \
            \"GoldenDict-ng-${CMAKE_PROJECT_VERSION}-Qt${Qt6_VERSION}-macOS-${CMAKE_SYSTEM_PROCESSOR}.dmg\" \
            \"${Assembling_Dir}\")"
        )
    else ()
        message(WARNING "create-dmg not found. No .dmg will be created")
    endif ()

endif ()

if (LINUX OR BSD)
    install(TARGETS ${GOLDENDICT})
    install(FILES ${CMAKE_SOURCE_DIR}/redist/io.github.xiaoyifang.goldendict_ng.desktop DESTINATION share/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/redist/io.github.xiaoyifang.goldendict_ng.metainfo.xml DESTINATION share/metainfo)

    if (NOT USE_ALTERNATIVE_NAME)
        # see: config.cc -> getProgramDataDir
        add_compile_definitions(PROGRAM_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/goldendict")
        install(FILES ${CMAKE_SOURCE_DIR}/redist/icons/goldendict.png DESTINATION share/pixmaps)
        install(FILES ${qm_files} DESTINATION share/goldendict/locale)
    else ()
        add_compile_definitions(PROGRAM_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/goldendict-ng")
        install(FILES ${CMAKE_SOURCE_DIR}/redist/icons/goldendict.png DESTINATION share/pixmaps
                RENAME goldendict-ng.png)
        install(FILES ${qm_files} DESTINATION share/goldendict-ng/locale)

        block() # patch the desktop file to adapt the binary & icon file's name change
            file(READ "${CMAKE_SOURCE_DIR}/redist/io.github.xiaoyifang.goldendict_ng.desktop" DESKTOP_FILE_CONTENT)
            string(REGEX REPLACE "\nIcon=goldendict\n" "\nIcon=goldendict-ng\n" DESKTOP_FILE_CONTENT "${DESKTOP_FILE_CONTENT}")
            string(REGEX REPLACE "\nExec=goldendict %u\n" "\nExec=goldendict-ng %u\n" DESKTOP_FILE_CONTENT "${DESKTOP_FILE_CONTENT}")
            file(WRITE "${CMAKE_SOURCE_DIR}/redist/io.github.xiaoyifang.goldendict_ng.desktop" "${DESKTOP_FILE_CONTENT}")
        endblock()
    endif ()
endif ()

if (WIN32)

    set_target_properties(${GOLDENDICT}
            PROPERTIES
            WIN32_EXECUTABLE TRUE
            RUNTIME_OUTPUT_DIRECTORY "${GD_WIN_OUTPUT_DIR}"
            LIBRARY_OUTPUT_DIRECTORY "${GD_WIN_OUTPUT_DIR}"
    )

    set(CMAKE_INSTALL_PREFIX "${GD_WIN_OUTPUT_DIR}" CACHE PATH "If you see this message, don't change this unless you want look into CMake build script. If you are an expert, yes, this is wrong. Help welcomed." FORCE)

    qt_generate_deploy_script(
            TARGET ${GOLDENDICT}
            OUTPUT_SCRIPT deploy_script
            CONTENT "qt_deploy_runtime_dependencies(
                    EXECUTABLE \"${CMAKE_INSTALL_PREFIX}/goldendict.exe\"
                    BIN_DIR .
                    LIB_DIR .
            )"
    )

    install(SCRIPT ${deploy_script})
    install(DIRECTORY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/opencc" DESTINATION .)
    # TODO: do we really need to carry a copy of openSSL?
    install(FILES "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libssl-3-x64.dll" DESTINATION .)
    install(FILES "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libcrypto-3-x64.dll" DESTINATION .)

    # trick CPack to make the output folder as NSIS installer
    install(DIRECTORY "${GD_WIN_OUTPUT_DIR}/"
            DESTINATION .
            FILES_MATCHING
            PATTERN "*"
            PATTERN "*.pdb" EXCLUDE
            PATTERN "*.ilk" EXCLUDE)


    set(CPACK_PACKAGE_FILE_NAME "GoldenDict-ng-${PROJECT_VERSION}-Qt${Qt6Widgets_VERSION}")
    set(CPACK_GENERATOR "7Z;NSIS64")

    # override the default install path, which is $PROGRAMFILES64\${project-name} ${project-version} in NSIS
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "GoldenDict-ng")

    # NSIS specificS
    set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/icons/programicon.ico")
    set(CPACK_NSIS_PACKAGE_NAME "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}")
    set(CPACK_NSIS_DISPLAY_NAME "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}")
    set(CPACK_NSIS_URL_INFO_ABOUT [=[https://xiaoyifang.github.io/goldendict-ng/]=])
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\GoldenDict-ng.lnk' '$INSTDIR\\\\${GOLDENDICT}.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$START_MENU\\\\GoldenDict-ng.lnk'")

    include(CPack)
endif ()

feature_summary(WHAT ALL DESCRIPTION "Build configuration:")

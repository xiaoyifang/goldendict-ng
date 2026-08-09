set(PLIST_FILE "${CMAKE_BINARY_DIR}/info_generated.plist")
configure_file("${CMAKE_SOURCE_DIR}/redist/mac_info_plist_template_cmake.plist" "${PLIST_FILE}" @ONLY)

set_target_properties(${GOLDENDICT} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${PLIST_FILE}"
)

set(Assembling_Dir "${CMAKE_BINARY_DIR}/redist")
set(App_Name "${GOLDENDICT}.app")
set(Redistributable_APP "${Assembling_Dir}/${App_Name}")

# if anything wrong, delete this and affect lines, and see what's Qt will generate by default.
set(QtConfPath "${Redistributable_APP}/Contents/Resources/qt.conf")

# Copy Homebrew's ICU libraries into the bundle before macdeployqt scans it.
# Passing them as ADDITIONAL_LIBRARIES would make macdeployqt treat them as
# extra executables and repeatedly rewrite them instead of simply deploying the
# application's dependency graph.
set(MACOS_DEPLOY_ICU_SETUP "")
if (BREW_ICU_ADDITIONAL_DYLIBS)
    string(REPLACE ";" "\" \"" BREW_ICU_COPY_ARGUMENTS "${BREW_ICU_ADDITIONAL_DYLIBS}")
    set(BREW_ICU_COPY_ARGUMENTS "\"${BREW_ICU_COPY_ARGUMENTS}\"")
    set(MACOS_DEPLOY_ICU_SETUP
            "file(MAKE_DIRECTORY \"${Redistributable_APP}/Contents/Frameworks\")
             file(COPY ${BREW_ICU_COPY_ARGUMENTS}
                  DESTINATION \"${Redistributable_APP}/Contents/Frameworks\"
                  FOLLOW_SYMLINK_CHAIN)")
endif ()

qt_generate_deploy_script(
        TARGET ${GOLDENDICT}
        OUTPUT_SCRIPT deploy_script
        CONTENT "
        set(QT_DEPLOY_PREFIX \"${Redistributable_APP}\")
        set(QT_DEPLOY_TRANSLATIONS_DIR \"Contents/Resources/translations\")
        ${MACOS_DEPLOY_ICU_SETUP}
        qt_deploy_runtime_dependencies(
                    EXECUTABLE \"${Redistributable_APP}\"
                    DEPLOY_TOOL_OPTIONS
                        \"-no-codesign\"
                    GENERATE_QT_CONF
                    NO_APP_STORE_COMPLIANCE)
        # Homebrew's aggregate Qt package exposes an optional virtual-keyboard
        # input plugin without its split framework on macdeployqt's default
        # search path. GoldenDict does not use this QML-only input method.
        file(REMOVE
             \"${Redistributable_APP}/Contents/PlugIns/platforminputcontexts/libqtvirtualkeyboardplugin.dylib\")
        # macdeployqt rewrites deployed framework references to
        # @executable_path/../Frameworks. That path is correct for the main
        # executable, but QtWebEngineProcess is nested several bundles deep.
        # Give the helper the same Frameworks view so dyld can start Chromium.
        if (NOT EXISTS
            \"${Redistributable_APP}/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks\"
            AND NOT IS_SYMLINK
            \"${Redistributable_APP}/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks\")
            file(CREATE_LINK
                 \"../../../../../..\"
                 \"${Redistributable_APP}/Contents/Frameworks/QtWebEngineCore.framework/Versions/A/Helpers/QtWebEngineProcess.app/Contents/Frameworks\"
                 SYMBOLIC)
        endif()

        qt_deploy_translations()
        qt_deploy_qt_conf(\"${QtConfPath}\"
                     PLUGINS_DIR PlugIns
                     TRANSLATIONS_DIR Resources/translations)
        "
)

install(TARGETS ${GOLDENDICT} BUNDLE DESTINATION "${Assembling_Dir}")
install(FILES ${qm_files} DESTINATION "${Redistributable_APP}/Contents/MacOS/locale")

if (IS_READABLE "/opt/homebrew/share/opencc/")
    set(OPENCC_DATA_PATH "/opt/homebrew/share/opencc/" CACHE PATH "opencc's data path")
elseif (IS_READABLE "/usr/local/share/opencc/")
    set(OPENCC_DATA_PATH "/usr/local/share/opencc/" CACHE PATH "opencc's data path")
else ()
    message(FATAL_ERROR "Cannot find opencc's data folder!")
endif ()

file(REAL_PATH "${OPENCC_DATA_PATH}" OPENCC_DATA_PATH_FOR_REAL)

message(STATUS "OPENCC data is found -> ${OPENCC_DATA_PATH_FOR_REAL}")
install(DIRECTORY "${OPENCC_DATA_PATH_FOR_REAL}" DESTINATION "${Redistributable_APP}/Contents/MacOS")

install(SCRIPT ${deploy_script})

set(QT_WEBENGINE_CORE_FRAMEWORK
        "${Redistributable_APP}/Contents/Frameworks/QtWebEngineCore.framework")
set(QT_WEBENGINE_HELPER_APP
        "${QT_WEBENGINE_CORE_FRAMEWORK}/Versions/A/Helpers/QtWebEngineProcess.app")
set(QT_WEBENGINE_HELPER_ENTITLEMENTS
        "${QT_WEBENGINE_HELPER_APP}/Contents/Resources/QtWebEngineProcess.entitlements")

install(CODE "
    # First sign every nested component, then restore the WebEngine helper's
    # required JIT entitlements. Re-sign its containing framework and the main
    # app from the inside out so all resource seals remain valid.
    execute_process(
        COMMAND codesign --force --deep --sign - \"${Redistributable_APP}\"
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(
        COMMAND codesign --force --sign -
                --entitlements \"${QT_WEBENGINE_HELPER_ENTITLEMENTS}\"
                \"${QT_WEBENGINE_HELPER_APP}\"
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(
        COMMAND codesign --force --sign - \"${QT_WEBENGINE_CORE_FRAMEWORK}\"
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(
        COMMAND codesign --force --sign - \"${Redistributable_APP}\"
        COMMAND_ERROR_IS_FATAL ANY)
    execute_process(
        COMMAND codesign --verify --deep --strict \"${Redistributable_APP}\"
        COMMAND_ERROR_IS_FATAL ANY)
")

find_program(CREATE-DMG "create-dmg")
if (CREATE-DMG)
    install(CODE "
    execute_process(COMMAND ${CREATE-DMG} \
        --skip-jenkins \
        --overwrite \
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

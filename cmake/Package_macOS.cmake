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

qt_generate_deploy_script(
        TARGET ${GOLDENDICT}
        OUTPUT_SCRIPT deploy_script
        CONTENT "
        set(QT_DEPLOY_PREFIX \"${Redistributable_APP}\")
        set(QT_DEPLOY_TRANSLATIONS_DIR \"Contents/Resources/translations\")
        qt_deploy_runtime_dependencies(
                    EXECUTABLE \"${Redistributable_APP}\"
                    ADDITIONAL_LIBRARIES ${BREW_ICU_ADDITIONAL_DYLIBS}
                    GENERATE_QT_CONF
                    NO_APP_STORE_COMPLIANCE)
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

set_target_properties(${GOLDENDICT}
        PROPERTIES
        WIN32_EXECUTABLE TRUE
        RUNTIME_OUTPUT_DIRECTORY "${GD_WIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${GD_WIN_OUTPUT_DIR}"
)

set(CMAKE_INSTALL_PREFIX "${GD_WIN_OUTPUT_DIR}" CACHE PATH "Windows packaging install prefix")

# For multi-config generators (VS/Xcode), CMake automatically appends the
# configuration name (Release/Debug) to RUNTIME_OUTPUT_DIRECTORY. For
# single-config generators (Ninja), the exe goes directly into the output dir.
# Adjust deployment and install paths accordingly.
get_property(GD_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(GD_MULTI_CONFIG)
    # Multi-config: exe at ${GD_WIN_OUTPUT_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/goldendict.exe
    set(GD_DEPLOY_EXE "${GD_WIN_OUTPUT_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/goldendict.exe")
else()
    # Single-config: exe at ${GD_WIN_OUTPUT_DIR}/goldendict.exe
    set(GD_DEPLOY_EXE "${GD_WIN_OUTPUT_DIR}/goldendict.exe")
endif()

qt_generate_deploy_script(
        TARGET ${GOLDENDICT}
        OUTPUT_SCRIPT deploy_script
        CONTENT "
    qt_deploy_runtime_dependencies(
        EXECUTABLE \"${GD_DEPLOY_EXE}\"
        BIN_DIR .
        LIB_DIR .
    )
")

install(SCRIPT ${deploy_script})
install(DIRECTORY "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/opencc" DESTINATION .)
# Note: This is runtime dependency that aren't copied automatically
# See Qt's network -> SSDL documentation https://doc.qt.io/qt-6/ssl.html#considerations-while-packaging-your-application
file(GLOB SSL_DLL "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libssl-*-x64.dll")
file(GLOB CRYPTO_DLL "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libcrypto-*-x64.dll")
install(FILES ${SSL_DLL} DESTINATION .)
install(FILES ${CRYPTO_DLL} DESTINATION .)

# Copy root-level config-independent files (locale, etc.)
# For single-config: also copies goldendict.exe and deployed DLLs
# For multi-config: excludes config subdirectories (handled separately below)
install(DIRECTORY "${GD_WIN_OUTPUT_DIR}/"
        DESTINATION .
        FILES_MATCHING
        PATTERN "*"
        PATTERN "Release/*" EXCLUDE
        PATTERN "Debug/*" EXCLUDE
        PATTERN "RelWithDebInfo/*" EXCLUDE
        PATTERN "MinSizeRel/*" EXCLUDE
        PATTERN "*.pdb" EXCLUDE
        PATTERN "*.ilk" EXCLUDE)

# For multi-config generators: copy artifacts from the config-specific subdirectory
if(GD_MULTI_CONFIG)
    install(CODE "
        file(COPY \"${GD_WIN_OUTPUT_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/\"
             DESTINATION \"\${CMAKE_INSTALL_PREFIX}/\"
             FILES_MATCHING
             PATTERN \"*\"
             PATTERN \"*.pdb\" EXCLUDE
             PATTERN \"*.ilk\" EXCLUDE
        )
    ")
endif()


set(CPACK_PACKAGE_FILE_NAME "GoldenDict-ng-${PROJECT_VERSION}-Qt${Qt6Widgets_VERSION}")
set(CPACK_GENERATOR "7Z;NSIS64")

# override the default install path, which is $PROGRAMFILES64\${project-name} ${project-version} in NSIS
set(CPACK_PACKAGE_INSTALL_DIRECTORY "GoldenDict-ng")

# NSIS specificS
set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/icons/programicon.ico")
set(CPACK_NSIS_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_NSIS_DISPLAY_NAME "${CMAKE_PROJECT_NAME}")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

# Copied from https://crascit.com/2015/08/07/cmake_cpack_nsis_shortcuts_with_parameters/
set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\GoldenDict-ng.lnk' '$INSTDIR\\\\${GOLDENDICT}.exe'")
set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$START_MENU\\\\GoldenDict-ng.lnk'")

include(CPack)

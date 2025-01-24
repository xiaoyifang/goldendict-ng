set_target_properties(${GOLDENDICT}
        PROPERTIES
        WIN32_EXECUTABLE TRUE
        RUNTIME_OUTPUT_DIRECTORY "${GD_WIN_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${GD_WIN_OUTPUT_DIR}"
)

# TODO: this breaks "Multi-Config" build systems like VisualStudio.
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
# Note: This is runtime dependency that aren't copied automatically
# See Qt's network -> SSDL documentation https://doc.qt.io/qt-6/ssl.html#considerations-while-packaging-your-application
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

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")

# Copied from https://crascit.com/2015/08/07/cmake_cpack_nsis_shortcuts_with_parameters/
set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\GoldenDict-ng.lnk' '$INSTDIR\\\\${GOLDENDICT}.exe'")
set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$START_MENU\\\\GoldenDict-ng.lnk'")

include(CPack)

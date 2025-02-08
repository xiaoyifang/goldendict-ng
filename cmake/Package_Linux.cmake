install(TARGETS ${GOLDENDICT})
install(FILES ${CMAKE_SOURCE_DIR}/redist/io.github.xiaoyifang.goldendict_ng.desktop DESTINATION share/applications)
install(FILES ${CMAKE_SOURCE_DIR}/redist/io.github.xiaoyifang.goldendict_ng.metainfo.xml DESTINATION share/metainfo)

if (NOT USE_ALTERNATIVE_NAME)
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

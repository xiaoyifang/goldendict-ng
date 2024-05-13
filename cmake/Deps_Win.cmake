target_include_directories(${GOLDENDICT} PUBLIC
        ${CMAKE_SOURCE_DIR}/winlibs/include/
        )

set_property(TARGET ${BIN_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

file(GLOB WINLIBS_FILES "${CMAKE_SOURCE_DIR}/winlibs/lib/msvc/*.lib")
foreach (A_WIN_LIB ${WINLIBS_FILES})
    target_link_libraries(${GOLDENDICT} PRIVATE ${A_WIN_LIB})
endforeach ()

set(THIRD_PARTY_LIBARY
        debug ${CMAKE_SOURCE_DIR}/winlibs/lib/dbg/lzma.lib optimized ${CMAKE_SOURCE_DIR}/winlibs/lib/lzma.lib
        debug ${CMAKE_SOURCE_DIR}/winlibs/lib/dbg/zstd.lib optimized ${CMAKE_SOURCE_DIR}/winlibs/lib/zstd.lib
        debug ${CMAKE_SOURCE_DIR}/winlibs/lib/dbg/xapian.lib optimized ${CMAKE_SOURCE_DIR}/winlibs/lib/xapian.lib
        debug ${CMAKE_SOURCE_DIR}/winlibs/lib/dbg/hunspell-1.7.lib optimized ${CMAKE_SOURCE_DIR}/winlibs/lib/hunspell-1.7.lib
        debug ${CMAKE_SOURCE_DIR}/winlibs/lib/dbg/zim.lib optimized ${CMAKE_SOURCE_DIR}/winlibs/lib/zim.lib
        debug ${CMAKE_SOURCE_DIR}/winlibs/lib/dbg/opencc.lib optimized ${CMAKE_SOURCE_DIR}/winlibs/lib/opencc.lib
        debug ${CMAKE_SOURCE_DIR}/winlibs/lib/dbg/zlibd.lib optimized ${CMAKE_SOURCE_DIR}/winlibs/lib/zlib.lib
        )
target_link_libraries(${GOLDENDICT} PRIVATE ${THIRD_PARTY_LIBARY})

# Copy .dlls to output dir

file(GLOB DLL_FILES LIST_DIRECTORIES false "${CMAKE_SOURCE_DIR}/winlibs/lib/msvc/*.dll" "${Qt6_ROOT}/bin/av*.dll" "${Qt6_ROOT}/bin/sw*.dll")
foreach (A_DLL_FILE ${DLL_FILES})
    get_filename_component(TEMP_VAR_HOLDING_DLL_FILENAME ${A_DLL_FILE} NAME)
    configure_file("${A_DLL_FILE}" "${GD_WIN_OUTPUT_DIR}/${TEMP_VAR_HOLDING_DLL_FILENAME}" COPYONLY)
endforeach ()

if (CMAKE_BUILD_TYPE MATCHES Debug)
    file(GLOB DLL_FILES LIST_DIRECTORIES false "${CMAKE_SOURCE_DIR}/winlibs/lib/dbg/*.dll")
else ()
    file(GLOB DLL_FILES LIST_DIRECTORIES false "${CMAKE_SOURCE_DIR}/winlibs/lib/*.dll")
endif ()
foreach (A_DLL_FILE ${DLL_FILES})
    get_filename_component(TEMP_VAR_HOLDING_DLL_FILENAME ${A_DLL_FILE} NAME)
    configure_file("${A_DLL_FILE}" "${GD_WIN_OUTPUT_DIR}/${TEMP_VAR_HOLDING_DLL_FILENAME}" COPYONLY)
endforeach ()

if (WITH_EPWING_SUPPORT)
    add_subdirectory(thirdparty/eb EXCLUDE_FROM_ALL)
    target_include_directories(${GOLDENDICT} PRIVATE
            thirdparty
            )
    target_link_libraries(${GOLDENDICT} PRIVATE eb)

    set_target_properties(eb PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${GD_WIN_OUTPUT_DIR})
endif ()

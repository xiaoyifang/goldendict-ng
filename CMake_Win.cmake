target_compile_definitions(${GOLDENDICT} PUBLIC
        __WIN32
        INCLUDE_LIBRARY_PATH # temporal hack to let singleapplication compile
        )

target_include_directories(${GOLDENDICT} PUBLIC
        ${CMAKE_SOURCE_DIR}/winlibs/include/
        )

set_property(TARGET ${BIN_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

file(GLOB WINLIBS_FILES "${CMAKE_SOURCE_DIR}/winlibs/lib/msvc/*.lib")
foreach (A_WIN_LIB ${WINLIBS_FILES})
    target_link_libraries(${GOLDENDICT} PRIVATE ${A_WIN_LIB})
endforeach ()

file(GLOB WINLIBS_FILES "${CMAKE_SOURCE_DIR}/winlibs/lib/xapian/rel/*.lib")
foreach (A_WIN_LIB ${WINLIBS_FILES})
    target_link_libraries(${GOLDENDICT} PRIVATE ${A_WIN_LIB})
endforeach ()

# zim dependencies
file(GLOB WINLIBS_FILES "${CMAKE_SOURCE_DIR}/winlibs/lib/*.lib")
foreach (A_WIN_LIB ${WINLIBS_FILES})
    target_link_libraries(${GOLDENDICT} PRIVATE ${A_WIN_LIB})
endforeach ()

# Copy .dlls to output dir

file(GLOB DLL_FILES LIST_DIRECTORIES false "${CMAKE_SOURCE_DIR}/winlibs/lib/msvc/*.dll")
foreach (A_DLL_FILE ${DLL_FILES})
    get_filename_component(TEMP_VAR_HOLDING_DLL_FILENAME ${A_DLL_FILE} NAME)
    configure_file("${A_DLL_FILE}" "${CMAKE_BINARY_DIR}/${TEMP_VAR_HOLDING_DLL_FILENAME}" COPYONLY)
endforeach ()

file(GLOB DLL_FILES LIST_DIRECTORIES false "${CMAKE_SOURCE_DIR}/winlibs/lib/xapian/rel/*.dll")
foreach (A_DLL_FILE ${DLL_FILES})
    get_filename_component(TEMP_VAR_HOLDING_DLL_FILENAME ${A_DLL_FILE} NAME)
    configure_file("${A_DLL_FILE}" "${CMAKE_BINARY_DIR}/${TEMP_VAR_HOLDING_DLL_FILENAME}" COPYONLY)
endforeach ()

file(GLOB DLL_FILES LIST_DIRECTORIES false "${CMAKE_SOURCE_DIR}/winlibs/lib/*.dll")
foreach (A_DLL_FILE ${DLL_FILES})
    get_filename_component(TEMP_VAR_HOLDING_DLL_FILENAME ${A_DLL_FILE} NAME)
    configure_file("${A_DLL_FILE}" "${CMAKE_BINARY_DIR}/${TEMP_VAR_HOLDING_DLL_FILENAME}" COPYONLY)
endforeach ()

if (WITH_EPWING_SUPPORT)
    add_subdirectory(thirdparty/eb)
    target_include_directories(${GOLDENDICT} PRIVATE
        thirdparty
    )
    target_link_libraries(${GOLDENDICT} PRIVATE eb)
endif ()

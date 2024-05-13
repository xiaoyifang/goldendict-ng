find_package(BZip2 REQUIRED)
find_package(Iconv REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(Vorbis CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PKGCONFIG_DEPS IMPORTED_TARGET
        hunspell
        libzim
        lzo2
        opencc
        xapian-core
)

target_link_libraries(${GOLDENDICT}
        PRIVATE
        PkgConfig::PKGCONFIG_DEPS
        BZip2::BZip2
        Iconv::Iconv
        LibLZMA::LibLZMA
        Vorbis::vorbis
        Vorbis::vorbisfile
        ZLIB::ZLIB
)

if (WITH_EPWING_SUPPORT)
    add_subdirectory(thirdparty/eb EXCLUDE_FROM_ALL)
    target_include_directories(${GOLDENDICT} PRIVATE
            thirdparty
            )
    target_link_libraries(${GOLDENDICT} PRIVATE eb)

    set_target_properties(eb PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${GD_WIN_OUTPUT_DIR})
endif ()

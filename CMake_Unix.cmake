#### Include Paths

if (APPLE)
    # old & new homebrew's include paths
    target_include_directories(${GOLDENDICT} PRIVATE /usr/local/include /opt/homebrew/include)
endif ()

target_include_directories(${GOLDENDICT} PRIVATE
        ${PROJECT_SOURCE_DIR}/thirdparty)

#### Special Platform supporting libraries

if (LINUX OR BSD)
    find_package(X11 REQUIRED)
    pkg_check_modules(LIBXTST IMPORTED_TARGET xtst)
    target_compile_definitions(${GOLDENDICT} PUBLIC HAVE_X11)
    target_link_libraries(${GOLDENDICT} PRIVATE X11 PkgConfig::LIBXTST)
endif ()

if (APPLE)
    find_library(CARBON_LIBRARY Carbon REQUIRED)
    target_link_libraries(${GOLDENDICT} PRIVATE ${CARBON_LIBRARY})
endif ()

##### Finding packages from package manager


find_package(PkgConfig REQUIRED)
# Provided by Cmake
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
find_package(Iconv REQUIRED)


# Consider all PkgConfig dependencies as one
pkg_check_modules(PKGCONFIG_DEPS IMPORTED_TARGET
        hunspell
        lzo2
        opencc
        vorbis # .ogg
        vorbisfile
        liblzma
        libzstd
        )

target_link_libraries(${GOLDENDICT} PRIVATE
        PkgConfig::PKGCONFIG_DEPS
        BZip2::BZip2
        ZLIB::ZLIB
        Iconv::Iconv
        )

if (WITH_FFMPEG_PLAYER)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
            libavcodec
            libavformat
            libavutil
            libswresample
            )
    target_link_libraries(${GOLDENDICT} PRIVATE PkgConfig::FFMPEG)
endif ()

if (WITH_XAPIAN)
    find_package(Xapian REQUIRED) # https://github.com/xapian/xapian/tree/master/xapian-core/cmake
    target_link_libraries(${GOLDENDICT} PRIVATE ${XAPIAN_LIBRARIES})
endif ()

if (WITH_EPWING_SUPPORT)
    find_library(EB_LIBRARY eb REQUIRED)
    target_link_libraries(${GOLDENDICT} PRIVATE ${EB_LIBRARY})
endif ()

if (WITH_ZIM)
    pkg_check_modules(ZIM REQUIRED IMPORTED_TARGET libzim)
    target_link_libraries(${GOLDENDICT} PRIVATE PkgConfig::ZIM)
endif ()

if (USE_SYSTEM_FMT)
    find_package(fmt)
    target_link_libraries(${GOLDENDICT} PRIVATE fmt::fmt)
endif ()

if (USE_SYSTEM_TOML)
    find_package(tomlplusplus)
    target_link_libraries(${GOLDENDICT} PRIVATE tomlplusplus::tomlplusplus)
endif ()
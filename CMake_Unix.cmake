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
    if (X11_FOUND AND X11_Xtst_FOUND)
        target_compile_definitions(${GOLDENDICT} PUBLIC HAVE_X11)
        target_link_libraries(${GOLDENDICT} PRIVATE ${X11_LIBRARIES} ${X11_Xtst_LIB})
        target_include_directories(${GOLDENDICT} PRIVATE ${X11_INCLUDE_DIR} ${X11_Xtst_INCLUDE_PATH})
    else ()
        message(FATAL_ERROR "Cannot find X11 and libXtst!")
    endif ()
endif ()

if (APPLE)
    find_library(CARBON_LIBRARY Carbon REQUIRED)
    target_link_libraries(${GOLDENDICT} PRIVATE ${CARBON_LIBRARY})
endif ()

##### Finding packages from package manager

find_package(PkgConfig REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)

# Consider all PkgConfig dependencies as one
pkg_check_modules(PKGCONFIG_DEPS IMPORTED_TARGET
        hunspell
        lzo2
        opencc
        vorbis # .ogg
        vorbisfile
        liblzma
        libzstd
        xapian-core
)

target_link_libraries(${GOLDENDICT} PRIVATE
        PkgConfig::PKGCONFIG_DEPS
        BZip2::BZip2
        ZLIB::ZLIB
)

# On FreeBSD, there are two iconv, libc iconv & GNU libiconv.
# The system one is good enough, the following is a workaround to use libc iconv on freeBSD.
if (BSD STREQUAL "FreeBSD")
    # Simply do nothing. libc includes iconv on freeBSD.
    # LIBICONV_PLUG is a magic word to turn /usr/include/local/inconv.h, which belong to GNU libiconv, into normal iconv.h
    # Same hack used by SDL https://github.com/libsdl-org/SDL/blob/d6ebbc2fa4abdbe0bd53d0ce8804a492ecb042b9/src/stdlib/SDL_iconv.c#L27-L28
    target_compile_definitions(${GOLDENDICT} PUBLIC LIBICONV_PLUG)
else ()
    find_package(Iconv REQUIRED)
    target_link_libraries(${GOLDENDICT} PRIVATE Iconv::Iconv)
endif ()

if (WITH_FFMPEG_PLAYER)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
            libavcodec
            libavformat
            libavutil
            libswresample
    )
    target_link_libraries(${GOLDENDICT} PRIVATE PkgConfig::FFMPEG)
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
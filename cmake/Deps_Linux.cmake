find_package(PkgConfig REQUIRED)

set(Optional_Pkgs "")
list(APPEND Optional_Pkgs "fmt")
if (USE_SYSTEM_TOML)
    list(APPEND Optional_Pkgs "tomlplusplus")
endif ()
if (WITH_ZIM)
    list(APPEND Optional_Pkgs "libzim")
endif ()
if (WITH_FFMPEG_PLAYER)
    list(APPEND Optional_Pkgs "libavcodec;libavformat;libavutil;libswresample")
endif ()
if (WITH_X11)
    set(X11_Pkgs "x11;xtst")
    target_compile_definitions(${GOLDENDICT} PUBLIC WITH_X11)
endif ()

pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET
        hunspell
        liblzma
        lzo2
        opencc
        vorbis # .ogg
        vorbisfile
        xapian-core
        zlib
        ${X11_Pkgs}
        ${Optional_Pkgs}
)


find_package(BZip2 REQUIRED) # FreeBSD misses .pc file https://www.freshports.org/archivers/bzip2
target_link_libraries(${GOLDENDICT} PRIVATE PkgConfig::DEPS BZip2::BZip2)

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

if (WITH_EPWING_SUPPORT)
    find_library(EB_LIBRARY eb REQUIRED)
    target_link_libraries(${GOLDENDICT} PRIVATE ${EB_LIBRARY})
endif ()

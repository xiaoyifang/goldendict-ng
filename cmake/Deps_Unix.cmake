#### Various workarounds
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

# Import all PkgConfig dependencies as one
pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET
        bzip2
        hunspell
        liblzma
        lzo2
        opencc
        vorbis # .ogg
        vorbisfile
        xapian-core
        zlib
)

target_link_libraries(${GOLDENDICT} PRIVATE PkgConfig::DEPS)

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
    if (APPLE)
        # ICU from homebrew is "key-only", we need to manually prioritize it -> see `brew info icu4c`
        # And we needs to find the correct one if multiple versions co exists.
        set(ENV{PATH} "$ENV{PATH}:/usr/local/bin/:/opt/homebrew/bin") # add brew command into PATH
        execute_process(
                COMMAND sh -c [=[brew --prefix $(brew deps libzim | grep icu4c)]=]
                OUTPUT_VARIABLE ICU_REQUIRED_BY_ZIM_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
        message(STATUS "Found correct homebrew icu path -> ${ICU_REQUIRED_BY_ZIM_PREFIX}")
        set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:${ICU_REQUIRED_BY_ZIM_PREFIX}/lib/pkgconfig")
        message(STATUS "Updated pkg_config_path -> $ENV{PKG_CONFIG_PATH}")

        # icu4c as transitive dependency of libzim may not be automatically copied into app bundle
        # so we manually discover the icu4c from homebrew, then find the relevent dylibs
        set(BREW_ICU_ADDITIONAL_DYLIBS "${ICU_REQUIRED_BY_ZIM_PREFIX}/lib/libicudata.dylib ${ICU_REQUIRED_BY_ZIM_PREFIX}/lib/libicui18n.dylib ${ICU_REQUIRED_BY_ZIM_PREFIX}/lib/libicuuc.dylib")
        message(STATUS "Additional ICU `.dylib`s -> ${BREW_ICU_ADDITIONAL_DYLIBS}")
    endif ()

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

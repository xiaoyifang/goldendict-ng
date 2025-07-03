set(ENV{PATH} "$ENV{PATH}:/usr/local/bin/:/opt/homebrew/bin") # add brew command into PATH
target_include_directories(${GOLDENDICT} PRIVATE /usr/local/include /opt/homebrew/include)

find_library(CARBON_LIBRARY Carbon REQUIRED) # for accessibility API
target_link_libraries(${GOLDENDICT} PRIVATE ${CARBON_LIBRARY})

find_package(PkgConfig REQUIRED)

set(Optional_Pkgs "")
if (USE_SYSTEM_FMT)
    list(APPEND Optional_Pkgs "fmt")
endif ()
if (USE_SYSTEM_TOML)
    list(APPEND Optional_Pkgs "tomlplusplus")
endif ()
if (WITH_ZIM)
    list(APPEND Optional_Pkgs "libzim")
endif ()
if (WITH_FFMPEG_PLAYER)
    list(APPEND Optional_Pkgs "libavcodec;libavformat;libavutil;libswresample")
    list(APPEND Optional_Pkgs "libsharpyuv;libwebp") # transitive dependencies that macdeployqt cannot find.
endif ()

if (WITH_ZIM)
    # ICU from homebrew is "key-only", we need to manually prioritize it -> see `brew info icu4c`
    # And we needs to find the correct one if multiple versions co exists.
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

pkg_check_modules(DEPS REQUIRED IMPORTED_TARGET
        hunspell
        liblzma
        lzo2
        opencc
        vorbis # .ogg
        vorbisfile
        xapian-core
        zlib
        ${Optional_Pkgs}
)

find_package(Iconv REQUIRED)
find_package(BZip2 REQUIRED)
target_link_libraries(${GOLDENDICT} PRIVATE PkgConfig::DEPS BZip2::BZip2 Iconv::Iconv)

if (WITH_EPWING_SUPPORT)
    find_library(EB_LIBRARY eb REQUIRED)
    target_link_libraries(${GOLDENDICT} PRIVATE ${EB_LIBRARY})
endif ()


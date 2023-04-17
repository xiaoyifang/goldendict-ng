cmake_minimum_required(VERSION 3.23)

# Experimental Cmake build only supposed to be used by dev
# Qt6.4+ only
# LINUX, APPLE and MSVC are used to detect system

option(WITH_FFMPEG_PLAYER "Enable support for FFMPEG player" ON)
option(WITH_EPWING_SUPPORT "Enable epwing support" ON)
# Remove this once cmake 5.25 becomes minimum version
if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if (APPLE)
    if(WITH_EPWING_SUPPORT)
    add_subdirectory(thirdparty/eb)
    endif()

    include_directories(/usr/local/include /opt/homebrew/include)
endif ()

project(goldendict
        VERSION 22.11.20
        LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#### Importing Libraries

find_package(Qt6 REQUIRED COMPONENTS
        Concurrent
        Core5Compat
        Help
        LinguistTools
        Multimedia
        WebEngineWidgets
        Widgets
        Svg
        Xml
        TextToSpeech
        )

if (APPLE)
    find_library(CARBON_LIBRARY Carbon REQUIRED)
endif ()

if (UNIX)
    # Provided by Cmake
    find_package(ZLIB REQUIRED)
    find_package(BZip2 REQUIRED)

    # PkgConfig only packages
    find_package (PkgConfig REQUIRED)
    pkg_check_modules (PKGCONFIG_DEPS IMPORTED_TARGET
            hunspell
            lzo2
            opencc
            vorbis # .ogg
            vorbisfile
            liblzma
            libzstd
            )
    if (WITH_FFMPEG_PLAYER)
        pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
                libavcodec
                libavformat
                libavutil
                libswresample
                )
    endif ()
endif ()

if(LINUX)
    find_package(X11 REQUIRED)
    pkg_check_modules (LIBXTST IMPORTED_TARGET xtst)

    if(WITH_EPWING_SUPPORT)
        add_subdirectory(thirdparty/eb EXCLUDE_FROM_ALL)
    endif()
    # add_library(libeb SHARED IMPORTED)
    # set_target_properties(libeb PROPERTIES IMPORTED_LOCATION /usr/lib/libeb.so)
    # set_target_properties(libeb PROPERTIES INCLUDE_DIRECTORIES /usr/include/)
endif()

#### Compile time files and preprocessor flags

# Obtain git commit hash
execute_process(
        COMMAND git rev-parse --short=8 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TIMESTAMP date_for_version_file) # note: this variable is cached for every run, but for user installation, this doesn't matter much
configure_file(version.txt.in ${CMAKE_SOURCE_DIR}/version.txt)

# hack -> this string will be compared with another string, thus having surrounding " "
add_compile_definitions(PROGRAM_VERSION="${PROJECT_VERSION}")

add_compile_definitions(CMAKE_USED_HACK) # temporal hack to avoid breaking qmake build
add_compile_definitions(
        MAKE_ZIM_SUPPORT
        MAKE_QTMULTIMEDIA_PLAYER
        MAKE_CHINESE_CONVERSION_SUPPORT
)

if (WITH_FFMPEG_PLAYER)
    add_compile_definitions(MAKE_FFMPEG_PLAYER)
endif ()

if(LINUX)
    add_compile_definitions(HAVE_X11)
endif()

if(MSVC)
    add_compile_definitions(NO_EPWING_SUPPORT
                            __WIN32)
endif()

#### Sources Files

set(QRC_RESOURCES
        flags.qrc
        resources.qrc
        scripts.qrc
        src/stylesheets/css.qrc
        )

# auto discovery of ui files https://cmake.org/cmake/help/v3.26/prop_tgt/AUTOUIC_SEARCH_PATHS.html
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/")

if(WITH_EPWING_SUPPORT)
set(EPWING_SUPPORT_SRC
        epwing.cc
        epwing.hh
        epwing_book.cc
        epwing_book.hh
        epwing_charmap.cc
        epwing_charmap.hh
        )
else()
    add_compile_definitions(NO_EPWING_SUPPORT)
endif()        

if (APPLE)
    set(MAC_SRC
            machotkeywrapper.mm
            macmouseover.hh
            macmouseover.mm

            src/platform/gd_clipboard.cpp
            src/platform/gd_clipboard.h
            )
endif ()


if(WITH_FFMPEG_PLAYER)
    set(FFMPEG_SUPPORT_SRC
            ffmpegaudio.cc
            ffmpegaudio.hh
            ffmpegaudioplayer.hh
            )
endif()

set(PROJECT_SOURCES
        ${MAC_SRC}
        ${QRC_RESOURCES}
        ${EPWING_SUPPORT_SRC}
        ${FFMPEG_SUPPORT_SRC}
        aard.cc
        aard.hh
        about.cc
        about.hh
        ankiconnector.cpp
        ankiconnector.h
        article_inspect.cpp
        article_inspect.h
        article_maker.cc
        article_maker.hh
        article_netmgr.cc
        article_netmgr.hh
        articlewebpage.cpp
        articlewebpage.h
        articlewebview.cc
        articlewebview.hh
        atomic_rename.cc
        atomic_rename.hh
        audiolink.cc
        audiolink.hh
        audiooutput.cpp
        audiooutput.h
        audioplayerfactory.cc
        audioplayerfactory.hh
        audioplayerinterface.hh
        base/globalregex.cc
        base/globalregex.hh
        belarusiantranslit.cc
        belarusiantranslit.hh
        bgl.cc
        bgl.hh
        bgl_babylon.cc
        bgl_babylon.hh
        btreeidx.cc
        btreeidx.hh
        chinese.cc
        chinese.hh
        chineseconversion.cc
        chineseconversion.hh
        chunkedstorage.cc
        chunkedstorage.hh
        config.cc
        config.hh
        country.cc
        country.hh
        decompress.cc
        decompress.hh
        delegate.cc
        delegate.hh
        dictdfiles.cc
        dictdfiles.hh
        dictheadwords.cc
        dictheadwords.hh
        dictinfo.cc
        dictinfo.hh
        dictionary.cc
        dictionary.hh
        dictionarybar.cc
        dictionarybar.hh
        dictserver.cc
        dictserver.hh
        dictspanewidget.hh
        dictzip.c
        dictzip.h
        dsl.cc
        dsl.hh
        dsl_details.cc
        dsl_details.hh
        editdictionaries.cc
        editdictionaries.hh
        ex.hh
        externalaudioplayer.cc
        externalaudioplayer.hh
        externalviewer.cc
        externalviewer.hh
        favoritespanewidget.cc
        favoritespanewidget.hh
        file.cc
        file.hh
        filetype.cc
        filetype.hh
        fixx11h.h
        folding.cc
        folding.hh
        forvo.cc
        forvo.hh
        fsencoding.cc
        fsencoding.hh
        ftshelpers.cc
        ftshelpers.hh
        fulltextsearch.cc
        fulltextsearch.hh
        gddebug.cc
        gddebug.hh
        german.cc
        german.hh
        gestures.cc
        gestures.hh
        globalbroadcaster.cpp
        globalbroadcaster.h
        gls.cc
        gls.hh
        greektranslit.cc
        greektranslit.hh
        groupcombobox.cc
        groupcombobox.hh
        groups.cc
        groups.hh
        groups_widgets.cc
        groups_widgets.hh
        headwordsmodel.cpp
        headwordsmodel.h
        history.cc
        history.hh
        historypanewidget.cc
        historypanewidget.hh
        hotkeywrapper.cc
        hotkeywrapper.hh
        htmlescape.cc
        htmlescape.hh
        hunspell.cc
        hunspell.hh
        iconv.cc
        iconv.hh
        iframeschemehandler.cpp
        iframeschemehandler.h
        inc_case_folding.hh
        inc_diacritic_folding.hh
        indexedzip.cc
        indexedzip.hh
        initializing.cc
        initializing.hh
        instances.cc
        instances.hh
        keyboardstate.cc
        keyboardstate.hh
        langcoder.cc
        langcoder.hh
        language.cc
        language.hh
        loaddictionaries.cc
        loaddictionaries.hh
        lsa.cc
        lsa.hh
        main.cc
        mainstatusbar.cc
        mainstatusbar.hh
        maintabwidget.cc
        maintabwidget.hh
        mainwindow.cc
        mainwindow.hh
        mdictparser.cc
        mdictparser.hh
        mdx.cc
        mdx.hh
        mediawiki.cc
        mediawiki.hh
        mruqmenu.cc
        mruqmenu.hh
        multimediaaudioplayer.cc
        multimediaaudioplayer.hh
        mutex.cc
        mutex.hh
        orderandprops.cc
        orderandprops.hh
        parsecmdline.cc
        parsecmdline.hh
        preferences.cc
        preferences.hh
        programs.cc
        programs.hh
        resourceschemehandler.cpp
        resourceschemehandler.h
        ripemd.cc
        ripemd.hh
        romaji.cc
        romaji.hh
        russiantranslit.cc
        russiantranslit.hh
        scanflag.cc
        scanflag.hh
        scanpopup.cc
        scanpopup.hh
        sdict.cc
        sdict.hh
        searchpanewidget.hh
        slob.cc
        slob.hh
        sounddir.cc
        sounddir.hh
        sources.cc
        sources.hh
        splitfile.cc
        splitfile.hh
        sptr.hh
        stardict.cc
        stardict.hh
        stylescombobox.cc
        stylescombobox.hh
        termination.cc
        termination.hh
        thirdparty/qtsingleapplication/src/qtlocalpeer.cpp
        thirdparty/qtsingleapplication/src/qtlocalpeer.h
        thirdparty/qtsingleapplication/src/qtsingleapplication.cpp
        thirdparty/qtsingleapplication/src/qtsingleapplication.h
        thirdparty/qtsingleapplication/src/qtsinglecoreapplication.cpp
        thirdparty/qtsingleapplication/src/qtsinglecoreapplication.h
        tiff.cc
        tiff.hh
        translatebox.cc
        translatebox.hh
        transliteration.cc
        transliteration.hh
        ufile.cc
        ufile.hh
        utf8.cc
        utf8.hh
        utils.cc
        utils.hh
        voiceengines.cc
        voiceengines.hh
        webmultimediadownload.cc
        webmultimediadownload.hh
        website.cc
        website.hh
        weburlrequestinterceptor.cpp
        weburlrequestinterceptor.h
        wildcard.cc
        wildcard.hh
        wordfinder.cc
        wordfinder.hh
        wordlist.cc
        wordlist.hh
        wstring.hh
        wstring_qt.cc
        wstring_qt.hh
        xdxf.cc
        xdxf.hh
        xdxf2html.cc
        xdxf2html.hh
        zim.cc
        zim.hh
        zipfile.cc
        zipfile.hh
        zipsounds.cc
        zipsounds.hh
        src/dict/lingualibre.cpp src/dict/lingualibre.h
        src/ui/articleview.cpp src/ui/articleview.h
        src/ui/ftssearchpanel.cpp src/ui/ftssearchpanel.h
        src/ui/searchpanel.cpp src/ui/searchpanel.h
        speechclient.hh
        speechclient.cc
        texttospeechsource.cc
        texttospeechsource.hh
        )

qt_add_executable(${CMAKE_PROJECT_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES})

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}/thirdparty/qtsingleapplication/src
        ${PROJECT_SOURCE_DIR}/src/)

if(MSVC)
    add_compile_definitions(INCLUDE_LIBRARY_PATH)
    target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
            ${CMAKE_SOURCE_DIR}/winlibs/include/
            )

    set_property(TARGET ${CMAKE_PROJECT_NAME} PROPERTY 
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")            
endif()

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
        Qt6::Xml
        Qt6::Concurrent
        Qt6::Core5Compat
        Qt6::Multimedia
        Qt6::WebEngineWidgets
        Qt6::Widgets
        Qt6::Svg
        Qt6::TextToSpeech
        )

if(APPLE)
    if(WITH_EPWING_SUPPORT)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE eb)
    endif()
endif()

if(UNIX)
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
            ${PROJECT_SOURCE_DIR}/thirdparty)
            
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
            # pkg-config packages need manually link
            PkgConfig::PKGCONFIG_DEPS
            BZip2::BZip2
            ZLIB::ZLIB #hidden requirement of dsl_details.cc and more?
            )
endif()

if(MSVC)
    file(GLOB WINLIBS_FILES "${CMAKE_SOURCE_DIR}/winlibs/lib/msvc/*.lib")
    foreach(A_WIN_LIB ${WINLIBS_FILES})
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${A_WIN_LIB})
    endforeach()
endif()


if(LINUX)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE X11 PkgConfig::LIBXTST)

    if(WITH_EPWING_SUPPORT)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE eb )
    endif()
endif ()

if(APPLE)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${CARBON_LIBRARY})
endif()

if (WITH_FFMPEG_PLAYER)
    if (APPLE OR LINUX)
        target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE  PkgConfig::FFMPEG)
    endif ()
endif ()

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
        )

install(TARGETS ${CMAKE_PROJECT_NAME}
        BUNDLE DESTINATION .
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})


# include all *ts files under locale
file(GLOB TRANS_FILES "locale/*.ts")

# Put generated files to output dir's locale
set_source_files_properties(${TRANS_FILES}
        PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/locale")

# a wrapper over qt_add_lupdate and  qt_add_lrelease
qt_add_translations(${CMAKE_PROJECT_NAME} TS_FILES ${TRANS_FILES}
        QM_FILES_OUTPUT_VARIABLE qm_files)

qt_finalize_target(${CMAKE_PROJECT_NAME})

if (LINUX)
    install(FILES ${CMAKE_SOURCE_DIR}/redist/org.goldendict.GoldenDict.desktop DESTINATION share/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/redist/org.goldendict.GoldenDict.metainfo.xml DESTINATION share/metainfo)

    install(FILES ${CMAKE_SOURCE_DIR}/redist/icons/goldendict.png DESTINATION share/pixmaps)

    install(FILES ${qm_files} DESTINATION share/goldendict/locale)
endif()

# Copy .dlls to output dir
if(MSVC)
    file(GLOB DLL_FILES LIST_DIRECTORIES false "${CMAKE_SOURCE_DIR}/winlibs/lib/msvc/*.dll")
    foreach(A_DLL_FILE ${DLL_FILES})
        get_filename_component(TEMP_VAR_HOLDING_DLL_FILENAME ${A_DLL_FILE} NAME)
        configure_file("${A_DLL_FILE}" "${CMAKE_BINARY_DIR}/${TEMP_VAR_HOLDING_DLL_FILENAME}" COPYONLY)
    endforeach()
endif()
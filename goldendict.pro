TEMPLATE = app
TARGET = goldendict
VERSION = 23.04.03-alpha

# Generate version file. We do this here and in a build rule described later.
# The build rule is required since qmake isn't run each time the project is
# rebuilt; and doing it here is required too since any other way the RCC
# compiler would complain if version.txt wouldn't exist (fresh checkouts).

system(git describe --tags --always --dirty): hasGit=1

!isEmpty(hasGit){
    GIT_HASH=$$system(git rev-parse --short=8 HEAD )
}

!exists( version.txt ) {
      message( "generate version.txt...." )
      system(echo $${VERSION}.$${GIT_HASH} on $${_DATE_} > version.txt)
}


!CONFIG( verbose_build_output ) {
  !win32|*-msvc* {
    # Reduce build log verbosity except for MinGW builds (mingw-make cannot
    # execute "@echo ..." commands inserted by qmake).
    CONFIG += silent
  }
}

CONFIG( release, debug|release ) {
  DEFINES += NDEBUG
}

# DEPENDPATH += . generators
INCLUDEPATH += .
INCLUDEPATH += ./src/

QT += core \
      gui \
      xml \
      network \
      svg \
      widgets \
      webenginewidgets\
      webchannel\
      printsupport \
      concurrent \
      texttospeech

greaterThan(QT_MAJOR_VERSION, 5): QT += webenginecore core5compat

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00

!CONFIG( no_qtmultimedia_player ) {
  QT += multimedia
  DEFINES += MAKE_QTMULTIMEDIA_PLAYER
}

!CONFIG( no_ffmpeg_player ) {
  # ffmpeg depended on multimedia now.
  QT += multimedia
  DEFINES += MAKE_FFMPEG_PLAYER
}

contains(DEFINES, MAKE_QTMULTIMEDIA_PLAYER|MAKE_FFMPEG_PLAYER) {
  HEADERS += \
  audiooutput.hh
  SOURCES += \
  audiooutput.cc
}

# on windows platform ,only works in release build

CONFIG( use_xapian ) {
  DEFINES += USE_XAPIAN

  LIBS+= -lxapian
}

CONFIG( use_iconv ) {
  DEFINES += USE_ICONV
  unix:!mac{
    #ignore
  }
  else {
      LIBS+= -liconv
  }
}

CONFIG += exceptions \
    rtti \
    stl  \
    c++17 \
    lrelease \
    utf8_source \
    force_debug_info

mac {
    CONFIG += app_bundle
}
    
OBJECTS_DIR = build
UI_DIR = build
MOC_DIR = build
RCC_DIR = build
LIBS += -lz \
        -lbz2 \
        -llzo2

win32 {
    QM_FILES_INSTALL_PATH = /locale/
    TARGET = GoldenDict

    win32-msvc* {
        # VS does not recognize 22.number.alpha,cause errors during compilation under MSVC++
        VERSION = 23.04.03 
        DEFINES += __WIN32 _CRT_SECURE_NO_WARNINGS
        contains(QMAKE_TARGET.arch, x86_64) {
            DEFINES += NOMINMAX __WIN64
        }
        LIBS += -L$${PWD}/winlibs/lib/msvc
        # silence the warning C4290: C++ exception specification ignored
        QMAKE_CXXFLAGS += /wd4290 /Zc:__cplusplus /std:c++17 /permissive- 
        # QMAKE_LFLAGS_RELEASE += /OPT:REF /OPT:ICF

        # QMAKE_CXXFLAGS_RELEASE += /GL # slows down the linking significantly
        LIBS += -lshell32 -luser32 -lsapi -lole32
        Debug: LIBS+= -lhunspelld
        Release: LIBS+= -lhunspell
        HUNSPELL_LIB = hunspell
    }

    LIBS += -lwsock32 \
        -lpsapi \
        -lole32 \
        -loleaut32 \
        -ladvapi32 \
        -lcomdlg32
    LIBS += -lvorbisfile \
        -lvorbis \
        -logg
    !CONFIG( no_ffmpeg_player ) {
        LIBS += -lswresample \
            -lavutil \
            -lavformat \
            -lavcodec
    }

    RC_ICONS += icons/programicon.ico icons/programicon_old.ico
    INCLUDEPATH += winlibs/include

    # Enable console in Debug mode on Windows, with useful logging messages
    Debug:CONFIG += console

    Release:DEFINES += NO_CONSOLE

    gcc48:QMAKE_CXXFLAGS += -Wno-unused-local-typedefs

    !CONFIG( no_chinese_conversion_support ) {
        CONFIG += chinese_conversion_support
    }
}
!CONFIG( no_macos_universal ) {
    DEFINES += INCLUDE_LIBRARY_PATH
}
unix:!mac {
    DEFINES += HAVE_X11

    lessThan(QT_MAJOR_VERSION, 6):     QT += x11extras

    CONFIG += link_pkgconfig

    PKGCONFIG += vorbisfile \
        vorbis \
        ogg \
        hunspell
    !CONFIG( no_ffmpeg_player ) {
        PKGCONFIG += libavutil \
            libavformat \
            libavcodec \
            libswresample \
    }
    !arm {
        LIBS += -lX11 -lXtst
    }

    # Install prefix: first try to use qmake's PREFIX variable,
    # then $PREFIX from system environment, and if both fails,
    # use the hardcoded /usr/local.
    PREFIX = $${PREFIX}
    isEmpty( PREFIX ):PREFIX = $$(PREFIX)
    isEmpty( PREFIX ):PREFIX = /usr/local
    message(Install Prefix is: $$PREFIX)

    DEFINES += PROGRAM_DATA_DIR=\\\"$$PREFIX/share/goldendict/\\\"
    target.path = $$PREFIX/bin/
    locale.path = $$PREFIX/share/goldendict/locale/
    locale.files = locale/*.qm
    INSTALLS += target \
        locale
    icons.path = $$PREFIX/share/pixmaps
    icons.files = redist/icons/*.*
    INSTALLS += icons
    desktops.path = $$PREFIX/share/applications
    desktops.files = redist/*.desktop
    INSTALLS += desktops
    metainfo.path = $$PREFIX/share/metainfo
    metainfo.files = redist/*.metainfo.xml
    INSTALLS += metainfo
}
freebsd {
    LIBS +=   -lexecinfo
}
mac {
    QM_FILES_INSTALL_PATH = /locale/
    TARGET = GoldenDict
    # Uncomment this line to make a universal binary.
    # You will need to use Xcode 3 and Qt Carbon SDK
    # if you want the support for PowerPC and/or Mac OS X 10.4
    # CONFIG += x86 x86_64 ppc
    LIBS += -lz \
        -lbz2 \
        -lvorbisfile \
        -lvorbis \
        -logg \
        -llzo2

    !CONFIG( no_ffmpeg_player ) {
        LIBS += -lswresample \
            -lavutil \
            -lavformat \
            -lavcodec
    }
    QT_CONFIG -= no-pkg-config
    CONFIG += link_pkgconfig


    !CONFIG( no_macos_universal ) {
        LIBS+=        -lhunspell
        INCLUDEPATH += $${PWD}/maclibs/include
        LIBS += -L$${PWD}/maclibs/lib -framework AppKit -framework Carbon
    }
    else{
        PKGCONFIG +=   hunspell
        INCLUDEPATH += /opt/homebrew/include /usr/local/include
        LIBS += -L/opt/homebrew/lib -L/usr/local/lib -framework AppKit -framework Carbon
    }

    OBJECTIVE_SOURCES += machotkeywrapper.mm \
                         macmouseover.mm
    ICON = icons/macicon.icns
    QMAKE_INFO_PLIST = myInfo.plist

    !CONFIG( no_macos_universal ) {
        QMAKE_POST_LINK = mkdir -p GoldenDict.app/Contents/Frameworks && \
                          cp -nR $${PWD}/maclibs/lib/ GoldenDict.app/Contents/Frameworks/ && \
                          mkdir -p GoldenDict.app/Contents/MacOS/locale && \
                          cp -R locale/*.qm GoldenDict.app/Contents/MacOS/locale/
    }
    else{
        QMAKE_POST_LINK = mkdir -p GoldenDict.app/Contents/Frameworks && \
                          cp -nR $${PWD}/maclibs/lib/libeb.dylib GoldenDict.app/Contents/Frameworks/ && \
                          mkdir -p GoldenDict.app/Contents/MacOS/locale && \
                          cp -R locale/*.qm GoldenDict.app/Contents/MacOS/locale/
    }

    !CONFIG( no_chinese_conversion_support ) {
        CONFIG += chinese_conversion_support
        QMAKE_POST_LINK += && mkdir -p GoldenDict.app/Contents/MacOS/opencc && \
                             cp -R $${PWD}/opencc/*.* GoldenDict.app/Contents/MacOS/opencc/
    }

}
DEFINES += PROGRAM_VERSION=\\\"$$VERSION\\\"

# Input
HEADERS += \
    about.hh \
    ankiconnector.hh \
    article_inspect.hh \
    article_maker.hh \
    article_netmgr.hh \
    articlewebpage.hh \
    articlewebview.hh \
    atomic_rename.hh \
    audiolink.hh \
    audioplayerfactory.hh \
    audioplayerinterface.hh \
    base/globalregex.hh \
    base_type.hh \
    btreeidx.hh \
    chunkedstorage.hh \
    config.hh \
    country.hh \
    decompress.hh \
    delegate.hh \
    dictdfiles.hh \
    dictheadwords.hh \
    dictinfo.hh \
    dictionarybar.hh \
    dictserver.hh \
    dictspanewidget.hh \
    dictzip.hh \
    editdictionaries.hh \
    ex.hh \
    externalaudioplayer.hh \
    externalviewer.hh \
    favoritespanewidget.hh \
    ffmpegaudio.hh \
    ffmpegaudioplayer.hh \
    file.hh \
    filetype.hh \
    folding.hh \
    fsencoding.hh \
    ftshelpers.hh \
    fulltextsearch.hh \
    gdappstyle.hh \
    gddebug.hh \
    gestures.hh \
    globalbroadcaster.h \
    groupcombobox.hh \
    groups.hh \
    groups_widgets.hh \
    headwordsmodel.hh \
    history.hh \
    historypanewidget.hh \
    hotkeywrapper.hh \
    htmlescape.hh \
    iconv.hh \
    iframeschemehandler.hh \
    inc_case_folding.hh \
    inc_diacritic_folding.hh \
    indexedzip.hh \
    initializing.hh \
    instances.hh \
    keyboardstate.hh \
    langcoder.hh \
    language.hh \
    loaddictionaries.hh \
    mainstatusbar.hh \
    maintabwidget.hh \
    mainwindow.hh \
    mdictparser.hh \
    mruqmenu.hh \
    multimediaaudioplayer.hh \
    mutex.hh \
    orderandprops.hh \
    parsecmdline.hh \
    preferences.hh \
    resourceschemehandler.hh \
    ripemd.hh \
    scanpopup.hh \
    searchpanewidget.hh \
    splitfile.hh \
    sptr.hh \
    src/dict/aard.hh \
    src/dict/belarusiantranslit.hh \
    src/dict/bgl.hh \
    src/dict/bgl_babylon.hh \
    src/dict/dictionary.hh \
    src/dict/dsl.hh \
    src/dict/dsl_details.hh \
    src/dict/forvo.hh \
    src/dict/german.hh \
    src/dict/gls.hh \
    src/dict/greektranslit.hh \
    src/dict/lingualibre.hh \
    src/dict/lsa.hh \
    src/dict/mdx.hh \
    src/dict/mediawiki.hh \
    src/dict/programs.hh \
    src/dict/romaji.hh \
    src/dict/russiantranslit.hh \
    src/dict/sdict.hh \
    src/dict/slob.hh \
    src/dict/sounddir.hh \
    src/dict/sources.hh \
    src/dict/stardict.hh \
    src/dict/transliteration.hh \
    src/dict/voiceengines.hh \
    src/dict/website.hh \
    src/dict/xdxf.hh \
    src/dict/xdxf2html.hh \
    src/dict/zim.hh \
    src/dict/zipsounds.hh \
    src/hunspell.hh \
    src/ui/articleview.hh \
    src/ui/ftssearchpanel.hh \
    src/ui/searchpanel.hh \
    stylescombobox.hh \
    termination.hh \
    tiff.hh \
    translatebox.hh \
    ufile.hh \
    utf8.hh \
    utils.hh \
    webmultimediadownload.hh \
    weburlrequestinterceptor.h \
    wordfinder.hh \
    wordlist.hh \
    wstring.hh \
    wstring_qt.hh \
    zipfile.hh

FORMS += $$files(src/ui/*.ui)

SOURCES += \
    about.cc \
    ankiconnector.cc \
    article_inspect.cc \
    article_maker.cc \
    article_netmgr.cc \
    articlewebpage.cc \
    articlewebview.cc \
    atomic_rename.cc \
    audiolink.cc \
    audioplayerfactory.cc \
    base/globalregex.cc \
    btreeidx.cc \
    chunkedstorage.cc \
    config.cc \
    country.cc \
    decompress.cc \
    delegate.cc \
    dictdfiles.cc \
    dictheadwords.cc \
    dictinfo.cc \
    dictionarybar.cc \
    dictserver.cc \
    dictzip.c \
    editdictionaries.cc \
    externalaudioplayer.cc \
    externalviewer.cc \
    favoritespanewidget.cc \
    ffmpegaudio.cc \
    file.cc \
    filetype.cc \
    folding.cc \
    fsencoding.cc \
    ftshelpers.cc \
    fulltextsearch.cc \
    gdappstyle.cc \
    gddebug.cc \
    gestures.cc \
    globalbroadcaster.cc \
    groupcombobox.cc \
    groups.cc \
    groups_widgets.cc \
    headwordsmodel.cc \
    history.cc \
    historypanewidget.cc \
    hotkeywrapper.cc \
    htmlescape.cc \
    iconv.cc \
    iframeschemehandler.cc \
    indexedzip.cc \
    initializing.cc \
    instances.cc \
    keyboardstate.cc \
    langcoder.cc \
    language.cc \
    loaddictionaries.cc \
    main.cc \
    mainstatusbar.cc \
    maintabwidget.cc \
    mainwindow.cc \
    mdictparser.cc \
    mruqmenu.cc \
    multimediaaudioplayer.cc \
    mutex.cc \
    orderandprops.cc \
    parsecmdline.cc \
    preferences.cc \
    resourceschemehandler.cc \
    ripemd.cc \
    scanpopup.cc \
    splitfile.cc \
    src/dict/aard.cc \
    src/dict/belarusiantranslit.cc \
    src/dict/bgl.cc \
    src/dict/bgl_babylon.cc \
    src/dict/dictionary.cc \
    src/dict/dsl.cc \
    src/dict/dsl_details.cc \
    src/dict/forvo.cc \
    src/dict/german.cc \
    src/dict/gls.cc \
    src/dict/greektranslit.cc \
    src/dict/hunspell.cc \
    src/dict/lingualibre.cc \
    src/dict/lsa.cc \
    src/dict/mdx.cc \
    src/dict/mediawiki.cc \
    src/dict/programs.cc \
    src/dict/romaji.cc \
    src/dict/russiantranslit.cc \
    src/dict/sdict.cc \
    src/dict/slob.cc \
    src/dict/sounddir.cc \
    src/dict/sources.cc \
    src/dict/stardict.cc \
    src/dict/transliteration.cc \
    src/dict/voiceengines.cc \
    src/dict/website.cc \
    src/dict/xdxf.cc \
    src/dict/xdxf2html.cc \
    src/dict/zim.cc \
    src/dict/zipsounds.cc \
    src/ui/articleview.cc \
    src/ui/ftssearchpanel.cc \
    src/ui/searchpanel.cc \
    stylescombobox.cc \
    termination.cc \
    tiff.cc \
    translatebox.cc \
    ufile.cc \
    utf8.cc \
    utils.cc \
    webmultimediadownload.cc \
    weburlrequestinterceptor.cc \
    wordfinder.cc \
    wordlist.cc \
    wstring_qt.cc \
    zipfile.cc

#speech to text
SOURCES += speechclient.cc \
           texttospeechsource.cc
HEADERS += texttospeechsource.hh \
           speechclient.hh

mac {
    HEADERS += macmouseover.hh \
    src/platform/gd_clipboard.hh
    SOURCES += \
    src/platform/gd_clipboard.cc
}

unix:!mac {
    HEADERS += scanflag.hh
    SOURCES += scanflag.cc
}


HEADERS += wildcard.hh
SOURCES += wildcard.cc


CONFIG( zim_support ) {
  DEFINES += MAKE_ZIM_SUPPORT
  LIBS += -llzma -lzstd
}

CONFIG( no_epwing_support ) {
  DEFINES += NO_EPWING_SUPPORT
}

!CONFIG( no_epwing_support ) {
  HEADERS += epwing.hh \
             epwing_book.hh \
             epwing_charmap.hh
  SOURCES += epwing.cc \
             epwing_book.cc \
             epwing_charmap.cc
  LIBS += -leb
}

CONFIG( chinese_conversion_support ) {
  DEFINES += MAKE_CHINESE_CONVERSION_SUPPORT
  FORMS   += chineseconversion.ui
  HEADERS += chinese.hh \
             chineseconversion.hh
  SOURCES += chinese.cc \
             chineseconversion.cc
  LIBS += -lopencc
}

RESOURCES += resources.qrc \
    scripts.qrc \
    flags.qrc \
    src/stylesheets/css.qrc
#EXTRA_TRANSLATIONS += thirdparty/qwebengine_ts/qtwebengine_zh_CN.ts
TRANSLATIONS += $$files(locale/*.ts)

# Build version file
!isEmpty( hasGit ) {
  PRE_TARGETDEPS      += $$PWD/version.txt
}

# This makes qmake generate translations


isEmpty(QMAKE_LRELEASE):QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease


# The *.qm files might not exist when qmake is run for the first time,
# causing the standard install rule to be ignored, and no translations
# will be installed. With this, we create the qm files during qmake run.
!win32 {
  system($${QMAKE_LRELEASE} -silent $${_PRO_FILE_} 2> /dev/null)
}
else{
  system($${QMAKE_LRELEASE} -silent $${_PRO_FILE_})
}

updateqm.input = TRANSLATIONS
updateqm.output = locale/${QMAKE_FILE_BASE}.qm
updateqm.commands = $$QMAKE_LRELEASE \
    ${QMAKE_FILE_IN} \
    -qm \
    ${QMAKE_FILE_OUT}
updateqm.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += updateqm
TS_OUT = $$TRANSLATIONS
TS_OUT ~= s/.ts/.qm/g
PRE_TARGETDEPS += $$TS_OUT

#QTBUG-105984
# avoid qt6.4.0-6.4.2 .  the qtmultimedia module is buggy in all these versions

include( thirdparty/qtsingleapplication/src/qtsingleapplication.pri )


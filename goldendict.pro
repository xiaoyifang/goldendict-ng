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
HEADERS += folding.hh \
    ankiconnector.hh \
    article_inspect.hh \
    articlewebpage.hh \
    base/globalregex.hh \
    base_type.hh \
    dictzip.hh \
    globalbroadcaster.h \
    headwordsmodel.hh \
    iframeschemehandler.hh \
    inc_case_folding.hh \
    inc_diacritic_folding.hh \
    mainwindow.hh \
    resourceschemehandler.hh \
    sptr.hh \
    dictionary.hh \
    ex.hh \
    config.hh \
    sources.hh \
    src/dict/lingualibre.hh \
    src/ui/articleview.hh \
    src/ui/ftssearchpanel.hh \
    src/ui/searchpanel.hh \
    utf8.hh \
    file.hh \
    bgl_babylon.hh \
    bgl.hh \
    initializing.hh \
    article_netmgr.hh \
    btreeidx.hh \
    stardict.hh \
    chunkedstorage.hh \
    weburlrequestinterceptor.h \
    xdxf2html.hh \
    iconv.hh \
    lsa.hh \
    htmlescape.hh \
    dsl.hh \
    dsl_details.hh \
    filetype.hh \
    fsencoding.hh \
    groups.hh \
    groups_widgets.hh \
    instances.hh \
    article_maker.hh \
    scanpopup.hh \
    audioplayerinterface.hh \
    audioplayerfactory.hh \
    ffmpegaudioplayer.hh \
    multimediaaudioplayer.hh \
    externalaudioplayer.hh \
    externalviewer.hh \
    wordfinder.hh \
    groupcombobox.hh \
    keyboardstate.hh \
    preferences.hh \
    mutex.hh \
    mediawiki.hh \
    sounddir.hh \
    hunspell.hh \
    dictdfiles.hh \
    audiolink.hh \
    wstring.hh \
    wstring_qt.hh \
    hotkeywrapper.hh \
    searchpanewidget.hh \
    langcoder.hh \
    editdictionaries.hh \
    loaddictionaries.hh \
    transliteration.hh \
    romaji.hh \
    belarusiantranslit.hh \
    russiantranslit.hh \
    german.hh \
    website.hh \
    orderandprops.hh \
    language.hh \
    dictionarybar.hh \
    history.hh \
    atomic_rename.hh \
    articlewebview.hh \
    zipfile.hh \
    indexedzip.hh \
    termination.hh \
    greektranslit.hh \
    webmultimediadownload.hh \
    forvo.hh \
    country.hh \
    about.hh \
    programs.hh \
    parsecmdline.hh \
    dictspanewidget.hh \
    maintabwidget.hh \
    mainstatusbar.hh \
    gdappstyle.hh \
    ufile.hh \
    xdxf.hh \
    sdict.hh \
    decompress.hh \
    aard.hh \
    mruqmenu.hh \
    dictinfo.hh \
    zipsounds.hh \
    stylescombobox.hh \
    translatebox.hh \
    historypanewidget.hh \
    wordlist.hh \
    mdictparser.hh \
    mdx.hh \
    voiceengines.hh \
    ffmpegaudio.hh \
    delegate.hh \
    zim.hh \
    gddebug.hh \
    utils.hh \
    gestures.hh \
    tiff.hh \
    dictheadwords.hh \
    fulltextsearch.hh \
    ftshelpers.hh \
    dictserver.hh \
    slob.hh \
    ripemd.hh \
    gls.hh \
    splitfile.hh \
    favoritespanewidget.hh

FORMS += $$files(src/ui/*.ui)

SOURCES += folding.cc \
    ankiconnector.cc \
    article_inspect.cc \
    articlewebpage.cc \
    base/globalregex.cc \
    globalbroadcaster.cc \
    headwordsmodel.cc \
    iframeschemehandler.cc \
    main.cc \
    dictionary.cc \
    config.cc \
    resourceschemehandler.cc \
    sources.cc \
    mainwindow.cc \
    src/dict/lingualibre.cc \
    src/ui/articleview.cc \
    src/ui/ftssearchpanel.cc \
    src/ui/searchpanel.cc \
    utf8.cc \
    file.cc \
    bgl_babylon.cc \
    bgl.cc \
    initializing.cc \
    article_netmgr.cc \
    dictzip.c \
    btreeidx.cc \
    stardict.cc \
    chunkedstorage.cc \
    utils.cc \
    weburlrequestinterceptor.cc \
    xdxf2html.cc \
    iconv.cc \
    lsa.cc \
    htmlescape.cc \
    dsl.cc \
    dsl_details.cc \
    filetype.cc \
    fsencoding.cc \
    groups.cc \
    groups_widgets.cc \
    instances.cc \
    article_maker.cc \
    scanpopup.cc \
    audioplayerfactory.cc \
    multimediaaudioplayer.cc \
    externalaudioplayer.cc \
    externalviewer.cc \
    wordfinder.cc \
    groupcombobox.cc \
    keyboardstate.cc \
    preferences.cc \
    mutex.cc \
    mediawiki.cc \
    sounddir.cc \
    hunspell.cc \
    dictdfiles.cc \
    audiolink.cc \
    wstring_qt.cc \
    hotkeywrapper.cc \
    langcoder.cc \
    editdictionaries.cc \
    loaddictionaries.cc \
    transliteration.cc \
    romaji.cc \
    belarusiantranslit.cc \
    russiantranslit.cc \
    german.cc \
    website.cc \
    orderandprops.cc \
    language.cc \
    dictionarybar.cc \
    history.cc \
    atomic_rename.cc \
    articlewebview.cc \
    zipfile.cc \
    indexedzip.cc \
    termination.cc \
    greektranslit.cc \
    webmultimediadownload.cc \
    forvo.cc \
    country.cc \
    about.cc \
    programs.cc \
    parsecmdline.cc \
    maintabwidget.cc \
    mainstatusbar.cc \
    gdappstyle.cc \
    ufile.cc \
    xdxf.cc \
    sdict.cc \
    decompress.cc \
    aard.cc \
    mruqmenu.cc \
    dictinfo.cc \
    zipsounds.cc \
    stylescombobox.cc \
    translatebox.cc \
    historypanewidget.cc \
    wordlist.cc \
    mdictparser.cc \
    mdx.cc \
    voiceengines.cc \
    ffmpegaudio.cc \
    delegate.cc \
    zim.cc \
    gddebug.cc \
    gestures.cc \
    tiff.cc \
    dictheadwords.cc \
    fulltextsearch.cc \
    ftshelpers.cc \
    dictserver.cc \
    slob.cc \
    ripemd.cc \
    gls.cc \
    splitfile.cc \
    favoritespanewidget.cc

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


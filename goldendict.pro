TEMPLATE = app
TARGET = goldendict
VERSION = 23.06.01-ChildrenDay

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
INCLUDEPATH += ./src/
INCLUDEPATH += ./src/ui    # for compiled .ui files to find headers
INCLUDEPATH += ./src/common
INCLUDEPATH += ./thirdparty/tomlplusplus
INCLUDEPATH += ./thirdparty/fmt/include

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
  src/audiooutput.hh
  SOURCES += \
  src/audiooutput.cc
}

#xapian is the must option now.
win32{
  Debug: LIBS+= -L$$PWD/winlibs/lib/xapian/dbg/ -lxapian
  Release: LIBS+= -L$$PWD/winlibs/lib/xapian/rel/ -lxapian
}else{
  LIBS += -lxapian
}

CONFIG( use_breakpad ) {
  DEFINES += USE_BREAKPAD

  win32: LIBS += -L$$PWD/thirdparty/breakpad/lib/ -llibbreakpad -llibbreakpad_client
  else:unix: LIBS += -L$$PWD/thirdparty/breakpad/lib/ -llibbreakpa

  INCLUDEPATH += $$PWD/thirdparty/breakpad/include
  DEPENDPATH += $$PWD/thirdparty/breakpad/include

  CONFIG( release, debug|release ) {
    # create debug symbols for release builds
    CONFIG*=force_debug_info
    QMAKE_CXXFLAGS_RELEASE_WITH_DEBUGINFO -= -O2
  }
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
    utf8_source

mac {
    CONFIG += app_bundle
}
    
OBJECTS_DIR = build
UI_DIR = build
MOC_DIR = build
RCC_DIR = build
LIBS += -lbz2 \
        -llzo2

win32{
    Debug: LIBS+= -lzlibd
    Release: LIBS+= -lzlib
}else{
  LIBS += -lz 
}


win32 {
    QM_FILES_INSTALL_PATH = /locale/
    TARGET = GoldenDict

    win32-msvc* {
        # VS does not recognize 22.number.alpha,cause errors during compilation under MSVC++
        VERSION = 23.06.01 
        DEFINES += __WIN32 _CRT_SECURE_NO_WARNINGS
        contains(QMAKE_TARGET.arch, x86_64) {
            DEFINES += NOMINMAX __WIN64
        }
        LIBS += -L$${PWD}/winlibs/lib/msvc
        # silence the warning C4290: C++ exception specification ignored,C4267  size_t to const T , lost data.
        QMAKE_CXXFLAGS += /wd4290 /wd4267 /Zc:__cplusplus /std:c++17 /permissive-
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

!mac {
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

    PKGCONFIG +=   hunspell
    INCLUDEPATH += /opt/homebrew/include /usr/local/include
    LIBS += -L/opt/homebrew/lib -L/usr/local/lib -framework AppKit -framework Carbon
    

    OBJECTIVE_SOURCES += src/macos/machotkeywrapper.mm \
                         src/macos/macmouseover.mm
    ICON = icons/macicon.icns
    QMAKE_INFO_PLIST = redist/myInfo.plist


    QMAKE_POST_LINK = mkdir -p GoldenDict.app/Contents/Frameworks && \
                      mkdir -p GoldenDict.app/Contents/MacOS/locale && \
                      cp -R locale/*.qm GoldenDict.app/Contents/MacOS/locale/
    

    !CONFIG( no_chinese_conversion_support ) {
        CONFIG += chinese_conversion_support
        QMAKE_POST_LINK += && mkdir -p GoldenDict.app/Contents/MacOS/opencc && \
                             cp -R $${PWD}/opencc/*.* GoldenDict.app/Contents/MacOS/opencc/
    }

}
DEFINES += PROGRAM_VERSION=\\\"$$VERSION\\\"

# Input
HEADERS += \
    src/ankiconnector.hh \
    src/article_maker.hh \
    src/article_netmgr.hh \
    src/common/atomic_rename.hh \
    src/audiolink.hh \
    src/audioplayerfactory.hh \
    src/audioplayerinterface.hh \
    src/btreeidx.hh \
    src/chunkedstorage.hh \
    src/common/base_type.hh \
    src/common/ex.hh \
    src/common/file.hh \
    src/common/filetype.hh \
    src/common/gddebug.hh \
    src/common/globalbroadcaster.hh \
    src/common/globalregex.hh \
    src/common/help.hh \
    src/common/htmlescape.hh \
    src/common/iconv.hh \
    src/common/inc_case_folding.hh \
    src/common/sptr.hh \
    src/common/ufile.hh \
    src/common/utf8.hh \
    src/common/utils.hh \
    src/common/wstring.hh \
    src/common/wstring_qt.hh \
    src/config.hh \
    src/decompress.hh \
    src/delegate.hh \
    src/dict/aard.hh \
    src/dict/belarusiantranslit.hh \
    src/dict/bgl.hh \
    src/dict/bgl_babylon.hh \
    src/dict/customtransliteration.hh \
    src/dict/dictdfiles.hh \
    src/dict/dictionary.hh \
    src/dict/dictserver.hh \
    src/dict/dsl.hh \
    src/dict/dsl_details.hh \
    src/dict/forvo.hh \
    src/dict/german.hh \
    src/dict/gls.hh \
    src/dict/greektranslit.hh \
    src/dict/hunspell.hh \
    src/dict/lingualibre.hh \
    src/dict/loaddictionaries.hh \
    src/dict/lsa.hh \
    src/dict/mdictparser.hh \
    src/dict/mdx.hh \
    src/dict/mediawiki.hh \
    src/dict/programs.hh \
    src/dict/ripemd.hh \
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
    src/dictzip.hh \
    src/externalaudioplayer.hh \
    src/externalviewer.hh \
    src/ffmpegaudio.hh \
    src/ffmpegaudioplayer.hh \
    src/common/folding.hh \
    src/ftshelpers.hh \
    src/fulltextsearch.hh \
    src/gestures.hh \
    src/headwordsmodel.hh \
    src/history.hh \
    src/hotkeywrapper.hh \
    src/iframeschemehandler.hh \
    src/indexedzip.hh \
    src/initializing.hh \
    src/instances.hh \
    src/keyboardstate.hh \
    src/langcoder.hh \
    src/language.hh \
    src/metadata.hh \
    src/multimediaaudioplayer.hh \
    src/parsecmdline.hh \
    src/resourceschemehandler.hh \
    src/splitfile.hh \
    src/termination.hh \
    src/tiff.hh \
    src/ui/about.hh \
    src/ui/article_inspect.hh \
    src/ui/articleview.hh \
    src/ui/articlewebpage.hh \
    src/ui/articlewebview.hh \
    src/ui/dictheadwords.hh \
    src/ui/dictinfo.hh \
    src/ui/dictionarybar.hh \
    src/ui/dictspanewidget.hh \
    src/ui/editdictionaries.hh \
    src/ui/favoritespanewidget.hh \
    src/ui/ftssearchpanel.hh \
    src/ui/groupcombobox.hh \
    src/ui/groups.hh \
    src/ui/groups_widgets.hh \
    src/ui/historypanewidget.hh \
    src/ui/mainstatusbar.hh \
    src/ui/maintabwidget.hh \
    src/ui/mainwindow.hh \
    src/ui/mruqmenu.hh \
    src/ui/orderandprops.hh \
    src/ui/preferences.hh \
    src/ui/scanpopup.hh \
    src/ui/searchpanel.hh \
    src/ui/searchpanewidget.hh \
    src/ui/stylescombobox.hh \
    src/ui/translatebox.hh \
    src/webmultimediadownload.hh \
    src/weburlrequestinterceptor.hh \
    src/wordfinder.hh \
    src/wordlist.hh \
    src/zipfile.hh \
    thirdparty/tomlplusplus/toml++/toml.h

FORMS += $$files(src/ui/*.ui)

SOURCES += \
    src/ankiconnector.cc \
    src/article_maker.cc \
    src/article_netmgr.cc \
    src/common/atomic_rename.cc \
    src/audiolink.cc \
    src/audioplayerfactory.cc \
    src/btreeidx.cc \
    src/chunkedstorage.cc \
    src/common/file.cc \
    src/common/filetype.cc \
    src/common/gddebug.cc \
    src/common/globalbroadcaster.cc \
    src/common/globalregex.cc \
    src/common/help.cc \
    src/common/htmlescape.cc \
    src/common/iconv.cc \
    src/common/ufile.cc \
    src/common/utf8.cc \
    src/common/utils.cc \
    src/common/wstring_qt.cc \
    src/config.cc \
    src/decompress.cc \
    src/delegate.cc \
    src/dict/aard.cc \
    src/dict/belarusiantranslit.cc \
    src/dict/bgl.cc \
    src/dict/bgl_babylon.cc \
    src/dict/customtransliteration.cc \
    src/dict/dictdfiles.cc \
    src/dict/dictionary.cc \
    src/dict/dictserver.cc \
    src/dict/dsl.cc \
    src/dict/dsl_details.cc \
    src/dict/forvo.cc \
    src/dict/german.cc \
    src/dict/gls.cc \
    src/dict/greektranslit.cc \
    src/dict/hunspell.cc \
    src/dict/lingualibre.cc \
    src/dict/loaddictionaries.cc \
    src/dict/lsa.cc \
    src/dict/mdictparser.cc \
    src/dict/mdx.cc \
    src/dict/mediawiki.cc \
    src/dict/programs.cc \
    src/dict/ripemd.cc \
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
    src/dictzip.c \
    src/externalaudioplayer.cc \
    src/externalviewer.cc \
    src/ffmpegaudio.cc \
    src/common/folding.cc \
    src/ftshelpers.cc \
    src/fulltextsearch.cc \
    src/gestures.cc \
    src/headwordsmodel.cc \
    src/history.cc \
    src/hotkeywrapper.cc \
    src/iframeschemehandler.cc \
    src/indexedzip.cc \
    src/initializing.cc \
    src/instances.cc \
    src/keyboardstate.cc \
    src/langcoder.cc \
    src/language.cc \
    src/main.cc \
    src/metadata.cc \
    src/multimediaaudioplayer.cc \
    src/parsecmdline.cc \
    src/resourceschemehandler.cc \
    src/splitfile.cc \
    src/termination.cc \
    src/tiff.cc \
    src/ui/about.cc \
    src/ui/article_inspect.cc \
    src/ui/articleview.cc \
    src/ui/articlewebpage.cc \
    src/ui/articlewebview.cc \
    src/ui/dictheadwords.cc \
    src/ui/dictinfo.cc \
    src/ui/dictionarybar.cc \
    src/ui/editdictionaries.cc \
    src/ui/favoritespanewidget.cc \
    src/ui/ftssearchpanel.cc \
    src/ui/groupcombobox.cc \
    src/ui/groups.cc \
    src/ui/groups_widgets.cc \
    src/ui/historypanewidget.cc \
    src/ui/mainstatusbar.cc \
    src/ui/maintabwidget.cc \
    src/ui/mainwindow.cc \
    src/ui/mruqmenu.cc \
    src/ui/orderandprops.cc \
    src/ui/preferences.cc \
    src/ui/scanpopup.cc \
    src/ui/searchpanel.cc \
    src/ui/stylescombobox.cc \
    src/ui/translatebox.cc \
    src/webmultimediadownload.cc \
    src/weburlrequestinterceptor.cc \
    src/wordfinder.cc \
    src/wordlist.cc \
    src/zipfile.cc \
    thirdparty/fmt/format.cc

#speech to text
SOURCES += src/speechclient.cc \
           src/texttospeechsource.cc
HEADERS += src/texttospeechsource.hh \
           src/speechclient.hh

mac {
    HEADERS += src/macos/macmouseover.hh \
               src/macos/gd_clipboard.hh
    SOURCES += src/macos/gd_clipboard.cc
}

unix:!mac {
    HEADERS += src/ui/scanflag.hh
    SOURCES += src/ui/scanflag.cc
}


HEADERS += src/common/wildcard.hh
SOURCES += src/common/wildcard.cc


CONFIG( zim_support ) {
  DEFINES += MAKE_ZIM_SUPPORT
  LIBS += -llzma -lzstd -lzim

    win32{
      Debug: LIBS+= -L$$PWD/winlibs/lib/dbg/
      Release: LIBS+= -L$$PWD/winlibs/lib/
    }
}

CONFIG( no_epwing_support ) {
  DEFINES += NO_EPWING_SUPPORT
}

!CONFIG( no_epwing_support ) {
  HEADERS += src/dict/epwing.hh \
             src/dict/epwing_book.hh \
             src/dict/epwing_charmap.hh
  SOURCES += src/dict/epwing.cc \
             src/dict/epwing_book.cc \
             src/dict/epwing_charmap.cc
  if(win32){
    INCLUDEPATH += thirdparty
    HEADERS += $$files(thirdparty/eb/*.h)
    SOURCES += $$files(thirdparty/eb/*.c)
  }
  else{
    LIBS += -leb
  }
}

CONFIG( chinese_conversion_support ) {
  DEFINES += MAKE_CHINESE_CONVERSION_SUPPORT
  FORMS   += src/ui/chineseconversion.ui
  HEADERS += src/dict/chinese.hh \
             src/ui/chineseconversion.hh
  SOURCES += src/dict/chinese.cc \
             src/ui/chineseconversion.cc
  LIBS += -lopencc
}

RESOURCES += resources.qrc \
    src/scripts/scripts.qrc \
    icons/flags.qrc \
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


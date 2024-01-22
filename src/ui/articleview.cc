/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "articleview.hh"
#include "dict/programs.hh"
#include "folding.hh"
#include "gddebug.hh"
#include "gestures.hh"
#include "globalbroadcaster.hh"
#include "speechclient.hh"
#include "utils.hh"
#include "webmultimediadownload.hh"
#include "wildcard.hh"
#include "wstring_qt.hh"
#include <QBuffer>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QVariant>
#include <QWebChannel>
#include <QWebEngineHistory>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineSettings>
#include <map>
#include <QApplication>
#include <QRandomGenerator>

#if ( QT_VERSION >= QT_VERSION_CHECK( 5, 0, 0 ) && QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 ) )
  #include <QWebEngineContextMenuData>
#endif
#if ( QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 ) )
  #include <QtCore5Compat/QRegExp>
  #include <QWebEngineContextMenuRequest>
  #include <QWebEngineFindTextResult>
  #include <utility>
#endif
#ifdef Q_OS_WIN32
  #include <windows.h>
  #include <QPainter>
#endif

using std::map;
using std::list;


namespace {

char const * const scrollToPrefix = "gdfrom-";

bool isScrollTo( QString const & id )
{
  return id.startsWith( scrollToPrefix );
}

QString dictionaryIdFromScrollTo( QString const & scrollTo )
{
  Q_ASSERT( isScrollTo( scrollTo ) );
  constexpr int scrollToPrefixLength = 7;
  return scrollTo.mid( scrollToPrefixLength );
}

QString searchStatusMessageNoMatches()
{
  return ArticleView::tr( "Phrase not found" );
}

QString searchStatusMessage( int activeMatch, int matchCount )
{
  Q_ASSERT( matchCount > 0 );
  Q_ASSERT( activeMatch > 0 );
  Q_ASSERT( activeMatch <= matchCount );
  return ArticleView::tr( "%1 of %2 matches" ).arg( activeMatch ).arg( matchCount );
}

} // unnamed namespace

QString ArticleView::scrollToFromDictionaryId( QString const & dictionaryId )
{
  Q_ASSERT( !isScrollTo( dictionaryId ) );
  return scrollToPrefix + dictionaryId;
}

ArticleView::ArticleView( QWidget * parent,
                          ArticleNetworkAccessManager & nm,
                          AudioPlayerPtr const & audioPlayer_,
                          std::vector< sptr< Dictionary::Class > > const & allDictionaries_,
                          Instances::Groups const & groups_,
                          bool popupView_,
                          Config::Class const & cfg_,
                          QAction & openSearchAction_,
                          QLineEdit const * translateLine_,
                          QAction * dictionaryBarToggled_,
                          GroupComboBox const * groupComboBox_ ):
  QWidget( parent ),
  articleNetMgr( nm ),
  audioPlayer( audioPlayer_ ),
  allDictionaries( allDictionaries_ ),
  groups( groups_ ),
  popupView( popupView_ ),
  cfg( cfg_ ),
  pasteAction( this ),
  articleUpAction( this ),
  articleDownAction( this ),
  goBackAction( this ),
  goForwardAction( this ),
  selectCurrentArticleAction( this ),
  copyAsTextAction( this ),
  inspectAction( this ),
  openSearchAction( openSearchAction_ ),
  searchIsOpened( false ),
  dictionaryBarToggled( dictionaryBarToggled_ ),
  groupComboBox( groupComboBox_ ),
  translateLine( translateLine_ )
{
  if ( groupComboBox_ )
    currentGroupId = groupComboBox_->getCurrentGroup();

  // setup GUI
  webview        = new ArticleWebView( this );
  ftsSearchPanel = new FtsSearchPanel( this );
  searchPanel    = new SearchPanel( this );

  // Layout
  auto * mainLayout = new QVBoxLayout( this );
  mainLayout->addWidget( webview );
  mainLayout->addWidget( ftsSearchPanel );
  mainLayout->addWidget( searchPanel );

  webview->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding );
  ftsSearchPanel->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Minimum );
  searchPanel->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Minimum );

  mainLayout->setContentsMargins( 0, 0, 0, 0 );

  // end UI setup

  connect( searchPanel->previous, &QPushButton::clicked, this, &ArticleView::on_searchPrevious_clicked );
  connect( searchPanel->next, &QPushButton::clicked, this, &ArticleView::on_searchNext_clicked );
  connect( searchPanel->close, &QPushButton::clicked, this, &ArticleView::on_searchCloseButton_clicked );
  connect( searchPanel->caseSensitive, &QPushButton::clicked, this, &ArticleView::on_searchCaseSensitive_clicked );
  connect( searchPanel->highlightAll, &QPushButton::clicked, this, &ArticleView::on_highlightAllButton_clicked );
  connect( searchPanel->lineEdit, &QLineEdit::textEdited, this, &ArticleView::on_searchText_textEdited );
  connect( searchPanel->lineEdit, &QLineEdit::returnPressed, this, &ArticleView::on_searchText_returnPressed );
  connect( ftsSearchPanel->next, &QPushButton::clicked, this, &ArticleView::on_ftsSearchNext_clicked );
  connect( ftsSearchPanel->previous, &QPushButton::clicked, this, &ArticleView::on_ftsSearchPrevious_clicked );

  //

  webview->setUp( const_cast< Config::Class * >( &cfg ) );

  goBackAction.setShortcut( QKeySequence( "Alt+Left" ) );
  webview->addAction( &goBackAction );
  connect( &goBackAction, &QAction::triggered, this, &ArticleView::back );

  goForwardAction.setShortcut( QKeySequence( "Alt+Right" ) );
  webview->addAction( &goForwardAction );
  connect( &goForwardAction, &QAction::triggered, this, &ArticleView::forward );

  webview->pageAction( QWebEnginePage::Copy )->setShortcut( QKeySequence::Copy );
  webview->addAction( webview->pageAction( QWebEnginePage::Copy ) );

  QAction * selectAll = webview->pageAction( QWebEnginePage::SelectAll );
  selectAll->setShortcut( QKeySequence::SelectAll );
  selectAll->setShortcutContext( Qt::WidgetWithChildrenShortcut );
  webview->addAction( selectAll );

  webview->setContextMenuPolicy( Qt::CustomContextMenu );

  connect( webview, &QWebEngineView::loadFinished, this, &ArticleView::loadFinished );

  connect( webview, &ArticleWebView::linkClicked, this, &ArticleView::linkClicked );

  connect( webview->page(), &QWebEnginePage::titleChanged, this, &ArticleView::handleTitleChanged );

  connect( webview->page(), &QWebEnginePage::urlChanged, this, &ArticleView::handleUrlChanged );

  connect( webview, &QWidget::customContextMenuRequested, this, &ArticleView::contextMenuRequested );

  connect( webview->page(), &QWebEnginePage::linkHovered, this, &ArticleView::linkHovered );

  connect( webview, &ArticleWebView::doubleClicked, this, &ArticleView::doubleClicked );

  pasteAction.setShortcut( QKeySequence::Paste );
  webview->addAction( &pasteAction );
  connect( &pasteAction, &QAction::triggered, this, &ArticleView::pasteTriggered );

  articleUpAction.setShortcut( QKeySequence( "Alt+Up" ) );
  webview->addAction( &articleUpAction );
  connect( &articleUpAction, &QAction::triggered, this, &ArticleView::moveOneArticleUp );

  articleDownAction.setShortcut( QKeySequence( "Alt+Down" ) );
  webview->addAction( &articleDownAction );
  connect( &articleDownAction, &QAction::triggered, this, &ArticleView::moveOneArticleDown );

  webview->addAction( &openSearchAction );
  connect( &openSearchAction, &QAction::triggered, this, &ArticleView::openSearch );

  selectCurrentArticleAction.setShortcut( QKeySequence( "Ctrl+Shift+A" ) );
  selectCurrentArticleAction.setText( tr( "Select Current Article" ) );
  webview->addAction( &selectCurrentArticleAction );
  connect( &selectCurrentArticleAction, &QAction::triggered, this, &ArticleView::selectCurrentArticle );

  copyAsTextAction.setShortcut( QKeySequence( "Ctrl+Shift+C" ) );
  copyAsTextAction.setText( tr( "Copy as text" ) );
  webview->addAction( &copyAsTextAction );
  connect( &copyAsTextAction, &QAction::triggered, this, &ArticleView::copyAsText );

  inspectAction.setShortcut( QKeySequence( Qt::Key_F12 ) );
  inspectAction.setText( tr( "Inspect" ) );
  webview->addAction( &inspectAction );


  connect( &inspectAction, &QAction::triggered, this, &ArticleView::inspectElement );

  webview->installEventFilter( this );
  searchPanel->installEventFilter( this );
  ftsSearchPanel->installEventFilter( this );

  QWebEngineSettings * settings = webview->settings();
  settings->setUnknownUrlSchemePolicy( QWebEngineSettings::UnknownUrlSchemePolicy::DisallowUnknownUrlSchemes );
#if ( QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 ) )
  settings->defaultSettings()->setAttribute( QWebEngineSettings::LocalContentCanAccessRemoteUrls, true );
  settings->defaultSettings()->setAttribute( QWebEngineSettings::LocalContentCanAccessFileUrls, true );
  settings->defaultSettings()->setAttribute( QWebEngineSettings::ErrorPageEnabled, false );
  settings->defaultSettings()->setAttribute( QWebEngineSettings::PluginsEnabled, true );
  settings->defaultSettings()->setAttribute( QWebEngineSettings::PlaybackRequiresUserGesture, false );
  settings->defaultSettings()->setAttribute( QWebEngineSettings::JavascriptCanAccessClipboard, true );
  settings->defaultSettings()->setAttribute( QWebEngineSettings::PrintElementBackgrounds, false );
#else
  settings->setAttribute( QWebEngineSettings::LocalContentCanAccessRemoteUrls, true );
  settings->setAttribute( QWebEngineSettings::LocalContentCanAccessFileUrls, true );
  settings->setAttribute( QWebEngineSettings::ErrorPageEnabled, false );
  settings->setAttribute( QWebEngineSettings::PluginsEnabled, true );
  settings->setAttribute( QWebEngineSettings::PlaybackRequiresUserGesture, false );
  settings->setAttribute( QWebEngineSettings::JavascriptCanAccessClipboard, true );
  settings->setAttribute( QWebEngineSettings::PrintElementBackgrounds, false );
#endif

  auto html = articleNetMgr.getHtml( ResourceType::UNTITLE );

  webview->setHtml( QString::fromStdString( html ) );

  expandOptionalParts = cfg.preferences.alwaysExpandOptionalParts;

  webview->grabGesture( Gestures::GDPinchGestureType );
  webview->grabGesture( Gestures::GDSwipeGestureType );

  // Variable name for store current selection range
  rangeVarName = QString( "sr_%1" ).arg( QString::number( (quint64)this, 16 ) );


  connect( GlobalBroadcaster::instance(), &GlobalBroadcaster::dictionaryChanges, this, &ArticleView::setActiveDictIds );

  connect( GlobalBroadcaster::instance(), &GlobalBroadcaster::dictionaryClear, this, &ArticleView::dictionaryClear );


  channel = new QWebChannel( webview->page() );
  agent   = new ArticleViewAgent( this );
  attachWebChannelToHtml();
  ankiConnector = new AnkiConnector( this, cfg );
  connect( ankiConnector, &AnkiConnector::errorText, this, [ this ]( QString const & errorText ) {
    emit statusBarMessage( errorText );
  } );

  // Set up an Anki action if Anki integration is enabled in settings.
  if ( cfg.preferences.ankiConnectServer.enabled ) {
    sendToAnkiAction.setShortcut( QKeySequence( "Ctrl+Shift+N" ) );
    webview->addAction( &sendToAnkiAction );
    connect( &sendToAnkiAction, &QAction::triggered, this, &ArticleView::handleAnkiAction );
  }
}

// explicitly report the minimum size, to avoid
// sidebar widgets' improper resize during restore
QSize ArticleView::minimumSizeHint() const
{
  return searchPanel->minimumSizeHint();
}

void ArticleView::setCurrentGroupId( unsigned currentGrgId )
{
  currentGroupId = currentGrgId;
}
unsigned ArticleView::getCurrentGroupId()
{
  return currentGroupId;
}

ArticleView::~ArticleView()
{
  cleanupTemp();
  audioPlayer->stop();
  //channel->deregisterObject(this);
  webview->ungrabGesture( Gestures::GDPinchGestureType );
  webview->ungrabGesture( Gestures::GDSwipeGestureType );
}

void ArticleView::showDefinition( QString const & word,
                                  unsigned group,
                                  QString const & scrollTo,
                                  Contexts const & contexts_ )
{
  GlobalBroadcaster::instance()->pronounce_engine.reset();
  currentWord = word.trimmed();
  if ( currentWord.isEmpty() )
    return;
  historyMode = false;
  currentActiveDictIds.clear();
  // first, let's stop the player
  audioPlayer->stop();

  QUrl req;
  Contexts contexts( contexts_ );

  req.setScheme( "gdlookup" );
  req.setHost( "localhost" );
  Utils::Url::addQueryItem( req, "word", word );
  Utils::Url::addQueryItem( req, "group", QString::number( group ) );
  if ( cfg.preferences.ignoreDiacritics )
    Utils::Url::addQueryItem( req, "ignore_diacritics", "1" );

  if ( scrollTo.size() )
    Utils::Url::addQueryItem( req, "scrollto", scrollTo );

  if ( delayedHighlightText.size() ) {
    Utils::Url::addQueryItem( req, "regexp", delayedHighlightText );
    delayedHighlightText.clear();
  }

  if ( Contexts::Iterator pos = contexts.find( "gdanchor" ); pos != contexts.end() ) {
    Utils::Url::addQueryItem( req, "gdanchor", contexts[ "gdanchor" ] );
    contexts.erase( pos );
  }

  if ( contexts.size() ) {
    QBuffer buf;

    buf.open( QIODevice::WriteOnly );

    QDataStream stream( &buf );

    stream << contexts;

    buf.close();

    Utils::Url::addQueryItem( req, "contexts", QString::fromLatin1( buf.buffer().toBase64() ) );
  }

  QString mutedDicts = getMutedForGroup( group );

  if ( mutedDicts.size() )
    Utils::Url::addQueryItem( req, "muted", mutedDicts );

  // Any search opened is probably irrelevant now
  closeSearch();
  //QApplication::setOverrideCursor( Qt::WaitCursor );
  webview->setCursor( Qt::WaitCursor );
  load( req );

  // Update headwords history
  emit sendWordToHistory( word );
}

void ArticleView::showDefinition( QString const & word,
                                  QStringList const & dictIDs,
                                  QRegExp const & searchRegExp,
                                  unsigned group,
                                  bool ignoreDiacritics )
{
  if ( dictIDs.isEmpty() )
    return;
  currentWord = word.trimmed();
  if ( currentWord.isEmpty() )
    return;
  historyMode = false;
  //clear founded dicts.
  currentActiveDictIds.clear();
  // first, let's stop the player
  audioPlayer->stop();

  QUrl req;

  req.setScheme( "gdlookup" );
  req.setHost( "localhost" );
  Utils::Url::addQueryItem( req, "word", word );
  Utils::Url::addQueryItem( req, "dictionaries", dictIDs.join( "," ) );
  Utils::Url::addQueryItem( req, "regexp", searchRegExp.pattern() );
  if ( searchRegExp.caseSensitivity() == Qt::CaseSensitive )
    Utils::Url::addQueryItem( req, "matchcase", "1" );
  if ( searchRegExp.patternSyntax() == QRegExp::WildcardUnix )
    Utils::Url::addQueryItem( req, "wildcards", "1" );
  Utils::Url::addQueryItem( req, "group", QString::number( group ) );
  if ( ignoreDiacritics )
    Utils::Url::addQueryItem( req, "ignore_diacritics", "1" );

  // Any search opened is probably irrelevant now
  closeSearch();

  // Clear highlight all button selection
  searchPanel->highlightAll->setChecked( false );
  webview->setCursor( Qt::WaitCursor );

  load( req );

  // Update headwords history
  emit sendWordToHistory( word );
}

void ArticleView::sendToAnki( QString const & word, QString const & dict_definition, QString const & sentence )
{
  ankiConnector->sendToAnki( word, dict_definition, sentence );
}

void ArticleView::showAnticipation()
{
  webview->setHtml( "" );
  webview->setCursor( Qt::WaitCursor );
}

void ArticleView::inspectElement()
{
  emit inspectSignal( webview->page() );
}

void ArticleView::loadFinished( bool result )
{
  setZoomFactor( cfg.preferences.zoomFactor );
  QUrl url = webview->url();
  qDebug() << "article view loaded url:" << url.url().left( 200 ) << result;

  if ( url.url() == "about:blank" ) {
    return;
  }

  if ( !result ) {
    qWarning() << "article loaded unsuccessful";
    return;
  }

  if ( cfg.preferences.autoScrollToTargetArticle ) {
    QString const scrollTo = Utils::Url::queryItemValue( url, "scrollto" );
    if ( isScrollTo( scrollTo ) ) {
      setCurrentArticle( scrollTo, true );
    }
    else {
      setActiveArticleId( "" );
    }
  }
  else {
    //clear current active dictionary id;
    setActiveArticleId( "" );
  }

  webview->unsetCursor();

  // Expand collapsed article if only one loaded
  webview->page()->runJavaScript( QString( "gdCheckArticlesNumber();" ) );

  if ( !Utils::Url::queryItemValue( url, "gdanchor" ).isEmpty() ) {
    const QString anchor = QUrl::fromPercentEncoding( Utils::Url::encodedQueryItemValue( url, "gdanchor" ) );

    // Find GD anchor on page
    url.clear();
    url.setFragment( anchor );
    webview->page()->runJavaScript(
      QString( "window.location.hash = \"%1\"" ).arg( QString::fromUtf8( url.toEncoded() ) ) );
  }

  //the click audio url such as gdau://xxxx ,webview also emit a pageLoaded signal but with the result is false.need future investigation.
  //the audio link click ,no need to emit pageLoaded signal
  if ( result ) {
    emit pageLoaded( this );
  }
  if ( Utils::Url::hasQueryItem( webview->url(), "regexp" ) ) {
    highlightFTSResults();
  }
}

void ArticleView::handleTitleChanged( QString const & title )
{
  if ( !title.isEmpty() ) // Qt 5.x WebKit raise signal titleChanges(QString()) while navigation within page
    emit titleChanged( this, title );
}

void ArticleView::handleUrlChanged( QUrl const & url )
{
  QIcon icon;

  if ( unsigned group = getGroup( url ) ) {
    // Find the group's instance corresponding to the fragment value
    for ( auto const & g : groups ) {
      if ( g.id == group ) {
        // Found it
        icon = g.makeIcon();
        break;
      }
    }
  }

  emit iconChanged( this, icon );
}

unsigned ArticleView::getGroup( QUrl const & url )
{
  if ( url.scheme() == "gdlookup" && Utils::Url::hasQueryItem( url, "group" ) )
    return Utils::Url::queryItemValue( url, "group" ).toUInt();

  return 0;
}

QStringList ArticleView::getArticlesList()
{
  return currentActiveDictIds;
}

QString ArticleView::getActiveArticleId()
{
  return activeDictId;
}

void ArticleView::setActiveArticleId( QString const & dictId )
{
  this->activeDictId = dictId;
}

QString ArticleView::getCurrentArticle()
{
  const QString dictId = getActiveArticleId();
  return scrollToFromDictionaryId( dictId );
}

void ArticleView::jumpToDictionary( QString const & id, bool force )
{

  // jump only if neceessary, or when forced
  if ( const QString targetArticle = scrollToFromDictionaryId( id ); force || targetArticle != getCurrentArticle() ) {
    setCurrentArticle( targetArticle, true );
  }
}

bool ArticleView::setCurrentArticle( QString const & id, bool moveToIt )
{
  if ( !isScrollTo( id ) )
    return false; // Incorrect id

  if ( !webview->isVisible() )
    return false; // No action on background page, scrollIntoView there don't work

  if ( moveToIt ) {
    QString dictId = id.mid( 7 );
    if ( dictId.isEmpty() )
      return false;
    QString script =
      QString(
        "var elem=document.getElementById('%1'); "
        "if(elem!=undefined){elem.scrollIntoView(true);} if(typeof gdMakeArticleActive !='undefined') gdMakeArticleActive('%2',true);" )
        .arg( id, dictId );
    onJsActiveArticleChanged( id );
    webview->page()->runJavaScript( script );
    setActiveArticleId( dictId );
  }
  return true;
}

void ArticleView::selectCurrentArticle()
{
  webview->page()->runJavaScript(
    QString(
      "gdSelectArticle( '%1' );var elem=document.getElementById('%2'); if(elem!=undefined){elem.scrollIntoView(true);}" )
      .arg( getActiveArticleId(), getCurrentArticle() ) );
}

void ArticleView::isFramedArticle( QString const & ca, const std::function< void( bool ) > & callback )
{
  if ( ca.isEmpty() )
    callback( false );

  webview->page()->runJavaScript( QString( "!!document.getElementById('gdexpandframe-%1');" ).arg( ca.mid( 7 ) ),
                                  [ callback ]( const QVariant & res ) {
                                    callback( res.toBool() );
                                  } );
}

void ArticleView::tryMangleWebsiteClickedUrl( QUrl & url, Contexts & contexts )
{
  // Don't try mangling audio urls, even if they are from the framed websites

  if ( !url.isValid() )
    return;
  if ( ( url.scheme() == "http" || url.scheme() == "https" ) && !Utils::Url::isWebAudioUrl( url ) ) {
    // Maybe a link inside a website was clicked?

    QString ca = getCurrentArticle();
    isFramedArticle( ca, []( bool framed ) {} );
  }
}

void ArticleView::load( QUrl const & url )
{
  webview->load( url );
}

void ArticleView::cleanupTemp()
{
  auto it = desktopOpenedTempFiles.begin();
  while ( it != desktopOpenedTempFiles.end() ) {
    if ( QFile::remove( *it ) )
      it = desktopOpenedTempFiles.erase( it );
    else
      ++it;
  }
}

bool ArticleView::handleF3( QObject * /*obj*/, QEvent * ev )
{
  if ( ev->type() == QEvent::ShortcutOverride || ev->type() == QEvent::KeyPress ) {
    QKeyEvent * ke = static_cast< QKeyEvent * >( ev );
    if ( ke->key() == Qt::Key_F3 && isSearchOpened() ) {
      if ( !ke->modifiers() ) {
        if ( ev->type() == QEvent::KeyPress )
          on_searchNext_clicked();

        ev->accept();
        return true;
      }

      if ( ke->modifiers() == Qt::ShiftModifier ) {
        if ( ev->type() == QEvent::KeyPress )
          on_searchPrevious_clicked();

        ev->accept();
        return true;
      }
    }
    if ( ke->key() == Qt::Key_F3 && ftsSearchIsOpened ) {
      if ( !ke->modifiers() ) {
        if ( ev->type() == QEvent::KeyPress )
          on_ftsSearchNext_clicked();

        ev->accept();
        return true;
      }

      if ( ke->modifiers() == Qt::ShiftModifier ) {
        if ( ev->type() == QEvent::KeyPress )
          on_ftsSearchPrevious_clicked();

        ev->accept();
        return true;
      }
    }
  }

  return false;
}

bool ArticleView::eventFilter( QObject * obj, QEvent * ev )
{
#ifdef Q_OS_MAC

  if ( ev->type() == QEvent::NativeGesture ) {
    qDebug() << "it's a Native Gesture!";
    // handle Qt::ZoomNativeGesture Qt::SmartZoomNativeGesture here
    // ignore swipe left/right.
    // QWebEngine can handle Qt::SmartZoomNativeGesture.
  }

#else
  if ( ev->type() == QEvent::Gesture ) {
    Gestures::GestureResult result;
    QPoint pt;

    bool handled = Gestures::handleGestureEvent( obj, ev, result, pt );

    if ( handled ) {
      if ( result == Gestures::ZOOM_IN )
        emit zoomIn();
      else if ( result == Gestures::ZOOM_OUT )
        emit zoomOut();
      else if ( result == Gestures::SWIPE_LEFT )
        back();
      else if ( result == Gestures::SWIPE_RIGHT )
        forward();
      else if ( result == Gestures::SWIPE_UP || result == Gestures::SWIPE_DOWN ) {
        int delta        = result == Gestures::SWIPE_UP ? -120 : 120;
        QWidget * widget = static_cast< QWidget * >( obj );

        QPoint angleDelta( 0, delta );
        QPoint pixelDetal;
        QWidget * child = widget->childAt( widget->mapFromGlobal( pt ) );
        if ( child ) {
          QWheelEvent whev( child->mapFromGlobal( pt ),
                            pt,
                            pixelDetal,
                            angleDelta,
                            Qt::NoButton,
                            Qt::NoModifier,
                            Qt::NoScrollPhase,
                            false );
          qApp->sendEvent( child, &whev );
        }
        else {
          QWheelEvent whev( widget->mapFromGlobal( pt ),
                            pt,
                            pixelDetal,
                            angleDelta,
                            Qt::NoButton,
                            Qt::NoModifier,
                            Qt::NoScrollPhase,
                            false );
          qApp->sendEvent( widget, &whev );
        }
      }
    }

    return handled;
  }
#endif

  if ( ev->type() == QEvent::MouseMove ) {
    if ( Gestures::isFewTouchPointsPresented() ) {
      ev->accept();
      return true;
    }
  }

  if ( handleF3( obj, ev ) ) {
    return true;
  }

  if ( obj == webview ) {

    if ( ev->type() == QEvent::KeyPress ) {
      auto keyEvent = static_cast< QKeyEvent * >( ev );

      if ( keyEvent->modifiers() & ( Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier ) )
        return false; // A non-typing modifier is pressed

      if ( Utils::ignoreKeyEvent( keyEvent ) || keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter )
        return false; // Those key have other uses than to start typing

      QString text = keyEvent->text();

      if ( text.size() ) {
        emit typingEvent( text );
        return true;
      }
    }
    else if ( ev->type() == QEvent::Wheel ) {
      QWheelEvent * pe = static_cast< QWheelEvent * >( ev );
      if ( pe->modifiers().testFlag( Qt::ControlModifier ) ) {
        if ( pe->angleDelta().y() > 0 ) {
          zoomIn();
        }
        else {
          zoomOut();
        }
      }
    }
  }
  else
    return QWidget::eventFilter( obj, ev );

  return false;
}

QString ArticleView::getMutedForGroup( unsigned group )
{
  if ( dictionaryBarToggled && dictionaryBarToggled->isChecked() ) {
    // Dictionary bar is active -- mute the muted dictionaries
    Instances::Group const * groupInstance = groups.findGroup( group );

    // Find muted dictionaries for current group
    Config::Group const * grp = cfg.getGroup( group );
    Config::MutedDictionaries const * mutedDictionaries;
    if ( group == Instances::Group::AllGroupId )
      mutedDictionaries = popupView ? &cfg.popupMutedDictionaries : &cfg.mutedDictionaries;
    else
      mutedDictionaries = grp ? ( popupView ? &grp->popupMutedDictionaries : &grp->mutedDictionaries ) : nullptr;
    if ( !mutedDictionaries )
      return {};

    QStringList mutedDicts;

    if ( groupInstance ) {
      for ( const auto & dictionarie : groupInstance->dictionaries ) {
        QString id = QString::fromStdString( dictionarie->getId() );

        if ( mutedDictionaries->contains( id ) )
          mutedDicts.append( id );
      }
    }

    if ( !mutedDicts.empty() )
      return mutedDicts.join( "," );
  }

  return {};
}

QStringList ArticleView::getMutedDictionaries( unsigned group )
{
  if ( dictionaryBarToggled && dictionaryBarToggled->isChecked() ) {
    // Dictionary bar is active -- mute the muted dictionaries
    Instances::Group const * groupInstance = groups.findGroup( group );

    // Find muted dictionaries for current group
    Config::Group const * grp = cfg.getGroup( group );
    Config::MutedDictionaries const * mutedDictionaries;
    if ( group == Instances::Group::AllGroupId )
      mutedDictionaries = popupView ? &cfg.popupMutedDictionaries : &cfg.mutedDictionaries;
    else
      mutedDictionaries = grp ? ( popupView ? &grp->popupMutedDictionaries : &grp->mutedDictionaries ) : nullptr;
    if ( !mutedDictionaries )
      return {};

    QStringList mutedDicts;

    if ( groupInstance ) {
      for ( const auto & dictionarie : groupInstance->dictionaries ) {
        QString id = QString::fromStdString( dictionarie->getId() );

        if ( mutedDictionaries->contains( id ) )
          mutedDicts.append( id );
      }
    }

    return mutedDicts;
  }

  return {};
}

void ArticleView::linkHovered( const QString & link )
{
  QString msg;
  QUrl url( link );

  if ( url.scheme() == "bres" ) {
    msg = tr( "Resource" );
  }
  else if ( url.scheme() == "gdau" || Utils::Url::isAudioUrl( url ) ) {
    msg = tr( "Audio" );
  }
  else if ( url.scheme() == "gdtts" ) {
    msg = tr( "TTS Voice" );
  }
  else if ( url.scheme() == "gdvideo" ) {
    if ( url.path().isEmpty() ) {
      msg = tr( "Video" );
    }
    else {
      QString path = url.path();
      if ( path.startsWith( '/' ) ) {
        path = path.mid( 1 );
      }
      msg = tr( "Video: %1" ).arg( path );
    }
  }
  else if ( url.scheme() == "gdlookup" || url.scheme().compare( "bword" ) == 0 ) {
    QString def = url.path();
    if ( def.startsWith( "/" ) ) {
      def = def.mid( 1 );
    }

    if ( Utils::Url::hasQueryItem( url, "dict" ) ) {
      // Link to other dictionary
      QString dictName( Utils::Url::queryItemValue( url, "dict" ) );
      if ( !dictName.isEmpty() )
        msg = tr( "Definition from dictionary \"%1\": %2" ).arg( dictName, def );
    }

    if ( msg.isEmpty() ) {
      if ( def.isEmpty() && url.hasFragment() )
        msg = '#' + url.fragment(); // this must be a citation, footnote or backlink
      else
        msg = tr( "Definition: %1" ).arg( def );
    }
  }
  else {
    msg = link;
  }

  emit statusBarMessage( msg );
}

void ArticleView::attachWebChannelToHtml()
{
  // set the web channel to be used by the page
  // see http://doc.qt.io/qt-5/qwebenginepage.html#setWebChannel
  webview->page()->setWebChannel( channel, QWebEngineScript::MainWorld );

  // register QObjects to be exposed to JavaScript
  channel->registerObject( QStringLiteral( "articleview" ), agent );
}

void ArticleView::linkClicked( QUrl const & url_ )
{
  Qt::KeyboardModifiers kmod = QApplication::keyboardModifiers();

  // Lock jump on links while Alt key is pressed
  if ( kmod & Qt::AltModifier )
    return;

  QUrl url( url_ );
  Contexts contexts;

  tryMangleWebsiteClickedUrl( url, contexts );

  if ( !popupView && ( webview->isMidButtonPressed() || ( kmod & ( Qt::ControlModifier | Qt::ShiftModifier ) ) )
       && !isAudioLink( url ) ) {
    // Mid button or Control/Shift is currently pressed - open the link in new tab
    webview->resetMidButtonPressed();
    emit openLinkInNewTab( url, webview->url(), getCurrentArticle(), contexts );
  }
  else
    openLink( url, webview->url(), getCurrentArticle(), contexts );
}

void ArticleView::linkClickedInHtml( QUrl const & url_ )
{
  webview->linkClickedInHtml( url_ );
  if ( !url_.isEmpty() ) {
    linkClicked( url_ );
  }
}

void ArticleView::makeAnkiCardFromArticle( QString const & article_id )
{
  auto const js_code = QString( R"EOF(document.getElementById("gdarticlefrom-%1").innerText)EOF" ).arg( article_id );
  webview->page()->runJavaScript( js_code, [ this ]( const QVariant & article_text ) {
    sendToAnki( webview->title(), article_text.toString(), translateLine->text() );
  } );
}

void ArticleView::openLink( QUrl const & url, QUrl const & ref, QString const & scrollTo, Contexts const & contexts_ )
{
  audioPlayer->stop();
  qDebug() << "open link url:" << url;

  auto [ valid, word ] = Utils::Url::getQueryWord( url );
  if ( valid && word.isEmpty() ) {
    //if valid=true and word is empty,the url must be a invalid gdlookup url.
    //else if valid=false,the url should be external urls.
    return;
  }

  Contexts contexts( contexts_ );

  if ( url.scheme().compare( "ankisearch" ) == 0 ) {
    ankiConnector->ankiSearch( url.path() );
    return;
  }
  else if ( url.scheme().compare( "ankicard" ) == 0 ) {
    // If article id is set in path and selection is empty, use text from the current article.
    // Otherwise, grab currently selected text and use it as the definition.
    if ( !url.path().isEmpty() && webview->selectedText().isEmpty() ) {
      makeAnkiCardFromArticle( url.path() );
    }
    else {
      sendToAnki( webview->title(), webview->selectedText(), translateLine->text() );
    }
    qDebug() << "requested to make Anki card.";
    return;
  }
  else if ( url.scheme().compare( "bword" ) == 0 || url.scheme().compare( "entry" ) == 0 ) {
    if ( Utils::Url::hasQueryItem( ref, "dictionaries" ) ) {
      QStringList dictsList = Utils::Url::queryItemValue( ref, "dictionaries" ).split( ",", Qt::SkipEmptyParts );

      showDefinition( word, dictsList, QRegExp(), getGroup( ref ), false );
    }
    else
      showDefinition( word, getGroup( ref ), scrollTo, contexts );
  }
  else if ( url.scheme() == "gdlookup" ) // Plain html links inherit gdlookup scheme
  {
    if ( url.hasFragment() ) {
      webview->page()->runJavaScript(
        QString( "window.location = \"%1\"" ).arg( QString::fromUtf8( url.toEncoded() ) ) );
    }
    else {
      if ( Utils::Url::hasQueryItem( ref, "dictionaries" ) ) {
        // Specific dictionary group from full-text search
        QStringList dictsList = Utils::Url::queryItemValue( ref, "dictionaries" ).split( ",", Qt::SkipEmptyParts );

        showDefinition( url.path().mid( 1 ), dictsList, QRegExp(), getGroup( ref ), false );
        return;
      }

      if ( Utils::Url::hasQueryItem( url, "dictionaries" ) ) {
        // Specific dictionary group from full-text search
        QStringList dictsList = Utils::Url::queryItemValue( url, "dictionaries" ).split( ",", Qt::SkipEmptyParts );

        showDefinition( word, dictsList, QRegExp(), getGroup( url ), false );
        return;
      }

      QString newScrollTo( scrollTo );
      if ( Utils::Url::hasQueryItem( url, "dict" ) ) {
        // Link to other dictionary
        QString dictName( Utils::Url::queryItemValue( url, "dict" ) );
        for ( const auto & allDictionarie : allDictionaries ) {
          if ( dictName.compare( QString::fromUtf8( allDictionarie->getName().c_str() ) ) == 0 ) {
            newScrollTo = scrollToFromDictionaryId( QString::fromUtf8( allDictionarie->getId().c_str() ) );
            break;
          }
        }
      }

      if ( Utils::Url::hasQueryItem( url, "gdanchor" ) )
        contexts[ "gdanchor" ] = Utils::Url::queryItemValue( url, "gdanchor" );

      showDefinition( word, getGroup( ref ), newScrollTo, contexts );
    }
  }
  else if ( url.scheme() == "bres" || url.scheme() == "gdau" || url.scheme() == "gdvideo"
            || Utils::Url::isAudioUrl( url ) ) {
    // Download it

    // Clear any pending ones

    resourceDownloadRequests.clear();

    resourceDownloadUrl = url;

    if ( Utils::Url::isWebAudioUrl( url ) ) {
      sptr< Dictionary::DataRequest > req = std::make_shared< Dictionary::WebMultimediaDownload >( url, articleNetMgr );

      resourceDownloadRequests.push_back( req );

      connect( req.get(), &Dictionary::Request::finished, this, &ArticleView::resourceDownloadFinished );
    }
    else if ( url.scheme() == "gdau" && url.host() == "search" ) {
      // Since searches should be limited to current group, we just do them
      // here ourselves since otherwise we'd need to pass group id to netmgr
      // and it should've been having knowledge of the current groups, too.

      unsigned currentGroup = getGroup( ref );

      std::vector< sptr< Dictionary::Class > > const * activeDicts = nullptr;

      if ( !groups.empty() ) {
        for ( const auto & group : groups )
          if ( group.id == currentGroup ) {
            activeDicts = &( group.dictionaries );
            break;
          }
      }
      else
        activeDicts = &allDictionaries;

      if ( activeDicts ) {
        unsigned preferred = UINT_MAX;
        if ( url.hasFragment() ) {
          // Find sound in the preferred dictionary
          QString preferredName = Utils::Url::fragment( url );
          try {
            for ( unsigned x = 0; x < activeDicts->size(); ++x ) {
              if ( preferredName.compare( QString::fromUtf8( ( *activeDicts )[ x ]->getName().c_str() ) ) == 0 ) {
                preferred = x;
                sptr< Dictionary::DataRequest > req =
                  ( *activeDicts )[ x ]->getResource( url.path().mid( 1 ).toUtf8().data() );

                resourceDownloadRequests.push_back( req );

                if ( !req->isFinished() ) {
                  // Queued loading
                  connect( req.get(), &Dictionary::Request::finished, this, &ArticleView::resourceDownloadFinished );
                }
                else {
                  // Immediate loading
                  if ( req->dataSize() > 0 ) {
                    // Resource already found, stop next search
                    resourceDownloadFinished();
                    return;
                  }
                }
                break;
              }
            }
          }
          catch ( std::exception & e ) {
            emit statusBarMessage( tr( "ERROR: %1" ).arg( e.what() ), 10000, QPixmap( ":/icons/error.svg" ) );
          }
        }
        for ( unsigned x = 0; x < activeDicts->size(); ++x ) {
          try {
            if ( x == preferred )
              continue;

            sptr< Dictionary::DataRequest > req =
              ( *activeDicts )[ x ]->getResource( url.path().mid( 1 ).toUtf8().data() );

            resourceDownloadRequests.push_back( req );

            if ( !req->isFinished() ) {
              // Queued loading
              connect( req.get(), &Dictionary::Request::finished, this, &ArticleView::resourceDownloadFinished );
            }
            else {
              // Immediate loading
              if ( req->dataSize() > 0 ) {
                // Resource already found, stop next search
                break;
              }
            }
          }
          catch ( std::exception & e ) {
            emit statusBarMessage( tr( "ERROR: %1" ).arg( e.what() ), 10000, QPixmap( ":/icons/error.svg" ) );
          }
        }
      }
    }
    else {
      // Normal resource download
      QString contentType;

      sptr< Dictionary::DataRequest > req = articleNetMgr.getResource( url, contentType );

      if ( !req.get() ) {
        qDebug() << "request failed: " << url;
        // Request failed, fail
      }
      else if ( req->isFinished() && req->dataSize() >= 0 ) {
        // Have data ready, handle it
        resourceDownloadRequests.push_back( req );
        resourceDownloadFinished();

        return;
      }
      else if ( !req->isFinished() ) {
        // Queue to be handled when done

        resourceDownloadRequests.push_back( req );

        connect( req.get(), &Dictionary::Request::finished, this, &ArticleView::resourceDownloadFinished );
      }
    }

    if ( resourceDownloadRequests.empty() ) // No requests were queued
    {
      qDebug() << tr( "The referenced resource doesn't exist." );
      return;
    }
    else
      resourceDownloadFinished(); // Check any requests finished already
  }
  else if ( url.scheme() == "gdprg" ) {
    // Program. Run it.
    QString id( url.host() );

    for ( const auto & program : cfg.programs ) {
      if ( program.id == id ) {
        // Found the corresponding program.
        Programs::RunInstance * req = new Programs::RunInstance;

        connect( req, &Programs::RunInstance::finished, req, &QObject::deleteLater );

        QString error;

        // Delete the request if it fails to start
        if ( !req->start( program, url.path().mid( 1 ), error ) ) {
          delete req;

          QMessageBox::critical( this, "GoldenDict", error );
        }

        return;
      }
    }

    // Still here? No such program exists.
    QMessageBox::critical( this, "GoldenDict", tr( "The referenced audio program doesn't exist." ) );
  }
  else if ( url.scheme() == "gdtts" ) {
    // Text to speech
    QString md5Id = Utils::Url::queryItemValue( url, "engine" );
    QString text( url.path().mid( 1 ) );

    for ( const auto & voiceEngine : cfg.voiceEngines ) {
      QString itemMd5Id =
        QString( QCryptographicHash::hash( voiceEngine.name.toUtf8(), QCryptographicHash::Md5 ).toHex() );

      if ( itemMd5Id == md5Id ) {
        SpeechClient * speechClient = new SpeechClient( voiceEngine, this );
        connect( speechClient, SIGNAL( finished() ), speechClient, SLOT( deleteLater() ) );
        speechClient->tell( text );
        break;
      }
    }
  }
  else if ( Utils::isExternalLink( url ) ) {
    // Use the system handler for the conventional external links
    QDesktopServices::openUrl( url );
  }
}

ResourceToSaveHandler * ArticleView::saveResource( const QUrl & url, const QString & fileName )
{
  return saveResource( url, webview->url(), fileName );
}

ResourceToSaveHandler * ArticleView::saveResource( const QUrl & url, const QUrl & ref, const QString & fileName )
{
  ResourceToSaveHandler * handler = new ResourceToSaveHandler( this, fileName );
  sptr< Dictionary::DataRequest > req;

  if ( url.scheme() == "bres" || url.scheme() == "gico" || url.scheme() == "gdau" || url.scheme() == "gdvideo" ) {
    if ( url.host() == "search" ) {
      // Since searches should be limited to current group, we just do them
      // here ourselves since otherwise we'd need to pass group id to netmgr
      // and it should've been having knowledge of the current groups, too.

      unsigned currentGroup = getGroup( ref );

      std::vector< sptr< Dictionary::Class > > const * activeDicts = nullptr;

      if ( groups.size() ) {
        for ( const auto & group : groups )
          if ( group.id == currentGroup ) {
            activeDicts = &( group.dictionaries );
            break;
          }
      }
      else
        activeDicts = &allDictionaries;

      if ( activeDicts ) {
        unsigned preferred = UINT_MAX;
        if ( url.hasFragment() && url.scheme() == "gdau" ) {
          // Find sound in the preferred dictionary
          QString preferredName = Utils::Url::fragment( url );
          for ( unsigned x = 0; x < activeDicts->size(); ++x ) {
            try {
              if ( preferredName.compare( QString::fromUtf8( ( *activeDicts )[ x ]->getName().c_str() ) ) == 0 ) {
                preferred = x;
                sptr< Dictionary::DataRequest > data_request =
                  ( *activeDicts )[ x ]->getResource( url.path().mid( 1 ).toUtf8().data() );

                handler->addRequest( data_request );

                if ( data_request->isFinished() && data_request->dataSize() > 0 ) {
                  handler->downloadFinished();
                  return handler;
                }
                break;
              }
            }
            catch ( std::exception & e ) {
              gdWarning( "getResource request error (%s) in \"%s\"\n",
                         e.what(),
                         ( *activeDicts )[ x ]->getName().c_str() );
            }
          }
        }
        for ( unsigned x = 0; x < activeDicts->size(); ++x ) {
          try {
            if ( x == preferred )
              continue;

            req = ( *activeDicts )[ x ]->getResource( Utils::Url::path( url ).mid( 1 ).toUtf8().data() );

            handler->addRequest( req );

            if ( req->isFinished() && req->dataSize() > 0 ) {
              // Resource already found, stop next search
              break;
            }
          }
          catch ( std::exception & e ) {
            gdWarning( "getResource request error (%s) in \"%s\"\n",
                       e.what(),
                       ( *activeDicts )[ x ]->getName().c_str() );
          }
        }
      }
    }
    else {
      // Normal resource download
      QString contentType;
      req = articleNetMgr.getResource( url, contentType );

      if ( req.get() ) {
        handler->addRequest( req );
      }
    }
  }
  else {
    req = std::make_shared< Dictionary::WebMultimediaDownload >( url, articleNetMgr );

    handler->addRequest( req );
  }

  if ( handler->isEmpty() ) // No requests were queued
  {
    emit statusBarMessage( tr( "ERROR: %1" ).arg( tr( "The referenced resource doesn't exist." ) ),
                           10000,
                           QPixmap( ":/icons/error.svg" ) );
  }

  // Check already finished downloads
  handler->downloadFinished();

  return handler;
}

void ArticleView::updateMutedContents()
{
  QUrl currentUrl = webview->url();

  if ( currentUrl.scheme() != "gdlookup" )
    return; // Weird url -- do nothing

  unsigned group = getGroup( currentUrl );

  if ( !group )
    return; // No group in url -- do nothing

  QString mutedDicts = getMutedForGroup( group );

  if ( Utils::Url::queryItemValue( currentUrl, "muted" ) != mutedDicts ) {
    // The list has changed -- update the url

    Utils::Url::removeQueryItem( currentUrl, "muted" );

    if ( mutedDicts.size() )
      Utils::Url::addQueryItem( currentUrl, "muted", mutedDicts );

    load( currentUrl );

    //QApplication::setOverrideCursor( Qt::WaitCursor );
    webview->setCursor( Qt::WaitCursor );
  }
}

bool ArticleView::canGoBack()
{
  return webview->history()->canGoBack();
}

bool ArticleView::canGoForward()
{
  return webview->history()->canGoForward();
}

void ArticleView::setSelectionBySingleClick( bool set )
{
  webview->setSelectionBySingleClick( set );
}

void ArticleView::setDelayedHighlightText( QString const & text )
{
  delayedHighlightText = text;
}

void ArticleView::back()
{
  // Don't allow navigating back to page 0, which is usually the initial
  // empty page
  if ( canGoBack() ) {
    currentActiveDictIds.clear();
    historyMode = true;
    webview->back();
  }
}

void ArticleView::forward()
{
  currentActiveDictIds.clear();
  historyMode = true;
  webview->forward();
}

void ArticleView::handleAnkiAction()
{
  // React to the "send *word* to anki" action.
  // If selected text is empty, use the whole article as the definition.
  if ( webview->selectedText().isEmpty() ) {
    makeAnkiCardFromArticle( getActiveArticleId() );
  }
  else {
    sendToAnki( webview->title(), webview->selectedText(), translateLine->text() );
  }
}

void ArticleView::reload()
{
  webview->reload();
}

void ArticleView::hasSound( const std::function< void( bool ) > & callback )
{
  webview->page()->runJavaScript( R"(if(typeof(gdAudioLinks)!="undefined") gdAudioLinks.first)",
                                  [ callback ]( const QVariant & v ) {
                                    bool has = false;
                                    if ( v.type() == QVariant::String )
                                      has = !v.toString().isEmpty();
                                    callback( has );
                                  } );
}

//use webengine javascript to playsound
void ArticleView::playSound()
{
  QString variable = R"( (function(){  var link=gdAudioMap.get(gdAudioLinks.current);           
       if(link==undefined){           
           link=gdAudioLinks.first;           
       }          
        return link;})();         )";

  webview->page()->runJavaScript( variable, [ this ]( const QVariant & result ) {
    if ( result.type() == QVariant::String ) {
      QString soundScript = result.toString();
      if ( !soundScript.isEmpty() )
        openLink( QUrl::fromEncoded( soundScript.toUtf8() ), webview->url() );
    }
  } );
}

void ArticleView::stopSound()
{
  audioPlayer->stop();
}

void ArticleView::toHtml( const std::function< void( QString & ) > & callback )
{
  webview->page()->toHtml( [ = ]( const QString & content ) {
    QString html = content;
    callback( html );
  } );
}

void ArticleView::setHtml( const QString & content, const QUrl & baseUrl )
{
  webview->page()->setHtml( content, baseUrl );
}

void ArticleView::setContent( const QByteArray & data, const QString & mimeType, const QUrl & baseUrl )
{
  webview->page()->setContent( data, mimeType, baseUrl );
}

QString ArticleView::getTitle()
{
  return webview->page()->title();
}

QString ArticleView::getWord() const
{
  return currentWord;
}

void ArticleView::print( QPrinter * printer ) const
{
  QEventLoop loop;
  bool result;
  auto printPreview = [ & ]( bool success ) {
    result = success;
    loop.quit();
  };
#if ( QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 ) )
  webview->page()->print( printer, std::move( printPreview ) );
#else
  connect( webview, &QWebEngineView::printFinished, &loop, std::move( printPreview ) );
  webview->print( printer );
#endif
  loop.exec();
  if ( !result ) {
    qDebug() << "print failed";
  }
}

void ArticleView::contextMenuRequested( QPoint const & pos )
{
  // Is that a link? Is there a selection?
  QWebEnginePage * r = webview->page();
  QMenu menu( this );

  QAction * followLink                = nullptr;
  QAction * followLinkExternal        = nullptr;
  QAction * followLinkNewTab          = nullptr;
  QAction * lookupSelection           = nullptr;
  QAction * lookupSelectionGr         = nullptr;
  QAction * lookupSelectionNewTab     = nullptr;
  QAction * lookupSelectionNewTabGr   = nullptr;
  QAction * maxDictionaryRefsAction   = nullptr;
  QAction * addWordToHistoryAction    = nullptr;
  QAction * addHeaderToHistoryAction  = nullptr;
  QAction * sendWordToInputLineAction = nullptr;
  QAction * saveImageAction           = nullptr;
  QAction * openImageAction           = nullptr;
  QAction * saveSoundAction           = nullptr;
  QAction * saveBookmark              = nullptr;

#if ( QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 ) )
  const QWebEngineContextMenuData * menuData = &( r->contextMenuData() );
#else
  QWebEngineContextMenuRequest * menuData = webview->lastContextMenuRequest();
#endif
  QUrl targetUrl( menuData->linkUrl() );
  Contexts contexts;

  tryMangleWebsiteClickedUrl( targetUrl, contexts );

  if ( !targetUrl.isEmpty() ) {
    if ( !Utils::isExternalLink( targetUrl ) ) {
      followLink = new QAction( tr( "Op&en Link" ), &menu );
      menu.addAction( followLink );

      if ( !popupView && !isAudioLink( targetUrl ) ) {
        followLinkNewTab = new QAction( QIcon( ":/icons/addtab.svg" ), tr( "Open Link in New &Tab" ), &menu );
        menu.addAction( followLinkNewTab );
      }
    }

    if ( Utils::isExternalLink( targetUrl ) ) {
      followLinkExternal = new QAction( tr( "Open Link in &External Browser" ), &menu );
      menu.addAction( followLinkExternal );
      menu.addAction( webview->pageAction( QWebEnginePage::CopyLinkToClipboard ) );
    }
  }

  QUrl imageUrl;
#if ( QT_VERSION < QT_VERSION_CHECK( 6, 0, 0 ) )
  if ( !popupView && menuData->mediaType() == QWebEngineContextMenuData::MediaTypeImage )
#else
  if ( !popupView && menuData->mediaType() == QWebEngineContextMenuRequest::MediaType::MediaTypeImage )
#endif
  {
    imageUrl = menuData->mediaUrl();
    if ( !imageUrl.isEmpty() ) {
      menu.addAction( webview->pageAction( QWebEnginePage::CopyImageToClipboard ) );
      saveImageAction = new QAction( tr( "Save &image..." ), &menu );
      menu.addAction( saveImageAction );

      openImageAction = new QAction( tr( "Open image in system viewer..." ), &menu );
      menu.addAction( openImageAction );
    }
  }

  if ( !popupView && isAudioLink( targetUrl ) ) {
    saveSoundAction = new QAction( tr( "Save s&ound..." ), &menu );
    menu.addAction( saveSoundAction );
  }

  QString const selectedText = webview->selectedText();
  QString text               = Utils::trimNonChar( selectedText );

  if ( text.size() && text.size() < 60 ) {
    // We don't prompt for selections larger or equal to 60 chars, since
    // it ruins the menu and it's hardly a single word anyway.

    lookupSelection = new QAction( tr( "&Look up \"%1\"" ).arg( text ), &menu );
    menu.addAction( lookupSelection );

    if ( !popupView ) {
      lookupSelectionNewTab =
        new QAction( QIcon( ":/icons/addtab.svg" ), tr( "Look up \"%1\" in &New Tab" ).arg( text ), &menu );
      menu.addAction( lookupSelectionNewTab );

      sendWordToInputLineAction = new QAction( tr( "Send \"%1\" to input line" ).arg( text ), &menu );
      menu.addAction( sendWordToInputLineAction );
    }

    addWordToHistoryAction = new QAction( tr( "&Add \"%1\" to history" ).arg( text ), &menu );
    menu.addAction( addWordToHistoryAction );

    Instances::Group const * altGroup =
      ( currentGroupId != getGroup( webview->url() ) ) ? groups.findGroup( currentGroupId ) : nullptr;

    if ( altGroup ) {
      QIcon icon = altGroup->icon.size() ? QIcon( ":/flags/" + altGroup->icon ) : QIcon();

      lookupSelectionGr = new QAction( icon, tr( "Look up \"%1\" in %2" ).arg( text ).arg( altGroup->name ), &menu );
      menu.addAction( lookupSelectionGr );

      if ( !popupView ) {
        lookupSelectionNewTabGr =
          new QAction( QIcon( ":/icons/addtab.svg" ),
                       tr( "Look up \"%1\" in %2 in &New Tab" ).arg( text ).arg( altGroup->name ),
                       &menu );
        menu.addAction( lookupSelectionNewTabGr );
      }
    }
  }

  if ( text.size() ) {
    // avoid too long in the menu ,use left 30 characters.
    saveBookmark = new QAction( tr( "Save &Bookmark \"%1...\"" ).arg( text.left( 30 ) ), &menu );
    menu.addAction( saveBookmark );
  }

  // Add anki menu (if enabled)
  // If there is no selected text, it will extract text from the current article.
  if ( cfg.preferences.ankiConnectServer.enabled ) {
    menu.addAction( &sendToAnkiAction );
    sendToAnkiAction.setText( webview->selectedText().isEmpty() ? tr( "&Send Current Article to Anki" ) :
                                                                  tr( "&Send selected text to Anki" ) );
  }

  if ( text.isEmpty() && !cfg.preferences.storeHistory ) {
    QString txt = webview->title();
    if ( txt.size() > 60 )
      txt = txt.left( 60 ) + "...";

    addHeaderToHistoryAction = new QAction( tr( "&Add \"%1\" to history" ).arg( txt ), &menu );
    menu.addAction( addHeaderToHistoryAction );
  }

  if ( selectedText.size() ) {
    menu.addAction( webview->pageAction( QWebEnginePage::Copy ) );
    menu.addAction( &copyAsTextAction );
  }
  else {
    menu.addAction( &selectCurrentArticleAction );
    menu.addAction( webview->pageAction( QWebEnginePage::SelectAll ) );
  }

  map< QAction *, QString > tableOfContents;

  // Add table of contents
  QStringList ids = getArticlesList();

  if ( !menu.isEmpty() && ids.size() )
    menu.addSeparator();

  unsigned refsAdded            = 0;
  bool maxDictionaryRefsReached = false;

  for ( QStringList::const_iterator i = ids.constBegin(); i != ids.constEnd(); ++i, ++refsAdded ) {
    // Find this dictionary

    for ( unsigned x = allDictionaries.size(); x--; ) {
      if ( allDictionaries[ x ]->getId() == i->toUtf8().data() ) {
        QAction * action = nullptr;
        if ( refsAdded == cfg.preferences.maxDictionaryRefsInContextMenu ) {
          // Enough! Or the menu would become too large.
          maxDictionaryRefsAction  = new QAction( ".........", &menu );
          action                   = maxDictionaryRefsAction;
          maxDictionaryRefsReached = true;
        }
        else {
          action = new QAction( allDictionaries[ x ]->getIcon(),
                                QString::fromUtf8( allDictionaries[ x ]->getName().c_str() ),
                                &menu );
          // Force icons in menu on all platforms,
          // since without them it will be much harder
          // to find things.
          action->setIconVisibleInMenu( true );
        }
        menu.addAction( action );

        tableOfContents[ action ] = *i;

        break;
      }
    }
    if ( maxDictionaryRefsReached )
      break;
  }

  menu.addSeparator();
  if ( !popupView || cfg.pinPopupWindow )
    menu.addAction( &inspectAction );

  if ( !menu.isEmpty() ) {
    connect( this, &ArticleView::closePopupMenu, &menu, &QWidget::close );
    QAction * result = menu.exec( webview->mapToGlobal( pos ) );

    if ( !result )
      return;

    if ( result == followLink )
      openLink( targetUrl, webview->url(), getCurrentArticle(), contexts );
    else if ( result == followLinkExternal )
      QDesktopServices::openUrl( targetUrl );
    else if ( result == lookupSelection )
      showDefinition( text, getGroup( webview->url() ), getCurrentArticle() );
    else if ( result == saveBookmark ) {
      emit saveBookmarkSignal( text.left( 60 ) );
    }
    else if ( result == &sendToAnkiAction ) {
      // This action is handled by a slot.
      return;
    }
    else if ( result == lookupSelectionGr && currentGroupId )
      showDefinition( selectedText, currentGroupId, QString() );
    else if ( result == addWordToHistoryAction )
      emit forceAddWordToHistory( selectedText );
    if ( result == addHeaderToHistoryAction )
      emit forceAddWordToHistory( webview->title() );
    else if ( result == sendWordToInputLineAction )
      emit sendWordToInputLine( selectedText );
    else if ( !popupView && result == followLinkNewTab )
      emit openLinkInNewTab( targetUrl, webview->url(), getCurrentArticle(), contexts );
    else if ( !popupView && result == lookupSelectionNewTab )
      emit showDefinitionInNewTab( selectedText, getGroup( webview->url() ), getCurrentArticle(), Contexts() );
    else if ( !popupView && result == lookupSelectionNewTabGr && currentGroupId )
      emit showDefinitionInNewTab( selectedText, currentGroupId, QString(), Contexts() );
    else if ( result == saveImageAction || result == saveSoundAction ) {
      QUrl url = ( result == saveImageAction ) ? imageUrl : targetUrl;
      QString savePath;
      QString fileName;

      if ( cfg.resourceSavePath.isEmpty() )
        savePath = QDir::homePath();
      else {
        savePath = QDir::fromNativeSeparators( cfg.resourceSavePath );
        if ( !QDir( savePath ).exists() )
          savePath = QDir::homePath();
      }

      QString name = Utils::Url::path( url ).section( '/', -1 );

      if ( result == saveSoundAction ) {
        // Audio data
        //        if ( name.indexOf( '.' ) < 0 )
        //          name += ".wav";

        fileName = savePath + "/" + name;
        fileName = QFileDialog::getSaveFileName(
          parentWidget(),
          tr( "Save sound" ),
          fileName,
          tr(
            "Sound files (*.wav *.opus *.ogg *.oga *.mp3 *.mp4 *.aac *.flac *.mid *.wv *.ape *.spx);;All files (*.*)" ) );
      }
      else {
        // Image data

        // Check for babylon image name
        if ( name[ 0 ] == '\x1E' )
          name.remove( 0, 1 );
        if ( name.length() && name[ name.length() - 1 ] == '\x1F' )
          name.chop( 1 );

        fileName = savePath + "/" + name;
        fileName = QFileDialog::getSaveFileName( parentWidget(),
                                                 tr( "Save image" ),
                                                 fileName,
                                                 tr( "Image files (*.bmp *.jpg *.png *.tif);;All files (*.*)" ) );
      }

      if ( !fileName.isEmpty() ) {
        QFileInfo fileInfo( fileName );
        emit storeResourceSavePath( QDir::toNativeSeparators( fileInfo.absoluteDir().absolutePath() ) );
        saveResource( url, webview->url(), fileName );
      }
    }
    else if ( result == openImageAction ) {
      QUrl url = imageUrl;
      QString fileName;


      QString name = Utils::Url::path( url ).section( '/', -1 );
      // Image data

      // Check for babylon image name
      if ( name[ 0 ] == '\x1E' )
        name.remove( 0, 1 );
      if ( name.length() && name[ name.length() - 1 ] == '\x1F' )
        name.chop( 1 );

      fileName = QDir::temp().filePath( QString::number( QRandomGenerator::global()->generate() ) + name );

      if ( !fileName.isEmpty() ) {
        QFileInfo fileInfo( fileName );
        auto handler = saveResource( url, webview->url(), fileName );

        if ( !handler->isEmpty() ) {
          connect( handler, &ResourceToSaveHandler::done, this, [ fileName ]() {
            QDesktopServices::openUrl( fileName );
          } );
        }
        else {
          QDesktopServices::openUrl( fileName );
        }
      }
    }
    else {
      if ( !popupView && result == maxDictionaryRefsAction )
        emit showDictsPane();

      // Match against table of contents
      QString id = tableOfContents[ result ];

      if ( id.size() )
        setCurrentArticle( scrollToFromDictionaryId( id ), true );
    }
  }

  qDebug() << "title = " << r->title();
}

void ArticleView::resourceDownloadFinished()
{
  if ( resourceDownloadRequests.empty() )
    return; // Stray signal

  // Find any finished resources
  for ( list< sptr< Dictionary::DataRequest > >::iterator i = resourceDownloadRequests.begin();
        i != resourceDownloadRequests.end(); ) {
    if ( ( *i )->isFinished() ) {
      if ( ( *i )->dataSize() >= 0 ) {
        // Ok, got one finished, all others are irrelevant now

        vector< char > const & data = ( *i )->getFullData();

        if ( resourceDownloadUrl.scheme() == "gdau" || Utils::Url::isWebAudioUrl( resourceDownloadUrl ) ) {
          // Audio data
          audioPlayer->stop();
          connect( audioPlayer.data(),
                   &AudioPlayerInterface::error,
                   this,
                   &ArticleView::audioPlayerError,
                   Qt::UniqueConnection );
          QString errorMessage = audioPlayer->play( data.data(), data.size() );
          if ( !errorMessage.isEmpty() )
            QMessageBox::critical( this, "GoldenDict", tr( "Failed to play sound file: %1" ).arg( errorMessage ) );
        }
        else {
          // Create a temporary file
          // Remove the ones previously used, if any
          cleanupTemp();
          QString fileName;

          {
            QTemporaryFile tmp( QDir::temp().filePath( "XXXXXX-" + resourceDownloadUrl.path().section( '/', -1 ) ),
                                this );

            if ( !tmp.open() || (size_t)tmp.write( &data.front(), data.size() ) != data.size() ) {
              QMessageBox::critical( this, "GoldenDict", tr( "Failed to create temporary file." ) );
              return;
            }

            tmp.setAutoRemove( false );

            desktopOpenedTempFiles.insert( fileName = tmp.fileName() );
          }

          if ( !QDesktopServices::openUrl( QUrl::fromLocalFile( fileName ) ) )
            QMessageBox::critical(
              this,
              "GoldenDict",
              tr( "Failed to auto-open resource file, try opening manually: %1." ).arg( fileName ) );
        }

        // Ok, whatever it was, it's finished. Remove this and any other
        // requests and finish.

        resourceDownloadRequests.clear();

        return;
      }
      else {
        // This one had no data. Erase it.
        resourceDownloadRequests.erase( i++ );
      }
    }
    else // Unfinished, wait.
      break;
  }

  if ( resourceDownloadRequests.empty() ) {
    // emit statusBarMessage(
    //     tr("WARNING: %1").arg(tr("The referenced resource failed to download.")),
    //     10000, QPixmap(":/icons/error.svg"));
  }
}

void ArticleView::audioPlayerError( QString const & message )
{
  emit statusBarMessage( tr( "WARNING: Audio Player: %1" ).arg( message ), 10000, QPixmap( ":/icons/error.svg" ) );
}

void ArticleView::pasteTriggered()
{
  QString word = cfg.preferences.sanitizeInputPhrase( QApplication::clipboard()->text() );

  if ( !word.isEmpty() ) {
    unsigned groupId = getGroup( webview->url() );
    if ( groupId == 0 ) {
      // We couldn't figure out the group out of the URL,
      // so let's try the currently selected group.
      groupId = currentGroupId;
    }
    showDefinition( word, groupId, getCurrentArticle() );
  }
}

unsigned ArticleView::getCurrentGroup()
{
  if ( !groupComboBox )
    return currentGroupId;
  return groupComboBox->getCurrentGroup();
}

void ArticleView::moveOneArticleUp()
{
  QString current = getCurrentArticle();

  if ( current.size() ) {
    QStringList lst = getArticlesList();

    int idx = lst.indexOf( dictionaryIdFromScrollTo( current ) );

    if ( idx != -1 ) {
      --idx;

      if ( idx < 0 )
        idx = lst.size() - 1;

      setCurrentArticle( scrollToFromDictionaryId( lst[ idx ] ), true );
    }
  }
}

void ArticleView::moveOneArticleDown()
{
  QString currentDictId = getActiveArticleId();
  QStringList lst       = getArticlesList();
  // if current article is empty .use the first as default.
  if ( currentDictId.isEmpty() && !lst.isEmpty() ) {
    currentDictId = lst[ 0 ];
  }

  int idx = lst.indexOf( currentDictId );

  if ( idx != -1 ) {
    idx = ( idx + 1 ) % lst.size();

    setCurrentArticle( scrollToFromDictionaryId( lst[ idx ] ), true );
  }
}

void ArticleView::openSearch()
{
  if ( !isVisible() )
    return;

  if ( ftsSearchIsOpened )
    closeSearch();

  if ( !searchIsOpened ) {
    searchPanel->show();
    searchPanel->lineEdit->setText( getTitle() );
    searchIsOpened = true;
  }

  searchPanel->lineEdit->setFocus();
  searchPanel->lineEdit->selectAll();

  // Clear any current selection
  if ( webview->selectedText().size() ) {
    webview->page()->runJavaScript( "window.getSelection().removeAllRanges();_=0;" );
  }

  if ( searchPanel->lineEdit->property( "noResults" ).toBool() ) {
    searchPanel->lineEdit->setProperty( "noResults", false );

    Utils::Widget::setNoResultColor( searchPanel->lineEdit, false );
  }
}

void ArticleView::on_searchPrevious_clicked()
{
  if ( searchIsOpened )
    performFindOperation( false, true );
}

void ArticleView::on_searchNext_clicked()
{
  if ( searchIsOpened )
    performFindOperation( false, false );
}

void ArticleView::on_searchText_textEdited()
{
  performFindOperation( true, false );
}

void ArticleView::on_searchText_returnPressed()
{
  on_searchNext_clicked();
}

void ArticleView::on_searchCloseButton_clicked()
{
  closeSearch();
}

void ArticleView::on_searchCaseSensitive_clicked()
{
  performFindOperation( true, false );
}

void ArticleView::on_highlightAllButton_clicked()
{
  performFindOperation( false, false, true );
}

//the id start with "gdform-"
void ArticleView::onJsActiveArticleChanged( QString const & id )
{
  if ( !isScrollTo( id ) )
    return; // Incorrect id

  QString dictId = dictionaryIdFromScrollTo( id );
  setActiveArticleId( dictId );
  emit activeArticleChanged( this, dictId );
}

void ArticleView::doubleClicked( QPoint pos )
{
  // We might want to initiate translation of the selected word
  audioPlayer->stop();
  if ( cfg.preferences.doubleClickTranslates ) {
    QString selectedText = webview->selectedText();

    // ignore empty word;
    if ( selectedText.isEmpty() )
      return;

    emit sendWordToInputLine( selectedText );
    // Do some checks to make sure there's a sensible selection indeed
    if ( Folding::applyWhitespaceOnly( gd::toWString( selectedText ) ).size() && selectedText.size() < 60 ) {
      // Initiate translation
      Qt::KeyboardModifiers kmod = QApplication::keyboardModifiers();
      if ( kmod & ( Qt::ControlModifier | Qt::ShiftModifier ) ) { // open in new tab
        emit showDefinitionInNewTab( selectedText, getGroup( webview->url() ), getCurrentArticle(), Contexts() );
      }
      else {
        QUrl const & ref = webview->url();

        if ( Utils::Url::hasQueryItem( ref, "dictionaries" ) ) {
          QStringList dictsList = Utils::Url::queryItemValue( ref, "dictionaries" ).split( ",", Qt::SkipEmptyParts );
          showDefinition( selectedText, dictsList, QRegExp(), getGroup( ref ), false );
        }
        else
          showDefinition( selectedText, getGroup( ref ), getCurrentArticle() );
      }
    }
  }
}


void ArticleView::performFindOperation( bool restart, bool backwards, bool checkHighlight )
{
  QString text = searchPanel->lineEdit->text();

  if ( restart || checkHighlight ) {
    if ( restart ) {
      // Anyone knows how we reset the search position?
      // For now we resort to this hack:
      if ( webview->selectedText().size() ) {
        webview->page()->runJavaScript( "window.getSelection().removeAllRanges();_=0;" );
      }
    }

    QWebEnginePage::FindFlags f( 0 );

    if ( searchPanel->caseSensitive->isChecked() )
      f |= QWebEnginePage::FindCaseSensitively;

    webview->findText( "", f );

    if ( searchPanel->highlightAll->isChecked() )
      webview->findText( text, f );

    if ( checkHighlight )
      return;
  }

  QWebEnginePage::FindFlags f( 0 );

  if ( searchPanel->caseSensitive->isChecked() )
    f |= QWebEnginePage::FindCaseSensitively;

  if ( backwards )
    f |= QWebEnginePage::FindBackward;

  findText( text, f, [ text, this ]( bool match ) {
    bool setMark = !text.isEmpty() && !match;

    if ( searchPanel->lineEdit->property( "noResults" ).toBool() != setMark ) {
      searchPanel->lineEdit->setProperty( "noResults", setMark );

      Utils::Widget::setNoResultColor( searchPanel->lineEdit, setMark );
    }
  } );
}

void ArticleView::findText( QString & text,
                            const QWebEnginePage::FindFlags & f,
                            const std::function< void( bool match ) > & callback )
{
#if ( QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 ) )
  webview->findText( text, f, [ callback ]( const QWebEngineFindTextResult & result ) {
    auto r = result.numberOfMatches() > 0;
    if ( callback )
      callback( r );
  } );
#else
  webview->findText( text, f, [ callback ]( bool result ) {
    if ( callback )
      callback( result );
  } );
#endif
}

bool ArticleView::closeSearch()
{
  if ( searchIsOpened ) {
    searchPanel->hide();
    webview->setFocus();
    searchIsOpened = false;

    return true;
  }
  else if ( ftsSearchIsOpened ) {
    firstAvailableText.clear();
    uniqueMatches.clear();
    ftsSearchIsOpened = false;

    ftsSearchPanel->hide();
    webview->setFocus();

    QWebEnginePage::FindFlags flags( 0 );

    webview->findText( "", flags );

    return true;
  }
  else
    return false;
}

bool ArticleView::isSearchOpened()
{
  return searchIsOpened;
}

void ArticleView::showEvent( QShowEvent * ev )
{
  QWidget::showEvent( ev );

  if ( !searchIsOpened )
    searchPanel->hide();

  if ( !ftsSearchIsOpened )
    ftsSearchPanel->hide();
}

void ArticleView::copyAsText()
{
  QString text = webview->selectedText();
  if ( !text.isEmpty() )
    QApplication::clipboard()->setText( text );
}

void ArticleView::highlightFTSResults()
{
  closeSearch();
  // Clear any current selection
  webview->findText( "" );
  QString regString = Utils::Url::queryItemValue( webview->url(), "regexp" );
  if ( regString.isEmpty() )
    return;


  //replace any unicode Number ,Symbol ,Punctuation ,Mark character to whitespace
  regString.replace( QRegularExpression( R"([\p{N}\p{S}\p{P}\p{M}])", QRegularExpression::UseUnicodePropertiesOption ),
                     " " );

  if ( regString.trimmed().isEmpty() )
    return;

  QString script = QString(
                     "var context = document.querySelector(\"body\");\n"
                     "var instance = new Mark(context);\n"
                     "instance.mark(\"%1\",{\"accuracy\": \"exactly\"});" )
                     .arg( regString );

  webview->page()->runJavaScript( script );
}

void ArticleView::setActiveDictIds( const ActiveDictIds & ad )
{
  auto groupId = ad.groupId;
  if ( groupId == 0 ) {
    groupId = Instances::Group::AllGroupId;
  }
  if ( ( ad.word == currentWord && groupId == getCurrentGroup() ) || historyMode ) {
    // ignore all other signals.
    qDebug() << "receive dicts, current word:" << currentWord << ad.word << ":" << ad.dictIds;
    currentActiveDictIds << ad.dictIds;
    currentActiveDictIds.removeDuplicates();
    emit updateFoundInDictsList();
  }
}

void ArticleView::dictionaryClear( const ActiveDictIds & ad )
{
  auto groupId = ad.groupId;
  if ( groupId == 0 ) {
    groupId = Instances::Group::AllGroupId;
  }
  // ignore all other signals.
  if ( ad.word == currentWord && groupId == getCurrentGroup() ) {
    qDebug() << "clear current dictionaries:" << currentWord;
    currentActiveDictIds.clear();
  }
}

void ArticleView::performFtsFindOperation( bool backwards )
{
  if ( !ftsSearchIsOpened )
    return;

  if ( firstAvailableText.isEmpty() ) {
    ftsSearchPanel->statusLabel->setText( searchStatusMessageNoMatches() );
    ftsSearchPanel->next->setEnabled( false );
    ftsSearchPanel->previous->setEnabled( false );
    return;
  }

  QWebEnginePage::FindFlags flags( 0 );

  if ( ftsSearchMatchCase )
    flags |= QWebEnginePage::FindCaseSensitively;


  // Restore saved highlighted selection
  webview->page()->runJavaScript(
    QString( "var sel=window.getSelection();sel.removeAllRanges();sel.addRange(%1);_=0;" ).arg( rangeVarName ) );

  if ( backwards ) {
#if ( QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 ) )
    webview->findText( firstAvailableText,
                       flags | QWebEnginePage::FindBackward,
                       [ this ]( const QWebEngineFindTextResult & result ) {
                         if ( result.numberOfMatches() == 0 )
                           return;
                         ftsSearchPanel->previous->setEnabled( true );
                         if ( !ftsSearchPanel->next->isEnabled() )
                           ftsSearchPanel->next->setEnabled( true );

                         ftsSearchPanel->statusLabel->setText(
                           searchStatusMessage( result.activeMatch(), result.numberOfMatches() ) );
                       } );
#else
    webview->findText( firstAvailableText, flags | QWebEnginePage::FindBackward, [ this ]( bool res ) {
      ftsSearchPanel->previous->setEnabled( res );
      if ( !ftsSearchPanel->next->isEnabled() )
        ftsSearchPanel->next->setEnabled( res );
    } );
#endif
  }
  else {
#if ( QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 ) )
    webview->findText( firstAvailableText, flags, [ this ]( const QWebEngineFindTextResult & result ) {
      if ( result.numberOfMatches() == 0 )
        return;
      ftsSearchPanel->next->setEnabled( true );
      if ( !ftsSearchPanel->previous->isEnabled() )
        ftsSearchPanel->previous->setEnabled( true );

      ftsSearchPanel->statusLabel->setText( searchStatusMessage( result.activeMatch(), result.numberOfMatches() ) );
    } );
#else

    webview->findText( firstAvailableText, flags, [ this ]( bool res ) {
      ftsSearchPanel->next->setEnabled( res );
      if ( !ftsSearchPanel->previous->isEnabled() )
        ftsSearchPanel->previous->setEnabled( res );
    } );

#endif
  }
}

void ArticleView::on_ftsSearchPrevious_clicked()
{
  performFtsFindOperation( true );
}

void ArticleView::on_ftsSearchNext_clicked()
{
  performFtsFindOperation( false );
}
void ArticleView::clearContent()
{
  auto html = articleNetMgr.getHtml( ResourceType::BLANK );

  webview->setHtml( QString::fromStdString( html ) );
}

ResourceToSaveHandler::ResourceToSaveHandler( ArticleView * view, QString fileName ):
  QObject( view ),
  fileName( std::move( fileName ) ),
  alreadyDone( false )
{
  connect( this, &ResourceToSaveHandler::statusBarMessage, view, &ArticleView::statusBarMessage );
}

void ResourceToSaveHandler::addRequest( const sptr< Dictionary::DataRequest > & req )
{
  if ( !alreadyDone ) {
    downloadRequests.push_back( req );

    connect( req.get(), &Dictionary::Request::finished, this, &ResourceToSaveHandler::downloadFinished );
  }
}

void ResourceToSaveHandler::downloadFinished()
{
  if ( downloadRequests.empty() )
    return; // Stray signal

  // Find any finished resources
  for ( auto i = downloadRequests.begin(); i != downloadRequests.end(); ) {
    if ( ( *i )->isFinished() ) {
      if ( ( *i )->dataSize() >= 0 && !alreadyDone ) {
        QByteArray resourceData;
        vector< char > const & data = ( *i )->getFullData();
        resourceData                = QByteArray( data.data(), data.size() );

        // Write data to file

        if ( !fileName.isEmpty() ) {
          const QFileInfo fileInfo( fileName );
          QDir().mkpath( fileInfo.absoluteDir().absolutePath() );

          QFile file( fileName );
          if ( file.open( QFile::WriteOnly ) ) {
            file.write( resourceData.data(), resourceData.size() );
            file.close();
          }

          if ( file.error() ) {
            emit statusBarMessage( tr( "ERROR: %1" ).arg( tr( "Resource saving error: " ) + file.errorString() ),
                                   10000,
                                   QPixmap( ":/icons/error.svg" ) );
          }
        }
        alreadyDone = true;

        // Clear other requests

        downloadRequests.clear();
        break;
      }
      else {
        // This one had no data. Erase it.
        downloadRequests.erase( i++ );
      }
    }
    else // Unfinished, wait.
      break;
  }

  if ( downloadRequests.empty() ) {
    if ( !alreadyDone ) {
      emit statusBarMessage( tr( "WARNING: %1" ).arg( tr( "The referenced resource failed to download." ) ),
                             10000,
                             QPixmap( ":/icons/error.svg" ) );
    }
    emit done();
    deleteLater();
  }
}

ArticleViewAgent::ArticleViewAgent( ArticleView * articleView ):
  QObject( articleView ),
  articleView( articleView )
{
}

void ArticleViewAgent::onJsActiveArticleChanged( QString const & id )
{
  articleView->onJsActiveArticleChanged( id );
}

void ArticleViewAgent::linkClickedInHtml( QUrl const & url )
{
  articleView->linkClickedInHtml( url );
}

void ArticleViewAgent::collapseInHtml( QString const & dictId, bool on ) const
{
  if ( GlobalBroadcaster::instance()->getPreference()->sessionCollapse ) {
    if ( on ) {
      GlobalBroadcaster::instance()->collapsedDicts.insert( dictId );
    }
    else {
      GlobalBroadcaster::instance()->collapsedDicts.remove( dictId );
    }
  }
}

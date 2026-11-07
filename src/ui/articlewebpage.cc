#include "articlewebpage.hh"
#include "utils.hh"
#include "common/globalbroadcaster.hh"
#include <QTimer>

ArticleWebPage::ArticleWebPage( QObject * parent, bool isPopup_ ):
  QWebEnginePage( parent ),
  isPopup( isPopup_ )
{
#if QT_VERSION >= QT_VERSION_CHECK( 6, 8, 0 )
  connect( this, &QWebEnginePage::permissionRequested, this, &ArticleWebPage::onPermissionRequested );
#endif
}

#if QT_VERSION >= QT_VERSION_CHECK( 6, 8, 0 )
void ArticleWebPage::onPermissionRequested( const QWebEnginePermission & permission )
{
  if ( permission.permissionType() == QWebEnginePermission::PermissionType::ClipboardReadWrite ) {
    if ( GlobalBroadcaster::instance()->getPreference()->enableJavaScriptClipboardAccess ) {
      permission.grant();
    }
    else {
      permission.deny();
    }
  }
}
#endif
bool ArticleWebPage::acceptNavigationRequest( const QUrl & resUrl, NavigationType type, bool isMainFrame )
{
  QUrl url = resUrl;
  QUrlQuery urlQuery{ url };
  if ( url.scheme() == "bword" || url.scheme() == "entry" ) {
    url.setScheme( "gdlookup" );
    url.setHost( "localhost" );
    url.setPath( "" );
    auto [ valid, word ] = Utils::Url::getQueryWord( resUrl );
    urlQuery.addQueryItem( "word", word );
    urlQuery.addQueryItem( "group", lastReq.group );
    if ( lastReq.isPopup ) {
      urlQuery.addQueryItem( "popup", "1" );
    }
    url.setQuery( urlQuery );

    // Use singleShot to avoid synchronous navigation request within acceptNavigationRequest,
    // which can lead to reentrancy issues or crashes.
    QTimer::singleShot( 0, this, [ this, url ]() { setUrl( url ); } );
    return false;
  }

  //save current gdlookup's values.
  if ( url.scheme() == "gdlookup" ) {
    lastReq.group      = Utils::Url::queryItemValue( url, "group" );
    // Use the parameter if present, otherwise fall back to our own field
    QString popupParam = Utils::Url::queryItemValue( url, "popup" );
    if ( !popupParam.isEmpty() ) {
      lastReq.isPopup = popupParam == "1";
    }
    else {
      lastReq.isPopup = isPopup;
    }
  }

  if ( type == QWebEnginePage::NavigationTypeLinkClicked ) {
    // In a website view (the top-level document is a remote page), links are
    // allowed to navigate the view itself when configured to open websites in
    // new tabs. In dictionary articles every click is routed through the
    // linkClicked signal, so that openLink() decides: internal schemes look
    // up / play audio, external ones go to the desktop handler.
    if ( GlobalBroadcaster::instance()->getPreference()->openWebsiteInNewTab ) {
      // Use the member function explicitly: the local variable `url` (the
      // navigation target) shadows QWebEnginePage::url(), and the context
      // must be judged by the *current* document, not the target.
      const QUrl currentUrl = QWebEnginePage::url();
      const bool websiteContext = currentUrl.scheme() == QLatin1String( "http" )
        || currentUrl.scheme() == QLatin1String( "https" );
      // Check if URL is external link, if so, allow navigation in current webview
      if ( websiteContext && Utils::isExternalLink( url ) ) {
        return true;
      }
    }

    // Internal resource links (audio/video) are played natively via the
    // linkClicked signal instead of being navigated. Rejecting the navigation
    // below still makes QtWebEngine emit loadFinished(false); account for it so
    // the view can tell this expected failure from a real one. Only main-frame
    // rejections are counted: exactly one loadFinished(false) is delivered for
    // each of them, while sub-frame rejections never reach the view.
    if ( isMainFrame && ( Utils::Url::isAudioUrl( url ) || url.scheme() == "bres" || url.scheme() == "gdvideo" ) ) {
      ++pendingResourceRejections;
    }

    // Use singleShot to emit signal asynchronously. This prevents crashes if the
    // signal handler deletes this object or the containing tab.
    QTimer::singleShot( 0, this, [ this, url ]() { emit linkClicked( url ); } );
    return false;
  }

  return QWebEnginePage::acceptNavigationRequest( url, type, isMainFrame );
}

bool ArticleWebPage::takeResourceClickRejection()
{
  if ( pendingResourceRejections <= 0 ) {
    return false;
  }
  --pendingResourceRejections;
  return true;
}

void ArticleWebPage::javaScriptAlert( const QUrl & securityOrigin, const QString & msg )
{
  if ( !GlobalBroadcaster::instance()->getPreference()->suppressWebDialogs ) {
    QWebEnginePage::javaScriptAlert( securityOrigin, msg );
    return;
  }
  qDebug() << "JavaScript Alert:" << msg << "from" << securityOrigin;
  runJavaScript( QString( "console.log('JavaScript Alert:', decodeURIComponent('%1'))" )
                   .arg( QString::fromUtf8( msg.toUtf8().toPercentEncoding() ) ) );
}

bool ArticleWebPage::javaScriptConfirm( const QUrl & securityOrigin, const QString & msg )
{
  if ( !GlobalBroadcaster::instance()->getPreference()->suppressWebDialogs ) {
    return QWebEnginePage::javaScriptConfirm( securityOrigin, msg );
  }
  qDebug() << "JavaScript Confirm:" << msg << "from" << securityOrigin;
  runJavaScript( QString( "console.log('JavaScript Confirm:', decodeURIComponent('%1'))" )
                   .arg( QString::fromUtf8( msg.toUtf8().toPercentEncoding() ) ) );
  return true;
}

bool ArticleWebPage::javaScriptPrompt( const QUrl & securityOrigin,
                                       const QString & msg,
                                       const QString & defaultValue,
                                       QString * result )
{
  if ( !GlobalBroadcaster::instance()->getPreference()->suppressWebDialogs ) {
    return QWebEnginePage::javaScriptPrompt( securityOrigin, msg, defaultValue, result );
  }
  qDebug() << "JavaScript Prompt:" << msg << "Default:" << defaultValue << "from" << securityOrigin;
  runJavaScript( QString( "console.log('JavaScript Prompt:', decodeURIComponent('%1'))" )
                   .arg( QString::fromUtf8( msg.toUtf8().toPercentEncoding() ) ) );
  return false;
}

void ArticleWebPage::javaScriptConsoleMessage( JavaScriptConsoleMessageLevel, const QString &, int, const QString & ) {}

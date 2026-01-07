#include "articlewebpage.hh"
#include "utils.hh"
#include "common/globalbroadcaster.hh"
#include <QTimer>

ArticleWebPage::ArticleWebPage( QObject * parent, bool isPopup_ ):
  QWebEnginePage( parent ), isPopup( isPopup_ )
{
}
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
    // If configured to open website in new tab, trigger linkClicked signal normally
    // Otherwise, for websiteview, we allow navigation request
    if ( GlobalBroadcaster::instance()->getPreference()->openWebsiteInNewTab ) {
      // Check if URL is external link, if so, allow navigation in current webview
      if ( Utils::isExternalLink( url ) ) {
        return true;
      }
    }

    // Use singleShot to emit signal asynchronously. This prevents crashes if the
    // signal handler deletes this object or the containing tab.
    QTimer::singleShot( 0, this, [ this, url ]() { emit linkClicked( url ); } );
    return false;
  }

  return QWebEnginePage::acceptNavigationRequest( url, type, isMainFrame );
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

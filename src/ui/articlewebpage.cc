#include "articlewebpage.hh"
#include "utils.hh"
#include "common/globalbroadcaster.hh"

ArticleWebPage::ArticleWebPage( QObject * parent ):
  QWebEnginePage{ parent }
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
    urlQuery.addQueryItem( "muted", lastReq.mutedDicts );
    url.setQuery( urlQuery );
    setUrl( url );
    return false;
  }

  //save current gdlookup's values.
  if ( url.scheme() == "gdlookup" ) {
    lastReq.group      = Utils::Url::queryItemValue( url, "group" );
    lastReq.mutedDicts = Utils::Url::queryItemValue( url, "muted" );
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
    emit linkClicked( url );
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
  runJavaScript( QString( "console.log('JavaScript Alert: %1')" ).arg( msg.toHtmlEscaped() ) );
}

bool ArticleWebPage::javaScriptConfirm( const QUrl & securityOrigin, const QString & msg )
{
  if ( !GlobalBroadcaster::instance()->getPreference()->suppressWebDialogs ) {
    return QWebEnginePage::javaScriptConfirm( securityOrigin, msg );
  }
  qDebug() << "JavaScript Confirm:" << msg << "from" << securityOrigin;
  runJavaScript( QString( "console.log('JavaScript Confirm: %1')" ).arg( msg.toHtmlEscaped() ) );
  return true;
}

bool ArticleWebPage::javaScriptPrompt( const QUrl & securityOrigin, const QString & msg, const QString & defaultValue, QString * result )
{
  if ( !GlobalBroadcaster::instance()->getPreference()->suppressWebDialogs ) {
    return QWebEnginePage::javaScriptPrompt( securityOrigin, msg, defaultValue, result );
  }
  qDebug() << "JavaScript Prompt:" << msg << "Default:" << defaultValue << "from" << securityOrigin;
  runJavaScript( QString( "console.log('JavaScript Prompt: %1')" ).arg( msg.toHtmlEscaped() ) );
  return false;
}

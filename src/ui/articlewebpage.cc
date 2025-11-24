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

void ArticleWebPage::javaScriptConsoleMessage( JavaScriptConsoleMessageLevel level, const QString & message, int lineNumber, const QString & sourceID )
{
  if ( GlobalBroadcaster::instance()->getPreference()->suppressWebDialogs ) {
    // If we are suppressing dialogs, we might also want to be less noisy about console errors,
    // or maybe we just want to downgrade them to debug.
    // For now, let's just log them as debug to avoid "Critical" in the log file.
  }

  QString levelStr;
  switch ( level ) {
    case InfoMessageLevel:
      levelStr = "Info";
      break;
    case WarningMessageLevel:
      levelStr = "Warning";
      break;
    case ErrorMessageLevel:
      levelStr = "Error";
      break;
    default:
      levelStr = "Log";
  }

  qDebug() << "JS Console" << levelStr << ":" << message << "Line:" << lineNumber << "Source:" << sourceID;
}

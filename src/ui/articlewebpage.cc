#include "articlewebpage.hh"
#include "utils.hh"

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
    emit linkClicked( url );
    return false;
  }

  return QWebEnginePage::acceptNavigationRequest( url, type, isMainFrame );
}

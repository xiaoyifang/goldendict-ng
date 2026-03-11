#include "weburlrequestinterceptor.hh"
#include "utils.hh"
#include "globalbroadcaster.hh"
#include "config.hh"

WebUrlRequestInterceptor::WebUrlRequestInterceptor( QObject * p ):
  QWebEngineUrlRequestInterceptor( p )
{
}
void WebUrlRequestInterceptor::interceptRequest( QWebEngineUrlRequestInfo & info )
{
  auto url = info.requestUrl();

  // When content is loaded inside GoldenDict's article view, we might face CORS issues.
  // Setting Origin and Referer headers can help bypass some CORS restrictions.
  if ( !GlobalBroadcaster::instance()->getPreference()->openWebsiteInNewTab ) {
    info.setHttpHeader( "origin", Utils::Url::getSchemeAndHost( url ).toUtf8() );
    info.setHttpHeader( "referer", url.url().toUtf8() );
  }

  if ( GlobalBroadcaster::instance()->getPreference()->disallowContentFromOtherSites && Utils::isExternalLink( url ) ) {
    // Block file:// links to prevent local file access
    if ( url.scheme() == "file" ) {
      info.block( true );
      return;
    }

    if ( info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame ) {
      return;
    }

    // Allow same host content even if "disallowContentFromOtherSites" is enabled
    if ( !url.host().isEmpty() && url.host() == info.firstPartyUrl().host() ) {
      return;
    }

    if ( GlobalBroadcaster::instance()->existedInHostWhitelist( Utils::Url::extractBaseDomain( url.host() ) )
         || GlobalBroadcaster::instance()->existedInRefererWhitelist(
           Utils::Url::extractBaseDomain( info.firstPartyUrl().host() ) ) ) {
      // Target host or referring site is in respective whitelist - do not block
      return;
    }
    if ( info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeImage
         || info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeFontResource
         || info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeStylesheet
         || info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMedia || Utils::isHtmlResources( url ) ) {
      //let throuth the resources file.
      return;
    }

    // block external links
    {
      qDebug() << "Blocked external link: " << url.toString();
      info.block( true );
      return;
    }
  }

  if ( QWebEngineUrlRequestInfo::NavigationTypeLink == info.navigationType()
       && info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame ) {
    //workaround to fix devtool "Switch devtool to chinese" interface was blocked.
    if ( url.scheme() == "devtools" ) {
      return;
    }

    // If the domain is the same as the current page, allow normal navigation
    // This enables links in website dictionaries to work normally within the same site.
    if ( !url.host().isEmpty() && url.host() == info.firstPartyUrl().host() ) {
      return;
    }

    emit linkClicked( url );
    qDebug() << "Blocked external link: " << url.toString();
    info.block( true );
  }

  //window.location=audio link
  if ( Utils::Url::isAudioUrl( url ) && info.navigationType() == QWebEngineUrlRequestInfo::NavigationTypeRedirect ) {
    qDebug() << "blocked audio url from page redirect" << url.url();
    info.block( true );
  }
}

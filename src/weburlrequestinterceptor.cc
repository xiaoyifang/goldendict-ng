#include "weburlrequestinterceptor.hh"
#include <QDebug>
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
  if ( url.scheme().startsWith( Config::WEBSITE_PROXY_PREFIX ) ) {
    url.setScheme( url.scheme().mid( 7 ) );
  }

  info.setHttpHeader( "origin", Utils::Url::getSchemeAndHost( url ).toUtf8() );
  info.setHttpHeader( "referer", url.url().toUtf8() );
  if ( GlobalBroadcaster::instance()->getPreference()->disallowContentFromOtherSites && Utils::isExternalLink( url ) ) {
    //file:// link ,pass
    if ( url.scheme() == "file" ) {
      return;
    }
    auto hostBase = url.host();
    if ( GlobalBroadcaster::instance()->existedInWhitelist( hostBase ) ) {
      //whitelist url does not block
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
    emit linkClicked( url );
    info.block( true );
  }

  //window.location=audio link
  if ( Utils::Url::isAudioUrl( url ) && info.navigationType() == QWebEngineUrlRequestInfo::NavigationTypeRedirect ) {
    qDebug() << "blocked audio url from page redirect" << url.url();
    info.block( true );
  }
}

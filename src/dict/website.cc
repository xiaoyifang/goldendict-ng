/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "website.hh"
#include "website_p.hh"
#include "wstring_qt.hh"
#include "utf8.hh"
#include <QUrl>
#include <QTextCodec>
#include <QDir>
#include <QFileInfo>
#include "globalbroadcaster.hh"
#include <QRegularExpression>

namespace WebSite {

using namespace Dictionary;

namespace {

class WebSiteDictionary: public Dictionary::Class
{
  string name;
  QByteArray urlTemplate;
  bool experimentalIframe;
  QString iconFilename;
  bool inside_iframe;
  QNetworkAccessManager & netMgr;

public:

  WebSiteDictionary( string const & id, string const & name_,
                     QString const & urlTemplate_,
                     QString const & iconFilename_,
                     bool inside_iframe_,
                     QNetworkAccessManager & netMgr_ ):
    Dictionary::Class( id, vector< string >() ),
    name( name_ ),
    iconFilename( iconFilename_ ),
    inside_iframe( inside_iframe_ ),
    netMgr( netMgr_ ),
    experimentalIframe(false)
  {
    if( urlTemplate_.startsWith( "http://" ) || urlTemplate_.startsWith( "https://" ) )
    {
      experimentalIframe = true;
    }
    //else file:/// local dictionary file path

    urlTemplate = QUrl( urlTemplate_ ).toEncoded() ;
    dictionaryDescription = urlTemplate_;
  }

  string getName() noexcept override
  { return name; }

  map< Property, string > getProperties() noexcept override
  { return map< Property, string >(); }

  unsigned long getArticleCount() noexcept override
  { return 0; }

  unsigned long getWordCount() noexcept override
  { return 0; }

  sptr< Request::WordSearch > prefixMatch( wstring const & word,
                                                 unsigned long ) override ;

  sptr< Request::Article > getArticle( wstring const &,
                                          vector< wstring > const & alts,
                                          wstring const & context, bool ) override
    ;

  sptr< Request::Blob > getResource( string const & name ) override ;

  void isolateWebCSS( QString & css );

protected:

  void loadIcon() noexcept override;
};

sptr< Request::WordSearch > WebSiteDictionary::prefixMatch( wstring const & /*word*/,
                                                          unsigned long ) 
{
  sptr< Request::WordSearchInstant > sr = std::make_shared< Request::WordSearchInstant >();

  sr->setUncertain( true );

  return sr;
}

void WebSiteDictionary::isolateWebCSS( QString & css )
{
  isolateCSS( css, ".website" );
}

sptr< Request::Article > WebSiteDictionary::getArticle( wstring const & str,
                                                   vector< wstring > const &,
                                                   wstring const & context, bool )
  
{
  QByteArray urlString;

  // Context contains the right url to go to
  if ( context.size() )
    urlString = Utf8::encode( context ).c_str();
  else
  {
    urlString = urlTemplate;

    QString inputWord = QString::fromStdU32String( str );

    urlString.replace( "%25GDWORD%25", inputWord.toUtf8().toPercentEncoding() );

    QTextCodec *codec = QTextCodec::codecForName( "Windows-1251" );
    if( codec )
      urlString.replace( "%25GD1251%25", codec->fromUnicode( inputWord ).toPercentEncoding() );

    codec = QTextCodec::codecForName( "Big-5" );
    if( codec )
      urlString.replace( "%25GDBIG5%25", codec->fromUnicode( inputWord ).toPercentEncoding() );

    codec = QTextCodec::codecForName( "Big5-HKSCS" );
    if( codec )
      urlString.replace( "%25GDBIG5HKSCS%25", codec->fromUnicode( inputWord ).toPercentEncoding() );

    codec = QTextCodec::codecForName( "Shift-JIS" );
    if( codec )
      urlString.replace( "%25GDSHIFTJIS%25", codec->fromUnicode( inputWord ).toPercentEncoding() );

    codec = QTextCodec::codecForName( "GB18030" );
    if( codec )
      urlString.replace( "%25GDGBK%25", codec->fromUnicode( inputWord ).toPercentEncoding() );


    // Handle all ISO-8859 encodings
    for( int x = 1; x <= 16; ++x )
    {
      codec = QTextCodec::codecForName( QString( "ISO 8859-%1" ).arg( x ).toLatin1() );
      if( codec )
        urlString.replace( QString( "%25GDISO%1%25" ).arg( x ).toUtf8(), codec->fromUnicode( inputWord ).toPercentEncoding() );

      if ( x == 10 )
        x = 12; // Skip encodings 11..12, they don't exist
    }
  }

  if( inside_iframe )
  {
    // Just insert link in <iframe> tag

    auto dr = std::make_shared<Request::ArticleInstant >( true );

    string result = "<div class=\"website_padding\"></div>";

    //heuristic add url to global whitelist.
    QUrl url(urlString);
    GlobalBroadcaster::instance()->addWhitelist(url.host());

    QString encodeUrl;
    if(experimentalIframe){
      encodeUrl="ifr://localhost?url="+QUrl::toPercentEncoding(  urlString);
    }
    else{
      encodeUrl = urlString;
    }


    result += string( "<iframe id=\"gdexpandframe-" ) + getId() +
                      "\" src=\""+encodeUrl.toStdString() +
                      "\" onmouseover=\"processIframeMouseOver('gdexpandframe-" + getId() + "');\" "
                      "onmouseout=\"processIframeMouseOut();\" "
                      "scrolling=\"no\" "
                      "style=\"overflow:visible; width:100%; display:block; border:none;\" sandbox=\"allow-same-origin allow-scripts allow-popups\">"
                      "</iframe>";

    dr->getData().resize( result.size() );

    memcpy( &( dr->getData().front() ), result.data(), result.size() );

    return dr;
  }

  // To load data from site

  return std::make_shared<WebSiteArticleRequest>( urlString, netMgr, this );
}

sptr< Request::Blob > WebSiteDictionary::getResource( string const & name )
{
  QString link = QString::fromUtf8( name.c_str() );
  int pos = link.indexOf( '/' );
  if( pos > 0 )
    link.replace( pos, 1, "://" );
  return std::make_shared< WebSiteResourceRequest >( link, netMgr, this );
}

void WebSiteDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded )
    return;

  if( !iconFilename.isEmpty() )
  {
    QFileInfo fInfo(  QDir( Config::getConfigDir() ), iconFilename );
    if( fInfo.isFile() )
      loadIconFromFile( fInfo.absoluteFilePath(), true );
  }
  if( dictionaryIcon.isNull() && !loadIconFromText(":/icons/webdict.svg", QString::fromStdString(name ) ) )
    dictionaryIcon = QIcon(":/icons/webdict.svg");
  dictionaryIconLoaded = true;
}

}

vector< sptr< Dictionary::Class > > makeDictionaries( Config::WebSites const & ws,
                                                      QNetworkAccessManager & mgr )
  
{
  vector< sptr< Dictionary::Class > > result;

  for( int x = 0; x < ws.size(); ++x )
  {
    if ( ws[ x ].enabled )
      result.push_back( std::make_shared<WebSiteDictionary>( ws[ x ].id.toUtf8().data(),
                                               ws[ x ].name.toUtf8().data(),
                                               ws[ x ].url,
                                               ws[ x ].iconFilename,
                                               ws[ x ].inside_iframe,
                                               mgr )
                      );
  }

  return result;
}

}

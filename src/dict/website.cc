/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "website.hh"
#include "text.hh"
#include <QUrl>
#include <QTextCodec>
#include <QDir>
#include <QFileInfo>
#include "globalbroadcaster.hh"
#include "fmt/compile.h"

#include <QRegularExpression>

namespace WebSite {

using namespace Dictionary;

namespace {

class WebSiteDictionary: public Dictionary::Class
{
  QByteArray urlTemplate;
  bool experimentalIframe;
  QString iconFilename;
  bool inside_iframe;
  QNetworkAccessManager & netMgr;

public:

  WebSiteDictionary( string const & id,
                     string const & name_,
                     QString const & urlTemplate_,
                     QString const & iconFilename_,
                     bool inside_iframe_,
                     QNetworkAccessManager & netMgr_ ):
    Dictionary::Class( id, vector< string >() ),
    iconFilename( iconFilename_ ),
    inside_iframe( inside_iframe_ ),
    netMgr( netMgr_ ),
    experimentalIframe( false )
  {
    dictionaryName = name_;

    if ( urlTemplate_.startsWith( "http://" ) || urlTemplate_.startsWith( "https://" ) ) {
      experimentalIframe = true;
    }
    //else file:/// local dictionary file path

    urlTemplate           = QUrl( urlTemplate_ ).toEncoded();
    dictionaryDescription = urlTemplate_;
  }

  unsigned long getArticleCount() noexcept override
  {
    return 0;
  }

  unsigned long getWordCount() noexcept override
  {
    return 0;
  }

  sptr< WordSearchRequest > prefixMatch( std::u32string const & word, unsigned long ) override;

  sptr< DataRequest > getArticle( std::u32string const &,
                                  vector< std::u32string > const & alts,
                                  std::u32string const & context,
                                  bool ) override;

  sptr< Dictionary::DataRequest > getResource( string const & name ) override;

  void isolateWebCSS( QString & css );

  Features getFeatures() const noexcept override
  {
    return Dictionary::WebSite;
  }

protected:

  void loadIcon() noexcept override;
};

class WebSiteDataRequestSlots: public Dictionary::DataRequest
{
  Q_OBJECT

protected slots:

  virtual void requestFinished( QNetworkReply * ) {}
};

sptr< WordSearchRequest > WebSiteDictionary::prefixMatch( std::u32string const & /*word*/, unsigned long )
{
  sptr< WordSearchRequestInstant > sr = std::make_shared< WordSearchRequestInstant >();

  sr->setUncertain( true );

  return sr;
}

void WebSiteDictionary::isolateWebCSS( QString & css )
{
  isolateCSS( css, ".website" );
}

class WebSiteArticleRequest: public WebSiteDataRequestSlots
{
  QNetworkReply * netReply;
  QString url;
  Class * dictPtr;
  QNetworkAccessManager & mgr;

public:

  WebSiteArticleRequest( QString const & url, QNetworkAccessManager & _mgr, Class * dictPtr_ );
  ~WebSiteArticleRequest() {}

  void cancel() override;

private:

  void requestFinished( QNetworkReply * ) override;
};

void WebSiteArticleRequest::cancel()
{
  finish();
}

WebSiteArticleRequest::WebSiteArticleRequest( QString const & url_, QNetworkAccessManager & _mgr, Class * dictPtr_ ):
  url( url_ ),
  dictPtr( dictPtr_ ),
  mgr( _mgr )
{
  connect( &mgr,
           SIGNAL( finished( QNetworkReply * ) ),
           this,
           SLOT( requestFinished( QNetworkReply * ) ),
           Qt::QueuedConnection );

  QUrl reqUrl( url );

  auto request = QNetworkRequest( reqUrl );
  request.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );
  netReply = mgr.get( request );

#ifndef QT_NO_SSL
  connect( netReply, SIGNAL( sslErrors( QList< QSslError > ) ), netReply, SLOT( ignoreSslErrors() ) );
#endif
}

void WebSiteArticleRequest::requestFinished( QNetworkReply * r )
{
  if ( isFinished() ) { // Was cancelled
    return;
  }

  if ( r != netReply ) {
    // Well, that's not our reply, don't do anything
    return;
  }

  if ( netReply->error() == QNetworkReply::NoError ) {
    // Check for redirect reply

    QVariant possibleRedirectUrl = netReply->attribute( QNetworkRequest::RedirectionTargetAttribute );
    QUrl redirectUrl             = possibleRedirectUrl.toUrl();
    if ( !redirectUrl.isEmpty() ) {
      disconnect( netReply, 0, 0, 0 );
      netReply->deleteLater();
      netReply = mgr.get( QNetworkRequest( redirectUrl ) );
#ifndef QT_NO_SSL
      connect( netReply, SIGNAL( sslErrors( QList< QSslError > ) ), netReply, SLOT( ignoreSslErrors() ) );
#endif
      return;
    }

    // Handle reply data

    QByteArray replyData = netReply->readAll();
    QString articleString;

    QTextCodec * codec = QTextCodec::codecForHtml( replyData, 0 );
    if ( codec ) {
      articleString = codec->toUnicode( replyData );
    }
    else {
      articleString = QString::fromUtf8( replyData );
    }

    // Change links from relative to absolute

    QString root = netReply->url().scheme() + "://" + netReply->url().host();
    QString base = root + netReply->url().path();
    while ( !base.isEmpty() && !base.endsWith( "/" ) ) {
      base.chop( 1 );
    }

    QRegularExpression tags( R"(<\s*(a|link|img|script)\s+[^>]*(src|href)\s*=\s*['"][^>]+>)",
                             QRegularExpression::CaseInsensitiveOption );
    QRegularExpression links( R"(\b(src|href)\s*=\s*(['"])([^'"]+['"]))", QRegularExpression::CaseInsensitiveOption );
    int pos = 0;
    QString articleNewString;
    QRegularExpressionMatchIterator it = tags.globalMatch( articleString );
    while ( it.hasNext() ) {
      QRegularExpressionMatch match = it.next();
      articleNewString += articleString.mid( pos, match.capturedStart() - pos );
      pos = match.capturedEnd();

      QString tag = match.captured();

      QRegularExpressionMatch match_links = links.match( tag );
      if ( !match_links.hasMatch() ) {
        articleNewString += tag;
        continue;
      }

      QString url = match_links.captured( 3 );

      if ( url.indexOf( ":/" ) >= 0 || url.indexOf( "data:" ) >= 0 || url.indexOf( "mailto:" ) >= 0
           || url.startsWith( "#" ) || url.startsWith( "javascript:" ) ) {
        // External link, anchor or base64-encoded data
        articleNewString += tag;
        continue;
      }

      QString newUrl = match_links.captured( 1 ) + "=" + match_links.captured( 2 );
      if ( url.startsWith( "//" ) ) {
        newUrl += netReply->url().scheme() + ":";
      }
      else if ( url.startsWith( "/" ) ) {
        newUrl += root;
      }
      else {
        newUrl += base;
      }
      newUrl += match_links.captured( 3 );

      tag.replace( match_links.capturedStart(), match_links.capturedLength(), newUrl );
      articleNewString += tag;
    }
    if ( pos ) {
      articleNewString += articleString.mid( pos );
      articleString = articleNewString;
      articleNewString.clear();
    }

    // Redirect CSS links to own handler

    QString prefix = QString( "bres://" ) + dictPtr->getId().c_str() + "/";
    QRegularExpression linkTags(
      R"((<\s*link\s[^>]*rel\s*=\s*['"]stylesheet['"]\s+[^>]*href\s*=\s*['"])([^'"]+)://([^'"]+['"][^>]+>))",
      QRegularExpression::CaseInsensitiveOption );
    pos = 0;
    it  = linkTags.globalMatch( articleString );

    while ( it.hasNext() ) {
      QRegularExpressionMatch match = it.next();
      articleNewString += articleString.mid( pos, match.capturedStart() - pos );
      pos = match.capturedEnd();

      QString newTag = match.captured( 1 ) + prefix + match.captured( 2 ) + "/" + match.captured( 3 );
      articleNewString += newTag;
    }
    if ( pos ) {
      articleNewString += articleString.mid( pos );
      articleString = articleNewString;
      articleNewString.clear();
    }

    // See Issue #271: A mechanism to clean-up invalid HTML cards.
    articleString += QString::fromStdString( Utils::Html::getHtmlCleaner() );


    QString divStr = QString( "<div class=\"website\"" );
    divStr += dictPtr->isToLanguageRTL() ? " dir=\"rtl\">" : ">";

    articleString.prepend( divStr.toUtf8() );
    articleString.append( "</div>" );

    articleString.prepend( "<div class=\"website_padding\"></div>" );

    appendString( articleString.toStdString() );

    hasAnyData = true;
  }
  else {
    if ( netReply->url().scheme() == "file" ) {
      qWarning( "WebSites: Failed loading article from \"%s\", reason: %s",
                dictPtr->getName().c_str(),
                netReply->errorString().toUtf8().data() );
    }
    else {
      setErrorString( netReply->errorString() );
    }
  }

  disconnect( netReply, nullptr, 0, 0 );
  netReply->deleteLater();

  finish();
}

sptr< DataRequest > WebSiteDictionary::getArticle( std::u32string const & str,
                                                   vector< std::u32string > const & /*alts*/,
                                                   std::u32string const & context,
                                                   bool /*ignoreDiacritics*/ )
{
  QString urlString = Utils::WebSite::urlReplaceWord( QString( urlTemplate ), QString::fromStdU32String( str ) );

  if ( inside_iframe ) {
    // Just insert link in <iframe> tag

    string result = R"(<div class="website_padding"></div>)";

    //heuristic add url to global whitelist.
    QUrl url( urlString );
    GlobalBroadcaster::instance()->addWhitelist( url.host() );

    QString encodeUrl;
    if ( experimentalIframe ) {
      encodeUrl = "ifr://localhost?url=" + QUrl::toPercentEncoding( urlString );
    }
    else {
      encodeUrl = urlString;
    }

    fmt::format_to( std::back_inserter( result ),
                    R"(<iframe id="gdexpandframe-{}" src="{}"
onmouseover="processIframeMouseOver('gdexpandframe-{}');"
onmouseout="processIframeMouseOut();" scrolling="no"
style="overflow:visible; width:100%; display:block; border:none;"
sandbox="allow-same-origin allow-scripts allow-popups"></iframe>)",
                    getId(),
                    encodeUrl.toStdString(),
                    getId() );

    auto dr = std::make_shared< DataRequestInstant >( true );
    dr->appendString( result );
    return dr;
  }

  // To load data from site

  return std::make_shared< WebSiteArticleRequest >( urlString, netMgr, this );
}

class WebSiteResourceRequest: public WebSiteDataRequestSlots
{
  QNetworkReply * netReply;
  QString url;
  WebSiteDictionary * dictPtr;
  QNetworkAccessManager & mgr;

public:

  WebSiteResourceRequest( QString const & url_, QNetworkAccessManager & _mgr, WebSiteDictionary * dictPtr_ );
  ~WebSiteResourceRequest() {}

  void cancel() override;

private:

  void requestFinished( QNetworkReply * ) override;
};

WebSiteResourceRequest::WebSiteResourceRequest( QString const & url_,
                                                QNetworkAccessManager & _mgr,
                                                WebSiteDictionary * dictPtr_ ):
  url( url_ ),
  dictPtr( dictPtr_ ),
  mgr( _mgr )
{
  connect( &mgr,
           SIGNAL( finished( QNetworkReply * ) ),
           this,
           SLOT( requestFinished( QNetworkReply * ) ),
           Qt::QueuedConnection );

  QUrl reqUrl( url );

  netReply = mgr.get( QNetworkRequest( reqUrl ) );

#ifndef QT_NO_SSL
  connect( netReply, SIGNAL( sslErrors( QList< QSslError > ) ), netReply, SLOT( ignoreSslErrors() ) );
#endif
}

void WebSiteResourceRequest::cancel()
{
  finish();
}

void WebSiteResourceRequest::requestFinished( QNetworkReply * r )
{
  if ( isFinished() ) { // Was cancelled
    return;
  }

  if ( r != netReply ) {
    // Well, that's not our reply, don't do anything
    return;
  }

  if ( netReply->error() == QNetworkReply::NoError ) {
    // Check for redirect reply

    QVariant possibleRedirectUrl = netReply->attribute( QNetworkRequest::RedirectionTargetAttribute );
    QUrl redirectUrl             = possibleRedirectUrl.toUrl();
    if ( !redirectUrl.isEmpty() ) {
      disconnect( netReply, 0, 0, 0 );
      netReply->deleteLater();
      netReply = mgr.get( QNetworkRequest( redirectUrl ) );
#ifndef QT_NO_SSL
      connect( netReply, SIGNAL( sslErrors( QList< QSslError > ) ), netReply, SLOT( ignoreSslErrors() ) );
#endif
      return;
    }

    // Handle reply data

    QByteArray replyData = netReply->readAll();
    QString cssString    = QString::fromUtf8( replyData );

    dictPtr->isolateWebCSS( cssString );

    appendString( cssString.toStdString() );

    hasAnyData = true;
  }
  else {
    setErrorString( netReply->errorString() );
  }

  disconnect( netReply, 0, 0, 0 );
  netReply->deleteLater();

  finish();
}

sptr< Dictionary::DataRequest > WebSiteDictionary::getResource( string const & name )
{
  QString link = QString::fromUtf8( name.c_str() );
  int pos      = link.indexOf( '/' );
  if ( pos > 0 ) {
    link.replace( pos, 1, "://" );
  }
  return std::make_shared< WebSiteResourceRequest >( link, netMgr, this );
}

void WebSiteDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  if ( !iconFilename.isEmpty() ) {
    QFileInfo fInfo( QDir( Config::getConfigDir() ), iconFilename );
    if ( fInfo.isFile() ) {
      loadIconFromFile( fInfo.absoluteFilePath(), true );
    }
  }
  if ( dictionaryIcon.isNull()
       && !loadIconFromText( ":/icons/webdict.svg", QString::fromStdString( dictionaryName ) ) ) {
    dictionaryIcon = QIcon( ":/icons/webdict.svg" );
  }
  dictionaryIconLoaded = true;
}

} // namespace

vector< sptr< Dictionary::Class > > makeDictionaries( Config::WebSites const & ws, QNetworkAccessManager & mgr )

{
  vector< sptr< Dictionary::Class > > result;

  for ( const auto & w : ws ) {
    if ( w.enabled ) {
      result.push_back( std::make_shared< WebSiteDictionary >( w.id.toUtf8().data(),
                                                               w.name.toUtf8().data(),
                                                               w.url,
                                                               w.iconFilename,
                                                               w.inside_iframe,
                                                               mgr ) );
    }
  }

  return result;
}

#include "website.moc"
} // namespace WebSite

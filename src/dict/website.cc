/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "website.hh"
#include <QUrl>
#include <QTextCodec>
#include <QDir>
#include <QFileInfo>
#include "globalbroadcaster.hh"
#include "fmt/compile.h"
#include <QRegularExpression>
#include <QCoreApplication>

namespace WebSite {

using namespace Dictionary;

namespace {

class WebSiteDictionary: public Dictionary::Class
{
  QByteArray urlTemplate;
  QString iconFilename;

public:

  WebSiteDictionary( const string & id,
                     const string & name_,
                     const QString & urlTemplate_,
                     const QString & iconFilename_ ):
    Dictionary::Class( id, vector< string >() ),
    iconFilename( iconFilename_ )
  {
    dictionaryName        = name_;
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

  sptr< WordSearchRequest > prefixMatch( const std::u32string & word, unsigned long ) override;

  sptr< DataRequest > getArticle( const std::u32string &,
                                  const vector< std::u32string > & alts,
                                  const std::u32string & context,
                                  bool ) override;

  sptr< DataRequest > getResource( const string & name ) override;

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

sptr< WordSearchRequest > WebSiteDictionary::prefixMatch( const std::u32string & /*word*/, unsigned long )
{
  sptr< WordSearchRequestInstant > sr = std::make_shared< WordSearchRequestInstant >();

  sr->setUncertain( true );

  return sr;
}

sptr< DataRequest > WebSiteDictionary::getArticle( const std::u32string & str,
                                                   const vector< std::u32string > & /*alts*/,
                                                   const std::u32string & context,
                                                   bool /*ignoreDiacritics*/ )
{
  QString urlString = Utils::WebSite::urlReplaceWord( QString( urlTemplate ), QString::fromStdU32String( str ) );

  string result = R"(<div class="website_padding"></div>)";

  //heuristic add url to global whitelist.
  QUrl url( urlString );
  GlobalBroadcaster::instance()->addWhitelist( url.host() );

  const QString & encodeUrl = urlString;

  if ( GlobalBroadcaster::instance()->getPreference()->openWebsiteInNewTab ) {

    //replace the word,and get the actual requested url
    QString url = urlTemplate;
    if ( !url.isEmpty() ) {
      auto word          = QString::fromStdU32String( str );
      QString requestUrl = Utils::WebSite::urlReplaceWord( url, word );
      auto title         = QString::fromStdString( getName() );
      // Pass dictId to the websiteDictionarySignal
      emit GlobalBroadcaster::instance()
        -> websiteDictionarySignal( title + "-" + word, requestUrl, QString::fromStdString( getId() ) );
    }

    fmt::format_to(
      std::back_inserter( result ),
      R"(<div class="website-new-tab-notice">
                          <p>{}</p>
                        </div>)",
      QCoreApplication::translate( "WebSite", "This website dictionary is opened in a new tab" ).toStdString() );
  }
  else {
    fmt::format_to( std::back_inserter( result ),
                    R"(<iframe id="gdexpandframe-{}" src="{}"
scrolling="no" data-gd-id="{}" 
class="website-iframe"
sandbox="allow-same-origin allow-scripts allow-popups allow-forms"></iframe>)",
                    getId(),
                    encodeUrl.toStdString(),
                    getId() );
  }
  auto dr = std::make_shared< DataRequestInstant >( true );
  dr->appendString( result );
  return dr;
}


sptr< DataRequest > WebSiteDictionary::getResource( const string & /*name*/ )
{
  return std::make_shared< DataRequestInstant >( false );
}

void WebSiteDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  if ( !iconFilename.isEmpty() ) {
    QFileInfo fInfo( QDir( Config::getConfigDir() ), iconFilename );
    if ( fInfo.isFile() ) {
      loadIconFromFilePath( fInfo.absoluteFilePath() );
    }
  }
  if ( dictionaryIcon.isNull()
       && !loadIconFromText( ":/icons/webdict.svg", QString::fromStdString( dictionaryName ) ) ) {
    dictionaryIcon = QIcon( ":/icons/webdict.svg" );
  }
  dictionaryIconLoaded = true;
}

} // namespace

vector< sptr< Dictionary::Class > > makeDictionaries( const Config::WebSites & ws, QNetworkAccessManager & mgr )

{
  vector< sptr< Dictionary::Class > > result;

  for ( const auto & w : ws ) {
    if ( w.enabled ) {
      result.push_back( std::make_shared< WebSiteDictionary >( w.id.toUtf8().data(),
                                                               w.name.toUtf8().data(),
                                                               w.url,
                                                               w.iconFilename ) );
    }
  }

  return result;
}

} // namespace WebSite

// fixes #2272
// automoc include for Q_OBJECT should be at the very end of source code file, not inside a namespace
#include "website.moc"

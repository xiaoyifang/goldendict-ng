#include "lingualibre.h"
#include "utf8.hh"
#include "audiolink.hh"
#include <string>
#include <mutex.hh>
#include <QJsonDocument>

namespace Lingua {

using namespace Dictionary;

namespace {


  class LinguaDictionary: public Dictionary::Class
  {

    string name;
    QString languageCode;
    QNetworkAccessManager & netMgr;

   public:
    LinguaDictionary(
      string const & id, string const & name_, QString const & languageCode_, QNetworkAccessManager & netMgr_ ):
        Dictionary::Class( id, vector< string >() ), name( name_ ), languageCode( languageCode_ ), netMgr( netMgr_ )
    {
    }

    string getName() noexcept override { return name; }

    map< Property, string > getProperties() noexcept override { return {}; }

    unsigned long getArticleCount() noexcept override { return 0; }

    unsigned long getWordCount() noexcept override { return 0; }

    sptr< WordSearchRequest > prefixMatch( wstring const & /*word*/, unsigned long /*maxResults*/ ) override
    {
      sptr< WordSearchRequestInstant > sr = std::make_shared< WordSearchRequestInstant >();

      sr->setUncertain( true );

      return sr;
    }

    sptr< DataRequest > getArticle(
      wstring const & word, vector< wstring > const & alts, wstring const &, bool ) override
    {
      if( word.size() > 50 )
      {
        return std::make_shared< DataRequestInstant >( false );
      }
      else
      {
        return std::make_shared< LinguaArticleRequest >( word, alts, languageCode, getId(), netMgr );
      }
    }

   protected:
    void loadIcon() noexcept override
    {
      if( dictionaryIconLoaded )
        return;

      dictionaryIcon = dictionaryNativeIcon = QIcon( ":/icons/lingualibre.svg" );
      dictionaryIconLoaded                  = true;
    }
  };

}

vector< sptr< Dictionary::Class > > makeDictionaries( Dictionary::Initializing &, QNetworkAccessManager & mgr )
{

  vector< sptr< Dictionary::Class > > result;

  QCryptographicHash hash( QCryptographicHash::Md5 );

  hash.addData( "Lingua libre via Wiki Commons" );

  result.push_back( std::make_shared< LinguaDictionary >( hash.result().toHex().data(),
    QString( "LinguaLibre" ).toUtf8().data(),
    nullptr,
    mgr ) );
  return result;
};


void LinguaArticleRequest::cancel() {}

LinguaArticleRequest::LinguaArticleRequest( const wstring & str,
  const vector< wstring > & alts,
  const QString & languageCode_,
  const string & dictionaryId_,
  QNetworkAccessManager & mgr )
{
  connect( &mgr, &QNetworkAccessManager::finished, this, &LinguaArticleRequest::requestFinished, Qt::QueuedConnection );

  addQuery( mgr, str );
}

void LinguaArticleRequest::addQuery( QNetworkAccessManager & mgr, const wstring & word )
{

  // Doc of the <https://www.mediawiki.org/wiki/API:Query>
  QString reqUrl = R"(https://commons.wikimedia.org/w/api.php?)"
                   R"(action=query)"
                   R"(&format=json)"
                   R"(&prop=imageinfo)"
                   R"(&generator=search)"
                   R"(&iiprop=url)"
                   R"(&iimetadataversion=1)"
                   R"(&iiextmetadatafilter=Categories)"
                   R"(&gsrsearch=intitle:LL-Q1860 \(eng\)-.*-%1\.wav/)"
                   R"(&gsrnamespace=6)"
                   R"(&gsrlimit=10)"
                   R"(&gsrwhat=text)";

  auto netReply =
    std::shared_ptr< QNetworkReply >( mgr.get( QNetworkRequest( reqUrl.arg( QString::fromStdU32String( word ) ) ) ) );

  netReplies.emplace_back( netReply, Utf8::encode( word ) );
}


void LinguaArticleRequest::requestFinished( QNetworkReply * r )
{

  qDebug() << "Lingua query finished";

  sptr< QNetworkReply > netReply = netReplies.front().reply;

  QJsonObject resultJson = QJsonDocument::fromJson( netReply->readAll() ).object();

  /*

 Code below is to process returned json:

{
  "batchcomplete": "",
  "query": {
    "pages": {
      "88511149": {
        "pageid": 88511149,
        "ns": 6,
        "title": "File:LL-Q1860 (eng)-Back ache-nice.wav",
        "index": 2,
        "imagerepository": "local",
        "imageinfo": [
          {
            "url": "https://upload.wikimedia.org/wikipedia/commons/6/6a/LL-Q1860_%28eng%29-Back_ache-nice.wav",
            "descriptionurl": "https://commons.wikimedia.org/wiki/File:LL-Q1860_(eng)-Back_ache-nice.wav",
            "descriptionshorturl": "https://commons.wikimedia.org/w/index.php?curid=88511149"
          }
        ]
      },
      "73937351": {
        "pageid": 73937351,
        "ns": 6,
        "title": "File:LL-Q1860 (eng)-Nattes à chat-nice.wav",
        "index": 1,
        "imagerepository": "local",
        "imageinfo": [
          {
            "url": "https://upload.wikimedia.org/wikipedia/commons/b/b0/LL-Q1860_%28eng%29-Nattes_%C3%A0_chat-nice.wav",
            "descriptionurl": "https://commons.wikimedia.org/wiki/File:LL-Q1860_(eng)-Nattes_%C3%A0_chat-nice.wav",
            "descriptionshorturl": "https://commons.wikimedia.org/w/index.php?curid=73937351"
          }
        ]
      }
    }
  }
}

*/

  if( resultJson.contains( "query" ) )
  {

    string articleBody = "<p>";

    for( auto pageJsonVal : resultJson[ "query" ].toObject()[ "pages" ].toObject() )
    {
      auto pageJsonObj = pageJsonVal.toObject();
      string title     = pageJsonObj[ "title" ].toString().toHtmlEscaped().toStdString();
      string audiolink =
        pageJsonObj[ "imageinfo" ].toArray().at( 0 ).toObject()[ "url" ].toString().toHtmlEscaped().toStdString();
      articleBody += addAudioLink( audiolink, dictionaryId );
      articleBody += "<a href=";
      articleBody += audiolink;
      articleBody += ">";
      articleBody += title;
      articleBody += "</a><br>";
    }

    articleBody += "</p>";

    Mutex::Lock _( dataMutex );

    size_t prevSize = data.size();
    data.resize( prevSize + articleBody.size() );

    memcpy( &data.front() + prevSize, articleBody.data(), articleBody.size() );

    hasAnyData = true;

    finish();
  }
  else
  {
    hasAnyData = false;
  }
}


} // end namespace Lingua

#include "lingualibre.h"
#include <string>

namespace Lingua {

using namespace Dictionary;

namespace {



class LinguaDictionary:public Dictionary::Class{

  string name;
  QString languageCode;
  QNetworkAccessManager & netMgr;

public:

  LinguaDictionary(string const & id, string const & name_,
    QString const & languageCode_,
    QNetworkAccessManager & netMgr_
  ):
  Dictionary::Class(id,vector< string> ()),
      name( name_ ),
      languageCode(languageCode_ ),
      netMgr( netMgr_ )
  {}

  virtual string getName() noexcept
  { return name; }

  virtual map< Property, string > getProperties() noexcept
  { return map< Property, string >(); }

  virtual unsigned long getArticleCount() noexcept
  { return 0; }

  virtual unsigned long getWordCount() noexcept
  { return 0; }

  virtual sptr< WordSearchRequest > prefixMatch( wstring const & /*word*/,
    unsigned long /*maxResults*/ )
  {
    sptr< WordSearchRequestInstant > sr =  std::make_shared<WordSearchRequestInstant>();

    sr->setUncertain( true );

    return sr;
  }

  virtual sptr< DataRequest > getArticle( wstring const & word, vector< wstring > const & alts,
    wstring const &, bool )
  {

    string result = "nice";

    sptr< Dictionary::DataRequestInstant > r =
      std::make_shared<Dictionary::DataRequestInstant>( true );

    r->getData().resize( result.size() );
    memcpy( &( r->getData().front() ), result.data(), result.size() );


    if( word.size()>50){
      return std::make_shared<DataRequestInstant>(false);
    }
    return r;
  }

 protected:
   void loadIcon() noexcept override {
    if ( dictionaryIconLoaded )
      return;

    dictionaryIcon = dictionaryNativeIcon = QIcon( ":/icons/lingualibre.svg");
    dictionaryIconLoaded = true;
  }
};

}

vector< sptr< Dictionary::Class > > makeDictionaries(
  Dictionary::Initializing &,
  QNetworkAccessManager & mgr){

  vector< sptr< Dictionary::Class > > result;

  QCryptographicHash hash( QCryptographicHash::Md5 );

  hash.addData( "Lingua libre via Wiki Commons");

  result.push_back(
    std::make_shared<LinguaDictionary>(
      hash.result().toHex().data(),
      QString("LinguaLibre").toUtf8().data(),
      nullptr,
      mgr
      ));
  return result;
  };

  void LinguaArticleRequest::requestFinished( QNetworkReply * ) {}
  void LinguaArticleRequest::addQuery( QNetworkAccessManager & mgr, const string & word ) {}
  void LinguaArticleRequest::cancel() {}
  LinguaArticleRequest::LinguaArticleRequest( const wstring & word,
    const vector< wstring > & alts,
    const QString & languageCode_,
    const string & dictionaryId_,
    QNetworkAccessManager & mgr )
  {

  }
  }
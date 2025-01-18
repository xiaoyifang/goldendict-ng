#include "lingualibre.hh"
#include "text.hh"
#include "audiolink.hh"

#include <QJsonArray>
#include <QJsonDocument>

#include <string>
#include <utility>
#include <QNetworkReply>

namespace Lingua {

using namespace Dictionary;

namespace {

class LinguaArticleRequest: public Dictionary::DataRequest
{
  Q_OBJECT

  struct NetReply
  {
    sptr< QNetworkReply > reply;
    string word;
    bool finished;

    NetReply( sptr< QNetworkReply > const & reply_, string const & word_ ):
      reply( reply_ ),
      word( word_ ),
      finished( false )
    {
    }
  };

  using NetReplies = std::list< NetReply >;
  NetReplies netReplies;
  QString languageCode, langWikipediaID;
  string dictionaryId;

public:

  LinguaArticleRequest( std::u32string const & word,
                        vector< std::u32string > const & alts,
                        QString const & languageCode_,
                        QString const & langWikipediaID_,
                        string const & dictionaryId_,
                        QNetworkAccessManager & mgr );

  void cancel() override;

private:

  void addQuery( QNetworkAccessManager & mgr, std::u32string const & word );

private slots:
  virtual void requestFinished( QNetworkReply * );
};

class LinguaDictionary: public Dictionary::Class
{

  QString languageCode;
  QString langWikipediaID;
  QNetworkAccessManager & netMgr;

public:
  LinguaDictionary( string const & id, string name_, QString languageCode_, QNetworkAccessManager & netMgr_ ):
    Dictionary::Class( id, vector< string >() ),
    languageCode( std::move( languageCode_ ) ),
    netMgr( netMgr_ )
  {
    dictionaryName = name_;
    /* map of iso lang code to wikipedia lang id

Data was obtained by this query on https://commons-query.wikimedia.org/
SELECT ?language ?languageLabel ?iso ?audios
WHERE {
  {
    SELECT ?language (COUNT(?audio) AS ?audios) WHERE {
      ?audio # Filter: P2 'instance of' is Q2 'record'
      wdt:P407 ?language .
    }
    GROUP BY ?language
  }

            SERVICE <https://query.wikidata.org/sparql> {
                ?language wdt:P220 ?iso .      # Assign value: P220 'ISO-639-3' into ?iso.
    }


                  SERVICE <https://query.wikidata.org/sparql> {
                  SERVICE wikibase:label {
    bd:serviceParam wikibase:language "[AUTO_LANGUAGE],en".
        ?language rdfs:label ?languageLabel .
    }
  }
}

      */

    const map< string, string > iso_to_wikipedia_id = {
      { "grc", "Q35497" },    { "non", "Q35505" },   { "ken", "Q35650" },   { "got", "Q35722" },
      { "gsc", "Q35735" },    { "ldn", "Q35757" },   { "csc", "Q35768" },   { "lns", "Q35788" },
      { "kab", "Q35853" },    { "ina", "Q35934" },   { "jam", "Q35939" },   { "mua", "Q36032" },
      { "mhk", "Q36068" },    { "mos", "Q36096" },   { "kmr", "Q36163" },   { "num", "Q36173" },
      { "mad", "Q36213" },    { "lin", "Q36217" },   { "mal", "Q36236" },   { "srr", "Q36284" },
      { "jbo", "Q36350" },    { "yas", "Q36358" },   { "kur", "Q36368" },   { "ell", "Q36510" },
      { "swl", "Q36558" },    { "gya", "Q36594" },   { "tvu", "Q36632" },   { "mlu", "Q36645" },
      { "tui", "Q36646" },    { "ota", "Q36730" },   { "rar", "Q36745" },   { "ckb", "Q36811" },
      { "tok", "Q36846" },    { "twi", "Q36850" },   { "vut", "Q36897" },   { "ybb", "Q36917" },
      { "bse", "Q36973" },    { "wls", "Q36979" },   { "lzh", "Q37041" },   { "ang", "Q42365" },
      { "ker", "Q56251" },    { "ary", "Q56426" },   { "sjn", "Q56437" },   { "hau", "Q56475" },
      { "arq", "Q56499" },    { "atj", "Q56590" },   { "mcn", "Q56668" },   { "tpw", "Q56944" },
      { "vls", "Q100103" },   { "gsw", "Q131339" },  { "hrx", "Q304049" },  { "lms", "Q427614" },
      { "sxu", "Q699284" },   { "pwn", "Q715755" },  { "tay", "Q715766" },  { "guc", "Q891085" },
      { "lnc", "Q942602" },   { "blc", "Q977808" },  { "avk", "Q1377116" }, { "sba", "Q2372207" },
      { "gcf", "Q3006280" },  { "far", "Q3067168" }, { "kld", "Q3111818" }, { "swh", "Q3197533" },
      { "rhg", "Q3241177" },  { "vsl", "Q3322064" }, { "xzh", "Q3437292" }, { "ane", "Q3571097" },
      { "kcg", "Q3912765" },  { "hav", "Q5684097" }, { "isu", "Q6089423" }, { "mdl", "Q6744816" },
      { "duf", "Q6983819" },  { "sru", "Q7646993" }, { "yat", "Q8048020" }, { "lem", "Q13479983" },
      { "mul", "Q20923490" }, { "cmn", "Q9192" },    { "vie", "Q9199" },    { "tha", "Q9217" },
      { "msa", "Q9237" },     { "ind", "Q9240" },    { "mon", "Q9246" },    { "tgk", "Q9260" },
      { "uzb", "Q9264" },     { "heb", "Q9288" },    { "aze", "Q9292" },    { "mkd", "Q9296" },
      { "bos", "Q9303" },     { "glg", "Q9307" },    { "cym", "Q9309" },    { "gla", "Q9314" },
      { "ben", "Q9610" },     { "tlh", "Q10134" },   { "bre", "Q12107" },   { "rcf", "Q13198" },
      { "xho", "Q13218" },    { "hsb", "Q13248" },   { "sms", "Q13271" },   { "nav", "Q13310" },
      { "min", "Q13324" },    { "mnw", "Q13349" },   { "ara", "Q13955" },   { "oci", "Q14185" },
      { "afr", "Q14196" },    { "sco", "Q14549" },   { "ase", "Q14759" },   { "pms", "Q15085" },
      { "fao", "Q25258" },    { "tat", "Q25285" },   { "cor", "Q25289" },   { "kal", "Q25355" },
      { "nds", "Q25433" },    { "fry", "Q27175" },   { "ace", "Q27683" },   { "ain", "Q27969" },
      { "aka", "Q28026" },    { "amh", "Q28244" },   { "anp", "Q28378" },   { "rup", "Q29316" },
      { "arz", "Q29919" },    { "myv", "Q29952" },   { "dag", "Q32238" },   { "dyu", "Q32706" },
      { "bfi", "Q33000" },    { "dua", "Q33013" },   { "ban", "Q33070" },   { "bas", "Q33093" },
      { "cos", "Q33111" },    { "bam", "Q33243" },   { "chy", "Q33265" },   { "shy", "Q33274" },
      { "bcl", "Q33284" },    { "gaa", "Q33287" },   { "fon", "Q33291" },   { "fil", "Q33298" },
      { "fsl", "Q33302" },    { "che", "Q33350" },   { "chr", "Q33388" },   { "fur", "Q33441" },
      { "smn", "Q33462" },    { "hat", "Q33491" },   { "syc", "Q33538" },   { "jav", "Q33549" },
      { "kas", "Q33552" },    { "haw", "Q33569" },   { "ibo", "Q33578" },   { "kik", "Q33587" },
      { "mnc", "Q33638" },    { "kan", "Q33673" },   { "krc", "Q33714" },   { "ory", "Q33810" },
      { "orm", "Q33864" },    { "mni", "Q33868" },   { "nso", "Q33890" },   { "sat", "Q33965" },
      { "scn", "Q33973" },    { "srd", "Q33976" },   { "srn", "Q33989" },   { "snd", "Q33997" },
      { "sun", "Q34002" },    { "pcd", "Q34024" },   { "ddo", "Q34033" },   { "tvl", "Q34055" },
      { "tgl", "Q34057" },    { "nmg", "Q34098" },   { "tsn", "Q34137" },   { "shi", "Q34152" },
      { "lua", "Q34173" },    { "rif", "Q34174" },   { "wln", "Q34219" },   { "wol", "Q34257" },
      { "bci", "Q35107" },    { "cak", "Q35115" },   { "ido", "Q35224" },   { "bbj", "Q35271" },
      { "bik", "Q35455" },    { "epo", "Q143" },     { "fra", "Q150" },     { "deu", "Q188" },
      { "tur", "Q256" },      { "isl", "Q294" },     { "lat", "Q397" },     { "ita", "Q652" },
      { "pol", "Q809" },      { "spa", "Q1321" },    { "fin", "Q1412" },    { "hin", "Q1568" },
      { "mar", "Q1571" },     { "eng", "Q1860" },    { "aym", "Q4627" },    { "guj", "Q5137" },
      { "por", "Q5146" },     { "que", "Q5218" },    { "jpn", "Q5287" },    { "tam", "Q5885" },
      { "hrv", "Q6654" },     { "cat", "Q7026" },    { "nld", "Q7411" },    { "rus", "Q7737" },
      { "swa", "Q7838" },     { "zho", "Q7850" },    { "ron", "Q7913" },    { "bul", "Q7918" },
      { "mlg", "Q7930" },     { "tel", "Q8097" },    { "yid", "Q8641" },    { "sqi", "Q8748" },
      { "eus", "Q8752" },     { "hye", "Q8785" },    { "ukr", "Q8798" },    { "swe", "Q9027" },
      { "dan", "Q9035" },     { "nor", "Q9043" },    { "ltz", "Q9051" },    { "ces", "Q9056" },
      { "slv", "Q9063" },     { "hun", "Q9067" },    { "est", "Q9072" },    { "bel", "Q9091" },
      { "ell", "Q9129" },     { "gle", "Q9142" },    { "mlt", "Q9166" },    { "fas", "Q9168" },
      { "kor", "Q9176" },     { "yue", "Q9186" } };
    // END OF iso_to_wikipedia_id

    auto it = iso_to_wikipedia_id.find( languageCode.toStdString() );
    if ( it != iso_to_wikipedia_id.end() ) {
      langWikipediaID = QString::fromStdString( it->second );
    }
  }

  unsigned long getArticleCount() noexcept override
  {
    return 0;
  }

  unsigned long getWordCount() noexcept override
  {
    return 0;
  }

  sptr< WordSearchRequest > prefixMatch( std::u32string const & /*word*/, unsigned long /*maxResults*/ ) override
  {
    sptr< WordSearchRequestInstant > sr = std::make_shared< WordSearchRequestInstant >();

    sr->setUncertain( true );

    return sr;
  }

  sptr< DataRequest > getArticle( std::u32string const & word,
                                  vector< std::u32string > const & alts,
                                  std::u32string const &,
                                  bool ) override
  {
    if ( word.size() < 50 ) {
      return std::make_shared< LinguaArticleRequest >( word, alts, languageCode, langWikipediaID, getId(), netMgr );
    }
    else {
      return std::make_shared< DataRequestInstant >( false );
    }
  }

protected:
  void loadIcon() noexcept override
  {
    if ( dictionaryIconLoaded ) {
      return;
    }

    dictionaryIcon       = QIcon( ":/icons/lingualibre.svg" );
    dictionaryIconLoaded = true;
  }
};

} // namespace

vector< sptr< Dictionary::Class > >
makeDictionaries( Dictionary::Initializing &, Config::Lingua const & lingua, QNetworkAccessManager & mgr )
{
  vector< sptr< Dictionary::Class > > result;
  if ( lingua.enable and !lingua.languageCodes.isEmpty() ) {
    QCryptographicHash hash( QCryptographicHash::Md5 );

    hash.addData( "Lingua libre via Wiki Commons" );

    result.push_back( std::make_shared< LinguaDictionary >( hash.result().toHex().data(),
                                                            QString( "LinguaLibre" ).toUtf8().data(),
                                                            lingua.languageCodes,
                                                            mgr ) );
  }
  return result;
};


void LinguaArticleRequest::cancel()
{
  finish();
}

LinguaArticleRequest::LinguaArticleRequest( const std::u32string & str,
                                            const vector< std::u32string > & alts,
                                            const QString & languageCode_,
                                            const QString & langWikipediaID,
                                            const string & dictionaryId_,
                                            QNetworkAccessManager & mgr ):
  languageCode( languageCode_ ),
  langWikipediaID( langWikipediaID )
{
  connect( &mgr, &QNetworkAccessManager::finished, this, &LinguaArticleRequest::requestFinished, Qt::QueuedConnection );

  addQuery( mgr, str );
}

void LinguaArticleRequest::addQuery( QNetworkAccessManager & mgr, const std::u32string & word )
{

  // Doc of the <https://www.mediawiki.org/wiki/API:Query>
  QString reqUrl =
    R"(https://commons.wikimedia.org/w/api.php?)"
    R"(action=query)"
    R"(&format=json)"
    R"(&prop=imageinfo)"
    R"(&generator=search)"
    R"(&iiprop=url)"
    R"(&iimetadataversion=1)"
    R"(&iiextmetadatafilter=Categories)"
    R"(&gsrsearch=intitle:LL-%1 \(%2\)-.*-%3\.wav/)" // https://en.wikipedia.org/wiki/Help:Searching/Regex
    R"(&gsrnamespace=6)"
    R"(&gsrlimit=10)"
    R"(&gsrwhat=text)";

  reqUrl = reqUrl.arg( langWikipediaID, languageCode, QString::fromStdU32String( word ) );

  qDebug() << "lingualibre query " << reqUrl;

  auto netRequest = QNetworkRequest( reqUrl );
  netRequest.setTransferTimeout( 3000 );

  auto netReply = std::shared_ptr< QNetworkReply >( mgr.get( netRequest ) );


  netReplies.emplace_back( netReply, Text::toUtf8( word ) );
}


void LinguaArticleRequest::requestFinished( QNetworkReply * r )
{

  qDebug() << "Lingua query finished ";

  sptr< QNetworkReply > netReply = netReplies.front().reply;

  if ( isFinished() ) {
    return;
  }
  if ( !netReply->isFinished() || netReply->error() != QNetworkReply::NoError ) {
    qWarning() << "Lingua query failed: " << netReply->error();
    cancel();
    return;
  }

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

  if ( resultJson.contains( "query" ) ) {

    string articleBody = "<div class=\"audio-play\">";

    for ( auto pageJsonVal : resultJson[ "query" ].toObject()[ "pages" ].toObject() ) {
      auto pageJsonObj = pageJsonVal.toObject();
      string title     = pageJsonObj[ "title" ].toString().toHtmlEscaped().toStdString();
      string audiolink =
        pageJsonObj[ "imageinfo" ].toArray().at( 0 ).toObject()[ "url" ].toString().toHtmlEscaped().toStdString();
      addAudioLink( audiolink, dictionaryId );

      articleBody += "<div class=\"audio-play-item\">";
      //play icon
      articleBody += R"(<a href=")";
      articleBody += audiolink;
      articleBody += R"(" role="button">)";
      articleBody += R"(<img src="qrc:///icons/playsound.png" border="0" alt="Play"/>)";
      articleBody += "</a>";
      //text
      articleBody += R"(<a href=")";
      articleBody += audiolink;
      articleBody += R"(" role="link">)";
      articleBody += title;
      articleBody += "</a>";
      articleBody += "</div>";
    }

    articleBody += "</div>";

    appendString( articleBody );


    hasAnyData = true;

    finish();
  }
  else {
    hasAnyData = false;
    finish();
  }
}

#include "lingualibre.moc"
} // end namespace Lingua

/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "forvo.hh"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QtXml>
#include <list>
#include "audiolink.hh"
#include "htmlescape.hh"
#include "text.hh"

namespace Forvo {

using namespace Dictionary;

namespace {

class ForvoDictionary: public Dictionary::Class
{
  QString apiKey, languageCode;
  QNetworkAccessManager & netMgr;

public:

  ForvoDictionary( string const & id,
                   string const & name_,
                   QString const & apiKey_,
                   QString const & languageCode_,
                   QNetworkAccessManager & netMgr_ ):
    Dictionary::Class( id, vector< string >() ),
    apiKey( apiKey_ ),
    languageCode( languageCode_ ),
    netMgr( netMgr_ )
  {
    dictionaryName = name_;
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

  sptr< DataRequest >
  getArticle( std::u32string const &, vector< std::u32string > const & alts, std::u32string const &, bool ) override;

protected:

  void loadIcon() noexcept override;
};

class ForvoArticleRequest: public Dictionary::DataRequest
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
  QString apiKey, languageCode;
  string dictionaryId;

public:

  ForvoArticleRequest( std::u32string const & word,
                       vector< std::u32string > const & alts,
                       QString const & apiKey_,
                       QString const & languageCode_,
                       string const & dictionaryId_,
                       QNetworkAccessManager & mgr );

  void cancel() override;

private:

  void addQuery( QNetworkAccessManager & mgr, std::u32string const & word );

private slots:
  virtual void requestFinished( QNetworkReply * );
};

sptr< DataRequest > ForvoDictionary::getArticle( std::u32string const & word,
                                                 vector< std::u32string > const & alts,
                                                 std::u32string const &,
                                                 bool )

{
  if ( word.size() > 80 || apiKey.isEmpty() ) {
    // Don't make excessively large queries -- they're fruitless anyway

    return std::make_shared< DataRequestInstant >( false );
  }
  else {
    return std::make_shared< ForvoArticleRequest >( word, alts, apiKey, languageCode, getId(), netMgr );
  }
}

void ForvoDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  dictionaryIcon       = QIcon( ":/icons/forvo.png" );
  dictionaryIconLoaded = true;
}

} // namespace

void ForvoArticleRequest::cancel()
{
  finish();
}

ForvoArticleRequest::ForvoArticleRequest( std::u32string const & str,
                                          vector< std::u32string > const & alts,
                                          QString const & apiKey_,
                                          QString const & languageCode_,
                                          string const & dictionaryId_,
                                          QNetworkAccessManager & mgr ):
  apiKey( apiKey_ ),
  languageCode( languageCode_ ),
  dictionaryId( dictionaryId_ )
{
  connect( &mgr, &QNetworkAccessManager::finished, this, &ForvoArticleRequest::requestFinished, Qt::QueuedConnection );

  addQuery( mgr, str );

  for ( const auto & alt : alts ) {
    addQuery( mgr, alt );
  }
}

void ForvoArticleRequest::addQuery( QNetworkAccessManager & mgr, std::u32string const & str )
{
  qDebug( "Forvo: requesting article %s", QString::fromStdU32String( str ).toUtf8().data() );

  QString key = apiKey;

  QUrl reqUrl =
    QUrl::fromEncoded( QString( "https://apifree.forvo.com"
                                "/key/"
                                + key
                                + "/action/word-pronunciations"
                                  "/format/xml"
                                  "/word/"
                                + QLatin1String( QUrl::toPercentEncoding( QString::fromStdU32String( str ) ) )
                                + "/language/" + languageCode + "/order/rate-desc" )
                         .toUtf8() );

  //  qDebug( "req: %s", reqUrl.toEncoded().data() );

  sptr< QNetworkReply > netReply = std::shared_ptr< QNetworkReply >( mgr.get( QNetworkRequest( reqUrl ) ) );

  netReplies.push_back( NetReply( netReply, Text::toUtf8( str ) ) );
}

void ForvoArticleRequest::requestFinished( QNetworkReply * r )
{
  qDebug( "Finished." );

  if ( isFinished() ) { // Was cancelled
    return;
  }

  // Find this reply

  bool found = false;

  for ( auto & netReplie : netReplies ) {
    if ( netReplie.reply.get() == r ) {
      netReplie.finished = true; // Mark as finished
      found              = true;
      break;
    }
  }

  if ( !found ) {
    // Well, that's not our reply, don't do anything
    return;
  }

  bool updated = false;

  for ( ; netReplies.size() && netReplies.front().finished; netReplies.pop_front() ) {
    sptr< QNetworkReply > netReply = netReplies.front().reply;

    if ( netReply->error() == QNetworkReply::NoError ) {
      QDomDocument dd;

      QString errorStr;
      int errorLine, errorColumn;

      if ( !dd.setContent( netReply.get(), false, &errorStr, &errorLine, &errorColumn ) ) {
        setErrorString(
          QString( tr( "XML parse error: %1 at %2,%3" ).arg( errorStr ).arg( errorLine ).arg( errorColumn ) ) );
      }
      else {
        //        qDebug( "%s", dd.toByteArray().data() );

        QDomNode items = dd.namedItem( "items" );

        if ( !items.isNull() ) {
          QDomNodeList nl = items.toElement().elementsByTagName( "item" );

          if ( nl.count() ) {
            string articleBody;

            articleBody += "<div class='forvo_headword'>";
            articleBody += Html::escape( netReplies.front().word );
            articleBody += "</div>";

            articleBody += "<table class=\"forvo_play\">";

            for ( int x = 0; x < nl.length(); ++x ) {
              QDomElement item = nl.item( x ).toElement();

              QDomNode mp3 = item.namedItem( "pathmp3" );

              if ( !mp3.isNull() ) {
                articleBody += "<tr>";

                QUrl url( mp3.toElement().text() );

                string ref = string( "\"" ) + url.toEncoded().data() + "\"";

                articleBody += addAudioLink( url.toEncoded(), dictionaryId ).c_str();

                bool isMale = ( item.namedItem( "sex" ).toElement().text().toLower() != "f" );

                QString user    = item.namedItem( "username" ).toElement().text();
                QString country = item.namedItem( "country" ).toElement().text();

                string userProfile =
                  string( "http://www.forvo.com/user/" ) + QUrl::toPercentEncoding( user ).data() + "/";

                int totalVotes    = item.namedItem( "num_votes" ).toElement().text().toInt();
                int positiveVotes = item.namedItem( "num_positive_votes" ).toElement().text().toInt();
                int negativeVotes = totalVotes - positiveVotes;

                string votes;

                if ( positiveVotes || negativeVotes ) {
                  votes += " ";

                  if ( positiveVotes ) {
                    votes += "<span class='forvo_positive_votes'>+";
                    votes += QByteArray::number( positiveVotes ).data();
                    votes += "</span>";
                  }

                  if ( negativeVotes ) {
                    if ( positiveVotes ) {
                      votes += " ";
                    }

                    votes += "<span class='forvo_negative_votes'>-";
                    votes += QByteArray::number( negativeVotes ).data();
                    votes += "</span>";
                  }
                }

                string addTime = tr( "Added %1" ).arg( item.namedItem( "addtime" ).toElement().text() ).toUtf8().data();

                articleBody += "<td><a href=" + ref + " title=\"" + Html::escape( addTime )
                  + R"("><img src="qrc:///icons/playsound.png" border="0" alt="Play"/></a></td>)";
                articleBody += string( "<td>" ) + tr( "by" ).toUtf8().data() + " <a class='forvo_user' href='"
                  + userProfile + "'>" + Html::escape( user.toUtf8().data() ) + "</a> <span class='forvo_location'>("
                  + ( isMale ? tr( "Male" ) : tr( "Female" ) ).toUtf8().data() + " " + tr( "from" ).toUtf8().data()
                  + " " + Html::escape( country.toUtf8().data() ) + ")</span>" + votes + "</td>";
                articleBody += "</tr>";
              }
            }

            articleBody += "</table>";

            appendString( articleBody );

            hasAnyData = true;

            updated = true;
          }
        }

        QDomNode errors = dd.namedItem( "errors" );

        if ( !errors.isNull() ) {
          QString text( errors.namedItem( "error" ).toElement().text() );
          setErrorString( text );
        }
      }
      qDebug( "done." );
    }
    else {
      //forvo return the error message with http status code=400.
      QDomDocument dd;

      QString errorStr;
      int errorLine, errorColumn;

      if ( !dd.setContent( netReply.get(), false, &errorStr, &errorLine, &errorColumn ) ) {
        setErrorString( netReply->errorString() );
      }
      else {
        QDomNode errors = dd.namedItem( "errors" );
        if ( !errors.isNull() ) {
          QString text( errors.namedItem( "error" ).toElement().text() );
          setErrorString( text );
        }
      }
    }
  }

  if ( netReplies.empty() ) {
    finish();
  }
  else if ( updated ) {
    update();
  }
}

vector< sptr< Dictionary::Class > >
makeDictionaries( Dictionary::Initializing &, Config::Forvo const & forvo, QNetworkAccessManager & mgr )

{
  vector< sptr< Dictionary::Class > > result;

  if ( forvo.enable && !forvo.apiKey.isEmpty() ) {
    QStringList codes = forvo.languageCodes.split( ',', Qt::SkipEmptyParts );

    QSet< QString > usedCodes;

    for ( const auto & x : codes ) {
      QString code = x.simplified();

      if ( code.size() && !usedCodes.contains( code ) ) {
        // Generate id

        QCryptographicHash hash( QCryptographicHash::Md5 );

        hash.addData( "Forvo source version 1.0" );
        hash.addData( code.toUtf8() );

        QString displayedCode( code.toLower() );

        if ( displayedCode.size() ) {
          displayedCode[ 0 ] = displayedCode[ 0 ].toUpper();
        }

        result.push_back(
          std::make_shared< ForvoDictionary >( hash.result().toHex().data(),
                                               QString( "Forvo (%1)" ).arg( displayedCode ).toUtf8().data(),
                                               forvo.apiKey,
                                               code,
                                               mgr ) );

        usedCodes.insert( code );
      }
    }
  }

  return result;
}

#include "forvo.moc"
} // namespace Forvo

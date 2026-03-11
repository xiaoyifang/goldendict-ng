/* This file is (c) 2008-2024 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "mediawiki.hh"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QRegularExpression>
#include <QMutexLocker>
#include <QFileInfo>
#include <QDir>
#include <algorithm>
#include <list>
#include "audiolink.hh"
#include "langcoder.hh"
#include "utils.hh"
#include "globalbroadcaster.hh"

namespace MediaWiki {

using namespace Dictionary;

namespace {

// Helper to fix URLs (protocol-relative or path-relative)
QString fixWikiUrl( const QString & url, const QUrl & baseUrl )
{
  if ( url.startsWith( "//" ) ) {
    return baseUrl.scheme() + ":" + url;
  }
  if ( url.startsWith( "/" ) ) {
    return baseUrl.scheme() + "://" + baseUrl.host() + url;
  }
  return url;
}

class MediaWikiDictionary: public Dictionary::Class
{
  string name;
  QString url, icon, lang;
  QNetworkAccessManager & netMgr;
  quint32 langId;

public:
  MediaWikiDictionary( const string & id,
                       const string & name_,
                       const QString & url_,
                       const QString & icon_,
                       const QString & lang_,
                       QNetworkAccessManager & netMgr_ ):
    Dictionary::Class( id, vector< string >() ),
    name( name_ ),
    url( url_ ),
    icon( icon_ ),
    lang( lang_ ),
    netMgr( netMgr_ ),
    langId( 0 )
  {
    int n = url.indexOf( "." );
    if ( n == 2 || ( n > 3 && url[ n - 3 ] == '/' ) ) {
      langId = LangCoder::code2toInt( url.mid( n - 2, 2 ).toLatin1().data() );
    }
  }

  string getName() noexcept override
  {
    return name;
  }
  unsigned long getArticleCount() noexcept override
  {
    return 0;
  }
  unsigned long getWordCount() noexcept override
  {
    return 0;
  }

  sptr< WordSearchRequest > prefixMatch( const std::u32string &, unsigned long maxResults ) override;
  sptr< DataRequest >
  getArticle( const std::u32string &, const vector< std::u32string > & alts, const std::u32string &, bool ) override;

  quint32 getLangFrom() const override
  {
    return langId;
  }
  quint32 getLangTo() const override
  {
    return langId;
  }

protected:
  void loadIcon() noexcept override;
};

// Parser for Table of Contents from sections array
class MediaWikiSectionsParser
{
public:
  static void generateToc( const QJsonArray & sections, QString & html )
  {
    const QString indicator = "<meta property=\"mw:PageProp/toc\" />";
    int pos                 = html.indexOf( indicator );
    if ( pos == -1 || sections.isEmpty() )
      return;

    QString toc =
      "<div id='toc' class='toc' role='navigation' aria-labelledby='mw-toc-heading'>"
      "<div class='toctitle'><h2 id='mw-toc-heading'>Contents</h2></div>\n";

    int prevLevel = 0;
    for ( const QJsonValue & val : sections ) {
      QJsonObject s = val.toObject();
      int level     = s[ "toclevel" ].toInt();
      if ( level <= 0 )
        continue;

      if ( level > prevLevel ) {
        toc += "<ul>\n";
      }
      else {
        while ( prevLevel > level ) {
          toc += "</li>\n</ul>\n";
          prevLevel--;
        }
        toc += "</li>\n";
      }

      toc += QString( "<li><a href='#%1'>%2 %3</a>" )
               .arg( s[ "linkAnchor" ].toString(), s[ "number" ].toString(), s[ "line" ].toString() );
      prevLevel = level;
    }

    while ( prevLevel-- > 0 )
      toc += "</li>\n</ul>\n";
    toc += "</div>";

    html.replace( pos, indicator.size(), toc );
  }
};

// Word Search Request using JSON API
class MediaWikiWordSearchRequest: public Dictionary::WordSearchRequest
{
  sptr< QNetworkReply > netReply;
  bool isCancelling = false;

public:
  MediaWikiWordSearchRequest( const std::u32string & word,
                              const QString & apiUrl,
                              const QString & lang,
                              QNetworkAccessManager & mgr )
  {
    QUrl url( apiUrl + "/api.php" );
    QUrlQuery query;
    query.addQueryItem( "action", "query" );
    query.addQueryItem( "list", "allpages" );
    query.addQueryItem( "aplimit", "40" );
    query.addQueryItem( "format", "json" );
    query.addQueryItem( "apprefix", QString::fromStdU32String( word ) );
    if ( !lang.isEmpty() )
      query.addQueryItem( "lang", lang );
    url.setQuery( query );

    GlobalBroadcaster::instance()->addHostWhitelist( url.host() );

    QNetworkRequest req( url );
    req.setTransferTimeout( 2000 );
    netReply = std::shared_ptr< QNetworkReply >( mgr.get( req ) );

    connect( netReply.get(), &QNetworkReply::finished, this, &MediaWikiWordSearchRequest::onFinished );
  }

  void cancel() override
  {
    isCancelling = true;
    netReply.reset();
    finish();
  }

private:
  void onFinished()
  {
    if ( isCancelling || !netReply )
      return;
    if ( netReply->error() == QNetworkReply::NoError ) {
      QJsonDocument doc = QJsonDocument::fromJson( netReply->readAll() );
      QJsonArray pages  = doc.object()[ "query" ].toObject()[ "allpages" ].toArray();
      QMutexLocker locker( &dataMutex );
      for ( const QJsonValue & p : pages ) {
        matches.emplace_back( p.toObject()[ "title" ].toString().toStdU32String() );
      }
    }
    else {
      setErrorString( netReply->errorString() );
    }
    finish();
  }
};

// Article Request using JSON API
class MediaWikiArticleRequest: public Dictionary::DataRequest
{
  Q_OBJECT
  struct ReplyHandle
  {
    QNetworkReply * reply;
    bool finished;
  };
  std::list< ReplyHandle > replies;
  QString apiUrl;
  QString lang;
  Class * dictPtr;
  QSet< long long > addedPageIds;

public:
  MediaWikiArticleRequest( const std::u32string & word,
                           const vector< std::u32string > & alts,
                           const QString & apiUrl_,
                           const QString & lang_,
                           QNetworkAccessManager & mgr,
                           Class * dictPtr_ ):
    apiUrl( apiUrl_ ),
    lang( lang_ ),
    dictPtr( dictPtr_ )
  {
    connect( &mgr,
             &QNetworkAccessManager::finished,
             this,
             &MediaWikiArticleRequest::onReplyFinished,
             Qt::QueuedConnection );
    addQuery( mgr, word );
    for ( const auto & alt : alts )
      addQuery( mgr, alt );
  }

  void cancel() override
  {
    finish();
  }

private:
  void addQuery( QNetworkAccessManager & mgr, const std::u32string & word )
  {
    QUrl url( apiUrl + "/api.php" );
    QUrlQuery query;
    query.addQueryItem( "action", "parse" );
    query.addQueryItem( "prop", "text|revid|sections" );
    query.addQueryItem( "format", "json" );
    query.addQueryItem( "redirects", "1" );
    query.addQueryItem( "page", QString::fromStdU32String( word ) );
    if ( !lang.isEmpty() )
      query.addQueryItem( "variant", lang );
    url.setQuery( query );

    QNetworkRequest req( url );
    req.setTransferTimeout( 3000 );
    replies.push_back( { mgr.get( req ), false } );
  }

  void onReplyFinished( QNetworkReply * r )
  {
    if ( isFinished() )
      return;
    auto it = std::find_if( replies.begin(), replies.end(), [ r ]( const ReplyHandle & h ) {
      return h.reply == r;
    } );
    if ( it == replies.end() )
      return;
    it->finished = true;

    while ( !replies.empty() && replies.front().finished ) {
      QNetworkReply * reply = replies.front().reply;
      if ( reply->error() == QNetworkReply::NoError ) {
        processResponse( reply->readAll() );
      }
      reply->deleteLater();
      replies.pop_front();
    }
    if ( replies.empty() )
      finish();
    else
      update();
  }

  void processResponse( const QByteArray & data )
  {
    QJsonDocument doc = QJsonDocument::fromJson( data );
    QJsonObject parse = doc.object()[ "parse" ].toObject();
    long long pageId  = parse[ "pageid" ].toVariant().toLongLong();

    if ( pageId == 0 || addedPageIds.contains( pageId ) )
      return;
    addedPageIds.insert( pageId );

    QString html = parse[ "text" ].toObject()[ "*" ].toString();
    if ( html.isEmpty() )
      return;

    QUrl base( apiUrl );

    // 1. Fix resource and link paths
    html.replace( " src=\"//", " src=\"" + base.scheme() + "://" );
    html.replace( " src=\"/", " src=\"" + base.scheme() + "://" + base.host() + "/" );
    html.replace( " href=\"//", " href=\"" + base.scheme() + "://" );
    html.replace( " url(\"//", " url(\"" + base.scheme() + "://" );

    // 2. Audio tags
    static QRegularExpression audioReg( "<audio[^>]*>.*?</audio>", QRegularExpression::DotMatchesEverythingOption );
    static QRegularExpression srcReg( "src=\"([^\"]+)\"" );
    html.replace( audioReg, [ & ]( const QRegularExpressionMatch & m ) {
      auto srcMatch = srcReg.match( m.captured() );
      if ( srcMatch.hasMatch() ) {
        QString src   = fixWikiUrl( srcMatch.captured( 1 ), base );
        string script = addAudioLink( src, dictPtr->getId() );
        return QString::fromStdString( script )
          + QString(
              "<a href=\"%1\"><img src=\"qrc:///icons/playsound.svg\" border=\"0\" align=\"absmiddle\" alt=\"Play\"/></a>" )
              .arg( src );
      }
      return m.captured();
    } );

    // 3. srcset fix
    static QRegularExpression srcsetReg( "srcset=\"([^\"]+)\"" );
    html.replace( srcsetReg, [ & ]( const QRegularExpressionMatch & m ) {
      QString srcset = m.captured( 1 );
      srcset.replace( "//", base.scheme() + "://" );
      return QString( "srcset=\"%1\"" ).arg( srcset );
    } );

    // 4. Internal links and index.php fixes
    html.replace( QRegularExpression( R"(<a\shref="(/([\w]*/)*index.php\?))" ),
                  QString( "<a href=\"%1%2" ).arg( base.scheme() + "://" + base.host(), "\\1" ) );

    html.replace( "<a href=\"/wiki/", "<a href=\"" );

    static QRegularExpression internalLinkValueReg( "<a\\s+href=\"([^/:\">#]+)" );
    html.replace( internalLinkValueReg, []( const QRegularExpressionMatch & m ) {
      return m.captured().replace( '_', ' ' );
    } );

    // Special file: fix
    html.replace(
      QRegularExpression( R"(<a\s+href="([^:/"]*file%3A[^/"]+"))", QRegularExpression::CaseInsensitiveOption ),
      QString( "<a href=\"%1/index.php?title=\\1" ).arg( apiUrl ) );

    // 5. TOC
    MediaWikiSectionsParser::generateToc( parse[ "sections" ].toArray(), html );

    // 6. Final wrapping
    QString wrapper = dictPtr->isToLanguageRTL() ? "<div class=\"mwiki\" dir=\"rtl\">" : "<div class=\"mwiki\">";
    appendString( ( wrapper + html + "</div>" ).toStdString() );
    hasAnyData = true;
  }
};

void MediaWikiDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded )
    return;
  if ( !icon.isEmpty() ) {
    QFileInfo f( QDir( Config::getConfigDir() ), icon );
    if ( f.isFile() )
      loadIconFromFilePath( f.absoluteFilePath() );
  }
  if ( dictionaryIcon.isNull() ) {
    dictionaryIcon = QIcon( url.contains( "tionary" ) ? ":/icons/wiktionary.svg" : ":/icons/icon32_wiki.png" );
  }
  dictionaryIconLoaded = true;
}

sptr< WordSearchRequest > MediaWikiDictionary::prefixMatch( const std::u32string & word, unsigned long )
{
  if ( word.size() > 80 )
    return std::make_shared< WordSearchRequestInstant >();
  return std::make_shared< MediaWikiWordSearchRequest >( word, url, lang, netMgr );
}

sptr< DataRequest > MediaWikiDictionary::getArticle( const std::u32string & word,
                                                     const vector< std::u32string > & alts,
                                                     const std::u32string &,
                                                     bool )
{
  if ( word.size() > 80 )
    return std::make_shared< DataRequestInstant >( false );
  return std::make_shared< MediaWikiArticleRequest >( word, alts, url, lang, netMgr, this );
}

} // namespace

vector< sptr< Dictionary::Class > >
makeDictionaries( Initializing &, const Config::MediaWikis & wikis, QNetworkAccessManager & mgr )
{
  vector< sptr< Dictionary::Class > > result;
  for ( const auto & wiki : wikis ) {
    if ( wiki.enabled ) {
      result.push_back( std::make_shared< MediaWikiDictionary >( wiki.id.toStdString(),
                                                                 wiki.name.toUtf8().data(),
                                                                 wiki.url,
                                                                 wiki.icon,
                                                                 wiki.lang,
                                                                 mgr ) );
    }
  }
  return result;
}

} // namespace MediaWiki

#include "mediawiki.moc"

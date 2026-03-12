/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "mediawiki.hh"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QSet>
#include <algorithm>
#include <list>
#include "audiolink.hh"
#include "langcoder.hh"
#include "utils.hh"

#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "globalbroadcaster.hh"

namespace MediaWiki {

using namespace Dictionary;

namespace {

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

class MediaWikiWordSearchRequestSlots: public Dictionary::WordSearchRequest
{
  Q_OBJECT

protected slots:

  virtual void downloadFinished() {}
};

class MediaWikiDataRequestSlots: public Dictionary::DataRequest
{
  Q_OBJECT

protected slots:

  virtual void requestFinished( QNetworkReply * ) {}
};

void MediaWikiDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded ) {
    return;
  }

  if ( !icon.isEmpty() ) {
    QFileInfo fInfo( QDir( Config::getConfigDir() ), icon );
    if ( fInfo.isFile() ) {
      loadIconFromFilePath( fInfo.absoluteFilePath() );
    }
  }
  if ( dictionaryIcon.isNull() ) {
    if ( url.contains( "tionary" ) ) {
      dictionaryIcon = QIcon( ":/icons/wiktionary.svg" );
    }
    else {
      dictionaryIcon = QIcon( ":/icons/icon32_wiki.png" );
    }
  }
  dictionaryIconLoaded = true;
}

class MediaWikiWordSearchRequest: public MediaWikiWordSearchRequestSlots
{
  sptr< QNetworkReply > netReply;
  bool isCancelling;

public:

  MediaWikiWordSearchRequest( const std::u32string &,
                              const QString & url,
                              const QString & lang,
                              QNetworkAccessManager & mgr );

  ~MediaWikiWordSearchRequest();

  void cancel() override;

private:

  void downloadFinished() override;
};

MediaWikiWordSearchRequest::MediaWikiWordSearchRequest( const std::u32string & str,
                                                        const QString & url,
                                                        const QString & lang,
                                                        QNetworkAccessManager & mgr ):
  isCancelling( false )
{
  qDebug( "wiki request begin" );
  QUrl reqUrl( url + "/api.php?action=query&list=allpages&aplimit=40&format=json" );

  GlobalBroadcaster::instance()->addHostWhitelist( reqUrl.host() );

  Utils::Url::addQueryItem( reqUrl, "apprefix", QString::fromStdU32String( str ).replace( '+', "%2B" ) );
  Utils::Url::addQueryItem( reqUrl, "lang", lang );

  QNetworkRequest req( reqUrl );
  //millseconds.
  req.setTransferTimeout( 2000 );
  netReply = std::shared_ptr< QNetworkReply >( mgr.get( req ) );

  connect( netReply.get(), SIGNAL( finished() ), this, SLOT( downloadFinished() ) );

#ifndef QT_NO_SSL

  connect( netReply.get(), SIGNAL( sslErrors( QList< QSslError > ) ), netReply.get(), SLOT( ignoreSslErrors() ) );

#endif
}

MediaWikiWordSearchRequest::~MediaWikiWordSearchRequest()
{
  qDebug( "request end" );
}

void MediaWikiWordSearchRequest::cancel()
{
  // We either finish it in place, or in the timer handler
  isCancelling = true;

  if ( netReply.get() ) {
    netReply.reset();
  }

  finish();

  qDebug( "cancel the request" );
}

void MediaWikiWordSearchRequest::downloadFinished()
{
  if ( isCancelling || isFinished() ) { // Was cancelled
    return;
  }

  if ( netReply->error() == QNetworkReply::NoError ) {
    QByteArray responseData = netReply->readAll();
    QJsonDocument jsonDoc   = QJsonDocument::fromJson( responseData );

    if ( jsonDoc.isNull() ) {
      setErrorString( tr( "JSON parse error" ) );
    }
    else {
      QJsonObject rootObj      = jsonDoc.object();
      QJsonObject queryObj     = rootObj.value( "query" ).toObject();
      QJsonArray allpagesArray = queryObj.value( "allpages" ).toArray();

      QMutexLocker _( &dataMutex );

      qDebug() << "matches" << matches.size();
      for ( const QJsonValue & value : allpagesArray ) {
        QJsonObject pageObj = value.toObject();
        QString title       = pageObj.value( "title" ).toString();
        matches.emplace_back( title.toStdU32String() );
      }
    }
    qDebug( "done." );
  }
  else {
    setErrorString( netReply->errorString() );
  }

  finish();
}

class MediaWikiSectionsParser
{
public:
  /// Since a recent Wikipedia UI redesign, the table of contents (ToC) is no longer part of an article's HTML.
  /// ToC is absent from the text node of Wikipedia's MediaWiki API reply. Quote from
  /// https://www.mediawiki.org/wiki/Reading/Web/Desktop_Improvements/Features/Table_of_contents#How_can_I_get_the_old_table_of_contents?
  /// We intentionally do not add the old table of contents to the article in addition to the new sidebar location...
  /// Users can restore the old table of contents position with the following JavaScript code:
  /// document.querySelector('mw\\3Atocplace,meta[property="mw:PageProp/toc"]').replaceWith( document.getElementById('mw-panel-toc') )
  ///
  /// This function searches for an indicator of the empty ToC in an article HTML. If the indicator is present,
  /// generates ToC HTML from the sections element and replaces the indicator with the generated ToC.


  static void generateTableOfContentsIfEmpty( const QJsonObject & parseObj, QString & articleString )
  {
    const QString emptyTocIndicator = "<meta property=\"mw:PageProp/toc\" />";
    const int emptyTocPos           = articleString.indexOf( emptyTocIndicator );
    if ( emptyTocPos == -1 ) {
      return; // The ToC must be absent or nonempty => nothing to do.
    }

    QJsonObject tocdataObj   = parseObj.value( "tocdata" ).toObject();
    QJsonArray sectionsArray = tocdataObj.value( "sections" ).toArray();
    if ( sectionsArray.isEmpty() ) {
      qWarning( "MediaWiki: empty table of contents and missing sections element." );
      return;
    }

    qDebug( "MediaWiki: generating table of contents from the sections element." );
    MediaWikiSectionsParser parser;
    parser.generateTableOfContents( sectionsArray );
    articleString.replace( emptyTocPos, emptyTocIndicator.size(), parser.tableOfContents );
  }

private:
  MediaWikiSectionsParser():
    previousLevel( 0 )
  {
  }
  void generateTableOfContents( const QJsonArray & sectionsArray );

  bool addListLevel( const QString & levelString );
  void closeListTags( int currentLevel );

  QString tableOfContents;
  int previousLevel;
};


void MediaWikiSectionsParser::generateTableOfContents( const QJsonArray & sectionsArray )
{
  // Use Wiktionary's ToC style, which had also been Wikipedia's ToC style until the UI redesign.
  // Replace double quotes with single quotes to avoid escaping " within string literals.

  if ( sectionsArray.isEmpty() ) {
    return;
  }

  tableOfContents =
    "<div id='toc' class='toc' role='navigation' aria-labelledby='mw-toc-heading'>"
    "<div class='toctitle'><h2 id='mw-toc-heading'>📑</h2></div>";

  for ( const QJsonValue & value : sectionsArray ) {
    QJsonObject sectionObj = value.toObject();

    if ( !addListLevel( sectionObj.value( "tocLevel" ).toString() ) ) {
      tableOfContents.clear();
      return;
    }

    // From https://gerrit.wikimedia.org/r/c/mediawiki/core/+/831147/
    // The anchor property ... should be used if you want to (eg) look up an element by ID using
    // document.getElementById(). The linkAnchor property ... contains additional escaping appropriate for
    // use in a URL fragment, and should be used (eg) if you are creating the href attribute of an <a> tag.
    tableOfContents += "<a href='#";
    tableOfContents += sectionObj.value( "anchor" ).toString();
    tableOfContents += "'>";

    // Omit <span class="tocnumber"> because it has no visible effect.
    tableOfContents += sectionObj.value( "number" ).toString();
    tableOfContents += ' ';
    // Omit <span class="toctext"> because it has no visible effect.
    tableOfContents += sectionObj.value( "line" ).toString();

    tableOfContents += "</a>";
  }

  closeListTags( 1 );
  // Close the first-level list tag and the toc div tag.
  tableOfContents += "</ul>\n</div>";
}

bool MediaWikiSectionsParser::addListLevel( const QString & levelString )
{
  bool convertedToInt;
  const int level = levelString.toInt( &convertedToInt );

  if ( !convertedToInt ) {
    qWarning( "MediaWiki: sections level is not an integer: %s", levelString.toUtf8().constData() );
    return false;
  }
  if ( level <= 0 ) {
    qWarning( "MediaWiki: unsupported nonpositive sections level: %s", levelString.toUtf8().constData() );
    return false;
  }
  if ( level > previousLevel + 1 ) {
    qWarning( "MediaWiki: unsupported sections level increase by more than one: from %d to %s",
              previousLevel,
              levelString.toUtf8().constData() );
    return false;
  }

  if ( level == previousLevel + 1 ) {
    // Don't close the previous list item tag to nest the current deeper level's list in it.
    tableOfContents += "\n<ul>\n";
    previousLevel = level;
  }
  else {
    closeListTags( level );
  }
  Q_ASSERT( level == previousLevel );

  // Open this list item tag.
  // Omit the (e.g.) class="toclevel-4 tocsection-9" attribute of <li> because it has no visible effect.
  tableOfContents += "<li>";

  return true;
}

void MediaWikiSectionsParser::closeListTags( int currentLevel )
{
  Q_ASSERT( currentLevel <= previousLevel );

  // Close the previous list item tag.
  tableOfContents += "</li>\n";
  // Close list and list item tags of deeper levels, if any.
  while ( currentLevel < previousLevel ) {
    tableOfContents += "</ul>\n</li>\n";
    --previousLevel;
  }
}

class MediaWikiArticleRequest: public MediaWikiDataRequestSlots
{
  using NetReplies = std::list< std::pair< QNetworkReply *, bool > >;
  NetReplies netReplies;
  QString url;
  QString lang;

public:

  MediaWikiArticleRequest( const std::u32string & word,
                           const vector< std::u32string > & alts,
                           const QString & url,
                           const QString & lang,
                           QNetworkAccessManager & mgr,
                           Class * dictPtr_ );

  void cancel() override;

private:

  void addQuery( QNetworkAccessManager & mgr, const std::u32string & word );

  void requestFinished( QNetworkReply * ) override;

  /// The page id set allows to filter out duplicate articles in case MediaWiki
  /// redirects the main word and words in the alts collection to the same page.
  QSet< long long > addedPageIds;
  Class * dictPtr;
};

void MediaWikiArticleRequest::cancel()
{
  finish();
}

MediaWikiArticleRequest::MediaWikiArticleRequest( const std::u32string & str,
                                                  const vector< std::u32string > & alts,
                                                  const QString & url_,
                                                  const QString & lang_,
                                                  QNetworkAccessManager & mgr,
                                                  Class * dictPtr_ ):
  url( url_ ),
  lang( lang_ ),
  dictPtr( dictPtr_ )
{
  connect( &mgr,
           SIGNAL( finished( QNetworkReply * ) ),
           this,
           SLOT( requestFinished( QNetworkReply * ) ),
           Qt::QueuedConnection );

  addQuery( mgr, str );

  for ( const auto & alt : alts ) {
    addQuery( mgr, alt );
  }
}

void MediaWikiArticleRequest::addQuery( QNetworkAccessManager & mgr, const std::u32string & str )
{
  qDebug( "MediaWiki: requesting article %s", QString::fromStdU32String( str ).toUtf8().data() );

  QUrl reqUrl( url + "/api.php?action=parse&prop=text|revid|tocdata&format=json&redirects" );

  Utils::Url::addQueryItem( reqUrl, "page", QString::fromStdU32String( str ).replace( '+', "%2B" ) );
  Utils::Url::addQueryItem( reqUrl, "variant", lang );
  QNetworkRequest req( reqUrl );
  //millseconds.
  req.setTransferTimeout( 3000 );
  QNetworkReply * netReply = mgr.get( req );
  connect( netReply, &QNetworkReply::errorOccurred, this, [ = ]( QNetworkReply::NetworkError e ) {
    qDebug() << "error:" << e;
  } );
#ifndef QT_NO_SSL

  connect( netReply, SIGNAL( sslErrors( QList< QSslError > ) ), netReply, SLOT( ignoreSslErrors() ) );

#endif

  netReplies.push_back( std::make_pair( netReply, false ) );
}

void MediaWikiArticleRequest::requestFinished( QNetworkReply * r )
{
  qDebug( "Finished." );

  if ( isFinished() ) { // Was cancelled
    return;
  }

  // Find this reply

  bool found = false;

  for ( auto & netReplie : netReplies ) {
    if ( netReplie.first == r ) {
      netReplie.second = true; // Mark as finished
      found            = true;
      break;
    }
  }

  if ( !found ) {
    // Well, that's not our reply, don't do anything
    return;
  }

  bool updated = false;

  for ( ; netReplies.size() && netReplies.front().second; netReplies.pop_front() ) {
    QNetworkReply * netReply = netReplies.front().first;

    if ( netReply->error() == QNetworkReply::NoError ) {
      QByteArray responseData = netReply->readAll();
      QJsonDocument jsonDoc   = QJsonDocument::fromJson( responseData );

      if ( jsonDoc.isNull() ) {
        setErrorString( tr( "JSON parse error" ) );
      }
      else {
        QJsonObject rootObj  = jsonDoc.object();
        QJsonObject parseObj = rootObj.value( "parse" ).toObject();

        long long pageId = 0;
        if ( !parseObj.isEmpty() && parseObj.value( "revid" ).toString() != "0" ) {
          pageId = parseObj.value( "pageid" ).toString().toLongLong();
        }

        if ( pageId != 0 && !addedPageIds.contains( pageId ) ) {
          addedPageIds.insert( pageId );

          QJsonObject textObj   = parseObj.value( "text" ).toObject();
          QString articleString = textObj.value( "*" ).toString();

          if ( !articleString.isEmpty() ) {

            // Replace all ":" in links, remove '#' part in links to other articles
            int pos = 0;
            QRegularExpression regLinks( "<a\\s+href=\"/([^\"]+)\"" );
            QString articleNewString;
            QRegularExpressionMatchIterator it = regLinks.globalMatch( articleString );
            while ( it.hasNext() ) {
              QRegularExpressionMatch match = it.next();
              articleNewString += articleString.mid( pos, match.capturedStart() - pos );
              pos = match.capturedEnd();

              QString link = match.captured( 1 );

              if ( link.indexOf( "://" ) >= 0 ) {
                // External link
                articleNewString += match.captured();

                continue;
              }

              if ( link.indexOf( ':' ) >= 0 ) {
                link.replace( ':', "%3A" );
              }

              int n = link.indexOf( '#', 1 );
              if ( n > 0 ) {
                QString anchor = link.mid( n + 1 ).replace( '_', "%5F" );
                link.truncate( n );
                link += QString( "?gdanchor=%1" ).arg( anchor );
              }

              QString newLink = QString( "<a href=\"/%1\"" ).arg( link );
              articleNewString += newLink;
            }
            if ( pos ) {
              articleNewString += articleString.mid( pos );
              articleString = articleNewString;
              articleNewString.clear();
            }


            QUrl wikiUrl( url );
            wikiUrl.setPath( "/" );

            // Update any special index.php pages to be absolute
            articleString.replace( QRegularExpression( R"(<a\shref="(/([\w]*/)*index.php\?))" ),
                                   QString( "<a href=\"%1\\1" ).arg( wikiUrl.toString() ) );


            // audio tag
            QRegularExpression reg1( "<audio\\s.+?</audio>",
                                     QRegularExpression::CaseInsensitiveOption
                                       | QRegularExpression::DotMatchesEverythingOption );
            QRegularExpression reg2( R"(<source\s+src="([^"]+))", QRegularExpression::CaseInsensitiveOption );
            pos = 0;
            it  = reg1.globalMatch( articleString );
            while ( it.hasNext() ) {
              QRegularExpressionMatch match = it.next();
              articleNewString += articleString.mid( pos, match.capturedStart() - pos );
              pos = match.capturedEnd();

              QString tag                    = match.captured();
              QRegularExpressionMatch match2 = reg2.match( tag );
              if ( match2.hasMatch() ) {
                QString ref = match2.captured( 1 );
                // audio url may like this <a href="//upload.wikimedia.org/wikipedia/a.ogg"
                if ( ref.startsWith( "//" ) ) {
                  ref = wikiUrl.scheme() + ":" + ref;
                }
                auto script       = addAudioLink( ref, this->dictPtr->getId() );
                QString audio_url = QString::fromStdString( script ) + "<a href=\"" + ref
                  + R"("><img src="qrc:///icons/playsound.svg" border="0" align="absmiddle" alt="Play"/></a>)";
                articleNewString += audio_url;
              }
              else {
                articleNewString += match.captured();
              }
            }
            if ( pos ) {
              articleNewString += articleString.mid( pos );
              articleString = articleNewString;
              articleNewString.clear();
            }


            // Add url scheme to image source urls
            articleString.replace( " src=\"//", " src=\"" + wikiUrl.scheme() + "://" );
            //fix src="/foo/bar/Baz.png"
            articleString.replace( "src=\"/", "src=\"" + wikiUrl.toString() );

            // Remove the /wiki/ prefix from links
            articleString.replace( "<a href=\"/wiki/", "<a href=\"" );

            // In those strings, change any underscores to spaces
            QRegularExpression rxLink( R"(<a\s+href="[^/:">#]+)" );
            it = rxLink.globalMatch( articleString );
            while ( it.hasNext() ) {
              QRegularExpressionMatch match = it.next();
              for ( int i = match.capturedStart() + 9; i < match.capturedEnd(); i++ ) {
                if ( articleString.at( i ) == QChar( '_' ) ) {
                  articleString[ i ] = ' ';
                }
              }
            }

            //fix file: url
            articleString.replace(
              QRegularExpression( R"(<a\s+href="([^:/"]*file%3A[^/"]+"))", QRegularExpression::CaseInsensitiveOption ),

              QString( "<a href=\"%1/index.php?title=\\1" ).arg( url ) );

            // Add url scheme to other urls like  "//xxx"
            articleString.replace( " href=\"//", " href=\"" + wikiUrl.scheme() + "://" );

            // Add url scheme to other urls like    embed css background: url("//upload.wikimedia.org/wikipedia/commons/6/65/Lock-green.svg")right 0.1em center/9px no-repeat
            articleString.replace( "url(\"//", "url(\"" + wikiUrl.scheme() + "://" );


            // Fix urls in "srcset" attribute
            pos = 0;
            QRegularExpression regSrcset( R"( srcset\s*=\s*"/[^"]+")" );
            it = regSrcset.globalMatch( articleString );
            while ( it.hasNext() ) {
              QRegularExpressionMatch match = it.next();
              articleNewString += articleString.mid( pos, match.capturedStart() - pos );
              pos = match.capturedEnd();

              QString srcset = match.captured();

              QString newSrcset = srcset.replace( "//", wikiUrl.scheme() + "://" );
              articleNewString += newSrcset;
            }
            if ( pos ) {
              articleNewString += articleString.mid( pos );
              articleString = articleNewString;
              articleNewString.clear();
            }


            // Insert the ToC in the end to improve performance because no replacements are needed in the generated ToC.
            MediaWikiSectionsParser::generateTableOfContentsIfEmpty( parseObj, articleString );

            articleString.prepend( dictPtr->isToLanguageRTL() ? R"(<div class="mwiki" dir="rtl">)" :
                                                                "<div class=\"mwiki\">" );
            articleString.append( "</div>" );

            appendString( articleString.toStdString() );

            hasAnyData = true;

            updated = true;
          }
        }
      }
      qDebug( "done." );
    }
    else {
      setErrorString( netReply->errorString() );
    }

    disconnect( netReply, 0, 0, 0 );
    netReply->deleteLater();
  }

  if ( netReplies.empty() ) {
    finish();
  }
  else if ( updated ) {
    update();
  }
}

sptr< WordSearchRequest > MediaWikiDictionary::prefixMatch( const std::u32string & word, unsigned long maxResults )

{
  (void)maxResults;
  if ( word.size() > 80 ) {
    // Don't make excessively large queries -- they're fruitless anyway

    return std::make_shared< WordSearchRequestInstant >();
  }
  else {
    return std::make_shared< MediaWikiWordSearchRequest >( word, url, lang, netMgr );
  }
}

sptr< DataRequest > MediaWikiDictionary::getArticle( const std::u32string & word,
                                                     const vector< std::u32string > & alts,
                                                     const std::u32string &,
                                                     bool )

{
  if ( word.size() > 80 ) {
    // Don't make excessively large queries -- they're fruitless anyway

    return std::make_shared< DataRequestInstant >( false );
  }
  else {
    return std::make_shared< MediaWikiArticleRequest >( word, alts, url, lang, netMgr, this );
  }
}

} // namespace

vector< sptr< Dictionary::Class > >
makeDictionaries( Dictionary::Initializing &, const Config::MediaWikis & wikis, QNetworkAccessManager & mgr )

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

// fixes #2272
// automoc include for Q_OBJECT should be at the very end of source code file, not inside a namespace
#include "mediawiki.moc"

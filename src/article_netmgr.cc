/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "article_netmgr.hh"
#include "globalbroadcaster.hh"
#include "utils.hh"
#include "config.hh"
#include <QNetworkAccessManager>
#include <QUrl>
#include <QWebEngineUrlRequestJob>
#include <QMimeDatabase>
#include <QMimeType>
#include <algorithm>
#include <array>
#include <stdint.h>

using std::string;

namespace {

// Extension whitelist table. Keeping these in a flat array (rather than a
// chain of ifs) makes the lookup O(table_size) with constant low overhead and,
// more importantly, keeps the cognitive complexity of the main resolver
// below SonarCloud's 25-point threshold.
struct ExtensionMime
{
  const char * ext;
  const char * mime;
};
const std::array< ExtensionMime, 17 > kExtensionMimeTable = { {
  { "html", "text/html; charset=utf-8" },
  { "htm", "text/html; charset=utf-8" },
  { "css", "text/css; charset=utf-8" },
  { "js", "text/javascript; charset=utf-8" },
  { "mjs", "text/javascript; charset=utf-8" },
  { "png", "image/png" },
  { "jpg", "image/jpeg" },
  { "jpeg", "image/jpeg" },
  { "gif", "image/gif" },
  { "svg", "image/svg+xml" },
  { "webp", "image/webp" },
  { "woff", "font/woff" },
  { "woff2", "font/woff2" },
  { "ttf", "font/ttf" },
  { "mp3", "audio/mpeg" },
  { "wav", "audio/wav" },
  { "ogg", "audio/ogg" },
} };

// Returns the whitelisted MIME type for the given file extension, or an
// empty QByteArray when the extension is not in the table.
QByteArray lookupMimeByExtension( const QString & ext )
{
  auto it =
    std::find_if( kExtensionMimeTable.begin(), kExtensionMimeTable.end(), [ &ext ]( const ExtensionMime & entry ) {
      return ext.compare( QLatin1String( entry.ext ), Qt::CaseInsensitive ) == 0;
    } );
  if ( it != kExtensionMimeTable.end() ) {
    return QByteArray( it->mime );
  }
  return {};
}

// Returns true when the payload smells like an HTML / XML document, so we
// can detect cases where the system MIME database mis-classified HTML as
// text/plain or application/octet-stream.
bool sniffHtmlContent( const QByteArray & data )
{
  const QByteArray trimmed = data.trimmed().toLower();
  return trimmed.startsWith( "<!doctype html" ) || trimmed.startsWith( "<html" ) || trimmed.startsWith( "<?xml" );
}

// Appends "; charset=utf-8" for textual MIME types so QtWebEngine renders
// pages with the correct encoding regardless of system locale defaults.
QByteArray withUtf8Charset( const QString & mimeName )
{
  QByteArray result = mimeName.toUtf8();
  if ( result.startsWith( "text/" ) || result == "application/javascript" ) {
    result.append( "; charset=utf-8" );
  }
  return result;
}

} // namespace

QByteArray getMimeTypeWithFallback( const QUrl & url, const QByteArray & data )
{
  const QString scheme = url.scheme().toLower();

  // 1. Article lookup / internal page schemes always serve HTML documents, so
  //    never subject them to dynamic MIME probing.
  if ( scheme == "gdlookup" || scheme == "bword" || scheme == "entry" || scheme == "gdinternal" ) {
    return "text/html; charset=utf-8";
  }

  // 2. Extension whitelist. This avoids depending on the system's
  //    shared-mime-info database, which is absent on some minimal Linux
  //    installations and would otherwise make these resources fall back to
  //    text/plain (and render as raw source) in QtWebEngine.
  const QString ext        = QFileInfo( url.path() ).suffix().toLower();
  const QByteArray extMime = lookupMimeByExtension( ext );
  if ( !extMime.isEmpty() ) {
    return extMime;
  }

  // 3. System MIME database (URL/extension based).
  QMimeDatabase mimeDb;
  QMimeType urlMime = mimeDb.mimeTypeForUrl( url );
  QString mimeName  = ( urlMime.isValid() && !urlMime.isDefault() ) ? urlMime.name() : QString();

  // 4. Content-based detection when the URL yielded nothing usable.
  if ( mimeName.isEmpty() && !data.isEmpty() ) {
    QMimeType dataMime = mimeDb.mimeTypeForData( data );
    if ( dataMime.isValid() && !dataMime.isDefault() ) {
      mimeName = dataMime.name();
    }
  }

  // 5. HTML content sniffing. Never let real HTML be served as text/plain or
  //    application/octet-stream, which is exactly what makes the main window
  //    render raw HTML source on misconfigured systems.
  const bool misclassifiedHtml = !data.isEmpty() && sniffHtmlContent( data )
    && ( mimeName.isEmpty() || mimeName == "application/octet-stream" || mimeName == "text/plain" );
  if ( misclassifiedHtml ) {
    return "text/html; charset=utf-8";
  }

  // 6. Final fallback. Prefer application/octet-stream over text/plain so that
  //    unrecognized HTML-ish payloads are not rendered as plain text. Keep the
  //    charset hint for textual types for consistent rendering.
  if ( mimeName.isEmpty() ) {
    return "application/octet-stream";
  }
  return withUtf8Charset( mimeName );
}


QNetworkReply * ArticleNetworkAccessManager::getArticleReply( const QNetworkRequest & req )
{
  auto op = GetOperation;

  QUrl url            = req.url();
  QMimeType mineType  = db.mimeTypeForUrl( url );
  QString contentType = mineType.name();

  if ( url.scheme() == "gdlookup" ) {
    QString path = url.path();
    if ( path.size() > 1 ) {
      url.setPath( "" );
      Utils::Url::addQueryItem( url, "word", path.mid( 1 ) );
      Utils::Url::addQueryItem( url, "group", QString::number( GlobalBroadcaster::instance()->currentGroupId ) );
    }
  }

  if ( auto dr = getResource( url, contentType ); dr.get() ) {
    return new ArticleResourceReply( this, req, dr, contentType );
  }

  // Blocking logic
  if ( !Utils::isExternalLink( url ) ) {
    qWarning( R"(Blocking element "%s" as built-in link )", url.toEncoded().data() );
    return new BlockedNetworkReply( this );
  }

  if ( disallowContentFromOtherSites && req.hasRawHeader( "Referer" ) ) {
    QUrl refererUrl = QUrl::fromEncoded( req.rawHeader( "Referer" ) );
    if ( !url.host().endsWith( refererUrl.host() )
         && Utils::Url::getHostBaseFromUrl( url ) != Utils::Url::getHostBaseFromUrl( refererUrl )
         && !url.scheme().startsWith( "data" ) ) {
      qWarning( R"(Blocking element "%s" due to not same domain)", url.toEncoded().data() );
      return new BlockedNetworkReply( this );
    }
  }

  // File scheme handling
  if ( QString fileName = url.toLocalFile();
       url.scheme() == "file" && url.host().isEmpty() && ArticleMaker::adjustFilePath( fileName ) ) {
    QUrl newUrl = QUrl::fromLocalFile( fileName );
    newUrl.setHost( newUrl.host() ); // Ensure host is set if needed
    QNetworkRequest newReq( req );
    newReq.setUrl( newUrl );
    return QNetworkAccessManager::createRequest( op, newReq, nullptr );
  }

  // Default request with spoofed User-Agent
  QNetworkRequest newReq;
  newReq.setUrl( url );
  newReq.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );

  QNetworkReply * reply = QNetworkAccessManager::createRequest( op, newReq, nullptr );

  if ( url.scheme() == "https" ) {
#ifndef QT_NO_SSL
    connect( reply, SIGNAL( sslErrors( QList< QSslError > ) ), reply, SLOT( ignoreSslErrors() ) );
#endif
  }

  return reply;
}

string ArticleNetworkAccessManager::getHtml( ResourceType resourceType )
{
  switch ( resourceType ) {
    case ResourceType::UNTITLE:
      return articleMaker.makeUntitleHtml();
    case ResourceType::WELCOME:
      return articleMaker.makeWelcomeHtml();
    case ResourceType::BLANK:
      return articleMaker.makeBlankHtml();
    default:
      return {};
  }
}

namespace {
// Helper function to handle user files access
sptr< Dictionary::DataRequest > handleUserFileRequest( const QUrl & url )
{
  QString filePath = url.path().mid( 1 ); // Get path part and remove leading slash

  // Look for the file in user's home directory
  QDir userDir     = Config::getHomeDir();
  QString fullPath = userDir.filePath( filePath );

  QFile file( fullPath );
  if ( file.open( QIODevice::ReadOnly ) ) {
    QByteArray content                         = file.readAll();
    sptr< Dictionary::DataRequestInstant > req = std::make_shared< Dictionary::DataRequestInstant >( true );
    req->getData().resize( content.size() );
    memcpy( &( req->getData().front() ), content.data(), content.size() );
    return req;
  }
  else {
    qWarning( "Failed to open user file: %s", fullPath.toUtf8().data() );
    return {};
  }
}

// Helper function to handle dictionary resource requests
sptr< Dictionary::DataRequest > handleDictionaryResource(
  const QUrl & url, const string & id, const std::vector< sptr< Dictionary::Class > > & dictionaries )
{
  for ( const auto & dictionary : dictionaries ) {
    if ( dictionary->getId() == id ) {
      if ( url.scheme() == "gico" ) {
        QByteArray bytes;
        QBuffer buffer( &bytes );
        buffer.open( QIODevice::WriteOnly );
        dictionary->getIcon().pixmap( 64 ).save( &buffer, "PNG" );
        buffer.close();
        sptr< Dictionary::DataRequestInstant > ico = std::make_shared< Dictionary::DataRequestInstant >( true );
        ico->getData().resize( bytes.size() );
        memcpy( &( ico->getData().front() ), bytes.data(), bytes.size() );
        return ico;
      }
      try {
        return dictionary->getResource( Utils::Url::path( url ).mid( 1 ).toUtf8().data() );
      }
      catch ( std::exception & e ) {
        qWarning( "getResource request error (%s) in \"%s\"", e.what(), dictionary->getName().c_str() );
        return {};
      }
    }
  }
  return {};
}
} // namespace

sptr< Dictionary::DataRequest > ArticleNetworkAccessManager::getResource( const QUrl & url, QString & contentType )
{
  qDebug() << "getResource:" << url.toString();
  const QString scheme = url.scheme();

  if ( scheme == "gdinternal" ) {
    return handleInternalScheme( url, contentType );
  }
  if ( scheme == "gdlookup" ) {
    return handleLookupScheme( url, contentType );
  }

  if ( ( scheme == "bres" || scheme == "gdau" || scheme == "gdvideo" || scheme == "gico" ) && url.path().size() ) {
    return handleResourceScheme( url, contentType );
  }

  return {};
}

sptr< Dictionary::DataRequest > ArticleNetworkAccessManager::handleInternalScheme( const QUrl & url,
                                                                                   QString & contentType )
{
  contentType = "text/html; charset=utf-8";
  if ( url.host() == "welcome-page" ) {
    return articleMaker.makeWelcomePage();
  }
  return articleMaker.makeEmptyPage();
}

sptr< Dictionary::DataRequest > ArticleNetworkAccessManager::handleLookupScheme( const QUrl & url,
                                                                                 QString & contentType )
{
  if ( !url.host().isEmpty() && url.host() != "localhost" ) {
    return std::make_shared< Dictionary::DataRequestInstant >( false );
  }

  contentType  = "text/html; charset=utf-8";
  QString word = Utils::Url::queryItemValue( url, "word" ).trimmed();

  bool groupIsValid = false;
  unsigned group    = Utils::Url::queryItemValue( url, "group" ).toUInt( &groupIsValid );

  if ( QString dictIDs = Utils::Url::queryItemValue( url, "dictionaries" ); !dictIDs.isEmpty() ) {
    return articleMaker.makeDefinitionFor( word, group, {}, {}, dictIDs.split( "," ) );
  }

  // Get muted dictionaries
  QSet< QString > mutedDicts;
  if ( QString muted = Utils::Url::queryItemValue( url, "muted" ); !muted.isEmpty() ) {
    QStringList lists = muted.split( ',' );
    mutedDicts        = QSet< QString >( lists.begin(), lists.end() );
  }
  else if ( const Config::Class * cfg = GlobalBroadcaster::instance()->getConfig() ) {
    bool isPopup              = Utils::Url::queryItemValue( url, "popup" ) == "1";
    const Config::Group * grp = cfg->getGroup( group );
    const QSet< QString > * ms = nullptr;
    if ( group == GroupId::AllGroupId ) {
      ms = isPopup ? &cfg->popupMutedDictionaries : &cfg->mutedDictionaries;
    }
    else if ( grp ) {
      ms = isPopup ? &grp->popupMutedDictionaries : &grp->mutedDictionaries;
    }

    if ( ms ) {
      mutedDicts = *ms;
    }
  }

  QMap< QString, QString > contexts = Utils::str2map( Utils::Url::queryItemValue( url, "contexts" ) );
  bool ignoreDiacritics             = Utils::Url::queryItemValue( url, "ignore_diacritics" ) == "1";

  if ( groupIsValid && !word.isEmpty() ) {
    return articleMaker.makeDefinitionFor( word, group, contexts, mutedDicts, {}, ignoreDiacritics );
  }

  return std::make_shared< Dictionary::DataRequestInstant >( false );
}

sptr< Dictionary::DataRequest > ArticleNetworkAccessManager::handleResourceScheme( const QUrl & url,
                                                                                   QString & contentType )
{
  // Use the robust resolver (extension whitelist + QMimeDatabase) instead of a
  // bare QMimeDatabase lookup, which may return text/plain on systems lacking
  // shared-mime-info. The payload is not available here, so only URL-based
  // heuristics are applied at this stage.
  contentType = QString::fromLatin1( getMimeTypeWithFallback( url ) );
  string id   = url.host().toStdString();

  // Special handling for 'user' host to access user configuration files
  if ( id == "user" && url.scheme() == "bres" ) {
    return handleUserFileRequest( url );
  }

  return handleDictionaryResource( url, id, dictionaries );
}

ArticleResourceReply::ArticleResourceReply( QObject * parent,
                                            const QNetworkRequest & netReq,
                                            const sptr< Dictionary::DataRequest > & req_,
                                            const QString & contentType ):
  QNetworkReply( parent ),
  req( req_ ),
  alreadyRead( 0 )
{
  setRequest( netReq );
  setOpenMode( ReadOnly );
  setUrl( netReq.url() );

  if ( contentType.size() ) {
    setHeader( QNetworkRequest::ContentTypeHeader, contentType );
  }

  connect( req.get(), &Dictionary::Request::updated, this, &ArticleResourceReply::reqUpdated );

  connect( req.get(), &Dictionary::Request::finished, this, &ArticleResourceReply::reqFinished );

  if ( req->isFinished() || req->dataSize() > 0 ) {
    connect( this,
             &ArticleResourceReply::readyReadSignal,
             this,
             &ArticleResourceReply::readyReadSlot,
             Qt::QueuedConnection );
    connect( this,
             &ArticleResourceReply::finishedSignal,
             this,
             &ArticleResourceReply::finishedSlot,
             Qt::QueuedConnection );

    emit readyReadSignal();

    if ( req->isFinished() ) {
      emit finishedSignal();
      qDebug( "In-place finish." );
    }
  }
}

ArticleResourceReply::~ArticleResourceReply()
{
  if ( req ) {
    req->cancel();
  }
}

void ArticleResourceReply::reqUpdated()
{
  emit readyRead();
}

void ArticleResourceReply::reqFinished()
{
  emit readyRead();
  finishedSlot();
}

qint64 ArticleResourceReply::bytesAvailable() const
{
  const qint64 avail = req->dataSize();

  if ( avail < 0 ) {
    return 0;
  }

  const qint64 availBytes = avail - alreadyRead + QNetworkReply::bytesAvailable();
  if ( availBytes == 0 && !req->isFinished() ) {
    return 10240;
  }

  return availBytes;
}


bool ArticleResourceReply::atEnd() const
{
  return req->isFinished() && bytesAvailable() == 0;
}

qint64 ArticleResourceReply::readData( char * out, qint64 maxSize )
{
  // From the doc: "This function might be called with a maxSize of 0,
  // which can be used to perform post-reading operations".
  if ( maxSize == 0 ) {
    return 0;
  }

  const bool finished = req->isFinished();

  const qint64 avail = req->dataSize();

  if ( avail < 0 ) {
    return finished ? -1 : 0;
  }

  const qint64 left = avail - alreadyRead;

  const qint64 toRead = maxSize < left ? maxSize : left;
  if ( !toRead && finished ) {
    return -1;
  }
  if ( toRead == 0 ) {
    return 0;
  }

  qDebug( "====reading  %lld of (%lld) bytes, %lld bytes readed . Finish status: %d",
          toRead,
          avail,
          alreadyRead,
          finished );

  try {
    req->getDataSlice( alreadyRead, toRead, out );
  }
  catch ( std::exception & e ) {
    qWarning( "getDataSlice error: %s", e.what() );
  }

  alreadyRead += toRead;

  if ( !toRead && finished ) {
    return -1;
  }
  else {
    return toRead;
  }
}

void ArticleResourceReply::readyReadSlot()
{
  emit readyRead();
}

void ArticleResourceReply::finishedSlot()
{
  if ( req->dataSize() < 0 ) {
    emit errorOccurred( ContentNotFoundError );
    setError( ContentNotFoundError, "content not found" );
  }
  //prevent sent multi times.
  if ( !finishSignalSent.loadAcquire() ) {
    finishSignalSent.ref();
    setFinished( true );
    emit finished();
  }
}

BlockedNetworkReply::BlockedNetworkReply( QObject * parent ):
  QNetworkReply( parent )
{
  setError( QNetworkReply::ContentOperationNotPermittedError, "Content Blocked" );

  connect( this, &BlockedNetworkReply::finishedSignal, this, &BlockedNetworkReply::finishedSlot, Qt::QueuedConnection );

  emit finishedSignal(); // This way we call readyRead()/finished() sometime later
}


void BlockedNetworkReply::finishedSlot()
{
  emit readyRead();
  setFinished( true );
  emit finished();
}

LocalSchemeHandler::LocalSchemeHandler( ArticleNetworkAccessManager & articleNetMgr, QObject * parent ):
  QWebEngineUrlSchemeHandler( parent ),
  mManager( articleNetMgr )
{
}

void LocalSchemeHandler::requestStarted( QWebEngineUrlRequestJob * requestJob )
{
  const QUrl url = requestJob->requestUrl();
  QNetworkRequest request;
  request.setUrl( url );

  // all the url reached here must be either gdlookup or bword scheme.
  if ( isInvalidLookupUrl( url ) ) {
    // Invalid lookup URL, abort the request.
    requestJob->fail( QWebEngineUrlRequestJob::RequestFailed );
    return;
  }

  QNetworkReply * reply = this->mManager.getArticleReply( request );
  // Local (HTML) schemes always serve HTML documents: pin the content type
  // explicitly so QtWebEngine never falls back to text/plain when the system
  // lacks shared-mime-info, which would render the article as raw source.
  requestJob->reply( "text/html; charset=utf-8", reply );
  connect( requestJob, &QObject::destroyed, reply, &QObject::deleteLater );
}

bool LocalSchemeHandler::isInvalidLookupUrl( const QUrl & url )
{
  auto [ schemeValid, word ] = Utils::Url::getQueryWord( url );
  // A valid lookup URL must contain a non-empty word.
  return schemeValid && word.isEmpty();
}

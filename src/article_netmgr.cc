/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "article_netmgr.hh"
#include "globalbroadcaster.hh"
#include "utils.hh"
#include <QNetworkAccessManager>
#include <QUrl>
#include <QWebEngineUrlRequestJob>
#include <stdint.h>

using std::string;

// AllowFrameReply

AllowFrameReply::AllowFrameReply( QNetworkReply * _reply ):
  baseReply( _reply )
{
  // Set base data

  setOperation( baseReply->operation() );
  setRequest( baseReply->request() );
  setUrl( baseReply->url() );

  QList< QByteArray > rawHeaders = baseReply->rawHeaderList();
  for ( const auto & header : rawHeaders ) {
    if ( header.toLower() != "x-frame-options" )
      setRawHeader( header, baseReply->rawHeader( header ) );
  }

  connect( baseReply, &QNetworkReply::errorOccurred, this, &AllowFrameReply::applyError );

  connect( baseReply, &QIODevice::readyRead, this, &QIODevice::readyRead );

  // Redirect QNetworkReply signals

  connect( baseReply, &QNetworkReply::finished, this, &QNetworkReply::finished );

  setOpenMode( QIODevice::ReadOnly );
}

qint64 AllowFrameReply::bytesAvailable() const
{
  return baseReply->bytesAvailable();
}

void AllowFrameReply::applyError( QNetworkReply::NetworkError code )
{
  setError( code, baseReply->errorString() );
  emit errorOccurred( code );
}

qint64 AllowFrameReply::readData( char * data, qint64 maxSize )
{
  auto bytesAvailable = baseReply->bytesAvailable();
  qint64 size         = qMin( maxSize, bytesAvailable );
  baseReply->read( data, size );
  return size;
}

QNetworkReply * ArticleNetworkAccessManager::getArticleReply( const QNetworkRequest & req )
{
  if ( req.url().scheme() == "qrcx" ) {
    // Do not support qrcx which is the custom define protocol.
    return new BlockedNetworkReply( this );
  }

  auto op = GetOperation;

  QUrl url            = req.url();
  QMimeType mineType  = db.mimeTypeForUrl( url );
  QString contentType = mineType.name();

  if ( req.url().scheme() == "gdlookup" ) {
    QString path = url.path();
    if ( path.size() > 1 ) {
      url.setPath( "" );

      Utils::Url::addQueryItem( url, "word", path.mid( 1 ) );
      Utils::Url::addQueryItem( url, "group", QString::number( GlobalBroadcaster::instance()->currentGroupId ) );
    }
  }

  auto dr = getResource( url, contentType );

  if ( dr.get() ) {
    return new ArticleResourceReply( this, req, dr, contentType );
  }

  //dr.get() can be null. code continue to execute.

  //can not match dictionary in the above code,means the url must be external links.
  //if not external url,can be blocked from here. no need to continue execute the following code.
  //such as bres://upload.wikimedia....  etc .
  if ( !Utils::isExternalLink( url ) ) {
    qWarning( R"(Blocking element "%s" as built-in link )", req.url().toEncoded().data() );
    return new BlockedNetworkReply( this );
  }

  // Check the Referer. If the user has opted-in to block elements from external
  // pages, we block them.

  if ( disallowContentFromOtherSites && req.hasRawHeader( "Referer" ) ) {
    QByteArray referer = req.rawHeader( "Referer" );

    QUrl refererUrl = QUrl::fromEncoded( referer );

    if ( !url.host().endsWith( refererUrl.host() )
         && Utils::Url::getHostBaseFromUrl( url ) != Utils::Url::getHostBaseFromUrl( refererUrl )
         && !url.scheme().startsWith( "data" ) ) {
      qWarning( R"(Blocking element "%s" due to not same domain)", url.toEncoded().data() );

      return new BlockedNetworkReply( this );
    }
  }

  if ( req.url().scheme() == "file" ) {
    // Check file presence and adjust path if necessary
    QString fileName = req.url().toLocalFile();
    if ( req.url().host().isEmpty() && ArticleMaker::adjustFilePath( fileName ) ) {
      QUrl newUrl( req.url() );
      QUrl localUrl = QUrl::fromLocalFile( fileName );

      newUrl.setHost( localUrl.host() );
      newUrl.setPath( Utils::Url::ensureLeadingSlash( localUrl.path() ) );

      QNetworkRequest newReq( req );
      newReq.setUrl( newUrl );

      return QNetworkAccessManager::createRequest( op, newReq, nullptr );
    }
  }

  // spoof User-Agent
  QNetworkRequest newReq;
  newReq.setUrl( url );
  newReq.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );
  if ( hideGoldenDictHeader && url.scheme().startsWith( "http", Qt::CaseInsensitive ) ) {
    newReq.setRawHeader( "User-Agent", req.rawHeader( "User-Agent" ).replace( qApp->applicationName().toUtf8(), "" ) );
  }

  QNetworkReply * reply = QNetworkAccessManager::createRequest( op, newReq, nullptr );

  if ( url.scheme() == "https" ) {
#ifndef QT_NO_SSL
    connect( reply, SIGNAL( sslErrors( QList< QSslError > ) ), reply, SLOT( ignoreSslErrors() ) );
#endif
  }

  return new AllowFrameReply( reply );
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

sptr< Dictionary::DataRequest > ArticleNetworkAccessManager::getResource( const QUrl & url, QString & contentType )
{
  qDebug() << "getResource:" << url.toString();

  if ( url.scheme() == "gdlookup" ) {
    if ( !url.host().isEmpty() && url.host() != "localhost" ) {
      // Strange request - ignore it
      return std::make_shared< Dictionary::DataRequestInstant >( false );
    }

    contentType = "text/html";

    if ( Utils::Url::queryItemValue( url, "blank" ) == "1" ) {
      return articleMaker.makeEmptyPage();
    }

    QString word = Utils::Url::queryItemValue( url, "word" ).trimmed();

    bool groupIsValid = false;
    unsigned group    = Utils::Url::queryItemValue( url, "group" ).toUInt( &groupIsValid );

    QString dictIDs = Utils::Url::queryItemValue( url, "dictionaries" );
    if ( !dictIDs.isEmpty() ) {
      // Individual dictionaries set from full-text search
      QStringList dictIDList = dictIDs.split( "," );
      return articleMaker.makeDefinitionFor( word, group, QMap< QString, QString >(), QSet< QString >(), dictIDList );
    }

    // See if we have some dictionaries muted

    QStringList mutedDictLists = Utils::Url::queryItemValue( url, "muted" ).split( ',' );
    QSet< QString > mutedDicts( mutedDictLists.begin(), mutedDictLists.end() );

    // Unpack contexts

    const QString contextsEncoded           = Utils::Url::queryItemValue( url, "contexts" );
    const QMap< QString, QString > contexts = Utils::str2map( contextsEncoded );

    // See for ignore diacritics

    bool ignoreDiacritics = Utils::Url::queryItemValue( url, "ignore_diacritics" ) == "1";

    if ( groupIsValid && !word.isEmpty() ) { // Require group and phrase to be passed
      return articleMaker.makeDefinitionFor( word, group, contexts, mutedDicts, QStringList(), ignoreDiacritics );
    }
  }

  if ( ( url.scheme() == "bres" || url.scheme() == "gdau" || url.scheme() == "gdvideo" || url.scheme() == "gico" )
       && url.path().size() ) {


    QMimeType mineType = db.mimeTypeForUrl( url );
    contentType        = mineType.name();
    string id          = url.host().toStdString();

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
  }

  return {};
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

  //all the url reached here must be either gdlookup or bword scheme.
  auto [ schemeValid, word ] = Utils::Url::getQueryWord( url );
  // or the condition can be (!queryWord.first || word.isEmpty())
  // ( queryWord.first && word.isEmpty() ) is only part of the above condition.
  if ( schemeValid && word.isEmpty() ) {
    // invalid gdlookup url.
    return;
  }

  QNetworkReply * reply = this->mManager.getArticleReply( request );
  requestJob->reply( "text/html", reply );
  connect( requestJob, &QObject::destroyed, reply, &QObject::deleteLater );
}

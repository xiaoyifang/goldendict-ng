/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include <stdint.h>
#include <QUrl>
#include "article_netmgr.hh"
#include "gddebug.hh"
#include "utils.hh"
#include <QNetworkAccessManager>
#include "globalbroadcaster.hh"

using std::string;

  // AllowFrameReply

  AllowFrameReply::AllowFrameReply( QNetworkReply * _reply ) :
    baseReply( _reply )
  {
    // Set base data

    setOperation( baseReply->operation() );
    setRequest( baseReply->request() );
    setUrl( baseReply->url() );

    // Signals to own slots

    connect( baseReply, &QNetworkReply::metaDataChanged, this, &AllowFrameReply::applyMetaData );

    connect( baseReply, &QNetworkReply::errorOccurred, this, &AllowFrameReply::applyError );

    connect( baseReply, &QIODevice::readyRead, this, &QIODevice::readyRead );

    // Redirect QNetworkReply signals

    connect( baseReply, &QNetworkReply::downloadProgress, this, &QNetworkReply::downloadProgress );

    connect( baseReply, &QNetworkReply::encrypted, this, &QNetworkReply::encrypted );

    connect( baseReply, &QNetworkReply::finished, this, &QNetworkReply::finished );

    connect( baseReply,
      &QNetworkReply::preSharedKeyAuthenticationRequired,
      this,
      &QNetworkReply::preSharedKeyAuthenticationRequired );

    connect( baseReply, &QNetworkReply::redirected, this, &QNetworkReply::redirected );

    connect( baseReply, &QNetworkReply::sslErrors, this, &QNetworkReply::sslErrors );

    connect( baseReply, &QNetworkReply::uploadProgress, this, &QNetworkReply::uploadProgress );

    // Redirect QIODevice signals

    connect( baseReply, &QIODevice::aboutToClose, this, &QIODevice::aboutToClose );

    connect( baseReply, &QIODevice::bytesWritten, this, &QIODevice::bytesWritten );

    connect( baseReply, &QIODevice::readChannelFinished, this, &QIODevice::readChannelFinished );

    setOpenMode( QIODevice::ReadOnly );
  }

  void AllowFrameReply::applyMetaData()
  {
    // Set raw headers except X-Frame-Options
    QList< QByteArray > rawHeaders = baseReply->rawHeaderList();
    for( QList< QByteArray >::iterator it = rawHeaders.begin(); it != rawHeaders.end(); ++it )
    {
      auto headerName = it->toLower();
      if( headerName != "x-frame-options" && headerName != "content-security-policy")
        setRawHeader( *it, baseReply->rawHeader( *it ) );
    }

    // Set known headers
    setHeader( QNetworkRequest::ContentDispositionHeader,
               baseReply->header( QNetworkRequest::ContentDispositionHeader ) );
    setHeader( QNetworkRequest::ContentTypeHeader,
               baseReply->header( QNetworkRequest::ContentTypeHeader ) );
    setHeader( QNetworkRequest::ContentLengthHeader,
               baseReply->header( QNetworkRequest::ContentLengthHeader ) );
    setHeader( QNetworkRequest::LocationHeader,
               baseReply->header( QNetworkRequest::LocationHeader ) );
    setHeader( QNetworkRequest::LastModifiedHeader,
               baseReply->header( QNetworkRequest::LastModifiedHeader ) );
    setHeader( QNetworkRequest::CookieHeader,
               baseReply->header( QNetworkRequest::CookieHeader ) );
    setHeader( QNetworkRequest::SetCookieHeader,
               baseReply->header( QNetworkRequest::SetCookieHeader ) );
    setHeader( QNetworkRequest::UserAgentHeader,
               baseReply->header( QNetworkRequest::UserAgentHeader ) );
    setHeader( QNetworkRequest::ServerHeader,
               baseReply->header( QNetworkRequest::ServerHeader ) );

    // Set attributes
    setAttribute( QNetworkRequest::HttpStatusCodeAttribute,
                  baseReply->attribute( QNetworkRequest::HttpStatusCodeAttribute ) );
    setAttribute( QNetworkRequest::HttpReasonPhraseAttribute,
                  baseReply->attribute( QNetworkRequest::HttpReasonPhraseAttribute ) );
    setAttribute( QNetworkRequest::RedirectionTargetAttribute,
                  baseReply->attribute( QNetworkRequest::RedirectionTargetAttribute ) );
    setAttribute( QNetworkRequest::ConnectionEncryptedAttribute,
                  baseReply->attribute( QNetworkRequest::ConnectionEncryptedAttribute ) );
    setAttribute( QNetworkRequest::SourceIsFromCacheAttribute,
                  baseReply->attribute( QNetworkRequest::SourceIsFromCacheAttribute ) );
    setAttribute( QNetworkRequest::HttpPipeliningWasUsedAttribute,
                  baseReply->attribute( QNetworkRequest::HttpPipeliningWasUsedAttribute ) );
    setAttribute( QNetworkRequest::BackgroundRequestAttribute,
                  baseReply->attribute( QNetworkRequest::BackgroundRequestAttribute ) );
    setAttribute( QNetworkRequest::Http2WasUsedAttribute,
                  baseReply->attribute( QNetworkRequest::Http2WasUsedAttribute ) );

    emit metaDataChanged();
  }

  void AllowFrameReply::setReadBufferSize( qint64 size )
  {
    QNetworkReply::setReadBufferSize( size );
    baseReply->setReadBufferSize( size );
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
    auto bytesAvailable= baseReply->bytesAvailable();
    qint64 size = qMin( maxSize, bytesAvailable );
    baseReply->read( data, size );
    return size;
  }
  void AllowFrameReply::finishedSlot()
  {
    setFinished( true );
    emit finished();
  }

QNetworkReply * ArticleNetworkAccessManager::getArticleReply( QNetworkRequest const & req )
{
  QUrl url;
  auto op = GetOperation;

  if ( req.url().scheme() == "qrcx" )
    {
    // We had to override the local load policy for the qrc URL scheme until QWebSecurityOrigin::addLocalScheme() was
    // introduced in Qt 4.6. Hence we used a custom qrcx URL scheme and redirected it here back to qrc. Qt versions
    // older than 4.6 are no longer supported, so GoldenDict itself no longer uses the qrcx scheme. However, qrcx has
    // been used for many years in our built-in article styles, and so may appear in user-defined article styles.
    // TODO: deprecate (print a warning or show a warning message box) and eventually remove support for the obsolete
    // qrcx URL scheme. A recent commit "Add support for qrc:// URL scheme" is the first one where the qrc scheme
    // works correctly. So the deprecation has to wait until older GoldenDict versions become rarely used.
      QUrl newUrl( req.url() );

      newUrl.setScheme( "qrc" );
      newUrl.setHost( "" );

      QNetworkRequest newReq( req );
      newReq.setUrl( newUrl );

      return QNetworkAccessManager::createRequest( op, newReq, nullptr );
    }

    url=req.url();
    QMimeType mineType=db.mimeTypeForUrl (url);
    QString contentType=mineType.name();

    if(req.url().scheme()=="gdlookup"){
        QString path=url.path();
        if( !path.isEmpty() )
        {
          url.setPath( "" );

          Utils::Url::addQueryItem( url, "word", path.mid( 1 ) );
          Utils::Url::addQueryItem( url, "group", QString::number(GlobalBroadcaster::instance()->currentGroupId ) );
        }
    }

    sptr< Dictionary::DataRequest > dr = getResource( url, contentType );

    if ( dr.get() )
      return new ArticleResourceReply( this, req, dr, contentType );

    //dr.get() can be null. code continue to execute.

  //can not match dictionary in the above code,means the url must be external links.
  //if not external url,can be blocked from here. no need to continue execute the following code.
  //such as bres://upload.wikimedia....  etc .
  if (!Utils::isExternalLink(url)) {
    gdWarning( "Blocking element \"%s\" as built-in link ", req.url().toEncoded().data() );
    return new BlockedNetworkReply( this );
  }

  // Check the Referer. If the user has opted-in to block elements from external
  // pages, we block them.

  if ( disallowContentFromOtherSites && req.hasRawHeader( "Referer" ) )
  {
    QByteArray referer = req.rawHeader( "Referer" );

    QUrl refererUrl = QUrl::fromEncoded( referer );

    if ( !url.host().endsWith( refererUrl.host() ) &&
        Utils::Url::getHostBaseFromUrl( url ) != Utils::Url::getHostBaseFromUrl( refererUrl ) && !url.scheme().startsWith("data") )
    {
      gdWarning( "Blocking element \"%s\" due to not same domain", url.toEncoded().data() );

      return new BlockedNetworkReply( this );
    }
  }

  if( req.url().scheme() == "file" )
  {
    // Check file presence and adjust path if necessary
    QString fileName = req.url().toLocalFile();
    if( req.url().host().isEmpty() && articleMaker.adjustFilePath( fileName ) )
    {
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
  newReq.setUrl(url);
  newReq.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );
  if ( hideGoldenDictHeader && url.scheme().startsWith("http", Qt::CaseInsensitive))
  {
    newReq.setRawHeader("User-Agent", req.rawHeader("User-Agent").replace(qApp->applicationName().toUtf8(), ""));
  }

  QNetworkReply *  reply = QNetworkAccessManager::createRequest( op, newReq, nullptr );

  if( url.scheme() == "https")
  {
#ifndef QT_NO_SSL
    connect( reply, SIGNAL( sslErrors( QList< QSslError > ) ),
             reply, SLOT( ignoreSslErrors() ) );
#endif
  }

  return  new AllowFrameReply( reply );
}

sptr< Dictionary::DataRequest > ArticleNetworkAccessManager::getResource(
  QUrl const & url, QString & contentType )
{
  qDebug() << "getResource:" << url.toString();
  qDebug() << "scheme:" << url.scheme();
  qDebug() << "host:" << url.host();

  if ( url.scheme() == "gdlookup" )
  {
    if( !url.host().isEmpty() && url.host() != "localhost" )
    {
      // Strange request - ignore it
      return std::make_shared<Dictionary::DataRequestInstant>( false );
    }

    contentType = "text/html";

    if ( Utils::Url::queryItemValue( url, "blank" ) == "1" )
      return articleMaker.makeEmptyPage();

    Config::InputPhrase phrase ( Utils::Url::queryItemValue( url, "word" ).trimmed(),
                                 Utils::Url::queryItemValue( url, "punctuation_suffix" ) );

    bool groupIsValid = false;
    unsigned group = Utils::Url::queryItemValue( url, "group" ).toUInt( &groupIsValid );
   
    QString dictIDs = Utils::Url::queryItemValue( url, "dictionaries" );
    if( !dictIDs.isEmpty() )
    {
      // Individual dictionaries set from full-text search
      QStringList dictIDList = dictIDs.split( "," );
      return articleMaker.makeDefinitionFor( phrase, group, QMap< QString, QString >(), QSet< QString >(), dictIDList );
    }

    // See if we have some dictionaries muted

    QStringList mutedDictLists=Utils::Url::queryItemValue( url, "muted" ).split( ',' );
    QSet< QString > mutedDicts ( mutedDictLists.begin(),mutedDictLists.end());

    // Unpack contexts

    QMap< QString, QString > contexts;

    QString contextsEncoded = Utils::Url::queryItemValue( url, "contexts" );

    if ( contextsEncoded.size() )
    {
      QByteArray ba = QByteArray::fromBase64( contextsEncoded.toLatin1() );

      QBuffer buf( & ba );

      buf.open( QBuffer::ReadOnly );

      QDataStream stream( &buf );

      stream >> contexts;
    }

    // See for ignore diacritics

    bool ignoreDiacritics = Utils::Url::queryItemValue( url, "ignore_diacritics" ) == "1";

    if ( groupIsValid && phrase.isValid() ) // Require group and phrase to be passed
      return articleMaker.makeDefinitionFor( phrase, group, contexts, mutedDicts, QStringList(), ignoreDiacritics );
  }

  if ( ( url.scheme() == "bres" || url.scheme() == "gdau" || url.scheme() == "gdvideo" || url.scheme() == "gico" ) &&
       url.path().size() )
  {
    //GD_DPRINTF( "Get %s\n", req.url().host().toLocal8Bit().data() );
    //GD_DPRINTF( "Get %s\n", req.url().path().toLocal8Bit().data() );

    string id = url.host().toStdString();

    bool search = ( id == "search" );

    if ( !search )
    {
      for( unsigned x = 0; x < dictionaries.size(); ++x )
        if ( dictionaries[ x ]->getId() == id )
        {
            if( url.scheme() == "gico" )
            {
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                dictionaries[ x ]->getIcon().pixmap( 64 ).save(&buffer, "PNG");
                buffer.close();
                sptr< Dictionary::DataRequestInstant > ico = std::make_shared<Dictionary::DataRequestInstant>( true );
                ico->getData().resize( bytes.size() );
                memcpy( &( ico->getData().front() ), bytes.data(), bytes.size() );
                return ico;
            }
            try
            {
              return  dictionaries[ x ]->getResource( Utils::Url::path( url ).mid( 1 ).toUtf8().data() );
            }
            catch( std::exception & e )
            {
              gdWarning( "getResource request error (%s) in \"%s\"\n", e.what(),
                         dictionaries[ x ]->getName().c_str() );
              return sptr< Dictionary::DataRequest >();
            }
        }
    }

  }

  if ( url.scheme() == "gdpicture" )
  {
    contentType = "text/html";
    QUrl imgUrl ( url );
    imgUrl.setScheme( "bres" );
    return articleMaker.makePicturePage( imgUrl.toEncoded().data() );
  }

  return sptr< Dictionary::DataRequest >();
}

ArticleResourceReply::ArticleResourceReply( QObject * parent,
  QNetworkRequest const & netReq,
  sptr< Dictionary::DataRequest > const & req_,
  QString const & contentType ):
  QNetworkReply( parent ), req( req_ ), alreadyRead( 0 )
{
  setRequest( netReq );
  setOpenMode( ReadOnly );
  setUrl(netReq.url());

  if ( contentType.size() )
    setHeader( QNetworkRequest::ContentTypeHeader, contentType );

  connect( req.get(), &Dictionary::Request::updated, this, &ArticleResourceReply::reqUpdated );

  connect( req.get(), &Dictionary::Request::finished, this, &ArticleResourceReply::reqFinished );

  if ( req->isFinished() || req->dataSize() > 0 )
  {
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

    if ( req->isFinished() )
    {
      emit finishedSignal();
      GD_DPRINTF( "In-place finish.\n" );
    }
  }
}

ArticleResourceReply::~ArticleResourceReply()
{
  req->cancel();
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
  qint64 avail = req->dataSize();
  
  if ( avail < 0 )
    return 0;
  
  return avail - alreadyRead + QNetworkReply::bytesAvailable();
}

bool ArticleResourceReply::atEnd() const
{
  // QWebEngineUrlRequestJob finishes and is destroyed as soon as QIODevice::atEnd() returns true.
  // QNetworkReply::atEnd() returns true while bytesAvailable() returns 0.
  // Return false if the data request is not finished to work around always-blank web page.
  return req->isFinished() && QNetworkReply::atEnd();
}

qint64 ArticleResourceReply::readData( char * out, qint64 maxSize )
{
  // From the doc: "This function might be called with a maxSize of 0,
  // which can be used to perform post-reading operations".
  if ( maxSize == 0 )
    return 0;

  bool finished = req->isFinished();
  
  qint64 avail = req->dataSize();

  if ( avail < 0 )
    return finished ? -1 : 0;

  qint64 left = avail - alreadyRead;
  
  if(  left == 0 && !finished )
  {
    // Work around endlessly repeated useless calls to readData(). The sleep duration is a tradeoff.
    // On the one hand, lowering the duration reduces CPU usage. On the other hand, overly long
    // sleep duration reduces page content update frequency in the web view.
    // Waiting on a condition variable is more complex and actually works worse than
    // simple fixed-duration sleeping, because the web view is not updated until
    // the data request is finished if readData() returns only when new data arrives.
    QThread::msleep( 30 );
    return 0;
  }

  qint64 toRead = maxSize < left ? maxSize : left;
  GD_DPRINTF( "====reading  %d of (%d) bytes . Finished: %d", (int)toRead, avail, finished );

  try
  {
    req->getDataSlice( alreadyRead, toRead, out );
  }
  catch( std::exception & e )
  {
    qWarning( "getDataSlice error: %s", e.what() );
  }

  alreadyRead += toRead;

  if ( !toRead && finished )
    return -1;
  else
    return toRead;
}

void ArticleResourceReply::readyReadSlot()
{
  emit readyRead();
}

void ArticleResourceReply::finishedSlot()
{
  if (req->dataSize() < 0) {
    emit errorOccurred(ContentNotFoundError);
    setError(ContentNotFoundError, "content not found");
  }
  //prevent sent multi times.
  if (!finishSignalSent.loadAcquire())
  {
    finishSignalSent.ref();
    setFinished( true );
    emit finished();
  }
}

BlockedNetworkReply::BlockedNetworkReply( QObject * parent ): QNetworkReply( parent )
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

LocalSchemeHandler::LocalSchemeHandler(ArticleNetworkAccessManager& articleNetMgr, QObject *parent):
    QWebEngineUrlSchemeHandler(parent),
    mManager(articleNetMgr){

}

void LocalSchemeHandler::requestStarted(QWebEngineUrlRequestJob *requestJob)
{
  QUrl url = requestJob->requestUrl();
  QNetworkRequest request;
  request.setUrl( url );

  //all the url reached here must be either gdlookup or bword scheme.
  auto [valid, word] = Utils::Url::getQueryWord( url );
  // or the condition can be (!queryWord.first || word.isEmpty())
  // ( queryWord.first && word.isEmpty() ) is only part of the above condition.
  if( valid && word.isEmpty() )
  {
    // invalid gdlookup url.
    return;
  }

  QNetworkReply * reply = this->mManager.getArticleReply( request );
  requestJob->reply( "text/html", reply );
  connect( requestJob, &QObject::destroyed, reply, &QObject::deleteLater );
}

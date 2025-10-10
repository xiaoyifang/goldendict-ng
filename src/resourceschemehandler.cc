#include "resourceschemehandler.hh"
#include <QWebEngineUrlRequestJob>

ResourceSchemeHandler::ResourceSchemeHandler( ArticleNetworkAccessManager & articleNetMgr, QObject * parent ):
  QWebEngineUrlSchemeHandler( parent ),
  mManager( articleNetMgr )
{
}
void ResourceSchemeHandler::requestStarted( QWebEngineUrlRequestJob * requestJob )
{
  const QUrl url = requestJob->requestUrl();
  QString content_type;
  const QMimeType mineType                    = db.mimeTypeForUrl( url );
  const sptr< ResourceRequest > reply         = this->mManager.getResource( url, content_type );
  content_type                                = mineType.name();

  if ( reply == nullptr ) {
    qDebug() << "Resource failed to load: " << url.toString();
    requestJob->fail( QWebEngineUrlRequestJob::RequestFailed );
  }
  else if ( reply->isFinished() ) {
    replyJob( reply, requestJob, content_type );
  }
  else {
    connect( reply.get(), &ResourceRequest::finished, requestJob, [ = ]() {
      replyJob( reply, requestJob, content_type );
    } );
  }
}


void ResourceSchemeHandler::replyJob( sptr< ResourceRequest > reply,
                                      QWebEngineUrlRequestJob * requestJob,
                                      QString content_type )
{
  if ( !reply.get() ) {
    requestJob->fail( QWebEngineUrlRequestJob::UrlNotFound );
    return;
  }
  const auto & data = reply->getFullData();
  if ( data.empty() ) {
    requestJob->fail( QWebEngineUrlRequestJob::UrlNotFound );
    return;
  }
  QByteArray * ba  = new QByteArray( data.data(), data.size() );
  QBuffer * buffer = new QBuffer( ba );
  buffer->open( QBuffer::ReadOnly );
  buffer->seek( 0 );

  // Reply segment
  requestJob->reply( content_type.toLatin1(), buffer );

  connect( requestJob, &QObject::destroyed, buffer, [ = ]() {
    buffer->close();
    ba->clear();
    delete ba;
    buffer->deleteLater();
  } );
}

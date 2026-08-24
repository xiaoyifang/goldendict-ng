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
  // The content type is resolved in replyJob, where the actual payload is
  // available and can be sniffed. The out-parameter below is therefore unused.
  QString content_type;
  const sptr< Dictionary::DataRequest > reply = this->mManager.getResource( url, content_type );

  if ( reply == nullptr ) {
    qDebug() << "Resource failed to load: " << url.toString();
    requestJob->fail( QWebEngineUrlRequestJob::RequestFailed );
  }
  else if ( reply->isFinished() ) {
    replyJob( reply, requestJob );
  }
  else {
    connect( reply.get(), &Dictionary::DataRequest::finished, requestJob, [ = ]() {
      replyJob( reply, requestJob );
    } );
  }
}


void ResourceSchemeHandler::replyJob( sptr< Dictionary::DataRequest > reply,
                                      QWebEngineUrlRequestJob * requestJob )
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

  // Resolve the content type robustly: extension whitelist first (independent
  // of the system's shared-mime-info), then QMimeDatabase, then sniff the
  // payload so HTML is never misreported as text/plain on misconfigured
  // systems. This is where the full data is available, so data sniffing works.
  const QByteArray contentType =
    getMimeTypeWithFallback( requestJob->requestUrl(), QByteArray( data.data(), data.size() ) );

  QByteArray * ba  = new QByteArray( data.data(), data.size() );
  QBuffer * buffer = new QBuffer( ba );
  buffer->open( QBuffer::ReadOnly );
  buffer->seek( 0 );

  // Reply segment
  requestJob->reply( contentType, buffer );

  connect( requestJob, &QObject::destroyed, buffer, [ = ]() {
    buffer->close();
    ba->clear();
    delete ba;
    buffer->deleteLater();
  } );
}

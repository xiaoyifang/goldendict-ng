#include "webmultimediadownload.hh"

namespace Dictionary {

#define MAX_REDIRECTS 10

WebMultimediaDownload::WebMultimediaDownload( QUrl const & url, QNetworkAccessManager & _mgr ):
  mgr( _mgr ),
  redirectCount( 0 )
{
  connect( &mgr, &QNetworkAccessManager::finished, this, &WebMultimediaDownload::replyFinished, Qt::QueuedConnection );

  reply = mgr.get( QNetworkRequest( url ) );

#ifndef QT_NO_SSL
  connect( reply, SIGNAL( sslErrors( QList< QSslError > ) ), reply, SLOT( ignoreSslErrors() ) );
#endif
}

void WebMultimediaDownload::cancel()
{
  reply = NULL;

  finish();
}

void WebMultimediaDownload::replyFinished( QNetworkReply * r )
{
  if ( !r || r != reply ) {
    return; // Not our reply
  }

  if ( r->error() == QNetworkReply::NoError ) {
    // Check for redirect reply

    QVariant possibleRedirectUrl = reply->attribute( QNetworkRequest::RedirectionTargetAttribute );
    QUrl redirectUrl             = possibleRedirectUrl.toUrl();
    if ( !redirectUrl.isEmpty() ) {
      disconnect( reply, 0, 0, 0 );
      reply->deleteLater();

      if ( ++redirectCount > MAX_REDIRECTS ) {
        setErrorString( "Too many redirects detected" );
        finish();
        return;
      }

      reply = mgr.get( QNetworkRequest( redirectUrl ) );
#ifndef QT_NO_SSL
      connect( reply, SIGNAL( sslErrors( QList< QSslError > ) ), reply, SLOT( ignoreSslErrors() ) );
#endif
      return;
    }

    // Handle reply data

    QMutexLocker _( &dataMutex );

    data.resize( r->bytesAvailable() );

    r->read( data.data(), data.size() );

    hasAnyData = true;
  }
  else {
    setErrorString( r->errorString() );
  }

  disconnect( r, 0, 0, 0 );
  r->deleteLater();
  reply = NULL;

  finish();
}

} // namespace Dictionary

#ifdef __APPLE__
  #include "mac_url_handler.hh"
  #include "utils.hh"
  #include <QDebug>

void MacUrlHandler::processURL( const QUrl & url )
{
  qDebug() << "External URL received: " << url;
  
  if ( url.scheme() == "goldendict" ) {
    QString word = url.authority();
    
    if ( word.isEmpty() && !url.path().isEmpty() ) {
      word = url.path().remove( 0, 1 );
    }
    
    word = Utils::Url::decodeUrlEncodedWord( word );
    
    if ( !word.isEmpty() ) {
      emit wordReceived( QStringLiteral( "action:translate|word:" ) + word );
    }
  }
}
#endif

#ifdef __APPLE__
  #include "mac_url_handler.hh"
  #include <QDebug>

void MacUrlHandler::processURL( const QUrl & url )
{
  qDebug() << "External URL received: " << url;
  emit wordReceived( QStringLiteral( "translateWord: " )
                     + QUrl::fromAce( url.authority().toLatin1(), QUrl::IgnoreIDNWhitelist ) );
}
#endif

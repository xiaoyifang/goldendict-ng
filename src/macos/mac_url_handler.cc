#ifdef __APPLE__
  #include "mac_url_handler.hh"
  #include "utils.hh"
  #include <QDebug>

void MacUrlHandler::processURL( const QUrl & url )
{
  qDebug() << "External URL received: " << url;

  if ( url.scheme() == "goldendict" ) {
    QString word = Utils::Url::extractWordFromUrl( url );

    if ( word.isEmpty() ) {
      word = Utils::Url::extractWordFromUrl( url.toString() );
    }

    if ( !word.isEmpty() ) {
      QString query   = url.query();
      QString message = QStringLiteral( "action:translate" );

      if ( !query.isEmpty() ) {
        QUrlQuery urlQuery( query );
        QString targetParam = urlQuery.queryItemValue( "target" );

        if ( targetParam == "popup" ) {
          message += "|window:popup";
        }
        else if ( targetParam == "main" ) {
          message += "|window:main";
        }
      }

      QString encodedWord = QUrl::toPercentEncoding( word );
      message += "|word:" + encodedWord;
      emit wordReceived( message );
    }
  }
}
#endif

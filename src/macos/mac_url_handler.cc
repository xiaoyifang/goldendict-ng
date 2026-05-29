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
      if ( url.path().startsWith( "/" ) ) {
        word = url.path().mid( 1 );
      }
      else {
        word = url.path();
      }
    }

    // Remove trailing slash if present
    if ( word.endsWith( "/" ) ) {
      word.chop( 1 );
    }

    word = Utils::Url::decodeUrlEncodedWord( word );

    if ( !word.isEmpty() ) {
      // Parse query parameters to determine target window
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

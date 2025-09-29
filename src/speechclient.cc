#ifdef TTS_SUPPORT

  #include "speechclient.hh"

  #include <QtCore>
  #include <QLocale>
  #include <QDebug>
SpeechClient::SpeechClient( const Config::VoiceEngine & e, QObject * parent ):
  QObject( parent ),
  internalData( new InternalData( e ) )
{
}


SpeechClient::Engines SpeechClient::availableEngines()
{
  Engines engines;
  const auto innerEngines = QTextToSpeech::availableEngines();

  for ( const auto & engine_name : innerEngines ) {
    const auto sp = new QTextToSpeech( engine_name );

  #if ( QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 ) )
    if ( sp->state() == QTextToSpeech::Error )
      continue;
  #else
    if ( !sp || sp->state() == QTextToSpeech::BackendError )
      continue;
  #endif

    qDebug() << engine_name << sp->state();

    //    const QList< QLocale > locales = sp->availableLocales();
    //    for ( const QLocale & locale : locales )
    {
      QLocale locale;
      //on some platforms ,change the locale will change voices too.
      sp->setLocale( locale );
      for ( const QVoice & voice : sp->availableVoices() ) {
        const QString name( QString( "%4 - %3 %1 (%2)" )
                              .arg( QLocale::languageToString( locale.language() ),
                                    ( QLocale::territoryToString( locale.territory() ) ),
                                    voice.name(),
                                    engine_name ) );
        Engine engine( Config::VoiceEngine( engine_name, name, voice.name(), QLocale( locale ), 50, 0 ) );
        engines.push_back( engine );
      }

      sp->deleteLater();
    }
  }

  return engines;
}

bool SpeechClient::tell( const QString & text, int volume, int rate ) const
{
  if ( !internalData || !internalData->sp || internalData->sp->state() != QTextToSpeech::Ready )
    return false;

  internalData->sp->setVolume( volume / 100.0 );
  internalData->sp->setRate( rate / 10.0 );

  internalData->sp->say( text );
  return true;
}

bool SpeechClient::tell( const QString & text ) const
{
  return tell( text, internalData->engine.volume, internalData->engine.rate );
}

#endif

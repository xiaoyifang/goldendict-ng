#include "speechclient.hh"

#include <QtCore>
SpeechClient::SpeechClient( Config::VoiceEngine const & e, QObject * parent ):
  QObject( parent ),
  internalData( std::make_unique< InternalData >( e ) )
{
}


SpeechClient::Engines SpeechClient::availableEngines()
{
  Engines engines;
  const auto innerEngines = QTextToSpeech::availableEngines();

  for ( const auto & engine_name : innerEngines ) {
    const auto sp = new QTextToSpeech( engine_name == "default" ? nullptr : engine_name );

    #if ( QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 ) )
    if ( !sp || sp->state() == QTextToSpeech::Error )
      continue;
    #else
      if ( !sp || sp->state() == QTextToSpeech::BackendError )
        continue;
    #endif

    qDebug() << engine_name << sp->state();

    const QVector< QLocale > locales = sp->availableLocales();
    for ( const QLocale & locale : locales ) {
      //on some platforms ,change the locale will change voices too.
      sp->setLocale( locale );
      for ( const QVoice & voice : sp->availableVoices() ) {
        const QString name( QString( "%4 - %3 %1 (%2)" )
          .arg( QLocale::languageToString( locale.language() ),
                ( QLocale::countryToString( locale.country() ) ),
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

bool SpeechClient::tell( QString const & text, int volume, int rate ) const
{
  if( internalData->sp->state() != QTextToSpeech::Ready )
    return false;

  internalData->sp->setVolume( volume / 100.0 );
  internalData->sp->setRate( rate / 10.0 );

  internalData->sp->say( text );
  return true;
}

bool SpeechClient::tell( QString const & text ) const
{
  return tell(text, internalData->engine.volume, internalData->engine.rate);
}

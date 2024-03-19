#pragma once
#ifndef NO_TTS_SUPPORT

#include <QObject>
#include "config.hh"
#include <QTextToSpeech>
#include <memory>
#include <QDebug>
#include <QSharedPointer>

class SpeechClient: public QObject
{
  Q_OBJECT

public:

  struct Engine
  {
    //engine name
    QString engine_name;
    QString name;
    //voice name
    QString voice_name;
    QString locale;
    // Volume vary from 0~1 and rate vary from -1 to 1
    int volume;
    int rate;
    explicit Engine( Config::VoiceEngine const & e ):
      engine_name( e.engine_name ),
      name( e.name ),
      voice_name( e.voice_name ),
      locale( e.locale.name() ),
      volume( e.volume ),
      rate( e.rate )
    {
    }
  };

  struct InternalData
  {
    explicit InternalData( Config::VoiceEngine const & e ):
      sp( new QTextToSpeech( e.engine_name ) ),
      engine( e )
    {
      qDebug() << QStringLiteral( "initialize tts" ) << e.engine_name;
#if ( QT_VERSION >= QT_VERSION_CHECK( 6, 0, 0 ) )
      if ( !sp || sp->state() == QTextToSpeech::Error )
        return;
#else
      if ( !sp || sp->state() == QTextToSpeech::BackendError )
        return;
#endif
      sp->setLocale( e.locale );
      auto voices = sp->availableVoices();
      for ( const auto & voice : voices ) {
        if ( voice.name() == e.voice_name ) {
          sp->setVoice( voice );

          break;
        }
      }

      sp->setVolume( e.volume / 100.0 );
      sp->setRate( e.rate / 10.0 );
    }

    QSharedPointer< QTextToSpeech > sp;
    Engine engine;
  };

  using Engines = QList< Engine >;

  explicit SpeechClient( Config::VoiceEngine const & e, QObject * parent = nullptr );

  static Engines availableEngines();

  bool tell( QString const & text, int volume, int rate ) const;
  bool tell( QString const & text ) const;

private:
  QSharedPointer< InternalData > internalData;
};

#endif

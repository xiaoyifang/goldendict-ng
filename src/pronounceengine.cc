#include "pronounceengine.hh"
#include <QMutexLocker>
#include "common/utils.hh"
#include <QUrl>

PronounceEngine::PronounceEngine( QObject * parent ):
  QObject{ parent }
{
}


void PronounceEngine::reset()
{
  QMutexLocker _( &mutex );
  state = PronounceState::AVAILABLE;

  dictAudioMap.clear();
}


void PronounceEngine::sendAudio( std::string dictId, QString audioLink )
{
  if ( state == PronounceState::OCCUPIED )
    return;

  if(!Utils::Url::isAudioUrl(QUrl(audioLink)))
    return;

  QMutexLocker _( &mutex );

  dictAudioMap.operator[]( dictId ).push_back( audioLink );
}

void PronounceEngine::finishDictionary( std::string dictId )
{
  if ( state == PronounceState::OCCUPIED )
    return;

  if ( dictAudioMap.contains( dictId ) ) {
    {
      //limit the mutex scope.
      QMutexLocker _( &mutex );
      if ( state == PronounceState::OCCUPIED )
        return;
      state = PronounceState::OCCUPIED;
    }
    emit emitAudio( dictAudioMap[ dictId ].first() );
  }
}

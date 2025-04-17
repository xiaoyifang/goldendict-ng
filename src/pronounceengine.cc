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
  firstAudioLink.clear();
  dictAudioMap.clear();
}

void PronounceEngine::getFirstAudioLink()
{
  return firstAudioLink;
}


void PronounceEngine::sendAudio( const std::string & dictId, const QString & audioLink )
{
  if ( state == PronounceState::OCCUPIED ) {
    return;
  }

  if ( !Utils::Url::isAudioUrl( QUrl( audioLink ) ) ) {
    return;
  }

  QMutexLocker _( &mutex );

  if ( firstAudioLink.isEmpty() ) {
    firstAudioLink = audioLink;
  }

  dictAudioMap[ dictId ].append( audioLink );
}

void PronounceEngine::finishDictionary( std::string dictId )
{
  if ( state == PronounceState::OCCUPIED ) {
    return;
  }

  if ( dictAudioMap.contains( dictId ) ) {
    {
      //limit the mutex scope.
      QMutexLocker _( &mutex );
      if ( state == PronounceState::OCCUPIED ) {
        return;
      }
      state = PronounceState::OCCUPIED;
    }
    auto link = dictAudioMap[ dictId ].first();
    emit emitAudio( link );
  }
}

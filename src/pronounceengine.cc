#include "pronounceengine.hh"

PronounceEngine::PronounceEngine(QObject *parent)
  : QObject{parent}
{

}


void PronounceEngine::reset()
{
  QMutexLocker _( &mutex );
  state = PronounceState::AVAILABLE;
}

void PronounceEngine::sendAudio( QString audioLink )
{
  QMutexLocker _( &mutex );
  if ( state == PronounceState::OCCUPIED )
    return;
  state = PronounceState::OCCUPIED;
  emit emitAudio( audioLink );
}

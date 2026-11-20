/* This file is (c) 2018 Igor Kushnir <igorkuo@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#ifdef MAKE_QTMULTIMEDIA_PLAYER

  #include "audioplayerinterface.hh"
  #include <QScopedPointer>
  #include <QTimer>

class QAudioOutput;
class QBuffer;
class QMediaDevices;
class QMediaPlayer;

class MultimediaAudioPlayer: public AudioPlayerInterface
{
  Q_OBJECT

public:
  MultimediaAudioPlayer();
  ~MultimediaAudioPlayer() override;

  QString play( const char * data, int size ) override;
  void stop() override;

private slots:
  void onMediaPlayerError();
  void audioOutputChange();

private:
  /// The Qt Multimedia objects are created on demand and destroyed as soon as
  /// playback stops. Keeping them alive while idle makes Windows treat the
  /// audio device as in use, which prevents the system from sleeping
  /// (see issue #2096).
  void createPlayer();
  void destroyPlayer();

  QScopedPointer< QBuffer > audioBuffer;
  QScopedPointer< QMediaPlayer > player; ///< Depends on audioBuffer.
  QScopedPointer< QAudioOutput > audioOutput;
  QScopedPointer< QMediaDevices > mediaDevices;

  /// Defers teardown to the next event loop iteration so that the player is
  /// never destroyed inside one of its own signal handlers.
  QTimer releaseTimer;
};

#endif // MAKE_QTMULTIMEDIA_PLAYER

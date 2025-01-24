/* This file is (c) 2018 Igor Kushnir <igorkuo@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#ifdef MAKE_QTMULTIMEDIA_PLAYER

  #include "audioplayerinterface.hh"
  #include <QAudioOutput>
  #include <QBuffer>
  #include <QMediaDevices>
  #include <QMediaPlayer>
  #include <QPointer>

class MultimediaAudioPlayer: public AudioPlayerInterface
{
  Q_OBJECT

public:
  MultimediaAudioPlayer();

  virtual QString play( const char * data, int size );
  virtual void stop();

private slots:
  void onMediaPlayerError();
  void audioOutputChange();


private:
  QPointer< QBuffer > audioBuffer;
  QMediaPlayer player; ///< Depends on audioBuffer.
  QAudioOutput audioOutput;
  QMediaDevices mediaDevices;
};

#endif // MAKE_QTMULTIMEDIA_PLAYER

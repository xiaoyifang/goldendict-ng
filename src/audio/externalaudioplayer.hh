/* This file is (c) 2018 Igor Kushnir <igorkuo@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#pragma once

#include "audioplayerinterface.hh"
#include <memory>

class ExternalViewer;

class ExternalAudioPlayer: public AudioPlayerInterface
{
  Q_OBJECT

public:
  ExternalAudioPlayer();
  ~ExternalAudioPlayer();
  /// \param playerCommandLine_ Will be used in future play() calls.
  void setPlayerCommandLine( const QString & playerCommandLine_ );

  virtual QString play( const char * data, int size );
  virtual void stop();

private slots:
  void onViewerDestroyed( QObject * destroyedViewer );

private:
  QString startViewer();

  QString playerCommandLine;
  ExternalViewer * exitingViewer; ///< If not null: points to the previous viewer,
                                  ///< the current viewer (if any) is not started yet
                                  ///< and waits for exitingViewer to be destroyed first.

  struct QObjectDeleteLater
  {
    void operator()( QObject * p )
    {
      if ( p )
        p->deleteLater();
    }
  };
  // deleteLater() is safer because viewer actively participates in the QEventLoop.
  std::unique_ptr< ExternalViewer, QObjectDeleteLater > viewer;
};

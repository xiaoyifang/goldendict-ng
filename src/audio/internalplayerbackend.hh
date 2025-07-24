#pragma once
#include "audioplayerinterface.hh"
#include "ffmpegaudioplayer.hh"
#include <QStringList>

/// Overly engineered dummy/helper/wrapper "backend", which is not, to manage backends.
class InternalPlayerBackend
{
public:
  /// Returns true if at least one backend is available.
  static bool anyAvailable();
  AudioPlayerInterface * getActualPlayer();
  /// Returns the name list of supported backends.
  /// The first one willl be the default one
  static QStringList availableBackends();

  const QString & getName() const
  {
    return name;
  }

  void setName( const QString & name_ )
  {
    name = name_;
  }

  bool operator==( const InternalPlayerBackend & other ) const
  {
    return name == other.name;
  }

  bool operator!=( const InternalPlayerBackend & other ) const
  {
    return !operator==( other );
  }

private:
  QString name;
};

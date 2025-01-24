#pragma once
#include "audioplayerinterface.hh"
#include "ffmpegaudioplayer.hh"
#include "multimediaaudioplayer.hh"
#include <QScopedPointer>
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

  QString const & getName() const
  {
    return name;
  }

  void setName( QString const & name_ )
  {
    name = name_;
  }

  bool operator==( InternalPlayerBackend const & other ) const
  {
    return name == other.name;
  }

  bool operator!=( InternalPlayerBackend const & other ) const
  {
    return !operator==( other );
  }

private:
  QString name;
};

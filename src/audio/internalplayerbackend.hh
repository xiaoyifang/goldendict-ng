#pragma once
#include <QStringList>

/// Overly engineered dummy/helper/wrapper "backend", which is not, to manage backends.
class InternalPlayerBackend
{
public:
  /// Returns true if at least one backend is available.
  static bool anyAvailable();
  /// Returns the default backend or null backend if none is available.
  static InternalPlayerBackend defaultBackend();
  /// Returns the name list of supported backends.
  static QStringList nameList();

  /// Returns true if built with FFmpeg player support and the name matches.
  bool isFfmpeg() const;
  /// Returns true if built with Qt Multimedia player support and the name matches.
  bool isQtmultimedia() const;

  QString const & uiName() const
  {
    return name;
  }

  void setUiName( QString const & name_ )
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
#ifdef MAKE_FFMPEG_PLAYER
  static InternalPlayerBackend ffmpeg()
  {
    return InternalPlayerBackend( "FFmpeg" );
  }
#endif

#ifdef MAKE_QTMULTIMEDIA_PLAYER
  static InternalPlayerBackend qtmultimedia()
  {
    return InternalPlayerBackend( "Qt Multimedia" );
  }
#endif

  explicit InternalPlayerBackend( QString const & name_ ):
    name( name_ )
  {
  }

  QString name;
};

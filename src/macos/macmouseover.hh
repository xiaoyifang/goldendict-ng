#pragma once

#ifdef __APPLE__

  #pragma once

  #include <QObject>
  #include <QTimer>
  #include <ApplicationServices/ApplicationServices.h>
  #include "config.hh"
  #include "keyboardstate.hh"
  #include <QMutexLocker>
  #include <QMutex>

/// This is a mouseover feature interface, where you can point your mouse at
/// any word in any window and wait a little, and it would provide that word
/// for the translation.

class MacMouseOver: public QObject, public KeyboardState
{
  Q_OBJECT

public:

  /// The class is a singleton.
  static MacMouseOver & instance();

  /// Enables mouseover. The mouseover is initially disabled.
  void enableMouseOver();
  /// Disables mouseover.
  void disableMouseOver();

  /// Set pointer to program configuration
  void setPreferencesPtr( const Config::Preferences * ppref )
  {
    pPref = ppref;
  };

  /// Called from event loop callback
  void mouseMoved();

  static bool isAXAPIEnabled();

signals:

  /// Emitted when there was some text under cursor which was hovered over.
  void hovered( const QString &, bool forcePopup );

private slots:
  void timerShot();

private:

  MacMouseOver();
  ~MacMouseOver();
  void handlePosition();
  QString CFStringRefToQString( CFStringRef str );
  void handleRetrievedString( QString & wordSeq, int wordSeqPos );

  const Config::Preferences * pPref;
  QTimer mouseTimer;
  CFMachPortRef tapRef;
  CFRunLoopSourceRef loop;
  QMutex mouseMutex;
  AXUIElementRef elementSystemWide;

  bool mouseOverEnabled;
};

#endif

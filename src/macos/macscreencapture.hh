#pragma once

#ifdef __APPLE__

  #include <QObject>
  #include <QTimer>
  #include <QMutex>
  #include <QMutexLocker>
  #include <QPoint>
  #include <ApplicationServices/ApplicationServices.h>
  #include "config.hh"
  #include "keyboardstate.hh"

/// ScreenCapture + Vision OCR based mouse-over word detection.
/// This is an alternative to MacMouseOver that uses screen capture and
/// OCR instead of the Accessibility API. It only needs Screen Recording
/// permission — no Accessibility permission required.
///
/// Works on macOS 12.3+ (for ScreenCaptureKit) or macOS 10.15+ with
/// CGDisplayCreateImageForRect fallback.

class MacScreenCapture: public QObject, public KeyboardState
{
  Q_OBJECT

public:
  static MacScreenCapture & instance();

  /// Check if Screen Recording permission is available.
  static bool isAvailable();

  /// Enable the capture-based mouse-over detection.
  void enableCapture();
  /// Disable it.
  void disableCapture();

  void setPreferencesPtr( const Config::Preferences * ppref )
  {
    pPref = ppref;
  }

  void mouseMoved();

signals:
  void hovered( const QString &, bool forcePopup );

private slots:
  void timerShot();
  void pollMousePosition();

private:
  MacScreenCapture();
  ~MacScreenCapture();

  void dispatchCapture();

  const Config::Preferences * pPref;
  QTimer   captureTimer;  // 300ms debounce
  QTimer   pollTimer;     // 100ms mouse polling fallback
  QMutex   captureMutex;
  QPoint   lastPollPos;
  CFMachPortRef    tapRef;
  CFRunLoopSourceRef loop;
  bool             usePolling;  // true if CGEventTap unavailable
};

#endif

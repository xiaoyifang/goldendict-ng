#pragma once

/// @file
/// Handling global hotkeys and some trick
/// Part of this header is implemented in
/// + `winhotkeywrapper`
/// + `machotkeywrapper`
/// + `x11hotkeywrapper`

#include <QGuiApplication>
#include <QThread>
#include "config.hh"
#include "ex.hh"
#include "utils.hh"

#ifdef WITH_X11
  #include <fixx11h.h>
  #include <set>
  #include <X11/Xlib.h>
  #include <X11/extensions/record.h>
  #include <X11/Xlibint.h>
  #undef Bool
  #undef min
  #undef max
#endif

#ifdef Q_OS_WIN
  #include <QAbstractNativeEventFilter>
#endif

#ifdef Q_OS_MAC
  #import <Carbon/Carbon.h>
#endif

//////////////////////////////////////////////////////////////////////////

struct HotkeyStruct
{
  HotkeyStruct() = default;
  HotkeyStruct( quint32 key, quint32 key2, quint32 modifier, int handle, int id ):
    key( key ),
    key2( key2 ),
    modifier( modifier ),
    handle( handle ),
    id( id ) {};

  quint32 key      = 0;
  quint32 key2     = 0;
  quint32 modifier = 0;
  int handle       = 0;
  int id           = 0;
#ifdef Q_OS_MAC
  EventHotKeyRef hkRef  = nullptr;
  EventHotKeyRef hkRef2 = nullptr;
#endif
};

class HotkeyWrapper: public QThread // Thread is actually only used on X11
{
  Q_OBJECT

  friend class QHotkeyApplication;

public:

  DEF_EX( exInit, "Hotkey wrapper failed to init", std::exception )

  explicit HotkeyWrapper( QObject * parent );
  ~HotkeyWrapper() override;

  /// The handle will be passed back in hotkeyActivated() to inform which hotkey was activated.
  /// 2 possible handles:
  /// 0 -> Invoke main window
  /// 1 -> translate clipboard
  bool setGlobalKey( const QKeySequence &, int handle );

  /// Unregisters everything
  void unregister();

signals:
  void hotkeyActivated( int handle );

  /*
 *
 * Every Below should NOT be accessed from outside.
 *
 */

protected slots:
  void waitKey2();

private:
  void init();
  quint32 nativeKey( int key );

  QList< HotkeyStruct > hotkeys;

  bool state2;
  HotkeyStruct state2waiter;

#ifdef Q_OS_WIN32
  virtual bool winEvent( MSG * message, qintptr * result );
  HWND hwnd;
private slots:
  bool checkState( quint32 vk, quint32 mod );
#endif

#ifdef Q_OS_MAC

private:
  void sendCmdC();

  static EventHandlerUPP hotKeyFunction;
  quint32 keyC;
  EventHandlerRef handlerRef;

public:
  void activated( int hkId );

#endif

#ifdef WITH_X11
  static void recordEventCallback( XPointer, XRecordInterceptData * );

  /// Called by recordEventCallback()
  void handleRecordEvent( XRecordInterceptData * );

  void run() override; // QThread

  // We do one-time init of those, translating keysyms to keycodes
  KeyCode lShiftCode, rShiftCode, lCtrlCode, rCtrlCode, lAltCode, rAltCode, cCode, insertCode, kpInsertCode, lMetaCode,
    rMetaCode;

  quint32 currentModifiers;

  Display * dataDisplay;
  XRecordRange * recordRange;
  XRecordContext recordContext;
  XRecordClientSpec recordClientSpec;

  /// Holds all the keys currently grabbed.
  /// The first value is keycode, the second is modifiers
  using GrabbedKeys = std::set< std::pair< quint32, quint32 > >;
  GrabbedKeys grabbedKeys;

  GrabbedKeys::iterator keyToUngrab; // Used for second stage grabs

  /// Returns true if the given key is usually used to copy from clipboard,
  /// false otherwise.
  bool isCopyToClipboardKey( quint32 keyCode, quint32 modifiers ) const;
  /// Returns true if the given key is grabbed, false otherwise
  bool isKeyGrabbed( quint32 keyCode, quint32 modifiers ) const;
  /// Grabs the given key, recording the fact in grabbedKeys. If the key's
  /// already grabbed, does nothing.
  /// Returns the key's iterator in grabbedKeys.
  GrabbedKeys::iterator grabKey( quint32 keyCode, quint32 modifiers );
  /// Ungrabs the given key. erasing it from grabbedKeys. The key's provided
  /// as an interator inside the grabbedKeys set.
  void ungrabKey( GrabbedKeys::iterator );

signals:

  /// Emitted from the thread
  void keyRecorded( quint32 vk, quint32 mod );

private slots:
  bool checkState( quint32 vk, quint32 mod );
#endif
};

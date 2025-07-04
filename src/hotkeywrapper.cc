#include <QtGlobal>
#ifndef Q_OS_MACOS
  #include "hotkeywrapper.hh"
  #include <QTimer>
  #include <QSessionManager>
  #include <QWidget>
  #ifdef Q_OS_WIN
    #include <windows.h>
    #include "windows/winhotkeyapplication.hh"
  #endif

HotkeyWrapper::HotkeyWrapper( QObject * parent ):
  QThread( parent ),
  state2( false )
{
  init();
  #ifdef Q_OS_WIN
  ( static_cast< QHotkeyApplication * >( qApp ) )->registerWrapper( this );
  #endif
}

HotkeyWrapper::~HotkeyWrapper()
{
  unregister();
}

void HotkeyWrapper::waitKey2()
{
  state2 = false;

  #ifdef HAVE_X11

  if ( keyToUngrab != grabbedKeys.end() ) {
    ungrabKey( keyToUngrab );
    keyToUngrab = grabbedKeys.end();
  }

  #endif
}

bool HotkeyWrapper::checkState( quint32 vk, quint32 mod )
{
  if ( state2 ) { // wait for 2nd key

    waitKey2(); // Cancel the 2nd-key wait stage

    if ( state2waiter.key2 == vk && state2waiter.modifier == mod ) {
      emit hotkeyActivated( state2waiter.handle );
      return true;
    }
  }

  for ( int i = 0; i < hotkeys.count(); i++ ) {

    const HotkeyStruct & hs = hotkeys.at( i );

    if ( hs.key == vk && hs.modifier == mod ) {

  #ifdef Q_OS_WIN32
      // If that was a copy-to-clipboard shortcut, re-emit it back so it could
      // reach its original destination so it could be acted upon.
      if ( hs.key2 != 0 || ( mod == MOD_CONTROL && ( vk == VK_INSERT || vk == 'c' || vk == 'C' ) ) ) {
        // Pass-through first part of compound hotkey or clipdoard copy command

        INPUT i[ 10 ];
        memset( i, 0, sizeof( i ) );
        int nextKeyNom       = 0;
        short emulateModKeys = 0;

        // If modifier keys aren't pressed it looks like emulation
        // We then also emulate full sequence

        if ( ( mod & MOD_ALT ) != 0 && ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) == 0 ) {
          emulateModKeys |= MOD_ALT;
          i[ nextKeyNom ].type   = INPUT_KEYBOARD;
          i[ nextKeyNom ].ki.wVk = VK_MENU;
          nextKeyNom += 1;
        }
        if ( ( mod & MOD_CONTROL ) != 0 && ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) == 0 ) {
          emulateModKeys |= MOD_CONTROL;
          i[ nextKeyNom ].type   = INPUT_KEYBOARD;
          i[ nextKeyNom ].ki.wVk = VK_CONTROL;
          nextKeyNom += 1;
        }
        if ( ( mod & MOD_SHIFT ) != 0 && ( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) == 0 ) {
          emulateModKeys |= MOD_SHIFT;
          i[ nextKeyNom ].type   = INPUT_KEYBOARD;
          i[ nextKeyNom ].ki.wVk = VK_SHIFT;
          nextKeyNom += 1;
        }
        if ( ( mod & MOD_WIN ) != 0 && ( GetAsyncKeyState( VK_LWIN ) & 0x8000 ) == 0
             && ( GetAsyncKeyState( VK_RWIN ) & 0x8000 ) == 0 ) {
          emulateModKeys |= MOD_WIN;
          i[ nextKeyNom ].type   = INPUT_KEYBOARD;
          i[ nextKeyNom ].ki.wVk = VK_LWIN;
          nextKeyNom += 1;
        }

        i[ nextKeyNom ].type   = INPUT_KEYBOARD;
        i[ nextKeyNom ].ki.wVk = vk;
        nextKeyNom += 1;
        i[ nextKeyNom ].type       = INPUT_KEYBOARD;
        i[ nextKeyNom ].ki.wVk     = vk;
        i[ nextKeyNom ].ki.dwFlags = KEYEVENTF_KEYUP;
        nextKeyNom += 1;

        if ( emulateModKeys & MOD_WIN ) {
          i[ nextKeyNom ].type       = INPUT_KEYBOARD;
          i[ nextKeyNom ].ki.wVk     = VK_LWIN;
          i[ nextKeyNom ].ki.dwFlags = KEYEVENTF_KEYUP;
          nextKeyNom += 1;
        }
        if ( emulateModKeys & MOD_SHIFT ) {
          i[ nextKeyNom ].type       = INPUT_KEYBOARD;
          i[ nextKeyNom ].ki.wVk     = VK_SHIFT;
          i[ nextKeyNom ].ki.dwFlags = KEYEVENTF_KEYUP;
          nextKeyNom += 1;
        }
        if ( emulateModKeys & MOD_CONTROL ) {
          i[ nextKeyNom ].type       = INPUT_KEYBOARD;
          i[ nextKeyNom ].ki.wVk     = VK_CONTROL;
          i[ nextKeyNom ].ki.dwFlags = KEYEVENTF_KEYUP;
          nextKeyNom += 1;
        }
        if ( emulateModKeys & MOD_ALT ) {
          i[ nextKeyNom ].type       = INPUT_KEYBOARD;
          i[ nextKeyNom ].ki.wVk     = VK_MENU;
          i[ nextKeyNom ].ki.dwFlags = KEYEVENTF_KEYUP;
          nextKeyNom += 1;
        }

        UnregisterHotKey( hwnd, hs.id );
        SendInput( nextKeyNom, i, sizeof( *i ) );
        RegisterHotKey( hwnd, hs.id, hs.modifier, hs.key );
      }
  #endif

      if ( hs.key2 == 0 ) {
        emit hotkeyActivated( hs.handle );
        return true;
      }

      state2       = true;
      state2waiter = hs;
      QTimer::singleShot( 500, this, &HotkeyWrapper::waitKey2 );

  #ifdef HAVE_X11

      // Grab the second key, unless it's grabbed already
      // Note that we only grab the clipboard key only if
      // the sequence didn't begin with it

      if ( ( isCopyToClipboardKey( hs.key, hs.modifier ) || !isCopyToClipboardKey( hs.key2, hs.modifier ) )
           && !isKeyGrabbed( hs.key2, hs.modifier ) )
        keyToUngrab = grabKey( hs.key2, hs.modifier );

  #endif

      return true;
    }
  }

  state2 = false;
  return false;
}
#endif

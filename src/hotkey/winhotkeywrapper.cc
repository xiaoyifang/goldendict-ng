#include <QtGlobal>
#ifdef Q_OS_WIN
  #include "hotkeywrapper.hh"
  #include "winhotkeyapplication.hh"
  #include <windows.h>
  #include <QWidget>
  #include <QTimer>

/// Implementation is pretty much using RegisterHotKey & UnregisterHotKey
/// https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey


HotkeyWrapper::HotkeyWrapper( QObject * parent ):
  QThread( parent ),
  state2( false )
{
  init();
  ( static_cast< QHotkeyApplication * >( qApp ) )->registerWrapper( this );
}

HotkeyWrapper::~HotkeyWrapper()
{
  unregister();
}

void HotkeyWrapper::waitKey2()
{
  state2 = false;
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

      if ( hs.key2 == 0 ) {
        emit hotkeyActivated( hs.handle );
        return true;
      }

      state2       = true;
      state2waiter = hs;
      QTimer::singleShot( 500, this, &HotkeyWrapper::waitKey2 );

      return true;
    }
  }

  state2 = false;
  return false;
}


void HotkeyWrapper::init()
{
  hwnd = (HWND)( ( static_cast< QWidget * >( this->parent() ) )->winId() );
}

bool HotkeyWrapper::setGlobalKey( const QKeySequence & seq, int handle )
{
  Config::HotKey hk( seq );
  int key                        = hk.key1;
  int key2                       = hk.key2;
  Qt::KeyboardModifiers modifier = hk.modifiers;

  if ( !key )
    return false; // We don't monitor empty combinations

  static int id = 0;
  if ( id > 0xBFFF - 1 )
    id = 0;

  quint32 mod = 0;
  if ( modifier & Qt::CTRL )
    mod |= MOD_CONTROL;
  if ( modifier & Qt::ALT )
    mod |= MOD_ALT;
  if ( modifier & Qt::SHIFT )
    mod |= MOD_SHIFT;
  if ( modifier & Qt::META )
    mod |= MOD_WIN;

  quint32 vk  = nativeKey( key );
  quint32 vk2 = key2 ? nativeKey( key2 ) : 0;

  hotkeys.append( HotkeyStruct( vk, vk2, mod, handle, id ) );

  if ( !RegisterHotKey( hwnd, id++, mod, vk ) )
    return false;

  if ( key2 && key2 != key )
    return RegisterHotKey( hwnd, id++, mod, vk2 );

  return true;
}


bool HotkeyWrapper::winEvent( MSG * message, qintptr * result )
{
  Q_UNUSED( result );
  if ( message->message == WM_HOTKEY )
    return checkState( ( message->lParam >> 16 ), ( message->lParam & 0xffff ) );

  return false;
}

void HotkeyWrapper::unregister()
{
  for ( int i = 0; i < hotkeys.count(); i++ ) {
    const HotkeyStruct & hk = hotkeys.at( i );

    UnregisterHotKey( hwnd, hk.id );

    if ( hk.key2 && hk.key2 != hk.key )
      UnregisterHotKey( hwnd, hk.id + 1 );
  }

  ( static_cast< QHotkeyApplication * >( qApp ) )->unregisterWrapper( this );
}

bool QHotkeyApplication::nativeEventFilter( const QByteArray & /*eventType*/, void * message, qintptr * result )
{
  MSG * msg = reinterpret_cast< MSG * >( message );

  if ( msg->message == WM_HOTKEY ) {
    for ( int i = 0; i < hotkeyWrappers.size(); i++ ) {
      if ( hotkeyWrappers.at( i )->winEvent( msg, result ) )
        return true;
    }
  }

  return false;
}

QHotkeyApplication::QHotkeyApplication( const QString & id, int & argc, char ** argv ):
  QtSingleApplication( id, argc, argv )
{
  installNativeEventFilter( this );
}

void QHotkeyApplication::registerWrapper( HotkeyWrapper * wrapper )
{
  if ( wrapper && !hotkeyWrappers.contains( wrapper ) ) {
    hotkeyWrappers.append( wrapper );
  }
}

void QHotkeyApplication::unregisterWrapper( HotkeyWrapper * wrapper )
{
  if ( wrapper && hotkeyWrappers.contains( wrapper ) ) {
    hotkeyWrappers.removeAll( wrapper );
  }
}

/// References:
/// https://learn.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
/// https://doc.qt.io/qt-6/qt.html#Key-enum
quint32 HotkeyWrapper::nativeKey( int key )
{
  // Qt's 0-9 & A-Z overlaps with Windows's VK
  if ( key >= Qt::Key_0 && key <= Qt::Key_9 || key >= Qt::Key_A && key <= Qt::Key_Z ) {
    return key;
  }

  switch ( key ) {
    case Qt::Key_Space:
      return VK_SPACE;
    case Qt::Key_Asterisk:
      return VK_MULTIPLY;
    case Qt::Key_Plus:
      return VK_ADD;
    case Qt::Key_Comma:
      return VK_SEPARATOR;
    case Qt::Key_Minus:
      return VK_SUBTRACT;
    case Qt::Key_Slash:
      return VK_DIVIDE;
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
      return VK_TAB;
    case Qt::Key_Backspace:
      return VK_BACK;
    case Qt::Key_Return:
    case Qt::Key_Escape:
      return VK_ESCAPE;
    case Qt::Key_Enter:
      return VK_RETURN;
    case Qt::Key_Insert:
      return VK_INSERT;
    case Qt::Key_Delete:
      return VK_DELETE;
    case Qt::Key_Pause:
      return VK_PAUSE;
    case Qt::Key_Print:
      return VK_PRINT;
    case Qt::Key_Clear:
      return VK_CLEAR;
    case Qt::Key_Home:
      return VK_HOME;
    case Qt::Key_End:
      return VK_END;
    case Qt::Key_Up:
      return VK_UP;
    case Qt::Key_Down:
      return VK_DOWN;
    case Qt::Key_Left:
      return VK_LEFT;
    case Qt::Key_Right:
      return VK_RIGHT;
    case Qt::Key_PageUp:
      return VK_PRIOR;
    case Qt::Key_PageDown:
      return VK_NEXT;
    case Qt::Key_F1:
      return VK_F1;
    case Qt::Key_F2:
      return VK_F2;
    case Qt::Key_F3:
      return VK_F3;
    case Qt::Key_F4:
      return VK_F4;
    case Qt::Key_F5:
      return VK_F5;
    case Qt::Key_F6:
      return VK_F6;
    case Qt::Key_F7:
      return VK_F7;
    case Qt::Key_F8:
      return VK_F8;
    case Qt::Key_F9:
      return VK_F9;
    case Qt::Key_F10:
      return VK_F10;
    case Qt::Key_F11:
      return VK_F11;
    case Qt::Key_F12:
      return VK_F12;
    case Qt::Key_F13:
      return VK_F13;
    case Qt::Key_F14:
      return VK_F14;
    case Qt::Key_F15:
      return VK_F15;
    case Qt::Key_F16:
      return VK_F16;
    case Qt::Key_F17:
      return VK_F17;
    case Qt::Key_F18:
      return VK_F18;
    case Qt::Key_F19:
      return VK_F19;
    case Qt::Key_F20:
      return VK_F20;
    case Qt::Key_F21:
      return VK_F21;
    case Qt::Key_F22:
      return VK_F22;
    case Qt::Key_F23:
      return VK_F23;
    case Qt::Key_F24:
      return VK_F24;
    case Qt::Key_Colon:
    case Qt::Key_Semicolon:
      return VK_OEM_1;
    case Qt::Key_Question:
      return VK_OEM_2;
    case Qt::Key_AsciiTilde:
    case Qt::Key_QuoteLeft:
      return VK_OEM_3;
    case Qt::Key_BraceLeft:
    case Qt::Key_BracketLeft:
      return VK_OEM_4;
    case Qt::Key_Bar:
    case Qt::Key_Backslash:
      return VK_OEM_5;
    case Qt::Key_BraceRight:
    case Qt::Key_BracketRight:
      return VK_OEM_6;
    case Qt::Key_QuoteDbl:
    case Qt::Key_Apostrophe:
      return VK_OEM_7;
    case Qt::Key_Less:
      return VK_OEM_COMMA;
    case Qt::Key_Greater:
      return VK_OEM_PERIOD;
    case Qt::Key_Equal:
      return VK_OEM_PLUS;
    case Qt::Key_ParenRight:
      return 0x30;
    case Qt::Key_Exclam:
      return 0x31;
    case Qt::Key_At:
      return 0x32;
    case Qt::Key_NumberSign:
      return 0x33;
    case Qt::Key_Dollar:
      return 0x34;
    case Qt::Key_Percent:
      return 0x35;
    case Qt::Key_AsciiCircum:
      return 0x36;
    case Qt::Key_Ampersand:
      return 0x37;
    case Qt::Key_copyright:
      return 0x38;
    case Qt::Key_ParenLeft:
      return 0x39;
    case Qt::Key_Underscore:
      return VK_OEM_MINUS;
    case Qt::Key_Meta:
      return VK_LWIN;
    default:;
  }

  return key;
}

#endif

#include "hotkeywrapper.hh"
#include "gddebug.hh"
#include <QTimer>
#include <QSessionManager>
#include <QWidget>
#ifdef Q_OS_WIN
  #include <windows.h>
#endif

//////////////////////////////////////////////////////////////////////////

QHotkeyApplication::QHotkeyApplication( int & argc, char ** argv ):
  QtSingleApplication( argc, argv )
{
  connect( this,
           &QGuiApplication::commitDataRequest,
           this,
           &QHotkeyApplication::hotkeyAppCommitData,
           Qt::DirectConnection );

  connect( this,
           &QGuiApplication::saveStateRequest,
           this,
           &QHotkeyApplication::hotkeyAppSaveState,
           Qt::DirectConnection );

#if defined( Q_OS_WIN )
  installNativeEventFilter( this );
#endif
}

QHotkeyApplication::QHotkeyApplication( QString const & id, int & argc, char ** argv ):
  QtSingleApplication( id, argc, argv )
{
  connect( this,
           &QGuiApplication::commitDataRequest,
           this,
           &QHotkeyApplication::hotkeyAppCommitData,
           Qt::DirectConnection );

  connect( this,
           &QGuiApplication::saveStateRequest,
           this,
           &QHotkeyApplication::hotkeyAppSaveState,
           Qt::DirectConnection );

#if defined( Q_OS_WIN )
  installNativeEventFilter( this );
#endif
}

void QHotkeyApplication::addDataCommiter( DataCommitter & d )
{
  dataCommitters.append( &d );
}

void QHotkeyApplication::removeDataCommiter( DataCommitter & d )
{
  dataCommitters.removeAll( &d );
}

void QHotkeyApplication::hotkeyAppCommitData( QSessionManager & mgr )
{
  for ( int x = 0; x < dataCommitters.size(); ++x ) {
    dataCommitters[ x ]->commitData( mgr );
  }
}

void QHotkeyApplication::hotkeyAppSaveState( QSessionManager & mgr )
{
  mgr.setRestartHint( QSessionManager::RestartNever );
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

//////////////////////////////////////////////////////////////////////////

HotkeyStruct::HotkeyStruct( quint32 key_, quint32 key2_, quint32 modifier_, int handle_, int id_ ):
  key( key_ ),
  key2( key2_ ),
  modifier( modifier_ ),
  handle( handle_ ),
  id( id_ )
{
}

//////////////////////////////////////////////////////////////////////////

#if !defined( Q_OS_MAC )
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


  #ifndef Q_OS_WIN

  //////////////////////////////////////////////////////////////////////////

    #include <X11/keysym.h>

void HotkeyWrapper::init()
{
  keyToUngrab = grabbedKeys.end();

    #if QT_VERSION < 0x060000
  Display * displayID = QX11Info::display();
    #else
  QNativeInterface::QX11Application * x11AppInfo = qApp->nativeInterface< QNativeInterface::QX11Application >();
  Display * displayID                            = x11AppInfo->display();
    #endif

  // We use RECORD extension instead of XGrabKey. That's because XGrabKey
  // prevents other clients from getting their input if it's grabbed.

  Display * display = displayID;

  lShiftCode = XKeysymToKeycode( display, XK_Shift_L );
  rShiftCode = XKeysymToKeycode( display, XK_Shift_R );

  lCtrlCode = XKeysymToKeycode( display, XK_Control_L );
  rCtrlCode = XKeysymToKeycode( display, XK_Control_R );

  lAltCode = XKeysymToKeycode( display, XK_Alt_L );
  rAltCode = XKeysymToKeycode( display, XK_Alt_R );

  lMetaCode = XKeysymToKeycode( display, XK_Super_L );
  rMetaCode = XKeysymToKeycode( display, XK_Super_R );

  cCode        = XKeysymToKeycode( display, XK_c );
  insertCode   = XKeysymToKeycode( display, XK_Insert );
  kpInsertCode = XKeysymToKeycode( display, XK_KP_Insert );

  currentModifiers = 0;

  // This one will be used to read the recorded content
  dataDisplay = XOpenDisplay( 0 );

  if ( !dataDisplay )
    throw exInit();

  recordRange = XRecordAllocRange();

  if ( !recordRange ) {
    XCloseDisplay( dataDisplay );
    throw exInit();
  }

  recordRange->device_events.first = KeyPress;
  recordRange->device_events.last  = KeyRelease;
  recordClientSpec                 = XRecordAllClients;

  recordContext = XRecordCreateContext( display, 0, &recordClientSpec, 1, &recordRange, 1 );

  if ( !recordContext ) {
    XFree( recordRange );
    XCloseDisplay( dataDisplay );
    throw exInit();
  }

  // This is required to ensure context was indeed created
  XSync( display, False );

  connect( this, &HotkeyWrapper::keyRecorded, this, &HotkeyWrapper::checkState, Qt::QueuedConnection );

  start();
}

void HotkeyWrapper::run() // Runs in a separate thread
{
  if ( !XRecordEnableContext( dataDisplay, recordContext, recordEventCallback, (XPointer)this ) )
    GD_DPRINTF( "Failed to enable record context\n" );
}


void HotkeyWrapper::recordEventCallback( XPointer ptr, XRecordInterceptData * data )
{
  ( (HotkeyWrapper *)ptr )->handleRecordEvent( data );
}

void HotkeyWrapper::handleRecordEvent( XRecordInterceptData * data )
{
  if ( data->category == XRecordFromServer ) {
    xEvent * event = (xEvent *)data->data;

    if ( event->u.u.type == KeyPress ) {
      KeyCode key = event->u.u.detail;

      if ( key == lShiftCode || key == rShiftCode )
        currentModifiers |= ShiftMask;
      else if ( key == lCtrlCode || key == rCtrlCode )
        currentModifiers |= ControlMask;
      else if ( key == lAltCode || key == rAltCode )
        currentModifiers |= Mod1Mask;
      else if ( key == lMetaCode || key == rMetaCode )
        currentModifiers |= Mod4Mask;
      else {
        // Here we employ a kind of hack translating KP_Insert key
        // to just Insert. This allows reacting to both Insert keys.
        if ( key == kpInsertCode )
          key = insertCode;

        emit keyRecorded( key, currentModifiers );
      }
    }
    else if ( event->u.u.type == KeyRelease ) {
      KeyCode key = event->u.u.detail;

      if ( key == lShiftCode || key == rShiftCode )
        currentModifiers &= ~ShiftMask;
      else if ( key == lCtrlCode || key == rCtrlCode )
        currentModifiers &= ~ControlMask;
      else if ( key == lAltCode || key == rAltCode )
        currentModifiers &= ~Mod1Mask;
      else if ( key == lMetaCode || key == rMetaCode )
        currentModifiers &= ~Mod4Mask;
    }
  }

  XRecordFreeData( data );
}

bool HotkeyWrapper::setGlobalKey( QKeySequence const & seq, int handle )
{
  Config::HotKey hk( seq );
  return setGlobalKey( hk.key1, hk.key2, hk.modifiers, handle );
}

bool HotkeyWrapper::setGlobalKey( int key, int key2, Qt::KeyboardModifiers modifier, int handle )
{
  if ( !key )
    return false; // We don't monitor empty combinations

  int vk  = nativeKey( key );
  int vk2 = key2 ? nativeKey( key2 ) : 0;

  quint32 mod = 0;
  if ( modifier & Qt::ShiftModifier )
    mod |= ShiftMask;
  if ( modifier & Qt::ControlModifier )
    mod |= ControlMask;
  if ( modifier & Qt::AltModifier )
    mod |= Mod1Mask;
  if ( modifier & Qt::MetaModifier )
    mod |= Mod4Mask;

  hotkeys.append( HotkeyStruct( vk, vk2, mod, handle, 0 ) );

  if ( !isCopyToClipboardKey( vk, mod ) )
    grabKey( vk, mod ); // Make sure it doesn't get caught by other apps

  return true;
}

bool HotkeyWrapper::isCopyToClipboardKey( quint32 keyCode, quint32 modifiers ) const
{
  return modifiers == ControlMask && ( keyCode == cCode || keyCode == insertCode || keyCode == kpInsertCode );
}

bool HotkeyWrapper::isKeyGrabbed( quint32 keyCode, quint32 modifiers ) const
{
  GrabbedKeys::const_iterator i = grabbedKeys.find( std::make_pair( keyCode, modifiers ) );

  return i != grabbedKeys.end();
}

namespace {

using X11ErrorHandler = int ( * )( Display * display, XErrorEvent * event );

class X11GrabUngrabErrorHandler
{
public:
  static bool error;

  static int grab_ungrab_error_handler( Display *, XErrorEvent * event )
  {
    qDebug() << "grab_ungrab_error_handler is invoked";
    switch ( event->error_code ) {
      case BadAccess:
      case BadValue:
      case BadWindow:
        if ( event->request_code == 33 /* X_GrabKey */ || event->request_code == 34 /* X_UngrabKey */ ) {
          error = true;
        }
    }
    return 0;
  }

  X11GrabUngrabErrorHandler()
  {
    error                 = false;
    previousErrorHandler_ = XSetErrorHandler( grab_ungrab_error_handler );
  }

  ~X11GrabUngrabErrorHandler()
  {
    #if QT_VERSION < 0x060000
    Display * displayID = QX11Info::display();
    #else
    QNativeInterface::QX11Application * x11AppInfo = qApp->nativeInterface< QNativeInterface::QX11Application >();
    Display * displayID                            = x11AppInfo->display();
    #endif
    XFlush( displayID );
    (void)XSetErrorHandler( previousErrorHandler_ );
  }

  bool isError() const
  {
    #if QT_VERSION < 0x060000
    Display * displayID = QX11Info::display();
    #else
    QNativeInterface::QX11Application * x11AppInfo = qApp->nativeInterface< QNativeInterface::QX11Application >();
    Display * displayID                            = x11AppInfo->display();
    #endif
    XFlush( displayID );
    return error;
  }

private:
  X11ErrorHandler previousErrorHandler_;
};

bool X11GrabUngrabErrorHandler::error = false;

} // anonymous namespace


HotkeyWrapper::GrabbedKeys::iterator HotkeyWrapper::grabKey( quint32 keyCode, quint32 modifiers )
{
  std::pair< GrabbedKeys::iterator, bool > result = grabbedKeys.insert( std::make_pair( keyCode, modifiers ) );


  if ( result.second ) {
    #if QT_VERSION < 0x060000
    Display * displayID = QX11Info::display();
    #else
    QNativeInterface::QX11Application * x11AppInfo = qApp->nativeInterface< QNativeInterface::QX11Application >();
    Display * displayID                            = x11AppInfo->display();
    #endif
    X11GrabUngrabErrorHandler errorHandler;
    XGrabKey( displayID, keyCode, modifiers, DefaultRootWindow( displayID ), True, GrabModeAsync, GrabModeAsync );

    if ( errorHandler.isError() ) {
      gdWarning( "Possible hotkeys conflict. Check your hotkeys options." );
      ungrabKey( result.first );
    }
  }

  return result.first;
}

void HotkeyWrapper::ungrabKey( GrabbedKeys::iterator i )
{
    #if QT_VERSION < 0x060000
  Display * displayID = QX11Info::display();
    #else
  QNativeInterface::QX11Application * x11AppInfo = qApp->nativeInterface< QNativeInterface::QX11Application >();
  Display * displayID                            = x11AppInfo->display();
    #endif
  X11GrabUngrabErrorHandler errorHandler;
  XUngrabKey( displayID, i->first, i->second, XDefaultRootWindow( displayID ) );

  grabbedKeys.erase( i );

  if ( errorHandler.isError() ) {
    gdWarning( "Cannot ungrab the hotkey" );
  }
}

quint32 HotkeyWrapper::nativeKey( int key )
{
  QString keySymName;

  switch ( key ) {
    case Qt::Key_Insert:
      keySymName = "Insert";
      break;
    default:
      keySymName = QKeySequence( key ).toString();
      break;
  }
    #if QT_VERSION < 0x060000
  Display * displayID = QX11Info::display();
    #else
  QNativeInterface::QX11Application * x11AppInfo = qApp->nativeInterface< QNativeInterface::QX11Application >();
  Display * displayID                            = x11AppInfo->display();
    #endif
  Display * display = displayID;
  return XKeysymToKeycode( display, XStringToKeysym( keySymName.toLatin1().data() ) );
}

void HotkeyWrapper::unregister()
{
    #if QT_VERSION < 0x060000
  Display * displayID = QX11Info::display();
    #else
  QNativeInterface::QX11Application * x11AppInfo = qApp->nativeInterface< QNativeInterface::QX11Application >();
  Display * displayID                            = x11AppInfo->display();
    #endif
  Display * display = displayID;

  XRecordDisableContext( display, recordContext );
  XSync( display, False );

  wait();

  XRecordFreeContext( display, recordContext );
  XFree( recordRange );
  XCloseDisplay( dataDisplay );

  while ( grabbedKeys.size() )
    ungrabKey( grabbedKeys.begin() );

  ( static_cast< QHotkeyApplication * >( qApp ) )->unregisterWrapper( this );
}

  #endif
#endif

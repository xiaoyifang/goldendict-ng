#include "clipboardlistener.hh"

#ifdef Q_OS_MACOS
  #include "clipboard/mac.hh"
#else
  #include "clipboard/simple.hh"
#endif


BaseClipboardListener * clipboardListener::get_impl( QObject * parent )
{
#ifdef Q_OS_MACOS
  return new MacClipboardListener( parent );
#else
  return new SimpleClipboardListener( parent );
#endif
}

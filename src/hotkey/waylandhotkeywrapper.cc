#include "hotkeywrapper.hh"


#include <QtGlobal>
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS) && !defined(WITH_X11)


HotkeyWrapper::HotkeyWrapper( QObject * parent ):
QThread(parent), state2(false)
{
  qDebug()<<"Global hotkey unimplemented";
}

HotkeyWrapper::~HotkeyWrapper()
{
}

bool HotkeyWrapper::setGlobalKey( const QKeySequence &, int handle )
{
  return true ;
};


void HotkeyWrapper::unregister()
{
};

void HotkeyWrapper::waitKey2(){
}



#endif


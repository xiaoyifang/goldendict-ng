#if !defined(__APPLE__)
#include "clipboard/simple.hh"
#include <QGuiApplication>
  ::SimpleClipboardListener(QObject * parent)
:BaseClipboardListener( parent ){
   sysClipboard=qApp->clipboard();
}

QString SimpleClipboardListener::text(){
   return sysClipboard->text();
 }

void SimpleClipboardListener::stop()
 {
   disconnect( this->sysClipboard,&QClipboard::changed,this, &BaseClipboardListener::changed );
 }

void SimpleClipboardListener::start(){
   connect( this->sysClipboard,&QClipboard::changed,this, &BaseClipboardListener::changed );
 }

#endif


#pragma once
#include <QtGlobal>
#ifdef Q_OS_WIN
  #include "qtsingleapplication.h"
class QHotkeyApplication: public QtSingleApplication, public QAbstractNativeEventFilter
{
  Q_OBJECT

  friend class HotkeyWrapper;

public:
  QHotkeyApplication( const QString & id, int & argc, char ** argv );


protected:
  void registerWrapper( HotkeyWrapper * wrapper );
  void unregisterWrapper( HotkeyWrapper * wrapper );

  virtual bool nativeEventFilter( const QByteArray & eventType, void * message, qintptr * result );

  QList< HotkeyWrapper * > hotkeyWrappers;
};

#endif

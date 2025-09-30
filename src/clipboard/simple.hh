#pragma once
#if !defined( __APPLE__ )
  #include "clipboard/base.hh"

class SimpleClipboardListener: public BaseClipboardListener
{
  Q_OBJECT

public:
  explicit SimpleClipboardListener( QObject * parent );
  QString text() override;
  void stop() override;
  void start() override;

private:
  QClipboard * sysClipboard;
};
#endif

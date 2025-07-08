#pragma once

#ifdef __APPLE__
  #include "clipboard/base.hh"

class MacClipboardListener: public BaseClipboardListener
{
  Q_OBJECT

public:
  explicit MacClipboardListener( QObject * parent );

  QString text() override;
  void stop() override;
  void start() override;

private:

  QClipboard * sysClipboard;
  QString curClipboardContent;

  QTimer m_monitoringTimer;
  QString m_currentContent;
  void updateClipboard();
};

#endif

#ifdef __APPLE__
  #include "clipboard/mac.hh"
  #include <QGuiApplication>

MacClipboardListener::MacClipboardListener( QObject * parent ):
  BaseClipboardListener( parent ),
  sysClipboard( QGuiApplication::clipboard() )
{

  connect( &m_monitoringTimer, &QTimer::timeout, this, [ this ]{
    updateClipboard();
  } );
}

QString MacClipboardListener::text()
{
  return m_currentContent;
}

void MacClipboardListener::updateClipboard()
{
  const QString newContent = this->sysClipboard->text().trimmed();

  // get rid of change if new clipboard content is equal to previous version
  if ( newContent == m_currentContent || newContent.length() == 0 ) { // avoid spaces
    return;
  }

  m_currentContent = newContent;
  emit changed( QClipboard::Clipboard );
}

void MacClipboardListener::stop()
{
  m_monitoringTimer.stop();
}

void MacClipboardListener::start()
{
  m_monitoringTimer.start( 1000 ); // 1s
}

#endif

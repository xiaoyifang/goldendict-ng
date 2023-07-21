#ifdef __APPLE__

  #include "gd_clipboard.hh"
  #include <QGuiApplication>

gd_clipboard::gd_clipboard( QObject * parent ):
  QObject{ parent },
  sysClipboard( QGuiApplication::clipboard() )
{

  connect( &m_monitoringTimer, &QTimer::timeout, this, [ this ]() {
    updateClipboard();
  } );
}

QString gd_clipboard::text() const
{
  return m_currentContent;
}

void gd_clipboard::updateClipboard()
{
  const QString newContent = this->sysClipboard->text().trimmed();

  // get rid of change if new clipboard content is equal to previous version
  if ( newContent == m_currentContent || newContent.length() == 0 ) { // avoid spaces
    return;
  }

  m_currentContent = newContent;
  emit changed( QClipboard::Clipboard );
}

void gd_clipboard::stop()
{
  m_monitoringTimer.stop();
}

void gd_clipboard::start()
{
  m_monitoringTimer.start( 1000 ); // 1s
}

#endif

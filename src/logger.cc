#include "config.hh"
#include "logger.hh"
#include <QDateTime>
#include <QFile>
#include <QMutexLocker>

QFile logFile;
QMutex logFileMutex; // Logging could happen in any threads!

void logToFileMessageHander( QtMsgType type, const QMessageLogContext & context, const QString & mess )
{
  QString message = qFormatLogMessage( type, context, mess ) + "\r\n";
  QMutexLocker _( &logFileMutex );

  if ( logFile.isOpen() ) {
    logFile.write( message.toUtf8() );
    logFile.flush();
    if ( type == QtFatalMsg ) {
      abort();
    }
    return;
  }
  else {
    fprintf( stderr, "log file failed to open\n!" );
    fprintf( stderr, "%s\n", message.toUtf8().constData() );
  }
}

namespace Logger {
void switchLoggingMethod( bool logToFile )
{
  QMutexLocker _( &logFileMutex );
  if ( logToFile ) {
    if ( !logFile.isOpen() ) {
      logFile.setFileName( Config::getConfigDir() + "gd_log.txt" );
      if ( !logFile.open( QFile::WriteOnly ) ) {
        qDebug() << "Failed to open log file!";
        return;
      };
    }
    qSetMessagePattern( "%{if-debug}[DEBUG ] %{endif}"
                        "%{if-info}[INFO ] %{endif}"
                        "%{if-warning}[WARNING ] %{endif}"
                        "%{if-critical}[CRITICAL] %{endif}"
                        "%{if-fatal}[FATAL ] %{endif}"
                        "%{time MM-dd hh:mm:ss} %{message}" );
    qInstallMessageHandler( logToFileMessageHander );
  }
  else {
    if ( logFile.isOpen() ) {
      logFile.flush();
    }
    qInstallMessageHandler( nullptr ); // restore the default one
  }
}

void closeLogFile()
{
  if ( logFile.isOpen() ) {
    logFile.flush();
    logFile.close();
  }
}
} // namespace Logger

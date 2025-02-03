#include "config.hh"
#include "logger.hh"
#include <QDateTime>
#include <QFile>
#include <QGlobalStatic>
#include <QMutexLocker>

QFile logFile;
QMutex logFileMutex; // Logging could happen in any threads!

void logToFileMessageHander( QtMsgType type, const QMessageLogContext & context, const QString & mess )
{
  QString strTime = QDateTime::currentDateTime().toString( "MM-dd hh:mm:ss" );
  QString message = QString( "%1 %2\r\n" ).arg( strTime, mess );
  QMutexLocker _( &logFileMutex );

  if ( logFile.isOpen() ) {
    switch ( type ) {
      case QtDebugMsg:
        message.prepend( "Debug: " );
        break;
      case QtWarningMsg:
        message.prepend( "Warning: " );
        break;
      case QtCriticalMsg:
        message.prepend( "Critical: " );
        break;
      case QtFatalMsg:
        message.prepend( "Fatal: " );
        logFile.write( message.toUtf8() );
        logFile.flush();
        abort();
      case QtInfoMsg:
        message.insert( 0, "Info: " );
        break;
    }

    logFile.write( message.toUtf8() );
    logFile.flush();

    return;
  }
  else {
    fprintf(stderr,"log file failed to open\n!");
    fprintf(stderr,"%s\n",message.toUtf8().constData());
  }
}

namespace Logger{
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
}

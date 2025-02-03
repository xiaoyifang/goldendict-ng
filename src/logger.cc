#include "config.hh"
#include "logger.hh"
#include <QGlobalStatic>
#include <QMutexLocker>
#include <QDateTime>

Q_GLOBAL_STATIC(Logger, gd_logger)

QMutex logMutex;

void logToFileMessageHander( QtMsgType type, const QMessageLogContext & context, const QString & mess )
{
  QString strTime = QDateTime::currentDateTime().toString( "MM-dd hh:mm:ss" );
  QString message = QString( "%1 %2\r\n" ).arg( strTime, mess );

  if ( gd_logger -> logFile.isOpen() ) {
    //without the lock ,on multithread,there would be assert error.
    QMutexLocker _( &logMutex );
    switch ( type ) {
      case QtDebugMsg:
        message.prepend( "Debug: " );
      break;
      case QtWarningMsg:
        message.prepend("Warning: " );
      break;
      case QtCriticalMsg:
        message.prepend( "Critical: " );
      break;
      case QtFatalMsg:
        message.prepend( "Fatal: " );
      gd_logger -> logFile.write( message.toUtf8() );
      gd_logger -> logFile.flush();
      abort();
      case QtInfoMsg:
        message.insert( 0, "Info: " );
      break;
    }

    gd_logger -> logFile.write( message.toUtf8() );
    gd_logger -> logFile.flush();

    return;
  } else {
    throw std::runtime_error( "logToFileMessageHandler fatal error!" );
  }
}

void Logger::retainDefaultMessageHandler(QtMessageHandler handler){
  gd_logger->defaultMessageHandler=handler;
}

void Logger::switchLoggingMethod(bool logToFile){
  if(logToFile){
    if(!gd_logger -> logFile.isOpen()){
      gd_logger -> logFile.setFileName( Config::getConfigDir() + "gd_log.txt" );
      if(!gd_logger -> logFile.open( QFile::WriteOnly )){
         qDebug()<<"Failed to open log file!";
        return;
      };
    }
    qInstallMessageHandler(logToFileMessageHander);
  } else {
    if(gd_logger -> logFile.isOpen()){
      gd_logger -> logFile.flush();
    }
    qInstallMessageHandler(gd_logger->defaultMessageHandler);
  }
}

void Logger::closeLogFile(){
  if(gd_logger -> logFile.isOpen()){
    gd_logger -> logFile.flush();
    gd_logger -> logFile.close();
  }
}

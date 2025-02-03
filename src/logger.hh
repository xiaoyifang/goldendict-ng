#pragma once
#include <QFile>
#include <QMutex>

/// Manage Logging, mainly for switching to log-to-file because of Windows
struct Logger {
    static void retainDefaultMessageHandler(QtMessageHandler);
    static void switchLoggingMethod( bool logToFile );
    static void closeLogFile();
    QFile logFile;
    QtMessageHandler defaultMessageHandler;
};

#pragma once

/// Manage Logging, mainly for switching to log-to-file because of Windows
namespace Logger {
void switchLoggingMethod( bool logToFile );
void closeLogFile();
}; // namespace Logger

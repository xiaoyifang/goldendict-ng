#pragma once

/// Manage Logging, mainly for switching to log-to-file because of Windows
namespace Logger {
static void switchLoggingMethod( bool logToFile );
static void closeLogFile();
}; // namespace Logger

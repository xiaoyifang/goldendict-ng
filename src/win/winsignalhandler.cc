/*
    Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file
*/

#include "winsignalhandler.hh"
#include <QDebug>

#ifdef Q_OS_WIN

WinSignalHandler * WinSignalHandler::instance = nullptr;

WinSignalHandler::WinSignalHandler()
{
  instance = this;
}

WinSignalHandler::~WinSignalHandler()
{
  uninstallHandler();
  instance = nullptr;
}

bool WinSignalHandler::installHandler()
{
  if ( !SetConsoleCtrlHandler( (PHANDLER_ROUTINE)handlerRoutine, TRUE ) ) {
    qWarning() << "Failed to install Windows console control handler";
    return false;
  }
  return true;
}

void WinSignalHandler::uninstallHandler()
{
  SetConsoleCtrlHandler( (PHANDLER_ROUTINE)handlerRoutine, FALSE );
}

BOOL WINAPI WinSignalHandler::handlerRoutine( DWORD dwCtrlType )
{
  if ( instance ) {
    // Emit the signalReceived signal with the control event type
    emit instance->signalReceived( dwCtrlType );
    
    // For CTRL_CLOSE_EVENT, we need to save data and exit quickly
    // The system gives us 20 seconds before terminating the process
    if ( dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_LOGOFF_EVENT || dwCtrlType == CTRL_SHUTDOWN_EVENT ) {
      // Perform quick cleanup operations here if needed
      // Note: We cannot do lengthy operations here as the system will terminate the process
      return TRUE; // Indicate that we have handled the event
    }
    
    // For other events like CTRL_C_EVENT, we can return FALSE to let the system handle them
    return FALSE;
  }
  return FALSE;
}

#endif // Q_OS_WIN
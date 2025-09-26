/*
    Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file
*/

#include "winsignalhandler.hh"
#include "mainwindow.hh"
#include <QDebug>
#include <QApplication>

#ifdef Q_OS_WIN

WinSignalHandler * WinSignalHandler::instance = nullptr;
static BOOL quitRequested                     = FALSE;

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

    // For events that allow time for cleanup, we can perform quick operations
    if ( dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_LOGOFF_EVENT || dwCtrlType == CTRL_SHUTDOWN_EVENT ) {
      // Set a flag to indicate quit was requested
      quitRequested = TRUE;

      // Try to get the main window and save history
      foreach ( QWidget * widget, QApplication::topLevelWidgets() ) {
        MainWindow * mainWindow = qobject_cast< MainWindow * >( widget );
        if ( mainWindow ) {
          // Save history immediately
          mainWindow->commitData();
          break;
        }
      }

      // Give the application a little time to process events
      QApplication::processEvents();

      // Return TRUE to indicate we've handled the event
      return TRUE;
    }

    // For other events like CTRL_C_EVENT, we can return FALSE to let the system handle them
    return FALSE;
  }
  return FALSE;
}

#endif // Q_OS_WIN

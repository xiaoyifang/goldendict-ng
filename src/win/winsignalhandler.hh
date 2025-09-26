/*
    Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file
*/

#pragma once

#include <QObject>

#ifdef Q_OS_WIN
  #include <windows.h>

/**
 * @brief The WinSignalHandler class handles Windows console control events
 * 
 * This class allows handling Windows console control events like CTRL_C_EVENT,
 * CTRL_BREAK_EVENT, and CTRL_CLOSE_EVENT to perform cleanup operations before
 * the application terminates.
 */
class WinSignalHandler: public QObject
{
  Q_OBJECT

public:
  /**
   * @brief Constructor
   */
  WinSignalHandler();

  /**
   * @brief Destructor
   */
  ~WinSignalHandler();

  /**
   * @brief Installs the signal handler for the specified control events
   * 
   * @return true if the handler was successfully installed, false otherwise
   */
  bool installHandler();

  /**
   * @brief Uninstalls the signal handler
   */
  void uninstallHandler();

Q_SIGNALS:
  /**
   * @brief Signal emitted when a control event is received
   * 
   * @param signal The type of control event received
   */
  void signalReceived( int signal );

private:
  /// Handler routine for console control events
  static BOOL WINAPI handlerRoutine( DWORD dwCtrlType );

  /// Pointer to the instance of WinSignalHandler
  static WinSignalHandler * instance;
};

#endif // Q_OS_WIN

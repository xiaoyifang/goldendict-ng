#pragma once

#ifdef __APPLE__

/// macOS helper for menu-bar (status item) resident mode.
class MacTray
{
public:
  /// Hides the app from the Dock and Cmd+Tab when running as a tray app.
  static void setDockIconVisible( bool visible );

  /// Brings the accessory app to the foreground when the status item is clicked.
  static void activateApplication();

  /// Registers/unregisters the app as a macOS login item.
  static bool setAutoStartEnabled( bool enabled );
};

#endif

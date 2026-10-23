#pragma once

namespace MacAppActivation {

/// Switch between a regular macOS application (Dock icon and application
/// menu) and a menu-bar accessory application.
void setDockIconVisible( bool visible );

/// Bring the application and its main menu to the foreground after switching
/// from accessory mode.
void activate();

} // namespace MacAppActivation

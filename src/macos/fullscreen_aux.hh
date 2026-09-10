#pragma once
#ifdef __APPLE__

class QWindow;

/// Makes an already-shown Qt window behave like a non-activating auxiliary
/// window that can float above fullscreen spaces (level + collection behavior).
void MakeWindowFullscreenAuxiliary( QWindow * window );

/// Configures a Qt window as a non-activating auxiliary panel without
/// ordering it front (safe to call before the window is ever shown).
void ConfigureWindowAsAuxiliaryPanel( QWindow * window );

/// Returns whether the frontmost application currently has a fullscreen
/// (edge-to-edge) window on any screen.
bool IsFrontmostAppFullscreen();

#endif

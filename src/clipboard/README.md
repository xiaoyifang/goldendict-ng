Custom clipboard listener for dealing with platform differences.

## macOS

When the app is not focus (or being the key window in Apple's term),
there is no clipboard change notification on macOS,
thus require constant polling QClipboard in background to get clipboard changes.

Code was inspired by
https://github.com/KDE/kdeconnect-kde/blob/v25.04.3/plugins/clipboard/clipboardlistener.cpp

## Wayland

TODO

Environment variable `GOLDENDICT_FORCE_WAYLAND` can be used to force GD to run in Wayland mode, like `env GOLDENDICT_FORCE_WAYLAND=1 goldendict`.

!!! danger "Don't use unless you know!"
    This flag only guarantees GD to run in wayland mode and won't crash, but nothing more.

    Enable this will break scan popup, global hotkeys and probably other things.

## Current reality

!!! note "Help wanted"
    Need help to redesign scan popup for wayland.

Scan popup is implemented with `querying mouse cursor's position` and `setting a window's absolute global position`.
Wayland does not support both by design and philosophy.

Wayland does not support registering global hotkeys until very recently, but a reasonable wayland desktop environment should provide some way to bind keys to commands globally.
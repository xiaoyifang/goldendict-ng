# Wayland Support and Configuration

GoldenDict-ng supports running both natively on **Wayland** and via **XWayland (X11)**. 

Since version 25.12.0, the application defaults to native Wayland on Wayland sessions to provide the best HiDPI and fractional scaling experience. However, due to Wayland's security design, certain features like global hotkeys and screen-grabbing popups may require switching back to X11 mode.

## Mode Selection Hardware

The display mode is determined by environment variables at startup. You can specify your preference using the following flags:

| Mode | Environment Variable | Best For |
| :--- | :--- | :--- |
| **Native Wayland** (Default) | (None) or `GOLDENDICT_FORCE_WAYLAND=1` | HiDPI screens, smooth scaling, security. |
| **XWayland (X11)** | `GOLDENDICT_FORCE_XCB=1` | Global hotkeys, advanced ScanPopup (ScanFlag). |

### How to Switch

#### 1. Native Wayland (Default)
By default, the app will try to run as a native Wayland client. If you want to force it explicitly:
```bash
env GOLDENDICT_FORCE_WAYLAND=1 goldendict
```

#### 2. XWayland / X11 Mode (Recommended for Hotkeys)
If you need global hotkeys (`Ctrl+C+C` etc.) or the "Scan Flag" feature to work across windows, run the app in X11 mode:
```bash
env GOLDENDICT_FORCE_XCB=1 goldendict
```

### Flatpak Configuration
If you are using the Flatpak version, you can use **Flatseal** or the command line to set these variables:
```bash
# Force X11 mode for Flatpak
flatpak override --env=GOLDENDICT_FORCE_XCB=1 io.github.xiaoyifang.goldendict_ng
```

---

## Technical Comparison

| Feature | Native Wayland | XWayland (X11) |
| :--- | :---: | :---: |
| **HiDPI / Sharp Text** | ⭐ Excellent | ⚠️ May be blurry |
| **Global Hotkeys** | ❌ Not Supported | ✅ Fully Supported |
| **Scan Flag (Small Flag)** | ❌ Disabled | ✅ Enabled |
| **Window Positioning** | ⚠️ Limited by compositor | ✅ Precise |
| **Input Method (IME)** | ✅ Supported (Qt 6) | ✅ Supported |

!!! danger "Global Hotkeys on Wayland"
    Native Wayland does not allow applications to listen to keyboard events globally. If you rely on `Ctrl+C+C` to trigger lookups from other apps, you **must** use X11 mode (`GOLDENDICT_FORCE_XCB=1`).

!!! tip "Workaround for Wayland Hotkeys"
    Users on KDE or GNOME can manually bind a custom system shortcut to a command that sends a word to GoldenDict, for example: `goldendict "selection"`.
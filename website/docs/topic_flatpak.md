Flatpak is a software platform that runs on top of Linux. It has idiosyncrasies by design.

This page provides some solutions to work with flatpak.
Exactly how to use Flatpak needs to refer [Flatpak's documentation](https://docs.flatpak.org/)

These solutions use [Flatseal](https://github.com/tchx84/Flatseal), but directly using the `flatpak` command or KDE's [flatpak-kcm](https://invent.kde.org/plasma/flatpak-kcm) works as well.

!!! note
    Feel free to add new solutions by clicking the edit button on the top right.

## Add external programs

Problem: Apps inside Flatpak cannot run programs on the host system directly.

Solution: Use `flatpak-spawn --host`.

First, add `org.freedesktop.Flatpak` to allowed session bus.

![](https://github.com/flathub/io.github.xiaoyifang.goldendict_ng/assets/20123683/77d16c85-a035-48d4-971e-6d71548339f6)

Then add application like `flatpak-spawn --host --directory=${the external program's pwd will be set to this value} ${The actual commands}`.

For example, [Translate Shell](https://github.com/soimort/translate-shell) can be added to GoldenDict as `flatpak-spawn --host --directory=. trans -no-ansi %GDWORD%`.

## Uses fixed paths instead of `/run/user/1000/doc/...`

Apps inside Flatpak don't have direct access to filesystem by default. A directory selected in file chooser will be exported to app as `/run/user/1000/doc/...`. The exported directories can be listed with `flatpak documents --columns=id,origin,application`

If you want to use fixed paths instead, just grant GolenDict either the whole file system, the whole home or a specific path.

Note that some special paths in the host system like `\tmp` cannot be accessed by flatpak apps in any way.

![](https://github.com/flathub/io.github.xiaoyifang.goldendict_ng/assets/20123683/40f4a401-a497-4f76-9239-b47c101aa06b)

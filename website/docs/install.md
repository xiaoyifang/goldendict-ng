<a href="https://repology.org/project/goldendict-ng/versions">
    <img src="https://repology.org/badge/vertical-allrepos/goldendict-ng.svg" alt="Packaging status" align="right">
</a>

## Download

GoldenDict-ng is available pre-built for Windows and macOS. It is available in a few Linux/Unix repos and FlatHub.

* [Latest stable version](https://github.com/xiaoyifang/goldendict/releases/latest) 
* [Pre-release test builds](https://github.com/xiaoyifang/goldendict/releases).

Because it is open source, you can always [build it for yourself](howto/build_from_source.md).

## Windows 

Choose either

* `****-installer.exe ` for traditional installer experience
* `****-installer.7z` for simply unzip and run experience

If Qt's version is not changed, you can also download a single `goldendict.exe` and drop it into previous installation's folder (If uncertain, don't do this).

Requires Windows 10 (1809 or later) with [MSVC runtime](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170#latest-microsoft-visual-c-redistributable-version) installed.


## Linux

<a href='https://flathub.org/apps/io.github.xiaoyifang.goldendict_ng'><img width='240' alt='Download on Flathub' src='https://dl.flathub.org/assets/badges/flathub-badge-en.svg'/></a>

* See the right side for available packages in various Linux distros.
* In Debian 12 and Ubuntu 23.04, `goldendict-webengine` is available (For later versions it is `goldendict-ng`).
* For ArchLinux, pre-built binary is available from [archlinuxcn's repo](https://github.com/archlinuxcn/repo/tree/master/archlinuxcn/goldendict-ng-git).
* [Gentoo package from PG_Overlay](https://gitlab.com/Perfect_Gentleman/PG_Overlay/-/tree/master/app-text/goldendict-ng)

Minimum supported "Linux" version is supposedly the current Ubuntu LTS and Debian's oldstable.

## macOS

Uses one of the `.dmg` installers in the [Download](#download).

Requires at least macOS 13.

## Versioning and Releasing

This project uses Calendar Versioning: `YY.MM.Patch`.

Releases will tentatively be done twice a year, considering factors like the major releases of Qt and the package freeze dates of Linux distros like Ubuntu.

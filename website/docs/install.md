<a href="https://repology.org/project/goldendict-ng/versions">
    <img src="https://repology.org/badge/vertical-allrepos/goldendict-ng.svg" alt="Packaging status" align="right">
</a>

## Download

Goldendict-ng is available pre-built for Windows and macOS. It is available in a few Linux/Unix repos and FlatHub.

* [Latest stable version](https://github.com/xiaoyifang/goldendict/releases/latest) 
* [Pre-release test builds](https://github.com/xiaoyifang/goldendict/releases).

Because it is open source, you can always [build it for yourself](howto/build_from_source.md).

## Windows 

Choose either

* `****-installer.exe ` for traditional installer experience
* `****.7z` for simply unzip and run experience

If Qt's version is not changed, you can also download a single `goldendict.exe` and drop it into previous installation's folder (If uncertain, don't do this).

Requires Windows 10 (1809 or later).

## Linux

<a href='https://flathub.org/apps/io.github.xiaoyifang.goldendict_ng'><img width='240' alt='Download on Flathub' src='https://dl.flathub.org/assets/badges/flathub-badge-en.svg'/></a>

* See the right side for available packages in various Linux distros.
* In Gnu Guix, goldendict-ng is available at the [ajattix repository](https://codeberg.org/hashirama/ajattix)
* In Debian 12 and Ubuntu 23.04, `goldendict-webengine` is available (For later versions it is `goldendict-ng`).
* Pre-built binary is also available from [archlinuxcn's repo](https://github.com/archlinuxcn/repo/tree/master/archlinuxcn/goldendict-ng-git).
* [Gentoo package from PG_Overlay](https://gitlab.com/Perfect_Gentleman/PG_Overlay/-/blob/master/app-text/goldendict/goldendict-9999-r6.ebuild)

Minimum supported "Linux" versions is supposedly the current Ubuntu LTS or Debian's old stable or Qt6.4.

## macOS

Uses one of the `.dmg` installers in the [Download](#download).

Requires at least macOS 12.

## Versioning and Releasing

This project uses Calendar Versioning: `YY.MM.Patch`.

Releases will tentatively be done twice a year, considering factors like the major releases of Qt and the package freeze dates of Linux distros like Ubuntu.
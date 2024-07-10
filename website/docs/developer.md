Developing GoldenDict is not hard.

If you know some C++ and optionally some Qt, you can start to modify GoldenDict right now.

This page is a brief introduction on how to get started, for more details see [how to build from source](howto/build_from_source.md).

## 1. Install Qt

If you use Linux, you already know.

To install Qt on macOS or Windows, uses the [Qt Online Installer](https://doc.qt.io/qt-6/get-and-install-qt.html). It can be downloaded from [Qt for Open Source](https://www.qt.io/download-open-source).

Those Qt components are needed

+ Qt
  + 6.7.2 (Or another version)
    + MSVC 2019 (or MSVC 2022)
    + Qt5 Compatible Module
    + Additional Libraries
      + Qt Image formats
      + Qt MultiMedia
      + Qt Positioning
      + Qt SerialPort (? super weird here, but `windepolyqt` will fail without this)
      + Qt Speech
      + Qt Webchannel
      + Qt Webengine
  + Developer and Design Tools
    + Qt Creator
    + CMake
    + Ninja

Note that MinGW is not supported.

## 2. Install a compiler

For windows, MSVC can be obtained by [installing Visual Studio's "Desktop development with C++"](https://learn.microsoft.com/cpp/build/vscpp-step-0-installation).

For macOS, install [XCode](https://developer.apple.com/xcode/).

## 3. Obtain dependencies

For Windows, pre-built libraries are included in the repo. You can also uses vcpkg which is much harder.

For macOS, install [Homebrew](https://brew.sh/) and install related packages as described in [how to build from source](howto/build_from_source.md) or search `brew install` command in [macOS release's build file](https://github.com/xiaoyifang/goldendict-ng/blob/staged/.github/workflows/release-macos-homebrew.yml).

## 4. Build

First, get GoldenDict's source code by [Cloning a repository](https://docs.github.com/repositories/creating-and-managing-repositories/cloning-a-repository).

Then choose your favorite IDE/editor and load the `CMakeLists.txt`, if unsure, just use Qt Creator.

### Qt Creator

Open `CMakeLists.txt` from Qt Creator, then you wil choose a "Kit" which is pretty much a Qt installation.

Qt Creator usually can auto detect your Qt installation. In case it doesn't, check out "Kit" settings. Note that, the compiler must be set to MSVC.

![](https://github.com/xiaoyifang/goldendict-ng/assets/20123683/27edeb6f-ae97-42c3-9c71-8d186bb4a9f6)

Then you can add CMake build flag by modifying "Current Configuration"

![](https://github.com/xiaoyifang/goldendict-ng/assets/20123683/e46102ce-706f-4678-93ba-69b9d3ecfd88)

By default everything will be built, you can disable ffmpeg, epwing...

Then, hit the "Run" button at bottom-right corner should build and run GoldenDict.

If it doesn't, click "Edit" and "build" `windeploy` target.

![](https://github.com/xiaoyifang/goldendict-ng/assets/20123683/b1aa483a-1c54-433c-a58d-f4f541107ce3)

If any `.dll` missing, copy them from winlibs to beside the built `goldendict.exe`.

### CLion
### CMake GUI
### Visual Studio
### XCode
### Command Line only
Help wanted.

## Related Things

[Qt's documentation](https://doc.qt.io/)

[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines).

Remember to enable `clang-tidy` support on your editor so that `.clang-tidy` will be respected.

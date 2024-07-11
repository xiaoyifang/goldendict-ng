Developing GoldenDict is not hard.

If you know some C++ and optionally some Qt, you can start to modify GoldenDict right now.

This page is a brief introduction on how to get started.

For technical details see [how to build from source](howto/build_from_source.md).

## 1. Install Qt

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
      + Qt SerialPort (? super weird here, but it is needed.)
      + Qt Speech
      + Qt Webchannel
      + Qt Webengine
  + Qt Creator (optional)
    + CMake
    + Ninja

Note that MinGW is not supported.

CMake and Ninja are needed.

## 2. Install a compiler

For windows, MSVC can be obtained by [installing Visual Studio's "Desktop development with C++"](https://learn.microsoft.com/cpp/build/vscpp-step-0-installation).

For macOS, install [XCode](https://developer.apple.com/xcode/).

## 3. Obtain dependencies

For Windows, prebuilt libraries will be automatically downloaded.

For macOS, install [Homebrew](https://brew.sh/) and install related packages as described in [how to build from source](howto/build_from_source.md) or search `brew install` command in [macOS release's build file](https://github.com/xiaoyifang/goldendict-ng/blob/staged/.github/workflows/release-macos-homebrew.yml).

## 4. Build

First, get GoldenDict's source code by [Cloning a repository](https://docs.github.com/repositories/creating-and-managing-repositories/cloning-a-repository).

Then choose your favorite IDE/editor and load the `CMakeLists.txt`.

If unsure, just use Qt Creator.

### Qt Creator

Open `CMakeLists.txt` from Qt Creator, then you wil choose a "Kit" which is pretty much a Qt installation.

Qt Creator usually can auto detect your Qt installation. In case it doesn't, check out "Kit" settings. Note that, the compiler must be set to MSVC on Windows.

By default everything will be built, you can disable ffmpeg, epwing...

![](https://github.com/xiaoyifang/goldendict-ng/assets/20123683/49f6a85e-50ec-4467-b0e4-cf088d218053)

Then, hit the "Run" button at bottom-right corner should build and run GoldenDict.

### Command Line only
See [how to build from source](howto/build_from_source.md).

[Qt's doc: Building projects on the command line](https://doc.qt.io/qt-6/cmake-build-on-cmdline.html)

### Visual Studio
VS2022 has CMake suppport. After opening the source code folder, VS will starts to configure CMake immediatly an failed immediatly.

You need to add Qt's path and other configure options then "save" the dialog with Ctrl+S or click one of the many "(re)configure cache" button.

![](https://github.com/xiaoyifang/goldendict-ng/assets/20123683/33a52c52-2e8a-4b8c-bb05-4a753f95ff7e)

Click run won't work, becuase the dependencies are not copied.

Simply click "install", which is actually copying dependencies.

![](https://github.com/xiaoyifang/goldendict-ng/assets/20123683/02e843b1-0842-445c-919c-75618346aaaf)

### Visual Studio Code

Install [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools).

Then open GoldenDict's folder, and the CMake extension will kicks in.

Then add Qt's path and various other options to configure arguments.

```
-DCMAKE_PREFIX_PATH={Your Qt install path}\6.7.2\msvc2019_64
```

![](https://github.com/xiaoyifang/goldendict-ng/assets/20123683/bd87155e-2e61-41d5-81e2-7bfb1f13c4c4)

### CLion

### XCode


### CMake GUI

? I have no idea how to make it work.

### LSP + Editor?

## Related Things

[Qt's documentation](https://doc.qt.io/)

[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines).

Remember to enable `clang-tidy` support on your editor so that `.clang-tidy` will be respected.

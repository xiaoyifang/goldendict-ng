## Dependencies

* C++17 compiler
* Latest QT6
* Various libraries for Linux & macOS, see below
* On Windows all the libraries are included in the repository

For debian/ubuntu, those packages are needed

```shell
libavformat-dev libavutil-dev libbz2-dev libeb16-dev libhunspell-dev \
liblzma-dev liblzo2-dev libopencc-dev libvorbis-dev \ 
libx11-dev libxtst-dev libzim-dev libzstd-dev qt6-5compat-dev \
qt6-base-dev qt6-multimedia-dev qt6-speech-dev qt6-svg-dev \
qt6-tools-dev qt6-tools-dev-tools qt6-webchannel-dev \
qt6-webengine-dev x11proto-record-dev zlib1g-dev
```

In other words, those libraries

* Qt6 (with webengine)
* ffmpeg
* libzim
* xapian
* hunspell
* opencc
* libeb
* libvorbis
* x11 (linux only)

And a few compression libraries:

* xz (lzma)
* bzip2
* lzo2
* zlib

## CMake Build

```shell
cd goldendict-ng && mkdir build_dir
# config step
cmake -S . -B build_dir \
      --install-prefix=/usr/local/ \
      -DCMAKE_BUILD_TYPE=Release
# actual build
cmake --build build_dir --parallel 7

cmake --install ./build_dir/
```

### Feature flags

Append `-D{flag_names}=ON/OFF` to cmake's config step

Available flags can be found on the top of `CMakeLists.txt`

### Windows

Install Qt6(msvc) through the standard installer and pass Qt's path to CMake

```
-DCMAKE_PREFIX_PATH=F:\Qt\6.4.1\msvc2019_64
```

The built artifacts will end up in `build_dir/goldendict`

To run the built `goldendict.exe` directly, you have to add `F:\Qt\6.5.2\msvc2019_64\bin` to your PATH environment variable

To have a redistributable goldendict (runable on someone else's computer by just copying the folder), you can build the deployment target which will copy necessary files to the folder

```
cmake --build . --target windeploy
```

The `build_dir/goldendict` will be ready to share with others.

Use`windeployqt.exe {your_build_dir}/goldendict.exe` which will copy the qt related `.dll` and other necessary files automatically.


### macOS

Similar to Linux build, but need `macdeployqt ./goldendict.app` to copy necessary dependencies to the app bundle.

## Qmake

```shell
git clone https://github.com/xiaoyifang/goldendict-ng.git
```

### Build Steps

**Notice**: All additional configs for `qmake` that must be combined in one  of pass config options to `qmake`:
```shell
qmake6 "CONFIG+=release"
make
```

For ARM macOS, `QMAKE_APPLE_DEVICE_ARCHS="arm64"` should be passed to qmake.

#### Building with Chinese conversion support

To add Chinese conversion support you need at first install libopencc-dev package, then pass `"CONFIG+=chinese_conversion_support"` to `qmake`.

```
sudo apt-get install libopencc-dev
```

#### Building with Zim dictionaries support

To add Zim formats support you need `libzim`, then pass `"CONFIG+=zim_support"` to `qmake`

**Note**: Some linux distros do not support latest zim version, so you need to compile from latest source.
On Windows,  you can use vcpkg to compile the libzim
```
vcpkg install libzim:x64-windows
```
and copy the corresponding(debug/release) library to the `winlibs/lib` folder. the zim's `include` directory to the `winlibs/include` directory.


#### Building without Epwing format support

If you have problem building with libeb-dev package, you can pass
`"CONFIG+=no_epwing_support"` to `qmake` in order to disable Epwing format support

#### Building without internal audio players

If you have problem building with FFmpeg (for example, very old linux distro), you can pass
`"CONFIG+=no_ffmpeg_player"` to `qmake` in order to disable FFmpeg internal audio player back end.

If you have problem building with Qt5 Multimedia or experience GStreamer run-time errors (for example, Ubuntu 14.04), you can pass
`"CONFIG+=no_qtmultimedia_player"` to `qmake` in order to disable Qt Multimedia internal audio player back end.

#### Building with xapian

build xapian from source, download and extract the xapian-core source code.

```
./configure
make 
make install
```

On Windows, `vcpkg install xapian:x64-windows` and copy the libs/dlls into the `winlibs`
A precompiled version of xapian lib has provided in winlibs.

On Linux,  install libxapian-dev package using package manager.

On Mac, use homebrew to install xapian `brew install xapian`

Goldendict-ng has used xapian as the default and the only one fulltext engine.

#### use iconv (recommend to enable)
use `CONFIG+=use_iconv` to enable this feature. when enabled ,iconv will be used to convert encoding other than the QTextCodec(which will be deprecated in future Qt version)

```
qmake "CONFIG+=use_iconv"
```

when enabled ,iconv should be installed on the platform at the same time.

#### use breakpad

use `CONFIG+=use_breakpad` to enable this crash dump. when enabled [breakpad](https://chromium.googlesource.com/breakpad/breakpad/+/master/docs), goldendict will generate a crash dump alongside with Goldendict in the `crash` directory.

on Windows:
`vcpkg install breakpad:x64-windows-release` and copy the installed packages into `thirdparty/breakpad` directory.
with a structure like this:
```
├─breakpad
│  ├─include
│  │  ├─client
│  │  │  └─windows
│  │  │      ├─common
│  │  │      ├─crash_generation
│  │  │      ├─handler
│  │  │      └─sender
│  │  ├─common
│  │  │  └─windows
│  │  └─google_breakpad
│  │      ├─common
│  │      └─processor
│  └─lib
```

on Mac/Linux:
[vcpkg](https://techviewleo.com/install-vcpkg-c-library-manager-on-linux-macos-windows/) can also be used or you can just install breakpad from source or use precompiled packages. 

Then enable google breakpad like this with qmake:

```
qmake "CONFIG+=use_breakpad"
```

### Build with VS2019

VS2019 support CMake project, open the source directory directly then you go.


#### build

After successful build, run WinDeployQT in the target folder (where GoldenDict.exe is located), which will copy all necessary files to this folder.
You can click on the exe to verify that the application can be run.

After all this, you can debug the application normally.

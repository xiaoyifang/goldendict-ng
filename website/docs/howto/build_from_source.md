The `release-*` and `PR-check-*` workflow files under `.github` in the source code has actual build & commands for reference.

## Dependencies

* C++17 compiler (For windows it must be MSVC)
* Latest QT6

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

## Build

Basically, you need those commands:

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

With *Ninja* (optional)

```shell
cd goldendict-ng && mkdir build_dir
# config step
cmake -S . -B build_dir -G Ninja
# actual build
cmake --build build_dir

cmake --install ./build_dir/
```

### Feature flags

Append `-D{flag_names}=ON/OFF` to cmake's config step.

Available flags can be found on the top of `CMakeLists.txt`

### Windows

Install Qt6 (MSVC) through [Qt Online Installer](https://doc.qt.io/qt-6/get-and-install-qt.html) and add Qt's path to CMake

```
-DCMAKE_PREFIX_PATH=F:\Qt\6.4.1\msvc2019_64
```

#### Make the `.exe` runable

Call `cmake --install {the cmake output folder}` will copy all necessary dependencies to correct locations.

TODO: (untested) you can also `${Qt's install path}\Qt\6.5.2\msvc2019_64\bin` and vcpkg's bin paths to your PATH environment variable

Note that using `-G Ninja` in CMake is assumed to be used. TODO: MSBuild has minor bugs for being "Multi-Config".

#### Vcpkg

vcpkg is the primary method to build GoldenDict-ng's dependencies.

There are a few ways to use it.

First, just do nothing. Without any additional CMake config options, a pre-built cached version of vcpkg will be automatically obtained and setup.

Second, install vcpkg on your local machine, then set `CMAKE_TOOLCHAIN_FILE` as described [here](https://learn.microsoft.com/vcpkg/consume/manifest-mode?tabs=cmake%2Cbuild-MSBuild#2---integrate-vcpkg-with-your-build-system), which says append `-DCMAKE_TOOLCHAIN_FILE={Your vcpkg install location}/scripts/buildsystems/vcpkg.cmake` to CMake's config step. Note that this cost long time to build


### macOS

If you build in an IDE, then the created `GoldenDict.app`  will be runnable from the IDE which set up necessary magics for you.

To make the `.app` runnable elsewhere, you can run `cmake --install build_dir/` which will invoke macdeployqt, ad-hoc code signing and various other things. The produced app will end up in `build_dir/redist/goldendict-ng.app`

To create `.dmg` installer, you have to have [create-dmg](https://github.com/create-dmg/create-dmg) installed on your machine, then also `cmake --install build_dir/`.

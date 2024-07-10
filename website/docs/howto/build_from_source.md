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

### Feature flags

Append `-D{flag_names}=ON/OFF` to cmake's config step

Available flags can be found on the top of `CMakeLists.txt`

### Windows

Install Qt6(msvc) through the standard installer and pass Qt's path to CMake

```
-DCMAKE_PREFIX_PATH=F:\Qt\6.4.1\msvc2019_64
```
The built artifacts will end up in `build_dir/goldendict`

#### Using pre-built winlibs

Use `windeploy` target to copy necessary runtime files.

```
cmake --build . --target windeploy
```

Or you can also manually run `windeployqt.exe {your_build_dir}/goldendict.exe` which will copy the qt related things to `build_dir`.

#### Using Vcpkg

The dependencies can be built via Vcpkg instead of using the pre-built ones.

Vcpkg CMake build utilize the "manifest mode", all you need to do is basically 
set `CMAKE_TOOLCHAIN_FILE` as described [here](https://learn.microsoft.com/en-us/vcpkg/consume/manifest-mode?tabs=cmake%2Cbuild-MSBuild#2---integrate-vcpkg-with-your-build-system).

Add this to cmake command:
```sh
-DUSE_VCPKG=ON
```

Most `.dll` built by vcpkg will be automatically copied, but the Qt ones won't.

You can

* run `cmake --install .` (recommended)
* manually run windeployqt
* add `${Qt's install path}\Qt\6.5.2\msvc2019_64\bin` to your PATH environment variable

Note that `-G Ninja` in CMake is assumed to be used. MSBuild has minor bugs for being "Multi-Config".

### macOS

If you build in an IDE, then the created `GoldenDict.app`  will be runnable from the IDE which set up necessary magics for you.

To make the `.app` runnable elsewhere, you can run `cmake --install build_dir/` which will invoke macdeployqt, ad-hoc code signing and various other things. The produced app will end up in `build_dir/redist/goldendict-ng.app`

To create `.dmg` installer, you have to have [create-dmg](https://github.com/create-dmg/create-dmg) installed on your machine, then also `cmake --install build_dir/`.


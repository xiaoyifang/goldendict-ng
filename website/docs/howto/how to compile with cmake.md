# Linux

```shell
cmake -S . -B build_dir \
      --install-prefix=/usr/local/ \
      -G Ninja \
      -DCMAKE_BUILD_TYPE=Release
      
cmake --build build_dir --parallel 7

 cmake --install ./build_dir/
```

# macOS

Install necessary dependencies

```shell
brew install pkg-config qt@6 bzip2 zlib \
hunspell opencc libvorbis ffmpeg
```

Use standard CMake build commands, then use `macdeployqt ./goldendict.app` to copy necessary dependencies to the app bundle.

# Windows 

## Steps

Install Qt6(msvc) through the standard installer

Pass those parameters to cmake, the path should be changed to your actual installation paths
```
-DCMAKE_PREFIX_PATH=F:\Qt\6.4.1\msvc2019_64
```

Use`windeployqt.exe {your_build_dir}/goldendict.exe` which will copy the qt related `.dll` and other necessary files automatically.

Due to the `winlibs` are built on Release mode, there are troubles to build GoldenDict on Debug mode.
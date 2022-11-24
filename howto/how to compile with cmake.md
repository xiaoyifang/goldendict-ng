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

Setup vcpkg <https://github.com/microsoft/vcpkg#quick-start-windows>

Pass those parameters to cmake, the path should be changed to your actual installation paths
```
-DCMAKE_PREFIX_PATH=F:\Qt\6.4.1\msvc2019_64
-DCMAKE_TOOLCHAIN_FILE=F:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

Use`windeployqt.exe {your_build_dir}/goldendict.exe` which will copy the `.dll` and other necessary files automatically.

## Notes

The vcpkg uses manifests mode which will read the `vcpkg.json` and the packages will be automatically install when loading `CMakeLists.txt`

https://github.com/microsoft/vcpkg/blob/master/docs/users/manifests.md
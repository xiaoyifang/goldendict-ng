# Use vcpkg to build ffmpeg(on Windows).

Steps:

1. follow the instructions at https://trac.ffmpeg.org/wiki/CompilationGuide/vcpkg

   
2. run the command 
```
vcpkg.exe install ffmpeg[core,avcodec,avdevice,avfilter,avformat,speex,avresample,mp3lame,opus,sdl2,swresample,vorbis]:x64-windows-rel 
```

3. copy dll and libs in vcpkg\installed\x64-windows-rel to goldendict's winlibs\lib\msvc

**Pros**: Can be compiled with speex.

# Alternative method 
simply download ffmpeg from the official website: https://github.com/BtbN/FFmpeg-Builds/releases
Then replace the dlls and libs in the winlibs\lib\msvc.

**Cons**: Seems to be missing libspeex or I just downloaded the wrong package.

**Pros**: Easy to manage.


## conan
  
  conan does not seem to have the libspeex option yet.


## Links worth checking
https://stackoverflow.com/a/44556505/968188

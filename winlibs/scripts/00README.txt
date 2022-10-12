FFmpeg, libao, speex, zLib build instructions
-----------------------------------

Build
=====

vcpkg install libao,libspeex,ffmpeg[core,avcodec,avformat,mp3lame,opus,speex,swresample,vorbis,fdk-aac,gpl],zlib:x64-windows-release

binaries (dlls):  ${vcpkg}/installed/bin
C headers:        ${vcpkg}/installed/include
Linker files:     ${vcpkg}/installed/lib

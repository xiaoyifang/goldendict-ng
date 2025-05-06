## Where is the ffmpeg audio player?


Since Qt6.5+, Qt uses ffmpeg as the default implementation of [QMultimedia](<https://doc.qt.io/qt-6/qtmultimedia-index.html#the-ffmpeg-backend>). So, the ffmpeg audio player option is not needed.

If you still want to use ffmpeg audio player, you can [configure `ffplay` as external program](ui_audioengine.md) or build from source with CMake flag `-DWITH_FFMPEG_PLAYER=ON`.

![alt text](img/audio-engines.png)

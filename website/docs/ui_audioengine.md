To configure external audio player: Preference --> Audio --> `Use external program`.

![external audio program](img/external-audio.png)


vlc:
```
vlc --intf dummy --play-and-exit
```

ffplay:
```
ffplay -autoexit -nodisp
```


mpv:
```
mpv --no-video --no-audio-display
```

or other audio player.

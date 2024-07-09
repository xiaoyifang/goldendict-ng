## Current Situation

GoldenDict offered a functionality to translate the word under cursor on Windows in the past, but the technique used there is old and does not work crossplatformly.

However, any OCR program that allows you to set "after capturing action" can be easily used in conjunction with GoldenDict.

A few examples are provided below, but there are many options:

## Capture2Text

Capture2Text can call executable after capturing, and you can set the executable to GoldenDict.

Detailed usage document: [Capture2Text](https://capture2text.sourceforge.net/)

[Capture2Text Download](https://github.com/xiaoyifang/Capture2Text/releases/tag/prerelease-20220806)

For example, change the Output action `Call Executable` to `path_to_the_GD_executable\GoldenDict.exe "${capture}"`

Then press <kbd>Win+Q</kbd> and select a region. After capturing a word, Capture2Text will forward the word to GoldenDict. If GoldenDict's scanpopup is enabled, it will show up.

![image](https://user-images.githubusercontent.com/105986/151507994-97ab732d-686a-47b1-b950-3b2db076ef4c.png)

The hotkeys can be configured:

![image](https://user-images.githubusercontent.com/105986/151481239-16cbb733-746c-425d-bc6c-2bb5e5a158c5.png)

Capture2Text can also obtain word near cursor without selecting a region via the "Forward Text Line Capture" by pressing <kdb> Win+W </kbd>

you may want to enable "First word only" so that only a single word would be captured

![image](https://user-images.githubusercontent.com/105986/151481312-4e9bc457-6667-4e80-95bd-6f2ad58c37e1.png)

### Use Capture2Text on Linux

Capture2Text does not have Linux version, but I have ported it to Linux <https://github.com/xiaoyifang/Capture2Text> thanks to [Capture2Text Linux Port](https://github.com/GSam/Capture2Text ) and
[sikmir](https://github.com/goldendict/goldendict/issues/1445#issuecomment-1022972220).

![2022-01-30 15-54-35屏幕截图](https://user-images.githubusercontent.com/105986/151691526-f28cc053-f6e0-4099-b677-f7a4657aa9fc.png)

## Shortcuts.app & Apple's OCR

Enable the Clipboard monitoring of GoldenDict, then create a "Shorcut" that will interactively take screnshot and change the clipboard.

![image](https://github.com/xiaoyifang/goldendict-ng/assets/20123683/3933141b-9f06-4829-8135-c69514111971)

You may also add additional capiblities like only getting the first word

![image](https://github.com/xiaoyifang/goldendict-ng/assets/20123683/d8eab075-1c4b-4e82-9515-eafd9df75489)

## Tesseract via command line

On Linux, you can combine command line screenshot then pass the output image to Tesseract then pass the text result to `goldendict`

Example with spectacle (KDE) and grim (wayland/wlroots)

```
#!/usr/bin/env bash

set -e

case $DESKTOP_SESSION in
    sway)
        grim -g "$(slurp)" /tmp/tmp.just_random_name.png
    ;;
    plasmawayland | plasma)
        spectacle --region --nonotify --background \
        --output /tmp/tmp.just_random_name.png
    ;;
    *)
        echo "Failed to know desktop type"
        exit 1
    ;;
esac

# note that tesseract will apppend .txt to output file
tesseract /tmp/tmp.just_random_name.png /tmp/tmp.just_random_name --oem 1  -l eng

goldendict "$(cat /tmp/tmp.just_random_name.txt)"

rm /tmp/tmp.just_random_name.png
rm /tmp/tmp.just_random_name.txt
```

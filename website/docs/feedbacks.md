## Suggestions & Ideas

Send them to [Discussions](https://github.com/xiaoyifang/goldendict/discussions)

## Bugs

Report any bugs & dysfunctions to [issues list](<https://github.com/xiaoyifang/goldendict/issues>)

Attach your version info in menu "About" -> "Copy version info".

GoldenDict-ng can be started with "--log-to-file" that will creates "gd_log.txt" in configuration folder and store various warnings, errors and debug messages.

On windows, try open command line and starts goldendict with `goldendict --log-to-file`.

If certain dictionary lead to problems, please attach the dict files in bug reports.

## Crash dump file

Gd-ng has built with a crash dmp handler, if you have a crash, please attach the crash dump file in bug reports.

![alt text](img/crash-dmp.png)

### Windows

Enabled by default.

upload the recent dmp file to [crash-dmp](https://github.com/xiaoyifang/goldendict/issues/new?assignees=&labels=crash-dmp&template=bug_report.md&title=crash-dmp-file)

### Linux

Not enabled by default.

Maybe you should build it from source and enable this option(`-DWITH_VCPKG_BREAKPAD=ON` ) and upload the above dmp file.

### macOS

macOS have a built-in crash reporter, copy all the info in the crash window.

[crash-dmp](https://github.com/xiaoyifang/goldendict/issues/new?assignees=&labels=crash-dmp&template=bug_report.md&title=crash-dmp-file)

## Bugs

Report any bugs & dysfunctions to [issues](<https://github.com/xiaoyifang/goldendict/issues>)

If a certain dictionary leads to problems, you **must** attach the dict files or provide download links in bug reports.

Attach your version info in the menu "About" --> "Copy version info".

## Windows

Try to gather crash dump and log files when reporting bugs.

### Crash Dump

If GD-ng crashes, upload the `.dmp` file in the `crash` folder, which is beside the main program file.

### Log File

To obtain runtime log, enable Preferences --> Advanced --> "Save debug messages to gd_log.txt in the config folder", a `gd_log.txt` will be generated in the configuration folder

Alternatively, start GD-ng with `--log-to-file` so that GD will create "gd_log.txt" in configuration folder.

## macOS

macOS has a built-in crash reporter, copy all the info from the bug report window.

## Linux

If you have no clue, search and learn how to obtain coredumps using tools provided by your distro, and/or learn how to use a debugger. Now is the time.

Noticeably, in recent years, popular distros support new technologies such as `systemd-coredump` and [Debuginfod](https://sourceware.org/elfutils/Debuginfod.html). Traditional approaches include installing debug packages or building from source.

At least, you should start the program in the terminal and upload the log.


Introduction
----------------------

Goldendict-ng has provide an option to build the application with [breakpad](https://docs.sentry.io/platforms/native/guides/breakpad/)

`CONFIG+=use_breakpad` to enable this feature.


Currently only Windows xapian has built with breakpad support.

How to analyze the dump created by breakpad.
------------

When a crash is encountered, a dump file is generated under the folder `crash`.Take the dump file as `a.dmp` for following example.

1. `dump_syms.exe GoldenDict.pdb > GoldenDict.sym`
The content of GoldenDict.sym is like this:
```
MODULE windows x86_64 904B2C52C1EC411D9D0271445CAD6DCD2 GoldenDict.pdb
INFO CODE_ID 645510C96CC000 GoldenDict.exe
```
2. create a folder such as `symbols`  and a series of folders like this:
```
GoldDict.exe
a.dmp                 (A)
symbols
└─GoldenDict.pdb    (B)
    └─904B2C52C1EC411D9D0271445CAD6DCD2   (C)
         └─GoldenDict.sym
```
- `A`   this is the dump file
- `B`   this is a folder name
- `C`   this folder takes the name from the first line of `GoldenDict.sym` file


3. anlaysis the dump file like this
```
minidump_stackwalk.exe  -s a.dmp symbols > a.txt
```
4.  check the a.txt file to find the possible crash reason. usually it will point to the actual crash line number of the source code.

Note
------------
The tools can be downloaded from [minidump-tools](https://gitee.com/wabwh/breakpad-minindump-tools/tree/master/)
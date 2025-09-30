Windows release is built with Google's [breakpad](https://github.com/google/breakpad).

The `.dmp` generated is [Microsoft's minidump format](https://github.com/google/breakpad/blob/main/docs/processor_design.md)

When a crash is encountered, a dump file is generated under the folder `crash`.Take the dump file as `a.dmp` for following example.

## Option 1: Visual Studio

1. Open the `.dmp` file with VS:
2. Click `Debug with Native Only`:
3. Click `Locate goldendict.pdb manually` -> click to a path that contains the `.pdf` file (Require exact naming of `goldendict.pdb`):

Bottom tab --> `locals` --> watch stack.

## Option 2: WinDbg 

1. Grab a modern version of WinDbg https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/:
2. Click Settings -> Debug settings -> Debugging paths -> Symbol path -> (add a path contains `goldendict.pdb`).


## Option 3: dump_syms + minidump-stackwalk

### Mozilla/Rust version (Better)

Download the exe files from

+ <https://github.com/mozilla/dump_syms>
+ <https://github.com/rust-minidump/rust-minidump>

```sh
.\dump_syms.exe goldendict.pdb > goldendict.sym

.\minidump-stackwalk.exe .\crash.dmp .\goldendict.sym > f.txt
```

### Google version

Part of Google breakpad's repo. Grab them from random places of internet (e.g. [minidump-tools](https://gitee.com/wabwh/breakpad-minindump-tools/tree/master/)).

1. `dump_syms.exe GoldenDict.pdb > GoldenDict.sym`:
The content of GoldenDict.sym is like this:
```
MODULE windows x86_64 904B2C52C1EC411D9D0271445CAD6DCD2 GoldenDict.pdb
INFO CODE_ID 645510C96CC000 GoldenDict.exe
```

2. Create a folder such as `symbols`  and a series of folders like this:

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

3. Analysis the dump file like this:
```
minidump_stackwalk.exe  -s a.dmp symbols > a.txt
```
4. Check the a.txt file to find the possible crash reason. Usually it will point to the actual crash line number of the source code.

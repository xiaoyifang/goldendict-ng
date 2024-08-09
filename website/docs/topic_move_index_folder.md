How to move the default index folder to other places?


## Windows

!!! note
    the [`mklink`](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/mklink#related-links) is built-in tool in Windows.


1. Open `cmd` as administrator
2. copy the index folder to another place,take `D:\gd-ng\index_new` for example.
3. Run `mklink /D "C:\Users\USERNAME\Application Data\GoldenDict\index" "D:\gd-ng\index_new"`
4. Run GoldenDict

## Linux

use `ln` to create a link.

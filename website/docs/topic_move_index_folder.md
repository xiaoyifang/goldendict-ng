# Move the index folder

The **index** folder stores the full-text search indexes produced by GoldenDict. For large dictionaries (e.g. Wikipedia, Wiktionary), the size of these indexes can grow very large, potentially filling up your system drive.

There are generally two approaches to deal with the growing index size:

- Enable [Portable Mode](topic_portablemode.md) so that indexes are kept in a location of your choosing.
- Move the index folder **in place** using a symbolic (soft) link or a hard link, keeping the original path intact.

This page explains the latter approach.

!!! note
    By default GoldenDict keeps its configurations and indexes in the user's home directory. To find out the exact location used on your system, see the storage layout in [Portable Mode](topic_portablemode.md).

!!! note
    The actual config / index directory path depends on your setup. It is usually `~/.goldendict`, but in an `XDG_BASE_DIRECTORY_COMPLIANCE` build it can be located under the XDG cache/data directories instead.

---

## Windows

!!! note
    The [`mklink`](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/mklink#related-links) command is a built-in tool in Windows and can be used from `cmd`.

1. Close GoldenDict.
2. Open `cmd` as administrator.
3. Copy the index folder to another place, take `D:\gd-ng\index_new` for example.
4. Delete the original index folder.
5. Run `mklink /D "C:\Users\USERNAME\Application Data\GoldenDict\index" "D:\gd-ng\index_new"`.
6. Run GoldenDict.

### Alternative: Directory junctions

`mklink /D` creates a *directory symbolic link*. If you do not have the privilege to create symbolic links (or are on an older Windows version), you can use a **directory junction** instead, which does not require administrator privileges.

A *junctional link* (or *directory junction*) is a hard link for directories: it points to the *location* of a directory rather than a *named symbol*, so it does not suffer the same permission and path-resolution restrictions as symbolic links. Because it is implemented at the NTFS file-system level, it also works on older Windows versions and does **not** require administrator privileges.

!!! note
    The `mklink /J` command below still needs to be run from the **Command Prompt (`cmd`)**, not from PowerShell (PowerShell has a differently-named `New-Item -ItemType Junction` alternative, shown further down).

To create a directory junction, use the `/J` switch instead of `/D`:

```bat
mklink /J "C:\Users\USERNAME\Application Data\GoldenDict\index" "D:\gd-ng\index_new"
```

For example, from an **administrator** Command Prompt (or a normal one, since no admin rights are required):

1. Close GoldenDict.
2. Copy the index folder to another place, take `D:\gd-ng\index_new` for example.
3. Delete the original index folder.
4. Run the `mklink /J` command above.
5. Run GoldenDict.

!!! note
    Both `mklink /D` and `mklink /J` work identically from a user-application point of view. The `/J` (junction) form is preferred when you cannot create symbolic links.

!!! tip "Using PowerShell instead of `cmd`"
    If you prefer to work in **PowerShell**, the equivalent of a directory junction is created with the `New-Item` cmdlet:

    ```powershell
    New-Item -ItemType Junction -Path "C:\Users\USERNAME\Application Data\GoldenDict\index" -Target "D:\gd-ng\index_new"
    ```

    And to remove a junction later, simply remove the link as you would any folder:

    ```powershell
    Remove-Item "C:\Users\USERNAME\Application Data\GoldenDict\index"
    ```

    !!! warning
        `Remove-Item` on a symbolic link or junction only removes the link itself, not the files/folder it points to, provided you did not previously follow the link. To be safe, check the target before deleting.

!!! warning
    Do **not** put the target folder (e.g. `D:\gd-ng\index_new`) *inside* the folder being linked, otherwise you may create an infinite recursion. Keep them on separate paths.



## Toolbar
![toolbar](img/toolbar.webp)

Type your word in Search Box and press `Enter` to search word in the current selected group. You can also choose a variant from a matches list.

Holding Ctrl or Shift will display the translation result in a new tab.

### Wildcard matching

The search line can use wildcard or glob symbols for matching words.

| Wildcard | Description                                                            |
|----------|------------------------------------------------------------------------|
| `?`      | Matches any single character.                                          |
| `*`      | Matches zero or more of any characters.                                |
| `[abc]`  | Matches one character given in the bracket.                            |
| `[a-c]`  | Matches one character from the range given in the bracket.             |
| `[!abc]` | Matches one character that is not given in the bracket.                |
| `[!a-c]` | Matches one character that is not from the range given in the bracket. |
| `\`      | Escaping wildcard symbols, e.g. `\?` to search `?`                     |

!!! note
    The wildcard symbol in the first character leads to scanning of every dictionary's every word and may take a long time.

More information about wildcard matching can be found in [Wikipedia's glob article](https://en.wikipedia.org/wiki/Glob_(programming)).

## Dictionary Bar

The dictionary bar shows dictionaries from the current group.

Click the icons to select/unselect them.

### Single Selection Mode (was Solo Mode)

When this mode is active, only one single dictionary can be selected at a time.

There are two ways to use this feature:

- ++shift+"Click"++ a dictionary icon to enter the mode
- ++shift+"Click"++ the selected dictionary to reselect dictioanries selected before entering the mode.

and

- ++ctrl+"Click"++ a dictionary icon to enter the mode
- ++ctrl+"Click"++ the selected dictionary to reselect all dictionaries

For example, there are 3 dictionaries A,B,C with A,B initially selected.

| Clicking Sequence                                                   | Outcome                                                      |
|---------------------------------------------------------------------|--------------------------------------------------------------|
| ++ctrl+"Click"++ + A                                                | select A only                                                |
| ++ctrl+"Click"++ + A --> ++ctrl+"Click"++ + A                       | select A --> reselect all A,B,C                              |
| ++ctrl+"Click"++ + A --> ++"Click"++ + B --> ++ctrl+"Click"++ + B   | select A --> select B --> reselect all A,B,C                 |
| ++shift+"Click"++ + A --> ++"Click"++ + B --> ++shift+"Click"++ + B | select A --> select B --> reselect the initally selected A,B |

!!! note
    This can also be used in the "Found in dictionaries" panel to temporarily focus on a single dictionary.

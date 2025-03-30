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

### Temporary Selection (was Solo Mode)

Focus on a single dictionary temporally.

+ ++ctrl+"Click"++ a dictionary --> select a single dictionary.
+ ++ctrl+"Click"++ again on another unselected dictionary --> select that single dictionary.
+ ++ctrl+"Click"++ again on the selected dictionary --> reselect all dictionaries.

To reselect the initially selected dictionaries before the continous clicking with ++ctrl++ or ++shift++, uses ++shift+"Click"++ instead .

For example, there are 3 dictionaries A,B,C with A,B initially selected.

| Clicking Sequence                                                       | Outcome                                                     |
|-------------------------------------------------------------------------|-------------------------------------------------------------|
| ++ctrl+"Click"++ + A                                                    | select A only                                               |
| ++ctrl+"Click"++ + A --> ++ctrl+"Click"++ + A                           | select A first --> all A,B,C reselected                     |
| ++ctrl+"Click"++ + A --> ++shift+"Click"++ + A                          | select A first --> reselect the inital selection A,B        |
| ++ctrl+"Click"++ + A --> ++ctrl+"Click"++ + B --> ++ctrl+"Click"++ + B  | select A --> select B --> then reselect all                 |
| ++ctrl+"Click"++ + A --> ++ctrl+"Click"++ + C --> ++shift+"Click"++ + A | select A --> select C --> reselect the inital selection A,B |

Note: This can also be used on the "Found in dictionaries" panel.


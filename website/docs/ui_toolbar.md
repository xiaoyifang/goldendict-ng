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

### Single Selection

++ctrl+"Click"++ will focus on a single dictionary.

If a dictionary is the only one selected, clicking it with ++ctrl++ will reselect all dictionaries.
### Temporary Selection

Temporarily capture the selection and restore it later.

- Capture Selection <-- ++shift+"Click"++ any dictionary icons.
- Restore Selection <-- Click the "Restore selection" in the right click context menu

!!! note
    "Found in dictionaries" panel can also uses above two special operations.

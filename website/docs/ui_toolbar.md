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

The dictionary bar contains all dictionaries from the current dictionaries group. Click the icons to disable/enable them.

### Quick Selection mode

Temporally focus on a single dictionary and restore back to all dictionaries or previously selected dictionaries.

To enter the mode:

++ctrl+left-button++ -> Select a single dictionary.
++ctrl+left-button++ -> Reselect all dictionaries.

To exit the mode:
++shift+left-button++ -> Restore all the selected dictionaries before entering the mode

For example, there are 4 dictionaries A,B,C,D with ABC selected.

| Cases                                    | Note                                         |
|------------------------------------------|----------------------------------------------|
| Ctrl+Click A                             | select A only                                |
| Ctrl+Click A, Ctrl+Click B               | select B only                                |
| Ctrl+Click A, Ctrl+Click A               | A,B,C,D selected (all dictionaries selected) |
| Ctrl+Click A, Shift+Click any dictionary | A,B,C selected                               |

Note: This can also be used on the "Found in dictionaries" panel.


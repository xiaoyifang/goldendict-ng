The full-text search dialog can be opened via menu "Search" or "Ctrl+Shift+F".

Full-text search allow to search words or sentences not in dictionary headwords but in articles text of dictionaries from current dictionaries group.

![full text serach](img/fulltext.png){ width="450" }

Type the desired word in "Search line" to search.

Search modes

* "Default" — This follows the [xapian search syntax](https://xapian.org/docs/queryparser.html). 
Note that phrase and NEAR search needs the positional information when indexing.  The positional information will make the xapian index file too big.  The program has used a balanced way to enable or disable the positional information automatically based on the total document length.which means the positional information will not be disabled when the ditionary has reached the limit.
* "Plain text" - mode like "Whole words" but every word in search line can be treated as word fragment.
* "Wildcards" - As xapian index only support wildcard syntax like this  "hell*" ,  the wildcard in the middle(eg."he*lo") is not supported.

"Available dictionaries in group" - here you can see how many dictionaries in the current group are suitable for full-text search, how many dictionaries are already indexed and how many dictionaries are waiting for indexing.

When you hover the cursor over a headword in the results list, a tooltip appears with a list of dictionaries that contain the search term.

!!! note
    The dictionary will index for full-text search in background and started immediately after program start, name of the currently indexing dictionary is displayed in the status line. This process can take a long time and require many computing resources.You may turn off indexing for huge dictionaries like Wikipedias or Wiktionaries in preferences. To find dictionary which can't be indexed check GoldenDict with `--log-to-file` or check `stdout`.

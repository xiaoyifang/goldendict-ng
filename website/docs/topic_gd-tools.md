# GoldenDict tools

A set of helpful programs to enhance goldendict for immersion learning.


# prerequisite
1. install [gd-tools](https://codeberg.org/hashirama/gd-tools) and configure it according to its README

# features:
- japanese sentence spliting, making each part of the sentence clickable
![Alt](https://codeberg.org/hashirama/gd-tools/raw/branch/main/misc/marisa.gif)

## How to setup:
Open GoldenDict, press "Edit" > "Dictionaries" > "Programs" and add the installed executables. Set type to html. Command Line: gd-tools <name of the program> --word %GDWORD% --sentence %GDSEARCH%. Optionally add arguments, such as: gd-tools marisa --word %GDWORD% --sentence %GDSEARCH% . These programs are treated as dictionaries and you can add them under "Dictionaries" or "Groups".
<br><br>
please notice that gd-tools does works in windows, and we have an [installer](https://www.mediafire.com/file/h1v7owj7np9j7wg/gd-tools_windows.zip/file) for it, you can install and then come back to the previous instruction.
And if you're at Gnu Guix, install it from our [channel](https://codeberg.org/hashirama/ajattix) <br><br>
other features:
- kanji stroke order: for those who want to know how to write a character
- image searching
and much more, please see our list [here](https://codeberg.org/hashirama/gd-tools/src/branch/main/README.md#table-of-contents)

# Misc
we have a mandarin version of gd-marisa, which relies on mecab (unix only) : <br><br>
![image](https://codeberg.org/hashirama/gd-tools/raw/branch/main/misc/mandarin.png)

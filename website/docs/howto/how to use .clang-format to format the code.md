# Goal


the project has included a .clang-format as the code guideline.

# How to use this file


## CommandLine

Stash changes via `git add files` then `git-clang-format`

<https://clang.llvm.org/docs/ClangFormat.html#git-integration>

## QtCreator:

Check the steps in the following webpage.
https://doc.qt.io/qtcreator/creator-indenting-code.html#automatic-formatting-and-indentation

## Visual Studio(Newer Version)

visual studio will automatically detect this file and apply the format guideline.


# Fix  warnings reported by SonarCloud

After PR submitted , a SonarCloud analysis will take place. Fix all the warnings introduced by your PR.
Previous code may also have many warnings ,can be left alone.
![image](https://user-images.githubusercontent.com/105986/226776188-e23c4da0-4ea5-4c53-86eb-5a3da971b691.png)


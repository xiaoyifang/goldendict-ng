Developing GoldenDict is not hard.

If you know some C++ and optionally some Qt, you can start to modify GoldenDict right now:

## Install Qt and QtCreator
  (On Linux, install dependencies)

  Windows(qtcreator for example)
### Prerequisite
Install visual studio community ,choose C++ component.
  
QtCreator Packages:
```
[x]qtX.X.X version
[x]MSVC2019 /GCC
[x]Qt5 Compatible Module
[*]Additional
  [x]Qt Image formats
  [x]Qt MultiMedia
  [x]Qt Positioning
  [x]Qt speech
  [x]Qt webchannel
  [x]Qt webengine
```

## Coding Standards

Please follow [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) and write modern C++ code.

Commit messages should follow [Conventional Commits](https://www.conventionalcommits.org)

Reformat changes with `clang-format` [how to use clang-format](https://github.com/xiaoyifang/goldendict/blob/staged/howto/how%20to%20use%20.clang-format%20to%20format%20the%20code.md)

Remember to enable `clang-tidy` support on your editor so that `.clang-tidy` will be respected.

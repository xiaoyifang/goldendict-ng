Developing GoldenDict is not hard.

If you know some C++ and optionally some Qt, you can start to modify GoldenDict right now:

* Install Qt and QtCreator
* (On Linux, install dependencies)
* Load `goldendict.pro`
* Modify some code
* Hit the `Run`.

A CMake build script is also provided `CMakeLists.txt` is provided which can be used directly in other IDEs like CLion or Visual Studio 2022.

## Coding Standards

Please follow [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines) and write modern C++ code.

Commit messages should follow [Conventional Commits](https://www.conventionalcommits.org)

Reformat changes with `clang-format` [how to use clang-format](https://github.com/xiaoyifang/goldendict/blob/staged/howto/how%20to%20use%20.clang-format%20to%20format%20the%20code.md)

Remember to enable `clang-tidy` support on your editor so that `.clang-tidy` will be respected.

## Architecture

What's under the hood after a word is queried?

After typing a word into the search box and press enter, the embedded browser will load `gdlookup://localhost?word=<wantted word>`. This url will be handled by Qt webengine's Url Scheme handler. The returned html page will be composed in the ArticleMaker which will initiate some DataRequest on dictionary formats. Resource files will be requested via `bres://` or `qrc://` which will went through a similar process.

TODO: other subsystems.
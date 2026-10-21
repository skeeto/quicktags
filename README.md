# ctags

A fast ctags for C, C++, and Fortran, made for [w64devkit][] and Vim. Its
tags files match [Universal Ctags][] 6.2.1 where that is correct, with a
`line:` field on each tag, and fix a number of its mistakes.

    $ ctags -R
    $ ctags -R --kinds-C=+p -f tags.local src
    $ ctags -R --extras=+q

Options: `-R` (`--recurse`), `-f`/`-o FILE` (`-` for standard output),
`--kinds-LANG=[+|-]KINDS` for C, C++, and Fortran (also `--LANG-kinds`),
`--extras=[+|-]EXTRAS` (`q` adds C and C++ tags qualified by scope, such
as `ns::Class::name`; `f` adds a tag for each file; `F` and `p` drop file-scoped tags and pseudo-tags
when turned off), and `--jobs=N`. `--help` lists the kinds. Files are
tagged by extension, as Universal Ctags does.

On a 24-thread machine it tags llama.cpp in 0.1 seconds and llvm-project
(800 MB in 77,000 files) in 1.3 seconds, where Universal Ctags takes 5
and 39 seconds. Where it departs from Universal Ctags, and why, is in
`docs/spec.md`, section 4.

## Build

No dependencies, and on Windows no C runtime:

    $ c++ -std=c++20 -O2 -nostartfiles -static-libgcc -Wl,--stack,0x800000 -s -o ctags.exe main_win32.cpp
    $ c++ -std=c++20 -O2 -pthread -o ctags main_posix.cpp

Or with CMake, which also runs the tests:

    $ cmake -B build && cmake --build build && ctest --test-dir build

This is free and unencumbered software released into the public domain.

[w64devkit]: https://github.com/skeeto/w64devkit
[Universal Ctags]: https://ctags.io/

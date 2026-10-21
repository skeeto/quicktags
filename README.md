# quicktags

Quicktags is a fast ctags for C, C++, and Fortran, made to replace
[Universal Ctags][] in [w64devkit][], for Vim. It installs as `ctags`,
and its tags files match Universal Ctags 6.2.1 where that is correct,
with a `line:` field on each tag, and fix a number of its mistakes.

    $ ctags -R
    $ ctags --kinds-C=+p -f tags.local src
    $ ctags -R --extras=+q --fields=+iaS --exclude=build
    $ ctags -a src/main.c
    $ ctags -e -R

It takes Universal Ctags' options, with its grammar and messages, so the
command lines of editor plugins and build scripts work unchanged:
tagbar, vista, ctrlp, taglist, fzf, fzf-lua, LeaderF, easytags,
gutentags, OmniCppComplete, Kate, Emacs, and Linux's `make tags`. Those
for features it lacks that would change the tags, such as `--regex-C`,
are refused, and those naming its other languages do nothing. `--help`
describes the rest, and lists the kinds.

A directory is always tagged recursively, so `ctags src` is `ctags -R
src`. `-a` replaces the tags of the files tagged again, rather than
adding to their stale ones. `--exclude`, `-L` file lists, `--languages`,
`--langmap`, `--fields` (`+iaS` for inheritance, access and signatures),
`--excmd`, `--sort`, `--tag-relative`, and Emacs `TAGS` files with `-e`
work as in Universal Ctags. Files are tagged by extension, as Universal
Ctags does.

It is about 30 times faster than Universal Ctags on a 24-thread machine,
and 4 to 9 times faster on one thread. It tags llama.cpp in 0.16 seconds
and llvm-project (800 MB in 77,000 files) in 1.5 seconds, where
Universal Ctags, limited to the same languages, takes 5 and 38 seconds.
Where it departs from Universal Ctags, and why, is in `docs/spec.md`,
section 4.

## Build

No dependencies, and on Windows no C runtime. The program is named
`ctags`, as editors and scripts expect:

    $ cc -O2 -nostartfiles -Wl,--stack,0x800000 -o ctags.exe main_win32.cpp
    $ cc -O2 -pthread -o ctags main_posix.cpp

Or with CMake, which also runs the tests:

    $ cmake -B build && cmake --build build && ctest --test-dir build

This is free and unencumbered software released into the public domain.

[w64devkit]: https://github.com/skeeto/w64devkit
[Universal Ctags]: https://ctags.io/

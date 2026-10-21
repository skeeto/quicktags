# CLAUDE.md

A fast ctags for C, C++, and Fortran, for Vim, written for w64devkit. Its
output matches Universal Ctags 6.2.1 (`--fields=+n`) wherever that is
correct, and departs from it where it is wrong. Public domain.

## Style

C++20 in the skeeto style (load the `skeeto-style` skill before editing):
unity build, arenas, counted `Str`, signed `iz` sizes, no `std::`, no
headers in the core, no CRT on Windows. `base.cpp` and `wide.cpp` come
from ssg (`~/src/skeeto.github.com/_ssg/`); keep them close to it.

## Build and test

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build -j24 -L '^unit$'    # unit tests, import check, cases
ctest --test-dir build -j24 -L units       # differential, Universal Ctags Units
ctest --test-dir build -j24 -R units/parser-c.r/c-sample.d --output-on-failure
```

- Corpus tests (opt-in): configure with `-DCTAGS_CORPUS_LLAMA=~/src/llama.cpp
  -DCTAGS_CORPUS_LLVM=~/src/llvm-project -DCTAGS_CORPUS_REUSE_REF=ON`, then
  `ctest -L corpus` (corpus/llama, corpus/llvm/TOPDIR). `CTAGS_UNITS_ONLY`
  and `CTAGS_CORPUS_ONLY` (e.g. `C:d,C++:d` or `Fortran:*`) restrict the
  comparison to some kinds while a parser is incomplete.
- Tag cases (`TagCase` in `main_test.cpp`) are the main unit tests of the
  parsers: a source, options, and exact output. `cases/uctags` exports
  them and checks SHARED ones against Universal Ctags' real output.
- 32-bit: `cmake -B build32 -G Ninja -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++`.
- XP: `sh test/xp.sh` builds on the XP laptop over ssh with the
  one-command builds at the top of `ctags.cpp`, which must keep working
  without CMake.
- Debug builds trap on undefined behavior and failed asserts (UBSan with
  `-fsanitize-trap`); run them under gdb to find the line.

## Layout

- `ctags.cpp`: the core; includes the modules, declares the platform API
  (`os_*`), and holds `ctags_main`.
- `kinds.cpp`, `options.cpp`: kind tables and the command line.
- `tags.cpp`: tag lines, patterns, escaping, anonymous names.
- `walk.cpp`: discovery and the worker threads (a lock-free work stack).
- `output.cpp`: per-worker sorting, merging, writing the tags file.
- `main_win32.cpp`, `main_posix.cpp`, `main_test.cpp`: platform layers.
- `cxx.cpp`, `fortran.cpp`: the parsers, called by `parsefile` in
  `ctags.cpp`, which retries a failed first pass with the second pass's
  rules.
- `test/`: unit tests (`*_test.cpp`, included by `main_test.cpp`, one
  CTest per group listed in `CTAGS_TEST_GROUPS`), `vfs.cpp` (the test
  platform), `tagdiff.cpp` (differential comparison), `ledger.txt`.
- `docs/spec.md`: Universal Ctags' rules in our words, with rule IDs, and
  our departures from it. Read the relevant section before porting a
  behavior.

## Parity with Universal Ctags

Universal Ctags is GPL-2. Port its *behavior*: read its source
(`~/src/w64devkit/source/ctags-6.2.1.tar.gz`, `tar -xzOf`) to learn the
rules, write them into `docs/spec.md` in our own words, then implement
them in our own structure and names. Never copy code, identifiers, or
comments. Its test inputs (Units) are extracted into the build tree at
configure time and never committed.

The differential tests compare against
`ctags --options=NONE --languages=C,C++,Fortran --fields=+n`. Every
difference must be either fixed or recorded in `test/ledger.txt` with a
reason. Triage a difference by minimizing it, then:

- Universal Ctags is right: add a unit case with its output, then fix ours.
- Universal Ctags is wrong (a bogus or lost tag, an unmatchable pattern,
  a wrong `file:`): fix it in its own commit, with a unit case recording
  both outputs and a ledger entry.
- Undecidable (macro-generated code): keep Universal Ctags' behavior.

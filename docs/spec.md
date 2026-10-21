# Behavioral specification

This program's output matches Universal Ctags 6.2.1, run as

    ctags --options=NONE --languages=C,C++,Fortran --fields=+n

wherever that output is correct, and departs from it where it is not.
This document records Universal Ctags' rules for C, C++, and Fortran, in
our own words, as established by reading its source and running it (it
is GPL-2; no code or comments of it are copied here or in the program).
Rule IDs (CLI-1, CXX-12, ...) name the rules in tests and commits.

Markers: **[V]** observed by running Universal Ctags 6.2.1 (the w64devkit
build); **[S]** read from its source; **[I]** inferred, unverified.
Paths like `parsers/cxx/cxx_parser.c` refer to its source tree.

Departures are listed in section 4, and noted ("Here ...") under the
rules they depart from.

## 1. Universal Ctags rules (6.2.1 defaults, w64devkit build)

### 1.0 Parity target

- **P-1 [V]** The reference is `ctags --options=NONE --languages=C,C++,Fortran --fields=+n`. Plain `ctags -R` also tags 20+ other languages, CUDA, and `.h` files it judges to be Objective-C (`vendor/miniaudio/miniaudio.h` in llama.cpp), which this program never does.
- **P-2 [V]** For differential runs, write the reference to a file (`-f FILE`): with `-f -`, Universal Ctags writes through a temporary file that leaks when the pipe closes early. Strip `\r` and pseudo-tags before comparing. `--options=NONE` prints a Notice on stderr.

### 1.1 CLI

- **CLI-1 [S]** (`options.c cArgRead`, 927-960).
  - An argument starting with `--` is a long option, split at the first `=`; with no `=`, the value is `""`.
  - An argument starting with `-` is a cluster of short options. The letters `fohiILpdDb` take a parameter, either the rest of the cluster or the next argument (`-f-`, `-Rf-`, `-ftags` all work [V]).
  - Anything else is a file.
  - Long option names are exact and case-sensitive: `--recur` and `--RECURSE` are FATAL [V].
  - `--` is FATAL `Unknown option: --` [V].
- **CLI-2 [V]** `-R`, `--recurse[=bool]`.
  - Booleans: `""`, `1 y yes on true` are true; `0 n no off false` are false, compared case-insensitively. Anything else is FATAL.
  - No files and no `-R`: FATAL `ctags.exe: No files specified. Try "ctags.exe --help".`, exit 1.
  - No files with `-R`: recurse into `"."`.
- **CLI-3 [V]** `-f`/`-o FILE`.
  - `-` or `/dev/stdout` means stdout; the default file is `tags`.
  - A name starting with `-` that is longer than one character is FATAL `output file name may not begin with a '-'`.
  - Repeating the option gives a warning naming the letter given, `-o option specified more than once, last value used`, and the last value wins, unchecked for a leading `-`.
  - Output to an existing directory is FATAL; so is overwriting a file that is not a tags file (G9) [S].
  - A file is a tags file (`entry.c isTagFile`) when it is empty, or its first line, up to a NUL, is an etags section's start (`\f`, then CR or LF), or a tag line: a name not starting with `#`, exactly one tab, a file not ending in `;`, exactly one tab, and an address up to the first CR or LF that starts with `/` or `?`, or is digits up to its first `;` (`12`, `12;"\tv`). So `int a;\t/* x */` and `#define X\t1` are not [S].
- **CLI-4 [V]** `--kinds-<LANG>=SPEC`.
  - The language name is case-insensitive (`c++`, `C++`, `fortran`). There are no aliases: `--kinds-cpp` gives WARNING `Unknown language "cpp" in "kinds-cpp" option` and is ignored.
  - SPEC grammar:
    - A leading `*` enables all kinds.
    - A first character other than `+`, `-` or `*` first disables all kinds.
    - `+` and `-` switch the mode for the letters that follow (`+p-d`).
    - `{longname}` is case-sensitive. Within braces `+` and `-` are part of the name, and an unclosed `{name` at the end is ignored. A `{` inside braces or a `}` outside them is FATAL `unexpected character in kind specification: '}'`.
    - An empty SPEC disables all kinds of that language.
    - An unknown letter or name gives WARNING `Unsupported kind: 'Q' for --kinds-C option` (the name without braces) and is ignored.
    - The file kind, `F` or `{file}`, is one kind that every language shares (`parse.c defaultFileKind`), off by default. Naming it in any language's SPEC, Python's as well as C's, enables or disables it for all, and resets (`*`, or a first character other than `+` or `-`) leave it alone. The inputFile extra's `F` tags are written only while it is enabled, and enabling or disabling that extra (`--extras`, `--extras-all`, `--file-tags`) sets it to the same state, so the last of the two options wins: `--kinds-C=-F --extras=+f` writes `F` tags, `--extras=+f --kinds-python=-F` does not [V].
    - Every language also has the placeholder kind, `' '` or `{ghost}`, which makes no tags; naming it does nothing, without a warning [V].
  - The old forms `--c-kinds=` and `--c++-types=` also work. The language is everything before the first `-`.
  - `--kinds-all=` accepts only `*`, `F`, `{file}` (OPT-10).
  - Options naming other languages: OPT-4.
- **CLI-5 [V]** C/C++ kind sharing. The letters `d e f g h l m p s t u v x z L D` are **shared** between C and C++ in both directions, so `--kinds-C=+p` turns on prototypes in `.h` files. The C++-only letters `c n A N U Z M P` are independent. CUDA is independent.
- **CLI-6 [V]** Options may follow files and apply to later files only. initOnly options after a file are FATAL: `-a -e -f -o -u -x`, `--sort`, `--pattern-length-limit`, `--tag-relative`, `--format`, `--output-format`, `--list-*`, … The full grammar of the options is section 1.12.
- **CLI-7 [V]** Exit status:
  - 0 with warnings (for example, a missing input: `ctags.exe: Warning: cannot open input file "x" : No such file or directory`);
  - 1 on FATAL.
  - Message format: `<exe basename>: ` + (`Warning: `|`Notice: `|``) + message [+ ` : strerror`].
- **CLI-8 [S]** Option files are read from `$XDG_CONFIG_HOME/ctags/*.ctags` or `$HOME/.config/ctags`, `$HOME/.ctags.d`, `%HOMEDRIVE%%HOMEPATH%\ctags.d`, `./.ctags.d`, `./ctags.d`. None exist on this machine [V]. `--options=NONE` skips them. The `CTAGS` environment variable is never read.
- **CLI-9 [S]** Defaults that matter:
  - `--sort=yes`, `--excmd=mixed`, `--pattern-length-limit=96`, `--tag-relative=no`, `--format=2`;
  - extras `F anonymous pseudo(file only) subparser`;
  - fields `N F P k s t f` (+`T`, which is used only for file-kind tags).

### 1.2 Discovery

- **DISC-1 [S]** Each argument, each `-L` entry (DISC-6) and each recursed entry goes through `createTagsForEntry` (`main.c` 202-225), in this order:
  1. Early exclude check on the name (DISC-3), skipped while any exception is given.
  2. Symlink check, with `--links=no` (DISC-7). It never fires on Windows (`lstat` is `stat`).
  3. Missing: WARNING `cannot open input file`.
  4. Directory: recurse if `-R`, else silently ignore. (Here a directory
     is always recursed, D8.)
  5. Not a regular file: ignore.
  6. Late exclude check, with the exceptions.
  7. Language detection, then parse.

  Command-line files and recursed files behave identically [V]. With no files and no `-L`, `-R` recurses into `.` directly, so the steps, and the exclude checks among them, never see `.` itself.
- **DISC-2 [V]** Traversal:
  - Order is raw `readdir`/`FindNextFile` order, depth-first, with each directory descended where it appears.
  - Only `.` and `..` are skipped; **hidden and dot entries are walked**.
  - Child path:
    - If the parent is exactly `"."`, the child is the bare entry name.
    - Otherwise it is `parent + "/" + name`, where `/` is added only if the parent does not end in `:`, `/` or `\`.
  - Examples:

    | Argument | Printed path |
    |---|---|
    | (none) or `-R .` | `ggml.h` |
    | `./` | `./CMakeLists.txt` |
    | `test/` | `test/hlcat.cpp` |
    | `x86//` | `x86//cpu-feats.cpp` |
    | `.\test` | `./test/hlcat.cpp` |

  - The w64devkit patch accepts trailing separators on directory arguments.
- **DISC-3 [V][S] Exclusion** (`options.c isExcludedFile`, `strlist.c`).
  - Two lists of patterns: `--exclude`, which starts as the default list, and `--exclude-exception`, which starts empty. The default list:
    `*.a *.class *.dll *.exe *.gcda *.gcno *.lib *.o *.obj *.pyc *.pyo *.so *~ .*.swp .DS_Store .arch-ids .arch-inventory .bzr .bzrignore .cvsignore .deps .dvi .git .gitattributes .gitignore .hg .hgignore .svn BitKeeper CVS EIFGEN PENDING RCS RESYNC SCCS _darcs autom4te.cache {arch}`
  - `--exclude=PATTERN` adds a pattern, and on Windows its `\` become `/`. `--exclude=@FILE` adds the lines of FILE as they are: split at LF, each without its trailing whitespace (a CR among it), and kept unless that leaves it empty, leading blanks and a leading `#` included. So on Windows their `\` stay escapes, and Vim `wildignore` entries written with them, like `build\*`, match nothing (here they become `/` too, D12). A FILE that cannot be read is FATAL `cannot open "FILE" : REASON`; `@` alone names the empty path (`Invalid argument` on Windows; here `No such file or directory`). An empty value, `--exclude=`, empties the list, the default patterns too. `--exclude-exception` takes the same three forms for its own list.
  - A path is excluded when its base name, or else the whole path as constructed (DISC-2), matches an `--exclude` pattern, and neither the base name nor the path matches an exception. On Windows, `\` in the path is taken as `/`, and the pattern and the path are uppercased before matching.
  - Matching is gnulib's `fnmatch(pattern, name, 0)` (DISC-8), so `*` and `?` match `/`, and a leading `.` is not special.
  - When (DISC-1): before anything else, unless any exception is given, an excluded entry is dropped, so a missing file that is excluded gets no warning, and an excluded directory is not descended. A regular file is checked again, now with the exceptions, just before it is parsed. Directories are checked only by the first step, so **[V] Bug:** with any exception given, excluded directories are walked: `--exclude=sub --exclude-exception=nothing -R` tags `sub/s.c` and `sub/deep/d.c`. Here every entry is checked once, with the exceptions, before it is examined, so an excluded directory is pruned unless an exception matches it, and a missing file that is excluded gets no warning, exceptions or not (D11).
  - Arguments are checked as given, `.` too: `--exclude=.*` excludes `.` and every path starting with `.`, so `ctags -R --exclude=.* .` tags nothing, while `ctags -R --exclude=.*` tags everything but the dot entries [V].
  - No corpus directory matches the case-insensitive list except `.git`, which I checked with `find`.
- **DISC-4 [S][I]** `--maxdepth` defaults to `0xffffffff`; the top directory counts as depth 1. There is no loop detection on Windows.
- **DISC-5 [V]** The printed file name is the argument or constructed path **verbatim**, with only `\`→`/` at output time. `//`, `./` and `..` are kept, and so is case. Absolute `C:\…` prints as `C:/…`. `--tag-relative` changes this (OPT-18).
- **DISC-6 [S][V] `-L FILE`** (`main.c createTagsFromListFile`, `args.c`) names a list of inputs, read from standard input for `-`.
  - Its entries come after the command line's files. A FILE that cannot be opened is FATAL `cannot open list file "FILE" : REASON`, once the command line's files are tagged.
  - **[V] Bug:** a `-L` that follows a file name is parsed (`--verbose` shows it), but its list is never read, nor opened: `ctags a.c -L list` tags `a.c` alone, and exits 0 when `list` does not exist. `main.c` tests for a list again after the files, but declares `Option` const, so this build apparently keeps the value it read before them. Here such a list is read as any other (D13).
  - The list is read as an option file is (OPT-14): one argument per line at LF, CR or CRLF, trimmed of whitespace, blank lines and those whose first non-blank is `#` skipped. A line of only blanks is an empty name, which warns `cannot open input file ""`.
  - A line starting with `-` is an option, applying to the entries after it, with the ordering rule of CLI-6: an initOnly option after an entry, in the list or on the command line, is FATAL `-f option may not follow a file name`. Any other line is an entry, which goes through DISC-1, so a directory is ignored without `-R`, and `-R` in the list does not make it recurse.
  - `-L` in the list warns `-L option specified more than once, last value used`, and is ignored, as the list being read is the only one read.
  - With a list, there are files, even if the list is empty, so `-R` does not recurse into `.` (CLI-2), and no files is not an error.
  - Here the list is read once the command line is parsed, before anything is tagged, so a list that cannot be read is FATAL first. Its options act as those of the command line do, on every file (D22), and none is refused for following a file name; a directory in it is recursed (D8); and a line of only blanks is skipped.
- **DISC-7 [S] `--links=no`** skips symbolic links (step 2 of DISC-1), arguments and list entries as well as walked entries, and directories as well as files, even dangling ones. The check uses `lstat`, which this Windows build lacks, so there the option does nothing [V]: a link to a file is tagged, and one to a directory recursed into, through the link. Here it works on Windows too, where a link is a reparse point that is a name surrogate (symbolic links and junctions), and walks never follow links to directories anyway (D4).
- **DISC-8 [S][V] Patterns** (gnulib `fnmatch_loop.c`, as built, single-byte, without `FNM_CASEFOLD`): a pattern matches the whole name, one element at a time.
  - `*` matches any run of characters, `/` and a leading `.` included; `?` matches any one character.
  - `\` makes the next character literal; at the end of the pattern it matches nothing.
  - `[` starts a bracket expression, which matches one character. A `!`, or `^` (`POSIXLY_CORRECT` unset), right after `[` negates it. Members follow up to the first `]` that is not the first member: a character; `\` and any character; a range `X-Y`, by byte value, where a `-` before `]` or the end is a character; a class `[:NAME:]`, NAME one of `alnum alpha blank cntrl digit graph lower print punct space upper xdigit`, the C locale's, so no byte from 0x80 is in any. An unknown NAME, a range missing its end (`[a-`) and a `\` ending the pattern fail the match. A `[:` not followed by lowercase letters (but `z`) and `:]` is a `[` member, then a `:`.
  - When the bracket expression never closes, it matches nothing if the character is among its members; otherwise the `[` is an ordinary character, and the rest of the pattern follows it. So `[ab` matches only `[ab`, and `[[a` nothing.
  - `[=` and `[.` are no different from `[` in a bracket expression, but while it skips the members after one that matched, `[=X=]` must be complete and `[.` must close with `.]`, else the match fails.
  - On Windows the pattern and the name are uppercased first (DISC-3), so classes are lost: `[[:alpha:]]` becomes `[[:ALPHA:]]`, a bracket expression of `[`, `:`, `A`, `L`, `P` and `H`, then a literal `]` [V].
  - A class is a member like the others, so a `-` after one is a character: `[[:digit:]-z]` holds the digits, `-` and `z`.

### 1.3 Language detection

- **LANG-1 [V][S]** Each language has a map of extensions and of file name patterns, by default no patterns and these extensions:

  | Language | Extensions |
  |---|---|
  | C | `c` |
  | C++ | `c++ cc cp cpp cxx h h++ hh hp hpp hxx inl` |
  | Fortran | `f for ftn f77 f90 f95 f03 f08 f15` |

  - Where file names have case, C++ also has `C H CPP CXX` and Fortran the uppercase of its own. This build has `CASE_INSENSITIVE_FILENAMES` and not them, and compares extensions ignoring case, so `foo.C` is **C** (on Unix it would be C++), and `.H`, `.CPP`, `.F90`, `.F` map as in lowercase.
  - Here the maps are those of this build, and C++'s also holds a `C` of its own (D1).
- **LANG-2 [S][V] Matching** (`parse.c getFileLanguageForRequestInternal`, `getSpecLanguageCommon`, `selectors.c`), on the base name, among the enabled languages:
  1. The candidates are the languages with a pattern that matches the name, matched as excludes are (DISC-8; on Windows, uppercased), or without any, those with an extension equal to the text after the name's last `.`, which is empty without one, so the empty extension (mapped by `.`) takes the names without a `.` [V].
  2. A lone candidate is chosen. Several, ordered by name ignoring case (C, C++, Fortran), are chosen among by a selector all of them have, if there is one. C and C++ share one, which takes ObjectiveC when it is a candidate and the file has its keywords (LANG-4), and otherwise the first candidate, so C before C++. Fortran's selector (for Forth) is its own, and a language defined by `--langdef` has none, so with Fortran or such a language among several none is chosen, and the first by name is kept as a fallback: `--langdef=Aaa --map-Aaa=+.c` makes `.c` Aaa, while `--langdef=Zzz --map-Zzz=+.c` leaves it C [V].
  3. With none chosen and a name ending in `.in`, exactly, steps 1 and 2 are made again on the name without it: `config.h.in` is C++, and `--map-C++=+.in` makes `x.c.in` C++, as only C++ then takes `.in`.
  4. With none chosen still, the name's fallback is taken, else that of the name without `.in`. So with `--map-Fortran=+.in --map-C++=+.in`, `x.in` is C++, the first of the two, but `x.c.in` is C and `y.f90.in` Fortran, by the names without `.in`; with `--map-C=+.in --map-C++=+.in`, C is chosen at once, and `y.f90.in` is C [V].
  - Here, when no pattern takes a name, C++'s own `C` (LANG-1) takes it if its extension is exactly `C`, before the extensions are compared ignoring case, which that `C` never is: so `.C` is C++, while `.c` is C, and with C disabled, no language (D1). The `C` is removed as an item would be, and before any other item equal to it: by `--map-C++=-.C` or `-.c`, by a `--langmap` item `.c` or `.C` for any language, and with the whole map when it is replaced; restoring the map restores it. A name ends in `.in` ignoring case (D2).
- **LANG-3 [V]** There is no content sniffing without `-G`. Extension-less files, including libc++'s `vector`, and `.inc .def .ipp .tcc .cppm .ixx .cuf .fpp .f18` are not tagged.
- **LANG-4 [V]** Under the parity target, every `.h` is C++, never C or ObjC. [I] Every `.f` is Fortran.
  - Under default languages:
    - `.h` becomes ObjectiveC if any `mio_gets` chunk of up to 0x800 bytes begins with `#import`, `@interface `, `@implementation ` or `@protocol ` (`selectors.c` 181-235).
    - `.f` becomes Forth if a chunk begins with `\`, or with `:` followed by whitespace (346-361).
- **LANG-5 [S][V]** A file is a header iff its full-name extension, compared case-insensitively, is one of `h H hh hpp hxx h++ inc def` (`options.c` 131; `-h` changes the list, LANG-6), whatever its language.
  - So `.inl`, `.hp`, `.cc`, `.cpp`, `.c` and `.h.in` are **not** headers.
  - Here the extension is that of the name without a final `.in`, compared ignoring case, and `inl` and `hp` are headers too (D2).
- **LANG-6 [S][V] Options** (their grammar and messages: OPT-12).
  - `--languages`: a disabled language takes no files, and its extensions go to no other language, so `--languages=-C x.c` tags nothing.
  - `--language-force=LANG` makes every input LANG, given or walked, whatever its name, so `-R --language-force=C` parses `Makefile` and every other file not excluded as C. When LANG is disabled, tagging a file is FATAL `C parser specified with --language-force is disabled`; under the parity target every other language is disabled, so forcing one is FATAL [V]. Here the check is made once the options are parsed, before anything is tagged, and only for our languages.
  - `--langmap=LANG:MAPS` first removes each extension or pattern from every language's map, LANG's too (the first item equal to it, ignoring case on this build, `stringListDeleteItemExtension`), then adds it to LANG's: `--langmap=c:+.h` makes `.h` C, and `--langmap=c:+(*.x) --langmap=c++:+(*.X)` makes `p.x` C++ [V]. Without `+`, LANG's map is emptied first, its patterns too: `--langmap=c:.h` leaves `.c` to no language.
  - `--map-LANG=+X` adds X to LANG's map alone, so another language may share it (and then LANG-2 chooses between them), `-X` removes the first item equal to it, ignoring case, from LANG's, and a bare X makes it LANG's whole map. Only the first item counts: `--map-C=.c.h` gives C `c`.
  - `default` restores the maps: all of them for `--langmap=default`, one for `LANG:default`.
  - `-h LIST` sets the header extensions: `default` (exactly) restores them, a leading `+` adds to them, and otherwise they are replaced. LIST is split at `.`, once a leading `.` is dropped, so `.h.hpp` and `h.hpp` are alike, `.` is the empty extension, and `-h +` adds nothing. The list is FATAL `-h: Invalid list` when it names an existing path, so `-h .` always is.
  - A language defined by `--langdef` starts enabled, whatever `--languages` said before, with an empty map, which these options edit as any other: `--langmap=default` empties it again. So under the parity target, `--langdef=Aaa --map-Aaa=+.c` takes `.c` from C (LANG-2) [V]. A file such a language takes gets no tags here, where Universal Ctags tags it by that language's regular expressions, if it has any (OPT-4).
  - Here the maps of the other languages Universal Ctags has are not kept: those languages never take a file (OPT-12), and a `--langmap` naming one only takes its items from the other maps. Forcing a language this program does not parse tags nothing, where Universal Ctags would parse every input as that language, if it is enabled, or else stop, as above.

### 1.4 Lexing and preprocessing (C and C++ share it)

**Input**
- **LEX-1 [S][V]** Reading:
  - Input is binary. A UTF-8 BOM at offset 0 is skipped.
  - Lines split only at LF; a final CR-LF becomes LF. A lone CR is not a line break (line numbers ignore it), is whitespace to the tokenizer, and ends the pattern text.
  - **NUL** truncates the logical line. The rest of that physical line is lost and the next physical line is joined on (`readLine` `strlen`).
- **LEX-2 [S][V]** Comments and splicing:
  - `/*…*/` is not nested and becomes one space. `//` runs to LF and is extended by `\`+LF.
  - **`/+` starts a D comment that runs to `+/`** (bug).
  - `\`+LF is deleted anywhere outside literals; `\`+spaces+LF is not a continuation.
  - Trigraphs `??( ??) ??< ??> ??/ ??! ??' ??- ??=` and digraphs `<: :> <% %> %:` are always on. `<::` not followed by `:` or `>` stays `<`.
- **LEX-3 [S][V]** Literals:
  - Strings handle `\` escapes but do **not** stop at a bare newline (bug: `#define FOO "foo` swallows the next line).
  - Char literals stop at `'` or newline.
  - Raw strings `R"d(…)d"` (delimiter ≤16 characters) are on for **C and C++** (`cxx_parser.c` 2000 passes `true // raw literals`). They are recognized when the character before `R` is not an identifier character, or is `L`/`u`/`U` with no identifier character before that, or is `u8`. The check looks at the physical line.
  - Digit separator: any hex digit `[0-9A-Fa-f]` immediately followed by `'` drops the `'`. This is the **u8 bug**: `u8'x'`, and any identifier ending in `[a-f0-9]` before `'`.

**Directives**
- **LEX-4 [S][V]** A directive must start with `#`, `%:` or `??=` as the first non-blank, non-comment character of a line. Form feed or vertical tab before `#` defeats this.
  - The name is up to 9 letters after optional blanks and comments.
  - Recognized: `define undef include pragma elif else endif`, plus **any name starting with `if`** (the third letter `d`/`n` means ifdef/ifndef).
  - Everything else is swallowed to end of line, with literals still lexed. That includes `#line`, `#error`, `#import`, `#include_next`, `#elifdef` and `#elifndef`.
- **LEX-5 [S][V]** `#define`:
  - The name is `[A-Za-z_~$][A-Za-z0-9_$]*`. A non-ASCII byte ends it; a non-ASCII first byte gives no tag.
  - Kind `d`. It is function-like iff `(` follows the name immediately.
  - The tag line is the line holding the name.
  - Pattern: truncate after the name (OUT-4).
  - `file:` iff not a header. No scope ever.
  - Every definition is emitted (duplicates are removed only if the lines are identical).
  - Definitions in ignored branches are not tagged.
- **LEX-6 [S][V]** `#pragma weak NAME` (spaces only before NAME) emits `d`, including when indented or inside functions.
- **LEX-7 [V]** `#include` and `#undef` produce nothing by default; they are reference tags that need `+r`.

**Conditionals** (`cpreprocessor.c` 850-944, `directiveIf` about 1286)
- **LEX-8 [S]** At `#if`, `#ifdef`, `#ifndef`:
  - `firstBranchChosen = (first non-blank, non-comment char of the condition != '0')`. It is also false when the first identifier is `__ASSEMBLER__`/`__ASSEMBLY__` and the directive is not `#ifndef`.
  - `singleBranch = resolveRequired` (a snapshot), and the brace depth is recorded.
  - `ignoring = parentIgnoring || (!firstBranchChosen && !BraceFormat && (singleBranch || !if0))`, where `if0` defaults to false.
  - Consequences: `#if 0x1`, `#if 01` and `#if 0||X` are skipped; `#if (0)` and `#if !1` are taken.
- **LEX-9 [S]** At `#else`/`#elif`:
  - If `(resolveRequired || depth != depthAtIf) && !BraceFormat`, then `singleBranch = true` (sticky).
  - The branch is ignored iff `ignoreAllBranches || (branchChosen && singleBranch)`.
  - Only at a non-ignored `#else`: `branchChosen = singleBranch || resolveRequired`.
  - The `#elif` condition is never inspected.
  - `#endif` pops. Nesting is limited to 20 levels; pushes stop at 19 but pops continue (bug).
- **LEX-10 [S][V]** `resolveRequired` is true:
  - after any token;
  - on entering any `{}` body (struct, class, namespace, `extern "C"`, function);
  - after `if/for/while/switch/catch(...)`;
  - after `else/do/try`.

  It is false only at statement end and at the top of the file.
  - So **every branch is followed** when the `#if` sits between complete statements and the brace depth is balanced.
  - A first `#if/#else` right after `{` keeps only the first branch (verified examples in cpp-preproc §4).
- **LEX-11 [S]** In pass 2, `BraceFormat=true`: no single-branch forcing at `#else`, and `#if 0` bodies **are parsed**.
- **LEX-12 [V]** No macro expansion by default: `_expand=false`, and `-D`/`-I` are empty. Macros defined in the file are never learned.

**Tokenizer**
- **LEX-13 [S][V]**
  - Identifiers are `[A-Za-z_$~][A-Za-z0-9_$]*` (`~ Foo` becomes `~Foo`). Bytes ≥0x80 become one-byte Unknown tokens, so declarations with non-ASCII names vanish.
  - `>` is never merged; `<` gives `<`, `<<`, `<<=`, `<=`, `<=>`.
  - Runs of `! % + - / ^ | ? =` merge into one Operator, except that a lone `=` is an Assignment token.
  - `__attribute__(…)`, `__declspec(…)`, `alignas(…)` and `_Alignas(…)` are dropped; `[[…]]` is dropped. `__attribute` without trailing underscores is kept.
  - Limits: 16384 tokens per statement; nesting depth 1024 (braces and parentheses counted together).
- **LEX-14 [S][V] Identifiers named by `-I`** (OPT-17; `cxx_parser_tokenizer.c`, the macro branch of `cxxParserParseNextToken`; `cpreprocessor.c`, the unget buffers).
  - Only a word that is no keyword of the language is looked up, so keywords are never replaced, not even those disabled for now (`final`, and `public` in a header, CXX-17): `-I final=char` leaves `final y;` a member of type `final`. In C, C++'s keywords are words like any other. Directives never reach the tokenizer, so a `#define` of a listed name is still tagged, and Fortran has no `-I`.
  - A listed word is dropped. If its entry takes a group, the whitespace after it is skipped, and if a `(` follows, the group to its matching `)` is dropped too, matching only parentheses (`X(a[)` is a group). A group that never closes ends the input there.
  - If the entry has a replacement, the character after the word, or after the group or the whitespace, is pushed back, and then the replacement in front of it, to be read again through the preprocessor's filter (LEX-2..LEX-4). So the replacement joins what follows: `-I X+=int` makes `X y;` and `X(a)y;` the word `inty` [V], and `unsigned X(a)long` the type `unsigned intlong` (here not, X11). Its characters report the dropped word's position, so tokens read from it take its line; once they are read, characters pushed back before it report their own position again (a stack of unget buffers). The filter looks one character past every hex digit `[0-9A-Fa-f]` (LEX-3's digit separator, which here applies only in numbers, X3), so a replacement ending in one is read to its end before a token starting at that digit takes its position: `-I X+=B` makes `struct X` + LF + `{` the struct `B` on the line of `{` [V].
  - No replacement is made, though the word and group are still dropped, while the statement holds 16384 tokens, or nesting (LEX-13, with the replacements being read) is at 1024, or the buffer being read has grown to 65536 bytes [V]. That is the size Universal Ctags allocates for pushed-back characters, in a stack of buffers: one for each replacement being read, and one below them for the rest, made when the first is pushed back and freed when reading returns to the file. A replacement's buffer starts 8 bytes longer than it, and the buffer it was pushed onto has its size again once it is read. A character pushed back makes an empty buffer 8 bytes, and a full one 8 bytes longer; a string of n characters pushed back where m wait makes the buffer 8+n+m bytes if they do not fit (when m is 0, if it is shorter than n). So `X` replaced by 65528 characters ending in `Y v;` leaves `Y` unreplaced, and by 65527, not. Nor is one made of an entry already replaced 8 times since the input was last read from the file, whose word then stays a plain identifier. That bounds loops within the file: `-I a=b,b=a` turns `a x;` back into `a` after 16 replacements [V]. In Universal Ctags, reading at the end of the file counts as reading it, so there a replacement that brings its own entry back after a token ending a statement never stops: `-I S+=;S` on `int a;` + LF + `S` + LF hangs [V]. Here it does not count (X12).
  - Then the next token is read in the word's place, its reading one level deeper in the nesting.

### 1.5 C/C++ tagging rules

**Driver**
- **CXX-1 [S][V]** Parse failure means `RESCAN_FAILED`. Causes:
  - an unmatched `)`, `]` or `}`;
  - a `}` at top level;
  - EOF inside `{`;
  - `struct X<` in C;
  - nesting above 1024;
  - a missing `;` after a nested struct body inside a struct;
  - a misplaced `namespace` or access specifier in confirmed C++.

  Then pass-1 tags are **discarded** and pass 2 runs (`BraceFormat`). If pass 2 fails too, tags up to the failure are kept and the rest of the file is lost.
  - The anonymous counter is **not** reset between passes.
  - Output is corked until the end of the file.
- **CXX-2 [V] Default kinds.**
  - C: `d e f g m s t u v` (and `h`, which is reference-only and never output).
  - C++ adds `c n M P`.
  - Off: `p l x z L D`, and in C++ also `A N U Z`.
- **CXX-3 [S] Statement analysis.**
  - At `;`, outside a function, try a function signature: if it matches, the statement is a prototype `p`, except when the type ends at `=`. Otherwise extract variables.
  - Inside a function, extract variables only.
  - At `{`, check in order: brace initializer, lambda, function definition (outside functions), then a plain block. **A plain block discards the pending statement.**

**Definitions and declarations**
- **CXX-4 Function definitions `f`** [S][V] (`cxx_parser_function.c` 697-1446, 1978-2349):
  - The **first** `identifier (paramlist-like)` at top level wins, skipping `<…>`.
  - The scan ends at `{ ; ,`, and in C++ also at `: = ->`.
  - Special cases:
    - `MACRO(x) name(…)` where the macro's parentheses have no comma and are followed by an identifier and then `(` or `;`;
    - `name __ARGS((…))`;
    - nested `T (*name(args))(…)`.
  - K&R definitions (C only) give `f` **with no typeref**, and parameters `z` (when enabled).
  - `friend` prototypes are skipped.
  - No prototypes or nested functions inside functions.
- **CXX-5 Variables** [S][V] (`cxx_parser_variable.c` 107-986):
  - Kind: `z` (K&R), else `x` if `extern`, else by scope: struct/union/class gives `m`, function gives `l`, otherwise `v`.
  - Out-of-class `T C::x = …` gives **`m class:C`**.
  - Several declarators reuse the base type, with each declarator's `* & [..]` stripped.
  - Brace and designated initializers are skipped.
  - File-scope compound literals lose the variable (bug).
  - `T x(args)` and `T x{…}` (C++) are variables.
  - Bit-fields: `typename:int:33`; non-numeric widths drop the member.
  - Structured bindings are never tagged.
- **CXX-6 struct/union/class `s u c`** [S][V]:
  - The name is the **last identifier** before `: ; { < (..) =`. A `(..)` is skipped only after `alignas` or when it does not look like a parameter list; otherwise the head becomes a function (B2 bug).
  - `struct S;`, forward declarations, `friend class` and `template class X<int>;` produce nothing.
  - The trailer after `}` yields variables or typedefs.
  - Anonymous bodies get anonymous names (CXX-12).
- **CXX-7 enum `g` and enumerators `e`** [S][V]:
  - The underlying type goes into typeref: `enum E : unsigned char` gives `typeref:typename:unsigned char`.
  - Each comma chunk whose first token is an identifier gives `e`, with scope `enum:<full enum name>`.
  - Opaque `enum X;` gives nothing.
  - **Bug C1:** `enum T x[] = {…}` and `std::vector<enum T> v = {…}` become a bogus enum plus enumerators.
- **CXX-8 typedef `t` and `using X = T`** [S][V]:
  - Name: the last identifier, or the function-pointer inner name.
  - Function types get typeref `void ()(void)`.
  - Typedef array dimensions are kept verbatim (`WORD[MIDIPATCHSIZE]`).
  - No typeref when there are "wild parentheses" or a `<…>` chain in a `using` (`using b32 = decltype(0);` gives no typeref).
  - Typedefs inside function bodies are tagged.
- **CXX-9 Namespaces `n`** [V]:
  - `namespace a::b {` gives `a n` and `b n namespace:a`.
  - `inline namespace` is plain.
  - An anonymous namespace is `__anon…11`.
  - `namespace X MACRO {` gives `X`; `namespace MACRO X {` gives `MACRO` (B4).
  - `namespace a::inline b` loses the rest of the file (B1).
- **CXX-10 Lambdas** [S][V]: **on by default** as `f`, named `__anon<h><nn>02`.
  - Detected by a `{` after `[…]`, `[…](…)` or `[…](…) …`. Generic `[]<…>` lambdas are not detected (B6).
  - Scope is the enclosing scope; typeref comes from `-> T`.
  - `file:` **always**, even in headers.
  - The line is the line of the `{`.
- **CXX-11 Modules** [V]: `export module A.B;` gives `A.B M` (no scope, no `file:`); `module;` gives `__gmod_<h><nn>16`; `module A:part;` gives `part P module:A`.

**Names, scope, anonymous names**
- **CXX-12 Anonymous names** [S][V]:
  - Name = `"__anon"` + `%08x` djb2(raw input path bytes; h=5381, h=h*33+c, unsigned, 32-bit) + `%02x`(++counter) + `%02x`(kind index).
  - The counter is per parser per input file (`anonResetMaybe`); `%02x` widens past 0xff.
  - It increments whenever an anonymous identifier is **created**, even if the kind is disabled.
  - Unnamed parameters create one only when `z` is on, and for prototypes only when both `p` and `z` are on (and the anonymous extra is on).
  - Kind indices:

    | Kind | d | e | f | g | h | l | m | p | s | t | u | v | x | z | L | D |
    |---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
    | Index | 00 | 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 0a | 0b | 0c | 0d | 0e | 0f |

    | C++-only kind | c | n | A | N | U | Z | M | P |
    |---|---|---|---|---|---|---|---|---|
    | Index | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 |

  - A typedef'd anonymous struct keeps its `__anon` name.
- **CXX-13 Scope field** [S][V]:
  - Value is `<innermost pushed kind long-name>:<all pushed names joined by ::>`.
  - **Qualifiers are always pushed as `class`**: `bool llvm::f(){}` gives `class:llvm`.
  - Template arguments are stripped from function qualifiers but **kept** for variables (`class:llvm::Any::TypeId<T>`).
  - Friend function definitions go one level up.
  - Function bodies push `function:<name with its qualifiers>`; plain blocks push nothing.
  - Macros never have a scope.
- **CXX-14 Name forms** [V]:
  - Operators: `operator` + space + operator tokens, with a space inside only between identifier/keyword pairs. Examples: `operator ==`, `operator ()`, `operator new[]`, `operator ""_x`, `operator std::string_view`.
  - Destructors are `~X`. Template-argument lists are dropped from names.
  - The tag line is the line of the name token; for operators, the line of `operator`.

**`file:` and typeref**
- **CXX-15 `file:`** [S][V]. In headers, nothing gets it except lambdas and the disabled `l`/`z`/`L`/`x`-in-function kinds. In non-headers:

  | Tag | `file:` in a non-header |
  |---|---|
  | `d`, `s u c`, `g`, `e`, `t`, `n` (including anonymous), `m`, in-class `f` | always |
  | `p` (prototypes) | always, even `extern` |
  | lambdas, `l`, `z`, `L`, `U` | always |
  | namespace-scope `f` and `v` | only if `static` seen before the type (not `const`, not an anonymous namespace) |
  | `x` at global scope | never |
  | out-of-class `T C::f(){}` and `C::x` | never |
  | `M`, `P` | never |

  - `export` clears `file:`.
  - `--extras=-F` **drops** these tags entirely.
- **CXX-16 typeref** [S][V] (`cxx_tag.c` 469-645, `cxx_token_chain.c` 1058-1270):
  - Emitted for `f`/`p` (when return-type tokens pass the sanity check), variables, `t` (unless wild parentheses) and `g` (`enum E : base` only). Never for `s u c e L d`, K&R functions, constructors, destructors or conversion operators.
  - Prefix: if there is more than one token and the first is `struct`, `union`, `enum`, `class` or `typename`, that keyword becomes the prefix and is dropped from the text. A lone marker keyword gives `meta`. Otherwise the prefix is `typename`. So `const struct S *` gives `typename:const struct S *`.
  - Leading `ExcludeFromTypeNames` keywords are skipped: `__declspec __forceinline __inline __inline__ __restrict __restrict__ __thread _Thread_local consteval constexpr constinit export friend inline restrict static thread_local virtual`. `register` and `volatile` are kept. Excluded keywords in the middle are removed **after** spacing is computed, which causes trailing-space bugs (`int * `).
  - Rejected when: more than 30 tokens, more than 4 consecutive identifiers, more than 3 parenthesized chains, an `id (..) id` pattern, or no identifier or keyword.
  - Spacing:
    - after a keyword, identifier, `>`, `&` or `&&`: a space only before `(..)`, an identifier, a keyword, `*`, `&` or `&&`;
    - after `*`: a space unless the next token is `*`, `,` or `)`;
    - `(..)`/`[..]` contents are normalized recursively, with a space only after `decltype`/`typeof`;
    - otherwise no space.
  - Variable array dimensions are emptied unless they are a single number (`uint8_t[]` vs `Map * [4]`).
  - Anonymous types use the full scope name: `struct:ggml_dyn_tallocr::__anon6f2de2020108[1024]`.

**Header mode and C vs C++**
- **CXX-17 [S] "Unconfirmed C++" in headers.** See contradiction 8.
  - `public`, `protected` and `private` are identifiers outside struct bodies until confirmation.
  - C++-only keywords make C code that uses them as identifiers disappear (`int class;`).
- **CXX-18 [V] C vs C++.** Only these behaviors are C-specific:
  - K&R definitions;
  - `struct X<` aborts the parse;
  - `::` before a variable name rejects it;
  - `=` inside a parameter list rejects it;
  - `return` skips to `;`;
  - no lambdas, templates, namespaces or qualified names.

  On ordinary C code the output is byte-identical whichever language is forced.

### 1.6 Fortran rules

The parser (`parsers/fortran.c`) reads characters through a layer for the
source form, groups them into tokens, and walks the tokens statement by
statement with recursive descent. It writes each tag as soon as it finds
it, and tags labels while it reads characters, so the scope of a label
depends on how far the walk had read when the label was seen. Our
`src/fortran.cpp` keeps that order of reads; FTN-1 to FTN-23 describe it.

**Source form and lexing**
- **FTN-1 Form detection** [S][V]. Content only, never extension. Pass 1 is fixed form; it fails, and pass 2 reads the file again as free form with pass-1 tags discarded, when any of these occurs:
  - (a) an invalid margin (FTN-2): a character other than a blank or a digit in columns 1–5 (column 1 is exempt for `*Cc!#$Dd`, which mark comments, and a TAB ends the margin), or a label with a column-6 character other than blank or `0`. The reader then returns end of input, but the walk may read on (an END reads the word after it, for one), taking a new margin from wherever the reading stopped. This matters only for the anonymous counter and the carried character below.
  - (b) `&` immediately followed by the line end (the line end is then lost).
  - (c) a line end inside a string.

  The anonymous-name counter goes on from pass 1, and so does a character the lexer put back at the end of pass 1 (it reads the next character to end a token, then puts it back): pass 2 reads it first, before the file's first byte. Universal Ctags resets that character only before pass 1.

  Real results: 116 `.f90` and 24 `.F90` files in flang/test stay fixed form; 13 `.f`/`.F` files switch to free form.
- **FTN-2 Fixed form** [S][V]:
  - Each line begins with a margin, columns 1–6, read when the previous line is done. Column 1 `*`, `C`, `c`, `!`, `#`, `$`, `D` or `d` makes a comment line (so `#` directives and `D` debug lines are skipped). A TAB anywhere in the margin ends it, and the line begins a statement. Columns 1–5 hold blanks and the digits of a label (blanks inside it are dropped: `1 0` is `10`). A line that ends before column 6 is skipped. Column 6 blank or `0` begins a statement; anything else continues the previous one.
  - A label is tagged as soon as its margin is read, whatever the line turns out to be.
  - After the margin, blanks are skipped. The first other character reads as itself if it is in column 7, and otherwise as a blank followed by it: blanks after column 6 on a continuation line split a name, and column 7 joins it (`longna` + `me` is `longname`).
  - A line end reads as the end of the statement only when the next line that is not a comment or blank begins a statement, or the input ends. So the margin of the next statement's line, and its label, are read before the statement end is returned, and that token belongs to the next line. An anonymous tag, which takes the position of the token after its keyword, therefore points at the next line (`interface` followed by a blank line points at the line after it).
  - No limit at column 72: columns 73 and beyond are code.
  - `!` outside a string comments out the rest of the line; inside a string it is text.
- **FTN-3 Free form** [S][V]:
  - `&`, optional blanks, then the line end or `!` (whose comment is skipped) continues the statement. Reading resumes after blank lines, lines whose first character is `!`, lines whose first character is `#`, leading blanks, and any `&` (each followed by blanks) at the start of the continuation. This works inside strings too.
  - A line whose first character is `!` or `#` is skipped. An indented `!` comment, or one after code, is read to the line end through the same reader, so an `&` at its end continues into the next line, which is lost; it reads as the end of a statement.
  - `;` ends a statement, in both forms.
  - After a statement end at a line end (not `;`, and not on line 1), blanks then up to five digits are a label; a sixth digit begins a number.
  - An indented `#` is not special; its words are read as tokens.
- **FTN-4 Tokens** [S][V]:
  - Identifiers are an ASCII letter, then letters, digits, `_` and `$`. **A leading `_` is not an identifier** (we depart, F3): `_`, `$` and bytes from 0x80 are one-character tokens of no type.
  - Keywords are matched ignoring ASCII case: `abstract allocatable assignment associate automatic bind block byte cexternal cglobal character class codimension common complex contains data deferred dimension dll_export dll_import do double elemental end entry enum enumerator equivalence extends external final forall format function generic if implicit import include inline integer intent interface intrinsic kind len logical map module namelist non_overridable nopass operator optional parameter pascal pass pexternal pglobal pointer precision private procedure program protected public pure real record recursive save select sequence static stdcall structure submodule subroutine target then type union use value virtual volatile where while`. `blockdata`, `doubleprecision`, `impure`, `non_recursive`, `simple`, `critical`, `team` and `enumeration` are not keywords.
  - **Joined END**: an identifier that is not a keyword, starts with lowercase `end`, and whose rest is a keyword (any case) reads as END with that keyword as the word after it (`enddo`, `endType`). `ENDDO` and `EndDo` are plain identifiers (we depart, F5).
  - Operators: one of `* / + - = < >`, continued by any of `* / + = < >` (not `-`). A `.` followed by identifier characters and a `.` is an operator (`.eq.`); without the closing dot it has no type.
  - Numbers: digits, a kind suffix `_` + letters (dropped), an optional `.` and digits, and an optional `e` or `E` and digits (a sign after it starts a new token).
  - Strings: from `'` or `"` to the same quote or the line end, which is consumed. A doubled quote reads as two strings.
  - `::`, `:`, `,`, `(`, `)`, `[`, `]` and `%` are tokens of their own.
  - A token's line and position are those of the input after its first character was read: the line of that character, except in fixed form where a line end was read ahead (FTN-2).
  - Universal Ctags drops the rest of a line after a NUL byte and joins the next line to it. We read NUL as an ordinary character of no type (X4), so `"\0"` strings in `folding05.f90` stay strings.
- **FTN-5 Default kinds** [V]: `E M N S b c e f i k l m n p s t v`; `L` and `P` are off.

**Program units and statements** [S][V]. When a unit or block ends, its END statement is consumed together with the word after END, which is read even when it is on the next line (so a bare `end` reads the next statement's first word, which then ends the statement); a joined END already has its word.
- **FTN-11 Top level.** Statement ends are skipped. BLOCK starts block data (FTN-23), END skips its statement, FUNCTION and SUBROUTINE start subprograms (FTN-19), MODULE always starts a module and SUBMODULE a submodule (FTN-22), and PROGRAM a main program. The prefixes `elemental`, `pure`, `recursive` and `stdcall` are skipped as words (and `impure`, `non_recursive`, `simple` too, departing, F1). Anything else is read as the specification part and then the execution part of a main program without a PROGRAM statement, with no scope; if neither read anything, one token is skipped.
- **FTN-12 Specification part.** USE statements, then IMPORT statements, then the implicit part (IMPLICIT, INCLUDE, PARAMETER and FORMAT skipped, ENTRY tagged), then declarations, until a statement is not one:
  - ENTRY name: `e`, if the name is an identifier.
  - INTERFACE (FTN-15), ABSTRACT INTERFACE (ABSTRACT followed by anything else skips the statement), ENUM (FTN-16).
  - STDCALL is skipped as a word; AUTOMATIC followed by a type specifier is a type declaration (else the statement is skipped).
  - Skipped: `cexternal cglobal dll_export dll_import external format include inline parameter pascal pexternal pglobal static value virtual volatile`.
  - Type declarations (FTN-13) start with `byte integer real double complex character logical record type procedure final generic class enumerator`.
  - A statement that starts with an identifier is skipped; the part goes on only if it is a statement function, `name(...) =`. That includes a joined END, so after a body of declarations only, `endmodule` is skipped with the next statement, the next unit's header (we depart, F5).
  - COMMON and NAMELIST (FTN-18), STRUCTURE (FTN-17); `allocatable data dimension equivalence external intent intrinsic optional pointer private protected public save target` statements are skipped.
  - Anything else ends the part, unread, including the attribute statements `bind(…) ::`, `codimension`, `asynchronous` and `contiguous` (we depart, F8).
- **FTN-13 Type declarations.** A type specifier, then `, attributes` and `::`, then a list of entities, each tagged with the kind of FTN-6.
  - Specifiers: CHARACTER `[*] (…)` or `[*] N`; BYTE, INTEGER, REAL, COMPLEX, LOGICAL, PROCEDURE and CLASS with `(…)`, `*N` or `*(…)`; DOUBLE PRECISION and DOUBLE COMPLEX (DOUBLE followed by anything else skips to the statement end); RECORD `/s/`; TYPE `(…)`, while TYPE followed by anything else defines a derived type (FTN-14); FINAL, GENERIC and ENUMERATOR are just the word.
  - Attributes: `parameter allocatable external intrinsic kind len optional private pointer protected public save target nopass abstract deferred non_overridable` alone, `dimension intent bind extends` with `(…)`, `pass` with an optional `(…)`, `codimension` with `[…]`. **Any other attribute skips to the end of the statement, losing its entities** (we depart, F8).
  - Entities are identifiers, or keywords other than FUNCTION and SUBROUTINE. After each, `(…)`, `[…]`, `*len` or `*(…)`, a DEC `/initial value/`, and an initializer after `=` or `=>` (to the next comma outside brackets) are skipped. A list that starts with `%` (DEC `%FILL`) skips the statement.
  - So a module-level `generic :: g => a, b` tags `g` and `b` as variables (we depart, F7).
- **FTN-14 Derived types.** TYPE `[, attributes] [::] name`: the name, an identifier or keyword, is tagged `t` and becomes the scope; one PRIVATE or SEQUENCE statement after it is skipped. Then statements that start with a type specifier declare components `k`, or methods `M` if they start with PROCEDURE, FINAL or GENERIC (which includes procedure pointer components); GENERIC tags its generic name, or the operator of `operator(op)` or `assignment(=)`; other statements are skipped, up to END. The walk then returns to the statement that held TYPE with the END statement's first token current: after `end type …` that is the end of the statement, but **a bare `end` closing a type is taken as the first entity** of the declaration, so `end` (and words after it) are tagged as variables of the enclosing unit.
- **FTN-15 Interfaces.** INTERFACE then a generic name (identifier or keyword), `operator(op)` or `assignment(=)` with the operator's text as name, or else an anonymous name; tagged `i`, and entered as a scope, though an operator name never serves as one. Inside, FUNCTION and SUBROUTINE start prototypes `P`, prefixes are skipped as words, type specifiers are skipped (for typed functions), and every other statement, MODULE PROCEDURE lists included, is skipped, up to END. So is `module function f` (we depart, F2), whose `end function` then ends the interface.
- **FTN-16 Enums.** ENUM `[, bind(c)] [kind] [::] [name]`: tagged `E`, named or anonymous, then entered after the rest of its statement is read. Type declarations inside, ENUMERATOR ones in particular, declare enumerators `N`, up to END. ENUMERATOR outside an enum declares variables.
- **FTN-17 DEC structures.** STRUCTURE `[/name/] [field names]`: a type `t`, named or anonymous (with the kind index of components, 06), and its field names on that line as components `k`. Fields: type declarations, nested structures, and UNION blocks of MAP blocks of fields, each up to its END. RECORD `/s/ name` declares `name`.
- **FTN-18 COMMON and NAMELIST.** Each `/name/` with an identifier is tagged `c` or `n`, and each identifier member is a local `L`, in any unit; array bounds are skipped. A blank common `//` or a COMMON without slashes tags only the members.
- **FTN-19 Subprograms and main programs.** PROGRAM, FUNCTION or SUBROUTINE, then a name (identifier or keyword) tagged `p`, `f` or `s`, or `P` when any enclosing unit is an interface. A subroutine's or prototype's argument list is read before the tag is made and the unit entered, so the line end after it is read, and a label on the next line tagged, in the enclosing scope; a function's arguments are read after. Without a name, the unit is entered with no tag (entities in it are `v`). Then the specification part, the execution part, a CONTAINS part, and the END statement.
- **FTN-20 Execution part.** Statements are skipped one by one; ENTRY is tagged and prefix words are skipped. It ends at CONTAINS, FUNCTION or SUBROUTINE starting a statement, and at END unless the word after END is ASSOCIATE, BLOCK, DO, ENUM, FORALL, IF, SELECT or WHERE: **any other END (`end critical`, `end team`, `end file`) ends the unit** (we depart, F4).
- **FTN-21 CONTAINS part** (internal and module subprograms). Read token by token, not by statement: FUNCTION or SUBROUTINE anywhere starts a subprogram, a type specifier is skipped as one (so TYPE not followed by `(` starts a type definition), END stops, and any other token is skipped. Unknown prefixes such as `impure` or `module` are thus harmless here.
- **FTN-22 Modules.** MODULE then a name (identifier or keyword), tagged `m`. SUBMODULE then `(parent)`, which must be identifiers and colons with at least one of them, or the statement is skipped; then the name, tagged `S`. Then the specification part, a CONTAINS part, then every statement up to the first END, which ends the module. Since MODULE always starts a module at the top level, a stray `module function f` makes a module named `function` (we depart, F2).
- **FTN-23 Block data.** BLOCK, then DATA and a name (an identifier) tagged `b`; the unit is entered with or without it. Then the specification part, and statements up to END. `blockdata` joined is an identifier.
- **FTN-6 Entity kinds** [S][V]. The kind of an entity in a type declaration depends only on the innermost unit:

  | Innermost unit | Entity kind |
  |---|---|
  | module or submodule | `v` |
  | type or structure | `M` if the statement started with `procedure`, `final` or `generic`, else `k` |
  | function, subroutine or prototype | `L` |
  | enum | `N` |
  | anything else (program, block data, a unit without a tag, none) | `v` |

  - Not tagged: statement functions, `result()`, `use`, `module procedure` lists, BLOCK declarations (they are in the execution part), `associate` names, construct names, `data`, `equivalence`.

**Fields and patterns**
- **FTN-7 Scope and `file:`** [V]:
  - The scope is the innermost enclosing unit that was tagged (its kind enabled or not) and whose name is an identifier: `<kind>:<name>`, unqualified. Scope kinds: `module`, `submodule`, `program`, `subroutine`, `function`, `prototype`, `interface`, `type`, `enum`, `blockData`.
  - Operator interfaces and **subprograms named by a keyword** (`function sequence`; we depart, F6) do not serve as scopes, though they still decide entity kinds (FTN-6) and prototypes (FTN-19).
  - A label takes the scope current when it is read: the first label of a subroutine is outside it (FTN-19), and one on the line after an END that ends a unit is inside that unit.
  - `file:` only on `l` and `L`. No typeref.
- **FTN-8 Patterns and excmd** [V]:
  - Every tag except labels is truncated after the last occurrence of its name (OUT-4); labels use the full line.
  - **`c` uses a line-number address `N;"`** (`lineNumberEntry`).
  - The pattern line is the physical line holding the token's first character (FTN-4), which may be a continuation line.
  - Names keep their case. A tag with an empty name is dropped.
- **FTN-9 Anonymous names** [S][V]:
  - Kind indices: b00 c01 e02 E03 f04 i05 k06 l07 L08 m09 M0a n0b N0c p0d P0e s0f t10 v11 S12. An anonymous DEC structure is named with 06.
  - The counter advances for every anonymous interface, enum and structure, even when its kind is disabled, and carries over from a failed fixed pass.
- **FTN-10 Preprocessor lines and INCLUDE** [V]: `#` lines are comments, both `#if` branches are parsed, and `INCLUDE` is not followed.

### 1.7 Fields

- **FLD-1 [V]** Default order: `kind letter` (bare, no `kind:`), then `[scopekind:scope]`, then `[typeref:A:B]`, then `[file:]` (empty value). `;"` is always present by default, because the kind is printed.
- **FLD-2 [S]** Escaping:
  - Name: `\`→`\\`; 0x01–0x1F and 0x7F become `\a \b \t \n \v \f \r` or `\xHH` (uppercase); a leading space becomes `\x20` and a leading `!` becomes `\x21`.
  - File field: same, without the leading-character rule.
  - Scope name, the typeref value, and the parser's fields (C++'s `name:`): same as the name.
  - Typeref prefix: as-is.
- **FLD-3 [V]** The C++ `name:` field is never seen by default; it appears only on `A` tags.
- **FLD-4 [S][V]** Order of all fields (`writer-ctags.c`):
  - the kind: its letter (`k`) or long name (`K`), after `kind:` with `z`;
  - `line:`, `language:`, the scope (`KIND:NAME`, or `scope:KIND:NAME` with `Z`), `typeref:`, `file:`;
  - `inherits:`, `access:`, `implementation:`, `signature:`, `roles:`, `extras:`, `end:`, `epoch:`, `nth:`;
  - then the parser's own fields.
  - `;"` comes before the first built-in field written (any of the above but the parser's own). The parser's fields are appended without it, so a line with only those has none: `--fields= --fields-C++=+{properties}` writes `st\ta.cpp\t/^static int st;$/\tproperties:static`. That is malformed: a reader takes `\tproperties:static` as part of the address. Here D10 writes `;"` before whichever field comes first.
  - `extras:` lists a tag's extras by commas in their table's order: `fileScope`, `inputFile`, `pseudo`, `qualified`, `reference`, `guest`, `subparser`, `anonymous`.
  - A tags file has a `!_TAG_FIELD_DESCRIPTION` pseudo-tag for each enabled field that has a long name (in `--list-fields`), so none for `k`, `s`, `K` or `R`: `file` goes with `--fields=-f`, `line` comes with `+n`, and `extras` with `+E`. A parser's own enabled fields get `!_TAG_FIELD_DESCRIPTION!LANG` lines, such as C++'s `name`.
- **FLD-5 [S][V] Field values** (`writer-ctags.c addExtensionFields`, `field.c`), each written only while its field is enabled (`--fields`, OPT-9):
  - The kind: its long name with `K`, else its letter with `k`, either after `kind:` with `z` (`kind:variable`). With neither `k` nor `K` there is no kind, and with no field at all no `;"` (`--fields=` writes `x\ta.c\t/^int x;$/`).
  - `line:N` (`n`), and `language:NAME` (`l`) with the parser's name, `C`, `C++` or `Fortran`, so a `.h` file's tags say `C++` (LANG-4).
  - The scope (`s`), `typeref:` (`t`) and `file:` (`f`) as by default (FLD-1), the scope after `scope:` with `Z`, only on tags that have one: a scope, a type, file scope.
  - `roles:def` (`r`) on every tag written here: only reference tags, which are not written (EX-6), have other roles.
  - `extras:` (`E`) on a tag with any extra, in FLD-4's order: `fileScope` on every tag with `file:` (those that `--extras=-F` drops, EX-2), `qualified` on the copies made by `--extras=+q` (EX-4), and `anonymous` on tags named for unnamed entities (EX-3), so at most `fileScope,qualified,anonymous`.
  - `inherits:` (`i`), `access:` (`a`), `implementation:` (`m`) and `signature:` (`S`) on the tags FLD-7 to FLD-11 give them.
  - Nothing for `C` (xref output only), `R`, `p` (no effect in tags output), `x` (it needs libxml2, which this build lacks), and `T` but on file-kind tags (`epoch:`, EX-7).
  - A file-kind tag (EX-7) has only the kind, `line:`, `language:`, `roles:`, `extras:` and `epoch:`, and in Universal Ctags `end:`.
  - Universal Ctags also writes `end:` (`e`) on definitions whose last line it knows, and `nth:` (`o`) on members, enumerators and macro parameters. Here `e` and `o` are accepted and write nothing (D25).
- **FLD-6 [S][V] `--output-format=e-ctags`** (`writer-ctags.c eCtagsWriter`), Exuberant Ctags' flavor of the format:
  - The name, the file field, the scope name and the signature (FLD-8) are written as they are, not escaped (FLD-2), but the typeref, the base classes (FLD-9) and the parser's fields still are.
  - On Windows the file field keeps the backslashes of the path as given (`sub\deep\z.c`, `.\sub/y.c` for `-R .\sub`), which Universal Ctags' own writer turns into `/` (DISC-5).
  - A tag whose name or file holds a tab or a newline, or with `--format=2` its scope name (with `s`) or signature (with `S`), or an enabled parser field, is dropped: it could not be read back. Here a NUL drops it too (X4), since a reader ends the line there: `void f(char c = '\0') {}` with a real NUL byte in the quotes loses `f` while `S` is enabled. Universal Ctags has none in its values, since it drops the rest of a line after a NUL.
  - The pseudo-tags say `e-ctags`, and on Windows `backslash` (PT-4).
- **FLD-7 [S][V] `access:` of C and C++ tags** (`cxx_tag.c`, `cxx_scope.c`). A tag begun inside a scope takes the access its innermost scope has at that moment: `public`, `private` or `protected`.
  - A class body begins `private`, a struct, union or enum body `public`, and an access label (`protected:`) changes it for the rest of that body. C is no different: its members and enumerators say `public`.
  - Other scopes have none: namespaces, functions, prototypes, lambdas, typedefs, modules, and the scopes pushed for a qualified name (`A` in `int A::f() {` or `struct A::B {`). Nor has file level. A tag in one of them gets no field.
  - So a member says what the label above it says, an enumerator says `public`, and a class nested in a class body says `private` unless a label precedes it. A friend function defined in a class body has the scope around the class (CXX-13), and that scope's access. Template parameters take the access of the class they belong to, and those of a variable template `public`.
  - Macros and macro parameters never have one, nor has Fortran.
  - The value is written as it is.
- **FLD-8 [S][V] `signature:` of C and C++ functions** (`cxx_parser_function.c`, `cxx_parser_lambda.c`).
  - A function or prototype gets its parameter list, parentheses included. The list is first spaced as a type name is (CXX-16), and its last token is not followed by a space. Then its tokens are joined, a space after each token marked so, a bracketed group joined the same way inside: `(int a,char * b)`, `(int (* fp)(int,char),unsigned long long x[3],...)`, `(T && t,std::vector<T> const & v)`, `(void)`, `()`. Default arguments stay: `(char c='x',int n=1)`.
  - Then ` const` when the function is `const` (`() const`), but nothing else that follows the list: not `volatile`, `&`, `noexcept`, `override` or `= 0`.
  - A K&R definition (CXX-4) gets its list of names, spaced as a type name: `(a,b)`.
  - A lambda (CXX-10) with a parameter list gets that list as written, not respaced, so with the source's spaces, and a space after the `)` when one follows it: `(int a, int b) ` for `[](int a, int b) { ... }`, but `(int q)` for `[](int q){ ... }`. A lambda without one (`[]{}`) gets none.
  - The value is escaped as a scope name is (FLD-2): `(const char * s="x\\\\y")` for `"x\\y"`. E-ctags writes it as it is, and drops a tag whose signature holds a tab or a newline (from a string literal in a default argument), or here a NUL (X4), while the field is enabled, as for a scope (FLD-6).
- **FLD-9 [S][V] `inherits:` of C and C++ classes, structs and unions** (`cxx_parser.c`), with a base clause, between `:` and `{` (CXX-6). The keywords `public`, `protected`, `private` and `virtual` are dropped, and the rest is joined without spaces, but a bracketed group, `(...)` or `[...]`, keeps the spaces inside it and after its closing bracket. Angle brackets are not groups.
  - `: public Base, protected virtual S2, private Tm<unsigned int>` gives `Base,S2,Tm<unsignedint>`; `: std::vector<std::pair<int, int> >` gives `std::vector<std::pair<int,int>>`; `: decltype(foo()) {` gives `decltype(foo()) ` with the space before `{`.
  - The value is escaped as a scope name is, also by e-ctags, which keeps a tag whose base clause holds a tab.
- **FLD-10 [S][V] `signature:` of macros** (`cpreprocessor.c`). A function-like macro (LEX-5) whose parameter list ends with `)` gets its parameters, joined by `,` in parentheses, whether or not the `D` kind is enabled.
  - The list is read raw, without removing comments: a parameter is a run of ASCII letters, digits, `_`, `$` and `.`, any other byte ends one, and a backslash also skips the byte after it. So the list may go on over later lines, up to the first `)`.
  - Each parameter is written in full, `args...` and `...` too, though their `D` tags are named `args` and none: `(a,b)` for `( a , b )`, `(fmt,...)`, `(x,args...)`, `()`.
  - A list the end of the file cuts off gets none, as do object-like macros.
- **FLD-11 [S][V] Fortran's fields** (`parsers/fortran.c`). It has no `access:`.
  - `signature:` on subroutines and prototypes, never on functions or programs (FTN-19). When a `(` follows the name: the identifiers and keywords inside the parentheses, nested ones too, each `,` written `, `, and nothing for other tokens, in parentheses. So `(a, b)` whatever the spacing and lines, `(Alpha, BETA)` in the source's case, `()`, and `(a, b, , c)` for `(a, b, *, c)`. The end of the statement ends the list.
  - `inherits:` on derived types, from an `extends(...)` attribute (FTN-14): the text of the last token inside its parentheses that is not a parenthesis, so `extends(shapes%circle)` gives `circle`, and none if that is empty (`extends(a,)`). On submodules, the parent as read (FTN-22), its identifiers and colons joined: `shapes:shapes_impl`.
  - `implementation:` `abstract`, `deferred` or `non_overridable`, the last such attribute of the list: on a derived type from the attributes of its TYPE statement (`type, abstract :: shape`), and on the components and methods a statement in a derived type declares from that statement's (`procedure(iface), deferred :: area`), but not on GENERIC's. Attributes elsewhere are ignored.

### 1.8 Output format

- **OUT-1 [V]** Line layout: `name\tfile\taddress;"\tkind[\tscope][\ttyperef][\tfile:]` followed by **CRLF**. Both the `tags` file and stdout are written in text mode.
- **OUT-2 [V]** The address is a pattern, except Fortran `c`, which uses `N;"`.
- **OUT-3 Pattern algorithm** [S][V]:
  1. Re-read the physical line, with a final CR-LF turned into LF.
  2. Optionally truncate (OUT-4).
  3. Write `/^`.
  4. Copy bytes up to the first `\r` or `\n`, escaping `\`→`\\`, `/`→`\/`, and `$`→`\$` **only if the next byte is `\n` or `\r`**.
  5. Stop when the output length reaches **96 bytes counted after escaping**, but allow up to 3 extra UTF-8 continuation bytes.
  6. Append `$` iff the (possibly truncated) line ends in `\n` and nothing was cut.
  7. Write `/`.

  - A raw TAB is kept. A leading `^` in the source is not escaped. A last line with no newline gets no `$`.
- **OUT-4 Truncation after the tag** [S][V]. Applies to C/C++ `d` and Fortran non-label tags (`truncateTagLineAfterTag`, `entry.c` about 765):
  - Find the **last** case-sensitive substring occurrence of the name (`strrstr`) and keep it plus one following byte.
  - If that byte is `\n`, the line keeps its newline and gets `$`.
  - If the name is not found, the full line is used.
  - Examples: `/^#define f(a) f(/`, `/^# define A(...)   [ __VA_AR/`, `/^  subroutine sub1(/`.
- **OUT-5 [V]** Emacs TAGS layout (`-e`), LF line endings: one section per input file, `\f\n<path>,<size>\n`, where the size counts the bytes of the entries after that line; each entry is `<text>\x7f<name>\x01<line>,<offset>\n`, with the line's text and the name always explicit. OPT-21 has the details.
- **OUT-6 [S][V] `--excmd`** chooses the address (`entry.c initTagEntryFull`, `writer-ctags.c`); `-n` is `number` and `-N` is `pattern` (OPT-7).
  - `mixed`, the default: OUT-2.
  - `number`: the line number for every tag (`12;"`).
  - `pattern`: a pattern for every tag. A common block's is cut after its name, as other Fortran tags' are (OUT-4): `/^  common \/blk\//`.
  - `combine`: the line number, `;`, then the pattern (`2;/^int x;$/;"`), but common blocks keep the bare number.
- **OUT-7 [S][V] `--pattern-length-limit=N`** replaces OUT-3's 96 (step 5); 0 means no limit, so the whole line is written.
- **OUT-8 [S][V] `--format=N`**: 2 by default. 1, the original format, and 0 write only the name, the file and the address, with no `;"` and no fields, nor `extras:pseudo` on the pseudo-tags (PT-4). The address still follows `--excmd`: `M\ta.c\t4;/^#define M /` under `combine`.

### 1.9 Sorting

- **SORT-1 [S][V]** Sort whole lines (without CR or LF) with `strcmp`, i.e. unsigned bytewise. An exact duplicate of the previous line is dropped; blank lines are skipped. Pseudo-tags are sorted along with everything else.
  - TAB sorts below every name byte, so a name that is a prefix of another sorts first; then the file, then the address, then the fields.
- **SORT-2 [I]** Sorted output does not depend on traversal order. Anonymous names depend only on each file's own path and content.
- **SORT-3 [S][V] `--sort=foldcase`** (`--sort=f`, `fold`, OPT-7) writes `!_TAG_FILE_SORTED 2`. It compares whole lines as `struppercmp` does (`sort.c`, `routines.c`): byte by byte, unsigned, after `toupper` in the C locale, so only `a` to `z` change and `_`, `[`, `\`, `]`, `^` and `` ` `` sort after every letter; a line that is a prefix of another sorts first. As SORT-1, an exact duplicate of the line before it is dropped.
  - Universal Ctags sorts with `qsort`, which leaves lines equal but for case in no defined order (`A` and `a`, or `aB` and `Ab`), so an exact duplicate may also survive when such a line falls between the two copies. Here lines equal ignoring case are ordered by SORT-1, so the order is total, duplicates always meet and are dropped, and the output does not depend on the threads.
- **SORT-4 [S][V] `--sort=no`** (and `-u`) writes `!_TAG_FILE_SORTED 0`, and the lines as they were made, none dropped: the tags of each file in the order its parser makes them, which is the order of the Units cases' expected output, and the files in the order they are visited (DISC-1): the arguments in order, then the `-L` entries, each directory depth-first in its listing order (DISC-2). A file named twice is tagged twice, and its lines written twice.
  - Here files are tagged by several threads in any order, so each file's lines are kept apart with a key that orders them as the walk would: an argument's or list entry's index, and for a directory's entry, its directory's key followed by its index in the listing. The pseudo-tags come first, in the order of PT-4, which is sorted.

### 1.10 Pseudo-tags

- **PT-1 [V][S]** Emitted when writing a file (by default), or to stdout only with `--extras=+p`.
  - The common set is written at open.
  - Each parser's set is written when that parser first runs; only languages actually used appear.
  - Kind lines list only the **enabled** kinds, with C/C++ sync applied.
- **PT-2 [V]** The exact default set (TAB shown as `→`). There are 22 common lines; C adds 16 when a `.c` file was parsed, C++ adds 24, Fortran adds 18.

```
!_TAG_EXTRA_DESCRIPTION→anonymous→/Include tags for non-named objects like lambda/
!_TAG_EXTRA_DESCRIPTION→fileScope→/Include tags of file scope/
!_TAG_EXTRA_DESCRIPTION→pseudo→/Include pseudo tags/
!_TAG_EXTRA_DESCRIPTION→subparser→/Include tags generated by subparsers/
!_TAG_FIELD_DESCRIPTION→epoch→/the last modified time of the input file (only for F\/file kind tag)/
!_TAG_FIELD_DESCRIPTION→file→/File-restricted scoping/
!_TAG_FIELD_DESCRIPTION→input→/input file/
!_TAG_FIELD_DESCRIPTION→name→/tag name/
!_TAG_FIELD_DESCRIPTION→pattern→/pattern/
!_TAG_FIELD_DESCRIPTION→typeref→/Type and name of a variable or typedef/
!_TAG_FIELD_DESCRIPTION!C++→name→/aliased names/                      [C++ only]
!_TAG_FILE_FORMAT→2→/extended format; --format=1 will not append ;" to lines/
!_TAG_FILE_SORTED→1→/0=unsorted, 1=sorted, 2=foldcase/
!_TAG_KIND_DESCRIPTION!<Lang>→<letter>,<longname>→/<description>/  (one per enabled kind)
!_TAG_OUTPUT_EXCMD→mixed→/number, pattern, mixed, or combineV2/
!_TAG_OUTPUT_FILESEP→slash→/slash or backslash/
!_TAG_OUTPUT_MODE→u-ctags→/u-ctags or e-ctags/
!_TAG_OUTPUT_VERSION→1.1→/current.age/
!_TAG_PARSER_VERSION!C→1.1→/current.age/       !C++ → 2.2      !Fortran → 1.1
!_TAG_PATTERN_LENGTH_LIMIT→96→/0 for no limit/
!_TAG_PROC_CWD→C:/Users/wellons/src/llvm-project/→//                  (getcwd, \→/, trailing /)
!_TAG_PROGRAM_AUTHOR→Universal Ctags Team→//
!_TAG_PROGRAM_NAME→Universal Ctags→/Derived from Exuberant Ctags/
!_TAG_PROGRAM_URL→https://ctags.io/→/official site/
!_TAG_PROGRAM_VERSION→6.2.1→//
!_TAG_ROLE_DESCRIPTION!C!function→foreigndecl→/declared in foreign languages/
!_TAG_ROLE_DESCRIPTION!C!header→local→/local header/
!_TAG_ROLE_DESCRIPTION!C!header→system→/system header/
!_TAG_ROLE_DESCRIPTION!C!macro→undef→/undefined/
!_TAG_ROLE_DESCRIPTION!C!struct→foreigndecl→/declared in foreign languages/
!_TAG_ROLE_DESCRIPTION!C++!header→exported→/exported with "exported imported ..."/
!_TAG_ROLE_DESCRIPTION!C++!header→imported→/imported with "imported ..."/
!_TAG_ROLE_DESCRIPTION!C++!header→local→/local header/
!_TAG_ROLE_DESCRIPTION!C++!header→system→/system header/
!_TAG_ROLE_DESCRIPTION!C++!macro→undef→/undefined/
!_TAG_ROLE_DESCRIPTION!C++!module→imported→/imported with "imported ..."/
!_TAG_ROLE_DESCRIPTION!C++!module→partOwner→/used for specifying a partition/
!_TAG_ROLE_DESCRIPTION!C++!partition→imported→/imported with "imported ..."/
```

- **PT-3 [V]** Kind descriptions, for KIND_DESCRIPTION lines, including kinds that are off by default:
  - **C:**
    - `D,macroparam` parameters inside macro definitions
    - `L,label` goto labels
    - `d,macro` macro definitions
    - `e,enumerator` enumerators (values inside an enumeration)
    - `f,function` function definitions
    - `g,enum` enumeration names
    - `h,header` included header files
    - `l,local` local variables
    - `m,member` **struct, and union members**
    - `p,prototype` function prototypes
    - `s,struct` structure names
    - `t,typedef` typedefs
    - `u,union` union names
    - `v,variable` variable definitions
    - `x,externvar` external and forward variable declarations
    - `z,parameter` function parameters inside function or prototype definitions
  - **C++:** the same, except `m,member` is **class, struct, and union members**, plus:
    - `A,alias` namespace aliases
    - `M,module` modules
    - `N,name` names imported via using scope::symbol
    - `P,partition` partitions
    - `U,using` using namespace statements
    - `Z,tparam` template parameters
    - `c,class` classes
    - `n,namespace` namespaces
  - **Fortran:**
    - `E,enum` enumerations
    - `L,local` local, common block, and namelist variables
    - `M,method` type bound procedures
    - `N,enumerator` enumeration values
    - `P,prototype` subprogram prototypes
    - `S,submodule` submodules
    - `b,blockData` block data
    - `c,common` common blocks
    - `e,entry` entry points
    - `f,function` functions
    - `i,interface` interface contents, generic names, and operators
    - `k,component` type and structure components
    - `l,label` labels
    - `m,module` modules
    - `n,namelist` namelists
    - `p,program` programs
    - `s,subroutine` subroutines
    - `t,type` derived types and structures
    - `v,variable` program (global) and module variables
  - Lines are sorted by `strcmp`, so `!C→…` precedes `!C++→…`, and uppercase letters precede lowercase.
- **PT-4 [S][V] The common values** follow the options (`ptag.c`, `writer.c`, `writer-ctags.c writeCtagsPtagEntry`):
  - `FILE_FORMAT`: the `--format` number, described `original ctags format` for 1, `extended format; --format=1 will not append ;" to lines` for 2, and `unknown format` for 0.
  - `FILE_SORTED`: 0 for `--sort=no`, 1 for yes, 2 for foldcase.
  - `OUTPUT_EXCMD`: `mixed`, `number`, `pattern`, or `combineV2` for `combine`.
  - `OUTPUT_FILESEP`: `backslash` for e-ctags on Windows (FLD-6), else `slash`.
  - `OUTPUT_MODE`: `u-ctags`, or `e-ctags`.
  - `PATTERN_LENGTH_LIMIT`: the limit, 0 for none (OUT-7).
  - `PROC_CWD`: the working directory (here left out when it cannot be found, OPT-18), on Windows with each `\` made `/`, and ending in `/`, added unless it ends in a separator (`/`, and on Windows `\` or `:`). The u-ctags writer escapes it as a file field (FLD-2); e-ctags writes it as it is, and skips the line with WARNING `skip priting !_TAG_PROC_CWD pseudo tag; the input field of the pseudo tag includes a tab character: DIR` (or `newline`) if it holds one.
  - With `E` and `--format=2`, every pseudo-tag line ends in `;"\textras:pseudo`.

### 1.11 Extras

- **EX-1 [S][V]** `--extras=SPEC` (`options.c processExtraTagsOption`).
  - The grammar is that of kinds (CLI-4): a leading `*` enables every extra, a leading character other than `+`, `-` or `*` first disables them all (so `--extras=q` turns off `F` and `anonymous`), `+` and `-` set the mode, and `{name}` names an extra in full.
  - Within braces, `+` and `-` belong to the name. An unclosed `{name` is ignored silently. A `{` inside braces or a `}` outside them is FATAL `unexpected character in extra specification: 'C'`.
  - An unknown letter or name gives WARNING `Unsupported parameter 'Q' for "extras" option` (or `'{name}'`), and is ignored.
  - `--extra=` is the same with WARNING `--extra option is obsolete; use --extras instead`, and its warnings name `"extra"`.
  - `--file-scope=BOOL` sets `F`, with WARNING `"--file-scope" option is obsolete; use "--extras=+F" instead` (or `-F` for false), after the value is checked: an invalid one is FATAL `Invalid value for "file-scope" option`.
  - The generic extras are `anonymous` (no letter), `F fileScope`, `f inputFile`, `g guest`, `p pseudo`, `q qualified`, `r reference`, `s subparser`, `z nulltag`. On by default: `anonymous F p s`.
- **EX-2 [S][V] fileScope.** Without it, every tag that would carry `file:` is dropped: C and C++ tags (CXX-15), macros outside headers, and Fortran `l` and `L` (FTN-7). Qualified copies go with them.
- **EX-3 [S][V] anonymous.** Without it, tags named for unnamed entities are dropped: anonymous structs, unions, classes, enums, namespaces, lambdas and unnamed parameters, and Fortran's anonymous components, interfaces and enums. Their members and typerefs keep the `__anon` names. Global module fragments (`__gmod_`) are not anonymous extras. Unnamed parameters are not even created, so they take no anonymous number (CXX-12) and the numbers after them shift down.
- **EX-4 [S][V] qualified** (`cxx_tag.c cxxTagCommit`), C and C++ only. After each C/C++ tag that has a scope field, the same tag again, named by the scope:
  - The name is the scope name, then `::` (`:` when the innermost scope is a module, so a partition is `m:part`), then the tag's name: `ns::Class::member`, `point::x` in C, `__anon…::member`.
  - Not in a function or prototype scope, so there are none for locals, parameters, labels and template parameters of prototypes.
  - An enumerator is qualified by its enum's own scope, without the enum: `ns::Class::Red`, also for `enum class`. An enumerator of an enum at global scope gets none.
  - Macros and macro parameters never have one, nor does Fortran.
  - The scope type is read when the tag is committed, its name when it began; they agree everywhere here.
- **EX-5 [S] pseudo.** Without it, no pseudo-tags are written. With it given explicitly, Universal Ctags also writes them to stdout.
- **EX-6** `guest`, `reference`, `subparser` and `nulltag` produce tags only from features not ported (guest parsers, reference tags with roles, subparsers, empty names).
- **EX-7 [S][V] inputFile** (`entry.c`, `parse.c`, `field.c`). While the `f` extra and the file kind are both enabled (CLI-4), every file a parser tags gets one more tag, of the file kind, made once the parse is over, so after a failed first pass's retry, and after all the file's other tags in unsorted output (SORT-4). A file with no other tags gets one too, an empty one included.
  - Its name is the base name of the path as constructed (DISC-2): what follows the last `/`, on Windows also the last `\` or `:` (`C:a.c` gives `a.c`). It is escaped as any name (FLD-2), so `!x.c` is `\x21x.c`. `--tag-relative` changes the file field only (OPT-18): with `--tag-relative -f sub/tags`, `a.c` is the tag `a.c` in `../a.c`.
  - Its address is the line number `1`, under every `--excmd`, `pattern` and `combine` too, followed by `;"` and the fields as any address is (OUT-6, OUT-8): `a.c\ta.c\t1;"\tF\tline:1\tepoch:1792612665`; with `--format=1`, `a.c\ta.c\t1`; with `--kinds-C= --extras=f --fields=T`, which leaves the file kind and drops the kind field, `a.c\ta.c\t1;"\tepoch:1792612665`.
  - Its fields, each while enabled (FLD-5): the kind `F`, `file` with `K`, after `kind:` with `z`; `line:1`; `language:` with the parser's name, so `C++` for a `.h` file (LANG-4); `roles:def`; `extras:inputFile`; and `epoch:` (`T`, enabled by default), the file's last modification time, in seconds since 1970 UTC. No other tag has `epoch:`. Universal Ctags also writes `end:` (`e`) with the number of lines; here `e` writes nothing (D25).
  - The other extras leave it alone: it is not dropped by `--extras=-F` or `--extras=-{anonymous}`, gets no qualified copy, and takes no anonymous number.
  - E-ctags (FLD-6) writes its name and file as they are, dropping it when either holds a tab or a newline, or here a NUL (X4).
  - Files that are not tagged get none: those of no language or a disabled one, excluded, missing or unreadable ones, and directories. Here neither do files over 64 MiB (D7), nor files of languages this program does not parse, which Universal Ctags tags with an empty parser or its own (`--langdef=Foo --map-Foo=+.foo` gives `x.foo` one, of the language `Foo`), but under etags, where such files, and those of the language `Unknown` (OPT-12), get their sections and file tags as Universal Ctags writes them (OPT-21).
  - Universal Ctags reads the time with `stat` when it opens the file. On Windows, this build's C runtime converts the system's time to local time and back with the daylight saving offset in effect now, not then, so in summer a file last modified in winter gets an epoch an hour late [V]: `touch -t 202601151200` in New York, then `1768500000` for `1768496400`. Here the time is the system's, unconverted (D17).

### 1.12 Options

How Universal Ctags parses its options (`main/options.c`, `main/parse.c`, `main/args.c`), and how this program takes them. Messages are quoted without the `ctags: ` prefix, which here replaces Universal Ctags' `ctags.exe: `.

**Grammar**
- **OPT-1 [S][V] Arguments** (refines CLI-1).
  - `--NAME=VALUE` splits at the first `=`; without one the value is empty. `--` and `--=x` are FATAL `Unknown option: --`, and a lone `-` is FATAL `Unknown option: -`.
  - In a short cluster, the letters `f o h i I L p d D b` take a parameter: the rest of the cluster, or else the whole next argument, even one starting with `-`. Leading spaces of a parameter are dropped, and an empty one is FATAL `Missing parameter for "f" option`.
  - Universal Ctags silently drops such a letter when it is the last argument. Here that is FATAL with the same message.
  - `-?` is `--help`, and `-w` is silently ignored (`options.c processShortOption`).
- **OPT-2 [S] Lookup order.** `--fields-LANG` and `--extras-LANG` are tried first, then the exact names, case-sensitively, then the families: `--kinddef-LANG`, `--kinds-LANG` with the old `--LANG-kinds` and `--LANG-types` (whose language runs to the first `-`, so `--list-kinds`, an exact name, is never one), `--regex-LANG`, `--mline-regex-LANG`, `--map-LANG`, `--param-LANG.NAME`, `--roles-LANG.KIND`. Then `--__anything` is ignored, and the rest are FATAL `Unknown option: --NAME`.
  - Without a language, `--kinddef-`, `--regex-`, `--mline-regex-`, `--map-` and `--param-` are unknown options; `--kinds-` warns `No language given in "kinds-" option` and `--roles-` warns `no language given in "roles-" option`; `--fields-` and `--extras-` are FATAL `No language given in "fields-" option`.
- **OPT-3 [S][V] Language names** are matched ignoring case, with no aliases. Universal Ctags 6.2.1 knows 153: C, C++, Fortran, the 148 others `--list-languages` prints, and the hidden UniversalCtags and CTagsSelfTest. `--langdef=NAME` adds one.
  - In `--regex-LANG`, `--map-LANG`, `--param-LANG.NAME` and the like, the language runs to the first `.` or `:`. A `:` warns `using `:' as a separator is obsolete; use `.' instead: --OPTION`.
  - An unknown language is FATAL `Unknown language "X" in "OPTION" option` for `--kinddef-`, `--regex-`, `--mline-regex-`, `--map-`, `--param-`, `--language-force` and the listings, and a WARNING otherwise: the same text for `--kinds-LANG` and `--languages`, `Unknown language: X (ignoring "--OPTION")` for `--fields-LANG` and `--extras-LANG`, `unknown language "X" in --OPTION option` for `--roles-LANG`, and for `--langmap` the whole value as the language.
- **OPT-4 Other languages** (here). An option naming a language Universal Ctags knows and this program does not parse is accepted and does nothing, without Universal Ctags' checks of that language's kinds, extras, fields or parameters (it warns `Unsupported kind: 'x' for --kinds-python option`, say). So option files written for Universal Ctags work unchanged. The exceptions are the file kind, which every language shares (CLI-4): `--kinds-python=-F` disables it for C too, and the grammar's FATAL errors (a stray brace) apply; and languages defined as subparsers of ours (OPT-12).

**What is refused, and what is accepted**
- **OPT-5 Refused** (here): options that would change which tags exist in C, C++ or Fortran, or write another output, are FATAL `OPTION: not supported`:
  - `--regex-LANG`, `--mline-regex-LANG` and `--kinddef-LANG` for our languages, and for languages defined as subparsers that our parsers run (OPT-12); the Fortran extra `linkName` (`--extras-Fortran=+{linkName}`, also by `--extras-all=*`);
  - `--param-CPreProcessor._expand` and `.if0` when true, and `.define`; `-D`; `--if0` and `--line-directives` when true;
  - `-G` and `--guess-language-eagerly` when true, which read files whose names match no language for modelines and the like, so that `-G` tags a file named `x` holding `/* vim: set ft=c: */` as C [V];
  - `-x`, `-B`, and `--output-format=xref`. `--output-format=json` is FATAL as in this build of Universal Ctags: `the output format "json" is not available on this platform`.
- **OPT-6 Accepted** (here). Every option that OPT-5 does not refuse acts as this section says, or is accepted silently without effect: the defaults, `-V`, `--verbose`, `--quiet`, `--totals[=yes|no|extra]`, `-F`, `-w`, `--recurse=no` and `--output-format=u-ctags`.

**Values**
- **OPT-7 [S][V]**
  - Booleans (`--append`, `--links`, `--quiet`, `--verbose`, `--if0`, `--line-directives`, `--recurse`, `--file-scope`, `--file-tags`): CLI-2, else FATAL `Invalid value for "NAME" option`.
  - `--excmd`: only the first letter of `m`, `n` or `p` counts (`--excmd=numberwang` is number); otherwise exactly `combine`, else FATAL `Invalid value for "excmd" option: VALUE`, the value possibly empty.
  - `--sort`: a boolean but for the empty value, or `f`, `fold` or `foldcase` ignoring case, else FATAL `Invalid value for "sort" option`. `-u` is `--sort=no`.
  - `--tag-relative`: a boolean, empty for yes, `always` or `never`. `--totals`: a boolean, empty for yes, or `extra`. Both FATAL `Invalid value for "NAME" option` otherwise.
  - `--format` is read as `scanf("%u")`: blanks, a sign, then decimal digits, ignoring what follows (`1x` is 1). No digits is FATAL `Invalid value for "format" option`; above 2, including a negative number, which wraps, is FATAL `Unsupported value for "format" option`.
  - `--pattern-length-limit` is read as `strtoul` in base 0 with 32-bit longs: blanks, a sign, `0x` for hex and `0` for octal, all of it a number (`08` is invalid), a negative number wrapping. Empty is FATAL `A parameter is needed after "pattern-length-limit" option`, and otherwise invalid FATAL `-pattern-length-limit: Invalid pattern length limit`.
  - `--output-format`: `u-ctags`, `e-ctags`, `etags` (as `-e`), `xref`, `json`, case-sensitive. Empty is FATAL `no output format name supplied for "output-format"`, another name FATAL `unknown output format name supplied for "output-format=NAME"`. `u-ctags`, the default, changes nothing (`options.c processOutputFormat` returns early on it), so it does not undo `-e` or `e-ctags`: `-e --output-format=u-ctags` writes etags [V].
  - `-e` and `--output-format=etags` also make the output unsorted and `--tag-relative=yes`; later `--sort` and `--tag-relative` override them. The default output file is then `TAGS`.
  - `--file-tags=BOOL` sets the `f` extra with WARNING `"--file-tags" option is obsolete; use "--extras=+f" instead`, as `--file-scope` does for `F` (EX-1).
  - `-L FILE` given twice warns `-L option specified more than once, last value used`.
  - `-h LIST` naming a file or directory that exists is FATAL `-h: Invalid list`.
  - `-i`, `-p`, `-d` and `-b` (debug options not built) take their parameter, then are FATAL `Unknown option: -i`.

**Kinds, extras, and fields**
- **OPT-8 [S][V] Specifications.** `--kinds-LANG`, `--extras`, `--fields`, `--extras-LANG` and `--fields-LANG` share CLI-4's grammar, and differ thus:

  | | kinds | extras | fields | extras-LANG, fields-LANG |
  |---|---|---|---|---|
  | first character not `+`, `-`, `*` | disables all | disables all | disables all | only `{` or empty disables all; else WARNING `Wrong per language field specification: SPEC` |
  | a letter | its kind | its extra | its field | FATAL `only long name can be used in per language field spec: 'n'` |
  | unknown letter | WARNING `Unsupported kind: 'Q' for --OPTION option` | WARNING `Unsupported parameter 'Q' for "extras" option` | the same, for `"fields"` | — |
  | unknown `{name}` | the same warning, unbraced | the same warning, `'{name}'` | FATAL `no such field: 'name'` | FATAL `no such field: 'name'` (`no such extra`) |
  | stray brace | FATAL `unexpected character in kind specification: '}'` | `... extra specification ...` | `... field specification ...` | as the common form |

  In every form an unclosed `{name` at the end is ignored, and `--extra` is `--extras` with WARNING `--extra option is obsolete; use --extras instead` and its warnings name `"extra"`.
- **OPT-9 [S][V] Fields** (`--list-fields`).
  - Common fields, by letter: `N name`, `F input`, `P pattern` (fixed), `C compact`, `E extras`, `K` (no long name), `R` (none), `S signature`, `T epoch`, `Z scope`, `a access`, `e end`, `f file`, `i inherits`, `k` (none), `l language`, `m implementation`, `n line`, `o nth`, `p scopeKind`, `r roles`, `s` (none), `t typeref`, `x xpath`, `z kind`. On by default: `N F P T f k s t`, and here also `n` (P-1).
  - Parser fields, by name only: C has `alias macrodef properties section`, C++ those and `captures name specialization template`, with `name` on; Fortran has none.
  - Once all options are parsed (and before `--list-fields`), for the tags output: `z` without `k` or `K` warns `though z/kind field is enabled, neither k nor K field is not enabled` and `enable the K field to make the z/kind field printable`, and enables `K`; `Z` without `s` warns `though Z/scope field is enabled, s field is not enabled` and `enable the s field to make the Z/scope field printable`, and enables `s`; a disabled fixed field is enabled again with WARNING `Cannot disable fixed field: 'N'{name} in ctags output mode`, unless any `--fields` or `--fields-all` has disabled all fields, which silences these warnings for good.
  - Here the fields are written as FLD-5 says. Of the parser fields only C++'s `name` is written; the others are accepted and do nothing.
- **OPT-10 [S][V] `--kinds-all`** (and the obsolete `--all-kinds`, `--all-types`, which warn `"--all-kinds" option is obsolete; use "--kinds-all" instead`).
  - The value may hold only `*`, `F` and `{file}`, each `F` or `{file}` perhaps after one `+` or `-`. Else FATAL: `don't repeat + (nor -) in --OPTION option`, `don't use '*' after + (nor -) in --OPTION option`, `no kind specification after + (or -) in --OPTION option`, or `only '*', 'F', "{file}" or their combination is acceptable as kind letter for --OPTION`.
  - It then applies to every language: `*` enables every kind, an empty value or `F` disables every kind (and `F` enables the file kind), `+F` and `-F` change only the file kind.
- **OPT-11 [S][V] `--extras-LANG` and `--fields-LANG`.**
  - The language `all` (exact case) covers the common entries and every language's: `--extras-all=` disables all extras, and a name enables every entry of that name in any language (`--fields-all={name}` enables `N` and C++'s `name`).
  - C and C++ have no extras of their own, and Fortran only `linkName` (OPT-5).

**Languages**
- **OPT-12 [S][V]**
  - `--languages=[+|-]LIST`: comma-separated items, each perhaps starting with `+` (add) or `-` (remove), a mode that carries on to later items; before any, the mode is replace, which disables every language before each item until a non-empty one. `all` (exact case) means every language. Unknown names warn (OPT-3), so `--languages=ALL` disables everything.
  - `--language-force=LANG|auto` (`auto` ignoring case), with `--lang` and `--language` warning `"--lang" option is obsolete; use "--language-force" instead` first.
  - `--langmap=default` restores every map. Otherwise items `LANG:[+]MAPS` separated by `,`: the language runs to the first `:` in the rest of the value; without `+` its map is first cleared; `LANG:default` (ignoring case) restores one map; each map is `.ext` (up to `.`, `(`, `,` or the end) or `(pattern)` (`\)` does not end it), and is taken from every other language. An unknown language warns with the whole value and ends the option; a bad map is FATAL `Badly formed language map for C language` or `Unterminated file name pattern for C language`.
  - `--map-LANG=[+|-]MAP` adds, removes, or replaces the map with one item, the rest ignored, without taking it from other languages. An empty value is FATAL `no parameter is given for map-C`.
  - `-h LIST`, `--languages`, `--language-force` and the maps act as LANG-6 says.
  - `Unknown` (`parse.c FallbackParser`) is a language with no map and no tags, which takes every file that no enabled language takes. It is disabled until `-e` enables it (OPT-21), or `--languages` names it or `all`; replacing the list disables it, so `-e --languages=C` gives no section to a file of no language, and `--languages=C -e` does [V]. Here it acts only under etags: outside it, its files would get only file tags (EX-7), as other languages this program does not parse would.
  - `all` also enables the languages this program does not parse. Of those, ObjectiveC claims some `.h` files (LANG-4), and QtMoc (of C++) and JNI (of C and C++), enabled by default in Universal Ctags, add their tags to our files; the reference disables them all (P-1). Here, as in the reference, they never take a file or add a tag, whatever `--languages` says, so `--languages=all` and `--languages=+QtMoc` enable only our languages, of all they name, and those defined by `--langdef`, whose maps may take files (LANG-6).
  - `--langdef=NAME[{flags}]`: FATAL `No language specified for "langdef" option` when empty, `Language "X" already defined` for a known name, `"all" is reserved; don't use it as the name for defining a new language` (also `NONE`, exact case), and `don't use `_' in a language name (a_b)` for any character but a letter, digit, `+` or `#` (with `"'"` for a quote or backquote). The new language is enabled, with an empty map (LANG-6).
  - Its flags (`main/flags.c`, `parse.c processLanguageDefineOption`) start at the first `{`, each `{FLAG}` or `{FLAG=VALUE}`; text between them is ignored, `{{` ends them (a script follows), and a `{` without a `}` ends them with WARNING `long flags specifier opened with `{' is not closed `}': "FLAGS"`, FLAGS from the first `{` on.
  - `{base=LANG}` makes NAME a subparser of LANG, ignoring case, the last valid one counting. An empty LANG warns `No base parser specified for "base" flag of --langdef option`, an unknown one `Unknown language(X) is specified for "base" flag of --langdef option`, and `CPreProcessor` `Because of an internal limitation, Making a sub parser based on the CPreProcessor parser is not allowed: X`, and each is ignored. `{dedicated}`, `{shared}` and `{bidirectional}` set the direction, the last one counting, shared by default; without a base they warn `Ignore the direction of subparser because "{base=}" is not given`. Universal Ctags crashes on `{base}` without `=`; here it warns as an empty one does. The other flags (`_autoFQTag`, `_foreignLanguage`, `version`) are accepted without their checks.
  - Unless the direction is dedicated, the base parser runs the subparser on its own files, so a subparser of C, C++ or Fortran, or of such a subparser, adds its regex tags to our files [V], and here its `--regex-`, `--mline-regex-` and `--kinddef-` are refused (OPT-5). Universal Ctags' own subparsers of ours, QtMoc (C++) and JNI (C and C++), are disabled in the reference (P-1), so neither they nor languages based on them tag our files.

**Parameters and roles**
- **OPT-13 [S][V]**
  - `--param-LANG.NAME=VALUE`: the separator after the language's name must be `.` or `:`, else FATAL `no separator(.) is given for param-C=1`; an empty value is FATAL `no value is given for param-C.x`. C, C++ and Fortran have no parameters: FATAL `no such parameter in C: x`. The C preprocessor's are `_expand` and `if0` (booleans, FATAL `Invalid value for "if0" parameter`), `define`, and `ignore`, which adds its value to `-I`'s table as one token, unsplit (OPT-17): `--param-CPreProcessor.ignore=a=int,b=char` replaces `a` with `int,b=char`.
  - `--roles-LANG.KIND=[+|-]{role}...`: roles mark only reference tags, which this program does not write, so the option is checked and does nothing. Without `.KIND`, or with `.*`, the value must be empty or `*` (FATAL `only '*' or '' (empty string) is acceptable as an argument for --roles-C: x`); `--roles-all.KIND` is FATAL; the kind is a letter or `{name}`, and an unknown one or the file kind only warns; garbage after it, an unclosed brace, an empty `{}`, and a character other than `+`, `-`, `*` or `{role}` are FATAL; an unknown role warns `no such role: "bogus" in kind 'd' in language "C"`. C's roles: `d` condition, undef; `f` foreigndecl; `h` local, system; `s` foreigndecl. C++'s: `M` imported, partOwner; `P` imported; `d` condition, undef; `h` exported, imported, local, system.

**Option files**
- **OPT-14 [S][V] `--options=PATH`.**
  - A file holds one argument per line, at LF, CR or CRLF, trimmed of whitespace. Empty lines and those whose first non-blank is `#` are skipped. Lines are arguments as on the command line, so `-f` may take the next line as its parameter.
  - The first line that is not an option ends the file with WARNING `Ignoring non-option in PATH`. Universal Ctags adds an empty line after it, and also counts a line of only blanks as a non-option, and from then on refuses `-f` and other initOnly options as following a file name (CLI-6); none of that is ported. Nor is this: its record of a non-option is global, so once a file name has appeared, on the command line or in an option file, every option file read later warns too, even one without a non-option line (`--options=n2.ctags --options=n3.ctags`, where only `n2.ctags` names a file, warns for both [V]). Here only a file that has a non-option line warns.
  - A directory reads its files ending in `.ctags`, but for names starting with `.` (so `.ctags` itself too), in byte order of their names, each path the directory, a `/` unless it ends in one, and the name. Subdirectories are skipped.
  - A file already read is skipped silently, by path (Universal Ctags compares files), and here files nest at most 16 deep: FATAL `too many nested option files: "PATH"`.
  - An empty path is FATAL `no option file supplied for "options"`, a missing one FATAL `cannot stat "PATH" : No such file or directory`, an unreadable one FATAL `cannot open option file "PATH" : REASON`.
  - Universal Ctags looks for a path not starting with `/` or `.` in its option directories first, and takes `--options=NONE` only before other options (elsewhere it is a missing file). Here a path is used as given, and `NONE` reads nothing anywhere, as no default option files are read (CLI-8).

**Listings**
- **OPT-15 [S][V]** `--list-languages`, `--list-kinds[=LANG|all]`, `--list-kinds-full[=LANG|all]`, `--list-extras[=LANG|all|NONE]` and `--list-fields[=LANG|all|NONE]` print to stdout and exit 0 at once, reflecting the options before them; `all` and `NONE` ignore case, an unknown language is FATAL (OPT-3, though `NONE` is one for the kinds).
  - `--list-languages`: one name per line, with ` [disabled]`. Here only C, C++ and Fortran; the categories `_libxml`, `_libyaml` and `_packcc` hold none of them.
  - `--list-kinds=LANG`: per kind, `LETTER  DESCRIPTION`, with ` [off]` when disabled, in table order. Without a language, each language's name (with ` [disabled]`) and its kinds indented by four spaces. C's `m` is described `struct, and union members`, C++'s `class, struct, and union members`.
  - The tables: columns padded to their widest cell (header included) and one space, but the last, and the first header cell marked `#`. `--list-kinds-full`: `LETTER NAME ENABLED REFONLY NROLES MASTER DESCRIPTION` by letter byte, with a first `LANGUAGE` column for all; only `h` is reference-only; the master is `C` for C's kinds and those C++ shares (CLI-5), else `NONE`. `--list-extras`: `LETTER NAME ENABLED LANGUAGE FIXED DESCRIPTION`, common ones by letter (`-` for none) then by language. `--list-fields`: `LETTER NAME ENABLED LANGUAGE JSTYPE FIXED OP DESCRIPTION`, the fixed fields first, the common ones by letter, then by language and name.
  - The writer decides what is fixed. Under `-e` (etags) no field is, so `N`, `F` and `P` are listed among the others by letter, and are neither enabled again nor warned about (OPT-9); the pseudo extra is fixed, and options that set it under etags leave it as it was.
  - The pseudo extra (`xtag.c`): until an option sets it, by naming it or by a reset, it is enabled only where the writer writes pseudo-tags (not etags) and the output is a file, not `-` or `/dev/stdout`. So `-f - --list-extras` lists it disabled, as does `-e --extras=+p --list-extras`, while `--extras=+p -e --list-extras` lists it enabled and fixed [V].
  - Here other languages list no kinds, extras or fields, and all-language tables hold only our rows, so their columns are narrower than Universal Ctags'.
- **OPT-19 [V] `--version`** prints to stdout and exits 0 at once, as the listings do. Universal Ctags prints several lines, the first `Universal Ctags 6.2.1, Copyright (C) 2015-2025 Universal Ctags Team`, then its derivation from `Exuberant Ctags 5.8` and its build's features.
  - Here it prints one line, `ctags 0.1 (Universal Ctags 6.2.1 compatible)`, naming this program and its version, as `PROGRAM_NAME` and `PROGRAM_VERSION` do (PT-4), and the version whose behavior it follows. Editor plugins read this text to decide what the program can do, and each accepts it:
    - tagbar requires a match of `\(Exuberant\|Universal\) Ctags`, case-sensitively, then takes its Universal Ctags path on `Universal Ctags`, ignoring case;
    - easytags matches `\(Exuberant\|Universal\) Ctags \zs\(\d\+\(\.\d\+\)*\|Development\)` in the first line, which finds `6.2.1`, above its minimum;
    - fzf-lua looks for `Universal`, after which it passes `--excmd=combine`, and reads `0.1` as a version it does not use; the Linux kernel's `tags.sh` greps for `universal`, ignoring case.
    - vim-clap requires the first line to start with `Universal Ctags`, which this does not.

**Order of checks**
- **OPT-16 [S]** After the options: `-a` with output to stdout is FATAL `append mode is not compatible with tags to stdout`; then OPT-9's field checks; then the default output file; then, with no files and no `-L`, `-R` tags `.` or else it is FATAL (CLI-2). Here the `-L` list is read before them all (DISC-6), and OPT-5's `linkName` check follows.

**Identifiers**
- **OPT-17 [S][V] `-I LIST|@FILE|-`** (`options.c processIgnoreOption`, `cpreprocessor.c saveIgnoreToken`) fills a table of identifiers that the C and C++ tokenizer drops or replaces (LEX-14), each option acting as it is read.
  - A parameter starting with `@`, `.`, `/` or `\`, or on Windows with a letter and `:`, names a file (after the `@`), read at once: each line, at LF or CRLF, up to a NUL (X4) and with its trailing whitespace removed, is one token, and empty lines are skipped. On Windows the file is read as text, so it ends at its first Ctrl-Z (0x1A) [V]. Leading whitespace, spaces and commas stay in the token, so `a b` is one that names nothing. A device or pipe is read to its end, so `-I @NUL` (on POSIX `-I /dev/null`) is an empty list. One that cannot be read is FATAL `cannot open "FILE" : REASON`, the reason being the C runtime's [V]: on Windows, `Permission denied` for a directory, with or without a trailing separator; `Invalid argument` for a file named with one (`list/`), a name with `*` or `<`, or an empty name (`-I @`); `No such file or directory` for a missing file, directory, drive or server.
  - `-I -` empties the table.
  - Any other parameter is a list split at spaces, tabs, newlines and commas.
  - `--param-CPreProcessor.ignore=TOKEN` adds one token, unsplit (OPT-13).
  - A token's name runs to its first `+` or `=`, and an empty one is ignored. `NAME` drops the identifier; a `+` before any `=` also drops a parenthesized group after it (`NAME+`); the text after the first `=`, when not empty, replaces them (`NAME=TEXT`, `NAME+=TEXT`). Text between the `+` and the `=` is ignored, `NAME=` is `NAME`, and a token ends at a NUL.
  - A later token for a name replaces an earlier one, until the table grows [V]. Universal Ctags keeps every token in a hash table of 1024 buckets, adding each at the front of its bucket and finding the first. Before adding one to a table at least 80% full, it grows the table to the next of the primes 1361, 2729, 5471, 10949, 21911, …, which reverses each bucket. So with `A=int,A=char` and 818 more tokens `char` is found, with 819 to 1087 more `int`, with 1088 more `char` again. The count is of tokens added, repeated names included, and empty names not, since the table was last emptied.

**Paths**
- **OPT-18 [S][V] `--tag-relative[=yes|no|always|never]`** (`read.c`, where an input's path for its tags is set; `routines.c`, which makes paths absolute and relative; `entry.c`, where the tags file is opened) changes only the file field. Warnings, exclusions (DISC-3), language detection and anonymous names (CXX-12) keep the path as constructed (DISC-2).
  - The values (OPT-7): `no`, the default, writes the path as constructed (DISC-5). `yes`, as a bare `--tag-relative` and `-e` set it, makes a relative path relative to the base and writes an absolute one as constructed. `always` makes every path relative to the base, and `never` makes every path absolute.
  - **The base** is the tags file's directory: its name up to and including the last separator, made absolute, or the current directory when the name has no separator or is `-`. On Windows `:` is a separator here, so `-f C:tags` has the base `C:`, and paths relative to it start at the drive's root: `/w/a.c`.
  - **Absolute.** A path is absolute when it starts with a separator, or on Windows with a letter and `:`, then a separator or nothing. Otherwise the current directory goes before it, with a `/` between unless the directory ends in a separator. On Windows an absolute path without a drive gets the current one, `X:` (`/x` is `C:/x`, and from a UNC current directory `@:/x`; here `//host/share/x`, D16). Then, left to right, each name `.` goes with the separator before it, and each name `..` goes with the separator before it and with everything back to the separator before that, of whatever kind, so on Windows the drive's `:` (`C:/../x` becomes `C/x`; here not, D15), or with no separator before, as at `/` on POSIX, only the `..` itself goes (`/../x` is `/x`). Doubled separators stay, and `..` takes the empty name between them (here the name before them, D14). An empty result is `/`. Every separator but `:` becomes `/`, and on Windows a lowercase drive letter becomes uppercase. A UNC path counts as one without a drive: `//host/share/a.c` becomes `C://host/share/a.c` (here not, D16).
  - **Relative.** The path is made absolute, then compared with the base byte by byte, ignoring the case of ASCII letters on Windows, up to the first byte that differs. The equal part is cut back to its last separator (`:` included on Windows); when there is none, as when the drives differ, the result is the absolute path. Otherwise it is a `../` for each separator the base has past that point, then the absolute path past it. As the base ends in a separator, a file in its directory or below gets the path from there, and any other climbs to the common directory.

    | `-f` (from `/w`) | `a.c` | `./sub/b.c` | `sub/deep/d.c` |
    |---|---|---|---|
    | `-`, `tags` | `a.c` | `sub/b.c` | `sub/deep/d.c` |
    | `sub/tags`, `./sub/tags`, `/w/sub/tags` | `../a.c` | `b.c` | `deep/d.c` |
    | `sub/deep/tags` | `../../a.c` | `../b.c` | `d.c` |
    | `other/tags` | `../a.c` | `../sub/b.c` | `../sub/deep/d.c` |
    | `../tags` | `w/a.c` | `w/sub/b.c` | `w/sub/deep/d.c` |

    On Windows the comparison makes `SUB/B.C` `B.C` under `-f sub/tags`, and an input on another drive stays absolute even with `always`. `never` writes `/w/a.c`, `/w/sub/b.c`, …, and `yes` writes an absolute input as constructed, `c:\w\a.c` as `c:/w/a.c`, where `always` and `never` uppercase its drive.
  - **[V] Bugs** with doubled separators, each counted as a name (here not, D14): `x//../a.c` is `x/a.c`; under `-f sub//tags`, `a.c` is `../../a.c`; under `-f -`, `.//a.c` is `/a.c`, and under `-f sub/tags`, `sub//b.c` is `/b.c`. Above a Windows drive's root, `C:/../x` and `/../x` are `C/x` (here `C:/x`, D15). A UNC path made absolute keeps a drive before it, `C://host/share/a.c`, or made relative climbs to the drive's root, `../../..//host/share/a.c`, and from a UNC current directory, `/x` is `@:/x` (D16).
  - **Drive-relative paths.** On Windows, making absolute a path that is a letter, `:`, and then not a separator is FATAL `C:a.c: relative file names with drive letters not supported`: an input once it is to be tagged, with any value but `no` (one that is missing only warns, and one of no language is skipped), and the tags file's directory once the file is opened (`-f C:sub/tags`: `C:sub/: relative …`), whatever the value, leaving the file created and empty.
  - **The current directory** [S] is looked up by Universal Ctags at every start (`main.c`, `routines.c`), whatever the options, in a buffer of `PATH_MAX` bytes. When that fails, as when the directory has been removed on POSIX or its name does not fit, it stops with FATAL `failed in getcwd() : REASON` (CLI-7). The system names a root with its separator, `C:\` or `/`, and any other directory without one, so the `/` added before a relative path is never doubled.
  - Here the current directory is looked up, of any length (`getcwd` with a buffer doubled until it fits; on Windows `GetCurrentDirectoryW`, asked first for the size, since `GetFullPathNameW(".")` at a drive's root asks for too little room, and then writes `C:` alone), only where it is used: once before the walk with a value other than `no`, and once more as the tags file is written, for its `PROC_CWD` pseudo-tag (PT-4), whatever the value. When it cannot be, a run with a value other than `no` stops before the walk with `ctags: failed in getcwd()`, without a reason, which the platform layer does not report; any other run goes on, and its tags file lacks the `PROC_CWD` line. The platform tests run the program from the root of its drive or file system, which the unit tests' file system cannot model.
  - Here the base is found only with a value other than `no`, so `-f C:sub/tags` works without `--tag-relative`. The path of each file is made absolute and relative by itself, as the base may lie inside a directory walked; this costs a few passes over the path, next to reading the file. An input that cannot be made absolute ends the run once the walk is done, naming the least such path, with no tags file written.

**Appending**
- **OPT-20 [S][V] `-a`, `--append[=BOOL]`** (`entry.c openTagFile`, `updatePseudoTags`, `updateSortedFlag`; `sort.c internalSortTags`) adds the new tags to the tags file instead of replacing it. With output to stdout it is FATAL (OPT-16). A file that does not exist is written as without the option. One that exists, a tags file by CLI-3's test, is kept:
  - **No pseudo-tags** of the common set are written, not even to an empty file, which so gets none at all.
  - **The sorted flag.** In the file's leading lines that start with `!`, a line starting `!_TAG_FILE_SORTED` and a tab has the byte after that tab rewritten to the mode's digit (PT-4) when it is `0` or `1`. A `2` is left alone, as is such a line after the leading ones.
  - **Sorted modes** (SORT-1, SORT-3): the old lines and the new are sorted together, the old pseudo-tags among them, exact duplicates dropped and blank lines skipped, so an old file out of order comes out in order, whatever its flag says. The file is read as text, so a CRLF ends a line as LF does.
  - **`--sort=no`** (SORT-4): the new lines follow the old, which are left as they were, blank lines and line ends included.
  - Only when a tag was added (a parser's pseudo-tags count): with none, the file is left as it was, out of order or not, but for the flag.
  - The old lines are all kept, those of the files tagged again too, so a file tagged again has its old tags and its new ones (D9).
  - [V] On Windows, in a file whose lines end in LF alone, the flag's byte is found at a miscounted position, so the digit lands elsewhere (`!_TAG_FILE_SORTE0\t1` for `--sort=no`) or the flag is left as it was (`0` under `--sort=yes`), and the lines may be left out of order. A file with CRLF line ends is rewritten as the rule says.
  - Here the whole file is read into memory, where CR, LF and CRLF each end a line and blank lines are dropped, in every mode. Its order is checked by the mode's comparison, never taken from its flag: a file in order is merged as it is read, and one out of order is first sorted. The new lines are merged in, or under `--sort=no` follow the old, and the result, with LF line ends (D6, D24), is written to a new file that replaces the old, as without `-a`. The flag is rewritten as above, wherever the leading lines' line ends were LF or CRLF, blank lines among them ignored; the file is rewritten (sorted) even when no tag was added; and an empty file is written as a missing one is, with the pseudo-tags. A file too large for the memory the program reserves (about 1 GiB in a 32-bit process) stops the run before it is read, with `out of memory` and status 2.
  - Here the old lines of each file tagged in this run are dropped before the merge (D9): the lines not starting with `!_` whose second field, up to a tab or the line's end, is byte for byte the file field this run writes for one of the files it tags, whether or not that file has tags now. A file that is skipped, because it cannot be read or is too large (D7), is not tagged, and keeps its old lines. So appending the files of a tree to its own tags file changes nothing, and a file appended again replaces its old tags.
  - Under etags, see OPT-21.

**Emacs tags**
- **OPT-21 [S][V] `-e`, `--output-format=etags`** (`writer-etags.c`; `entry.c truncateTagLineAfterTag`; `read.c readLineRaw`, `readLine`; `options.c setEtagsMode`) write an Emacs TAGS file: `TAGS` unless `-f` names another, standard output for `-`.
  - **The mode.** Either option makes the output unsorted, sets `--tag-relative=yes` and enables the `Unknown` language (OPT-12), each until a later option changes it (OPT-7); `--output-format=e-ctags` after it writes e-ctags, but leaves the rest. The tags are those the same options write in a tags file: kinds, `--extras` (`F`, `anonymous`, `q`, `f`) and `-I` act as there, and `--fields`, `--format`, `--excmd`, `-n` and `-N` change nothing. There are no pseudo-tags, whatever the pseudo extra says.
  - **Sections.** Each file tagged gets one section, in the order the files are tagged (SORT-4), so a file named twice gets two: a form feed, LF, the path, `,`, the size, LF, then the entries. The size is the number of bytes of the entries, in decimal. The path is the file field's (OPT-18) as it is, unescaped, with its `\` on Windows: by default `--tag-relative=yes` makes a relative one relative to the TAGS file's directory, so `sub\c.f90` is `sub/c.f90`, while with `--tag-relative=no` it is `sub\c.f90`.
  - A file is tagged here as in a tags file, and also when it is of a language this program does not parse: `Unknown`, one from `--langdef`, or one forced (OPT-12). Such a file has no tags, so an empty section, `\f\nREADME,0\n`, and here it is not read.
  - **Entries**, one per tag, in the order the parser makes them: the tag's line, DEL (0x7f), the name as it is, SOH (0x01), the line number, `,`, the byte offset in the file where the line starts, a BOM counted, and LF. `int x;\x7fx\x011,0`, and for a file starting with a BOM, `int a;\x7fa\x011,3`.
    - The line is read from its start through its LF, with a CR before that LF dropped; another CR stays, so `int x;\r\r\n` gives `int x;\r`, as does a lone CR inside the line.
    - Then for a tag cut as OUT-4 says (macros, and Fortran's tags but labels), the line is cut after the last occurrence of the name, keeping one byte more unless that is the LF or the end of the file: `#define FOO 1` gives `#define FOO `, `  common /blk/ x` gives `  common /blk/`, `#define Y` at the end of a line `#define Y`. When the name is not in the line, the whole line is kept, with its LF: `subroutine fo&` continued on the next line by `&o(x)` gives the entry `subroutine fo&\n\x7ffoo\x011,0` [V].
    - Otherwise the LF goes.
    - Then the line is cut to the pattern length limit (OUT-7, 96 by default, 0 for none) if it is longer, keeping up to 3 more bytes past the limit while they are UTF-8 continuation bytes. Universal Ctags compares the limit with the line's length before the cut after the name, which only cuts it shorter, so the result is the same.
    - A tag whose line is past the end of the file gets no entry.
    - A NUL ends the line as Universal Ctags reads it, which then joins the next line on, after the NUL's: `int a; /*`, NUL, ` */`, LF, `int b;` gives `a` the text `int a; /* int b;` [V] (and LEX-1 loses `b`). Here a NUL is a byte of the text like any other (X4).
  - **File tags** (EX-7, `--extras=+f`): DEL, the name, SOH, `1,0`, LF, after the file's other entries. Files of the languages this program does not parse get them too.
  - Every line ends in LF. On Windows, Universal Ctags writes standard output as text, so there each LF becomes CRLF, as for tags (D6).
  - **The TAGS file** is created before any file is tagged, so a walk of its directory finds it, of no language, and gives it an empty section: `-R` writes `TAGS,0` [V]. Here, too, the file is created empty before the walk if it does not exist.
  - **A file that cannot be read** gets an empty section, named by the path as constructed even where `--tag-relative` makes the others relative, with WARNING `cannot open "PATH" : REASON`: with `-f ../T -R` from `wl`, `wl/a.c` but `sub/b.c,0` [V]. A file of no language is read too, so one that cannot be gets the same. Here a file that cannot be read gets no section, as it gets no tags (D19), nor does one too large to read (D7); one of a language not parsed here is not read, so it gets its section, named as any other, and no warning.
  - **`--sort=yes` or `foldcase`** after `-e` sorts the lines of the TAGS file as if they were tags, which merges the sections and loses lines: `a.c`, then `z.c`, give one section of `a.c`'s entries, sorted, and none of `z.c`'s [V]. Here the order is that of the walk, whatever `--sort` says (D18).
  - **`-a`** (OPT-20) adds the new sections after the bytes of the file as they are [V]. An existing file must still pass CLI-3's test, which accepts a TAGS file by its first line, or a tags file, to which the sections are then added.
  - Here the old sections of the files tagged in this run are dropped (D9): a section starts at a form feed that starts a line and is followed by LF or CRLF, and runs to the next; it is dropped when its head, up to its last comma, a CR before the LF dropped, is byte for byte the path this run writes for a file it gives a section, whether or not that has entries now. The rest is kept as it was, but for an LF added at the end of a file that lacks one, so that the first new section starts a line. So appending a tree to its own TAGS file changes nothing.

---

## 2. Universal Ctags inaccuracies

All are [V] unless marked. Each would be a whitelisted difference in an "improved" mode.

**Detection and discovery**
1. `.h` with ObjC markers is parsed as ObjectiveC under default languages: `miniaudio.h` yields 4,089 tags instead of 8,621.
2. `foo.C` is parsed as C on Windows (`copyconstructor.C` produces garbage).
3. Extension-less libc++ headers (228 files), `.inc` (569; e.g. `Unix/Path.inc` gives 96 tags when forced to C++), `.cppm` and `.ipp` are never tagged.
4. `.inl`, `.hp` and `.cuh` are not treated as headers, so everything in them gets `file:`.
5. Windows paths: ANSI code page, `MAX_PATH`, no junction-loop protection [I]. `--exclude-exception` re-walks `.git`. Temp files leak on broken pipe or FATAL. The tags file is rewritten in place, not atomically (an interrupted run left `~/src/llama.cpp/tags` claiming `SORTED 1` while unsorted).

**Lexing and preprocessing**
1. `/+` is taken as a D comment, which swallowed the rest of `aidrivers.c` from line 225.
2. Strings do not end at a newline (`KOO` lost).
3. The digit-separator rule eats the quote in `u8'x'` (`u8_char` lost).
4. NUL bytes merge lines (`null-character-in-literal.c` gives 0 tags; `folding05.f90` gives 3 tags instead of 129).
5. Non-ASCII identifiers are dropped. UCN macro names give bogus tags (`u00FC`, `a`).
6. `_Pragma(...)` with no `;` merges into the next declaration.
7. The first `#if/#else` inside any `{}` keeps only one branch (compiler-rt `cmsg_len`, hexagon `HMX_QUEUE_POLL_COUNT`). An unterminated statement before a top-level `#if/#else` drops the else branch (gnulib `time.in.h` gives 0 tags).
8. `#if 0x1` and `#if 0||X` are skipped while `#if (0)` is taken [S]. `#elifdef` is not recognized [S]. Nesting above 19 desynchronizes [S]. Pass 2 tags `#if 0` code [S].

**C/C++ parsing**
1. Macro invocations without `;` eat the next definition (C3/B7). This is the most frequent loss: about 250 `INITIALIZE_PASS*` sites in llvm/lib, each likely losing one definition.
2. A nested call in a constructor-style initializer loses the variable (C2): 16% of `cl::opt` globals.
3. `enum T` used in a declarator's type gives a bogus enum and enumerators (C1): 11 in llama.cpp, including `common.h` `samplers`.
4. `MACRO(x)` before a class name gives a bogus function (B2).
5. A leading `requires` clause gives the wrong name (B3).
6. `namespace MACRO X` tags the macro (B4); `a::inline b` loses the rest of the file (B1).
7. `T x = U{…}` is missed (B5); generic lambdas are missed (B6).
8. Trailing annotation macros (B8) and `} PACKED;` (B9).
9. Explicit instantiations with an export macro become variables (B10, C7).
10. A macro access label loses the next member (C6); a call inside a macro argument becomes the function name (C4); a macro return type becomes the function name (C5).
11. Parenthesized function names, and locals leaking as globals.
12. File-scope compound literals lose the variable.
13. Bit-field widths that are not plain numbers lose the member.
14. `static` after a struct body is ignored and leaves a trailing space in typeref.
15. Typerefs keep noise (`register`, `_Atomic`, `__extension__`, export macros) and gain trailing spaces from mid-type `restrict`.
16. K&R definitions have no typeref.
17. Qualifier scopes are always `class:` (2,574 `class:llvm` tags in llvm/lib).
18. Template arguments in scope are inconsistent (kept for variables, stripped for functions).
19. Functions in an anonymous namespace get no `file:`.
20. In headers, C code using C++ keywords as identifiers is lost (`int class;`).
21. Error recovery is all or nothing: `recovery.c` loses everything after line 19.
22. Concepts are inconsistent, and structured bindings are never tagged.

**Fortran**
1. F2008 `module function`/`module subroutine` in interfaces create bogus modules named `subroutine`/`function`, and real units are lost.
2. `module procedure … end procedure` bodies end the submodule.
3. The prefixes `impure`, `non_recursive` and `simple` lose the unit.
4. `end critical`, `end team` and `end file` end the unit early.
5. Unknown attribute statements end the specification part; unknown attributes in a list drop the whole entity list.
6. Joined `end` forms: uppercase is not recognized; lowercase after a spec-only body swallows the next unit.
7. Module-level `generic ::` gives bogus variables.
8. A leading `_` in a module name loses the module tag.
9. Keyword-named subprograms lose their scope.
10. Anonymous fixed-form tags point at the next line.
11. Labels after a subroutine header are unscoped.

**Output**
1. Truncation uses the **last** occurrence of the name, so patterns like `H	/^H /` are ambiguous.
2. A cut pattern ending in `$` leaves the `$` unescaped; a lone CR or NUL keeps a `$` anchor that cannot match [S].
3. Identical lines from different `#ifdef` branches are merged.
4. There is no `line:` field: 4.5% of named tags in a llama.cpp sample land on an earlier identical line in vim.
5. Fortran common-block line numbers sort as strings (`180;"` before `63;"`).

---

## 3. Hard parts and risks

1. **The parity surface is heuristic and bug-defined.** Byte equality needs u-ctags' statement-chain heuristics reproduced exactly: first matching `ident(paramlist)`, the parameter-list test, the `MACRO(x)` special cases, struct trailers, prefix cleanup, typeref spacing and sanity checks. Each fix in section 5 is a deliberate divergence.
   - Recommend two modes, `--compat` and improved, plus a ledger of expected differences.
2. **The preprocessor and parser are coupled** through `resolveRequired` and block depth, which decide which branches are scanned. The branch decision changes which tags exist and how anonymous names are numbered.
3. **The two-pass protocol.**
   - Pass 2 changes the preprocessor (`#if 0` parsed, no single-branch forcing).
   - The anonymous counter carries over from the failed pass, and a failing pass 2 truncates output at the failure point.
   - Copying this exactly is required for parity on broken files, and it is the most likely source of rare diffs.
4. **Anonymous numbering** is a global per-file sequence over every creation site, including disabled kinds, unnamed parameters when `z`/`p` are on, and the failed pass. Any deviation renames many tags and every scope that contains them.
5. **Pattern bytes.** The truncation rules, the 96-byte limit counted after escapes, the UTF-8 allowance, the `$` rule, CR-LF and lone-CR handling, NUL truncation, and the BOM skip.
6. **Fortran form detection** depends on exact column rules and look-ahead side effects: labels made during look-ahead, statement-end tokens carrying the next line's number. Pass-1 output is discarded on failure.
7. **Windows path semantics.** Verbatim printing with `\`→`/` at output time only, while the hash uses the raw string; the join rules (`.` root gives bare names; no separator added after an existing one); CP1252 bytes.
8. **Pseudo-tags** depend on which languages occurred, which kinds are enabled (with C/C++ sync), and the working directory.
9. **Policy decisions:**
   - CUDA (`.cu`/`.cuh`, 281 files in llama.cpp): skip for parity, or tag as C++ in improved mode.
   - ObjC `.h` files: C++ under the parity target.
   - Extension-less libc++ headers.
   - CRLF vs LF output: CRLF for byte parity; LF is fine for vim.
10. **Performance.** The u-ctags baseline is 11–26 MB/s. llama.cpp includes 53 MB of hex arrays and a 13 MB single line.
11. **Corpus assumptions.** No file in either tree is ObjC-ambiguous in llvm except about 200 `.h` files; none has a lone CR or a junction. The NUL, CRLF, BOM and Latin-1 cases are few but present (6 NUL, 559 CRLF, 4 BOM, 14 non-UTF-8 in llvm).

---


## 4. Departures

Decided for this program, deliberately different from Universal Ctags.
The IDs are stable names, used in tests, the ledger and commits: D for
output, options and discovery, O for patterns, X for lexing, P for C
and C++ parsing, F and R for Fortran. Their numbers give no order of
importance.

### Output, options and discovery

- **D1** `.C` is C++ (on Windows, Universal Ctags makes it C). C++'s
  map holds a `C` of its own, which takes only names whose extension is
  exactly `C`, and takes them before any extension is compared ignoring
  case (LANG-1, LANG-2), as where file names have case. Nothing else
  departs: extensions given in options match ignoring case, capitals
  or not, so `--langmap=c:.X` takes `.x` too, and the `C` goes as an
  item equal to it would, so `--map-C++=-.C` makes `.C` C again. With C
  disabled or without its `c`, `.C` stays C++, where Universal Ctags
  tags it as nothing.
- **D2** `.inl` and `.hp` are headers, and `X.h.in` is tested by the
  extension of `X.h` (no spurious `file:` fields), whatever the
  language, forced or not. A final `.in` is recognized ignoring case, so
  `x.c.IN` is C, where Universal Ctags tags nothing.
- **D3** `line:N` is on every tag, right after the kind, as
  `--fields=+n` writes it, so that Vim starts its search at the tag's
  line instead of jumping to an earlier identical line (4.5% of tags in
  a llama.cpp sample).
- **D4** Links and junctions to directories are skipped while recursing,
  so there are no cycles. Links to files are followed, unless
  `--links=no`, which skips them and links given as arguments, on
  Windows too (DISC-7).
- **D5** Paths are UTF-8 (WTF-8) with no MAX_PATH limit.
- **D6** Lines end in LF, also on Windows, where Universal Ctags writes
  CRLF.
- **D7** Files over 64 MiB are skipped with a warning.
- **D8** A directory argument is always tagged recursively, so `ctags .`
  is `ctags -R`. Without `-R`, Universal Ctags silently ignores it (exit
  0, no tags), which is never what was meant. `-R` keeps one meaning of
  its own: with no files, it tags the current directory.
- **D9** `-a` replaces the old lines of each file tagged again
  (OPT-20): those lines of the tags file whose file field is the one
  this run writes for a file it tags, even one without tags now, are
  dropped, and pseudo-tags never are. Universal Ctags keeps every old
  line, so a file appended again has its stale tags and its new ones,
  both found by Vim, and one emptied of tags keeps them all. Plugins
  that append, as gutentags does, filter the file's lines out first,
  which now changes nothing. Under etags, the old sections of the files
  tagged again are dropped in the same way (OPT-21), where Universal
  Ctags adds a second section for each, so Emacs offers both the stale
  tags and the new ones.
- **D10** `;"` is written whenever any field is (FLD-4), also before a
  parser's field written alone, as C++'s `name:` is with `--fields=`.
  Universal Ctags writes `A\ta.cpp\t/^namespace A = N;$/\tname:N`,
  which Vim and other readers take as an address ending in
  `\tname:N`, so the tag is not found.
- **D11** With `--exclude-exception` given, an excluded directory is
  still pruned, unless an exception matches the directory itself, and
  an excluded file is dropped before it is examined (DISC-3). Universal
  Ctags then skips its first exclusion check altogether, so it walks
  every excluded directory, `.git` included (`--exclude=sub
  --exclude-exception=x` tags `sub/`), checking only regular files, and
  warns that a missing file it excludes cannot be opened.
- **D12** On Windows, `\` becomes `/` in the patterns of an
  `--exclude=@FILE` or `--exclude-exception=@FILE` list too, as it does
  in those given directly and in the names they match (DISC-3).
  Universal Ctags leaves them, so there they quote the next character:
  `sub\deep` in a list matches only `subdeep`, and Windows `wildignore`
  entries written with backslashes match nothing.
- **D13** A `-L` list given after a file name is read, as one given
  before is (DISC-6): `ctags a.c -L list` tags `a.c` and the files the
  list names, and a list that cannot be opened is FATAL. Universal Ctags
  parses such an option but never reads its list, so it tags `a.c`
  alone, and exits 0 though the list does not exist.
- **D14** Under `--tag-relative` (OPT-18), doubled separators are no
  name of their own where Universal Ctags' counting them goes wrong:
  `..` after them takes the name before them (`x//../a.c` is `a.c`,
  where Universal Ctags writes `x/a.c`, which is not the file); they
  are no level of the tags file's directory (with `-f sub//tags`,
  `a.c` is `../a.c`, not `../../a.c`); and a relative path does not
  begin with a separator (with `-f -`, `.//a.c` is `a.c`, not `/a.c`,
  and with `-f sub/tags`, `sub//b.c` is `b.c`, not `/b.c`). They stay
  where they lead to the file, as there: `..//a.c`, `/w//a.c`.
- **D15** Under `--tag-relative` (OPT-18), a `..` at the root of a
  Windows drive stays there, as Windows resolves it: `C:\..\x` is
  `C:/x`. Universal Ctags removes the drive's `:` with it, as its
  separator, writing `C/x`, which names no file, relative or absolute.
- **D16** Under `--tag-relative` (OPT-18), a UNC path, `\\host\share\…`,
  is absolute with its share as its root, as a drive is: made absolute
  it stays `//host/share/…`, `..` does not leave the share, and it is
  made relative only to a base under the same share, matched
  regardless of case, else written absolute. A path rooted without a
  drive takes the current directory's root, drive or share. Universal
  Ctags puts the current drive before a UNC path, `C://host/share/a.c`,
  which names no file, or relative climbs to that drive's root,
  `../../..//host/share/a.c`; and from a UNC current directory it puts
  `@:` before `/x`, its drive number 0 as a letter.
- **D17** A file tag's `epoch:` (EX-7) is the file's modification time
  as the system keeps it, in seconds since 1970 UTC, on Windows too.
  There Universal Ctags takes it from the C runtime's `stat`, which
  shifts it by the daylight saving offset in effect when ctags runs, so
  while that is on, a file last modified while it was off is written an
  hour late (`1768500000` for `1768496400`), and the epoch of an
  unchanged file changes twice a year.
- **D18** Emacs tags are written in the walk's order, a section per
  file, whatever `--sort` says (OPT-21). With `--sort=yes` or
  `foldcase` after `-e`, Universal Ctags sorts the TAGS file's lines as
  a tags file's, so the sections' heads gather at the top, entries leave
  their sections, and some are lost: a file that Emacs cannot read.
- **D19** Under etags, a file of our languages that cannot be read gets
  no section, as it gets no tags (OPT-21). Universal Ctags writes it an
  empty one, named by the path as constructed where `--tag-relative`
  names the other sections from the TAGS file's directory, so the name
  may be of no file at all, or of another one.
- **D20** Fewer pseudo-tags: the nine whose values readers act on,
  `!_TAG_FILE_FORMAT`, `!_TAG_FILE_SORTED`, `!_TAG_OUTPUT_EXCMD`,
  `!_TAG_OUTPUT_FILESEP`, `!_TAG_OUTPUT_MODE`,
  `!_TAG_PATTERN_LENGTH_LIMIT`, `!_TAG_PROC_CWD`, `!_TAG_PROGRAM_NAME`
  (`ctags`) and `!_TAG_PROGRAM_VERSION`, with PT-4's values, only in a
  tags file, and not with `--extras=-p` (EX-5). There are no
  descriptions of kinds, fields, extras or roles, and no parser or
  output versions. Nine lines are enough for LeaderF, which skips six.
- **D21** The tags file is replaced atomically (written to `NAME.tmpN`,
  then renamed), so Vim never sees a partial file, and no temporary
  files leak.
- **D22** Options apply to every file wherever they appear: on the
  command line, in option files, or in a `-L` list (CLI-6). Universal
  Ctags' options are parsed with its grammar and messages (section
  1.12). Those that act here are `-R`, `--recurse`, `-f`, `-o`,
  `--kinds-LANG` (and `--LANG-kinds`, `--LANG-types`, `--kinds-all`),
  `--extras` (and `--extra`, `--file-scope`, `--file-tags`), `--fields`
  (FLD-5), `--excmd`, `-n`, `-N` (OUT-6), `--pattern-length-limit`
  (OUT-7), `--format` (OUT-8), `--output-format=e-ctags` (FLD-6), `-e`
  and `--output-format=etags` (OPT-21), `--options`, `--exclude`,
  `--exclude-exception`, `-L`, `--links`, `--languages`,
  `--language-force` (and `--lang`, `--language`), `--langmap`,
  `--map-LANG`, `-h`, `-I` (and `--param-CPreProcessor.ignore`),
  `--tag-relative`, `--sort` and `-u` (SORT-3, SORT-4), `-a` and
  `--append` (OPT-20), the listings (OPT-15), `--version` (OPT-19),
  `--help`, and this program's `--jobs`. The others are accepted at
  their defaults and where they name other languages (OPT-4, OPT-6),
  and otherwise refused (OPT-5). Of the extras, only `anonymous`, `F`,
  `f`, `p` and `q` have effects (EX-6, EX-7).
- **D23** Smaller departures in parsing options:
  - a letter that takes a parameter is FATAL as the last argument
    (OPT-1);
  - option files skip lines of only blanks, print no empty line after
    their warning, do not make later options follow a file, and do not
    make later option files warn; `--options=NONE` is taken anywhere,
    and paths are not searched for (OPT-14);
  - listings hold only our languages (OPT-15);
  - `--langdef=NAME{base}` warns where Universal Ctags crashes (OPT-12);
  - a `-L` list is read before anything is tagged, skips lines of only
    blanks, and may hold initOnly options after its files (DISC-6);
  - forcing a language this program does not parse tags nothing, where
    the reference is FATAL as the language is disabled, and files of
    such languages, as `--langdef` makes them, get no file tag (EX-7);
    forcing a disabled one of ours is FATAL even when no file would be
    tagged (LANG-6);
  - a tags file named on a drive but not from its root, `-f C:sub/tags`,
    is refused only with `--tag-relative`, and an input so named ends
    the run after the walk, not while tagging, with no tags file
    written; and the current directory is looked up only with
    `--tag-relative` and for the `PROC_CWD` pseudo-tag, so that failing
    to find it ends the run only with `--tag-relative`, and the message
    gives no reason, and otherwise leaves that pseudo-tag out (OPT-18).
- **D24** Appending (OPT-20) reads the tags file whole and writes it
  anew, so CR, LF and CRLF each end an old line, blank lines go, and
  every line ends in LF, also under `--sort=no`; the sorted flag is
  rewritten where Universal Ctags on Windows misplaces its byte in a
  file with LF line ends; the result is sorted even when no tag was
  added, where Universal Ctags leaves an old file out of order as it
  was; and an empty file gets the pseudo-tags a new one does, where
  Universal Ctags writes it none, which leaves readers that skip a fixed
  number of lines, as LeaderF does, skipping tags. Under etags (OPT-21),
  an old file whose last line lacks its LF gets one before the new
  sections, where Universal Ctags runs that line into the first new form
  feed, which then starts no section.
- **D25** The fields `e` (`end:`) and `o` (`nth:`) are accepted and write
  nothing (FLD-5), since the parsers do not track where definitions end
  or count members. A reader of either must treat a missing field as
  unknown, as it would for a tag Universal Ctags has none for.
- **O1** A `$` that ends a cut pattern is escaped, so Vim cannot read it
  as an anchor.
- **O2** A lone CR ends the pattern without the `$` anchor.

### Lexing

- **X1** `/+` does not start a comment.
- **X2** Strings end at an unescaped newline.
- **X3** A `'` digit separator applies only inside numbers, so `u8'x'`
  is a character literal.
- **X4** NUL is ordinary data, not the end of a line, and ends a pattern
  without the `$` anchor. Field values escape it as `\x00`, but e-ctags,
  which writes a signature as it is, drops a tag whose signature holds
  one (FLD-6), as it does for a tab, so it never writes a NUL byte.
  Emacs tags keep it in an entry's text (OPT-21), which Emacs searches
  for as it is, where Universal Ctags writes the line up to the NUL and
  the next line after it. In a
  `-I` file it ends the line's token, and the line still ends at its
  newline, where Universal Ctags drops the rest of the line and joins
  the next one on (LEX-1): `A`, NUL, `junk`, LF, `B` is its one token
  `AB`.
- **X5** Bytes 0x80 and above are identifier characters.
- **X6** `_Pragma(...)` and MSVC's `__pragma(...)` are dropped like
  `__attribute__(...)`, so pragmas between a declaration's parts do not
  hide its type. When `-I` names one, it acts as `-I` says (LEX-14).
- **X7** `#elifdef` and `#elifndef` act as `#elif`.
- **X8** Conditionals nest 64 deep, and push and pop stay in step beyond.
- **X9** A `??` that is not a trigraph is pushed back in order.
- **X10** A macro parameter keeps its `macro:` scope when the macro is
  not tagged, because its kind is disabled or (outside headers) the
  fileScope extra is. Universal Ctags finds the scope through the
  macro's tag, and so loses it.
- **X11** A `-I` replacement ending in an identifier character is kept
  apart from an identifier character after it, as a preprocessor keeps
  tokens apart (LEX-14). Universal Ctags pushes back the character after
  the macro's group, or the blanks it skipped looking for one, right
  after the replacement, so with `-I X+=int` it loses `y` in `X y;` and
  `X(a)y;` (`inty`), and types `unsigned X(a)long t` as
  `unsigned intlong`. Here a space goes between them.
- **X12** Reaching the end of the file does not reset the counts that
  limit a `-I` entry to 8 replacements in a row (LEX-14), so a loop of
  replacements at the end of the input stops. Universal Ctags resets
  them whenever it reads the file, at its end too, so it never finishes
  `-I S+=;S` on a file ending in `S`, where the entry, looking for a
  group, reaches the end, nor `-I T=int*v;T` on one ending in `T`
  without a newline. Here the statements stop after 8 replacements.

### Parsing

Each landed as its own commit with an IMPROVED tag case and ledger
entries for its corpus differences.

C and C++:

- **P1** Macro calls without a `;` (`INITIALIZE_PASS_BEGIN(...)` and
  `...END(...)`, `LLVM_PREFERRED_TYPE(bool)`) no longer join the
  declaration on the lines after them. When the signature found lies
  in a leading run of `NAME(...)` pairs, and the next token begins a
  later line, the rest is tried alone: as a signature (without
  parameters, so no anonymous names), or at `;` as variables (a dry
  run, writing nothing). If it declares something, the run is dropped
  and the statement analyzed again; otherwise nothing changes.
- **P2** The search for a declarator nested in parentheses
  (`int (*f)(void)`) skips parentheses with a top-level comma or a
  leading literal, which are a call's arguments:
  `cl::opt<bool> X("x", cl::init(false));` declares `X`, not `init`.
- **P3** `MACRO(x) *name(...)`, with `*`, `&` or `&&` between, names the
  function `name`, as `MACRO(x) name(...)` already did.
- **P4** A statement of only `NAME :` is cleared after the label check,
  so a macro used as an access specifier keeps the next member.
- **P5** `enum` after `<` or `,` is not a definition, and an enum head
  ending at `=` declares variables: `enum T x[] = {a, b};` is a variable
  `x`, not an enum `x` with enumerators `a` and `b`.
- **P6** `template class X<int>;` (an explicit instantiation, perhaps
  after `extern`) declares nothing, and nor does a class head ending at
  `;` after a template prefix (`template<class T> class API X;`).
- **P7** `struct MACRO(x) Name {` is `struct Name {`: a parenthesized
  list right after the first name following the keyword is dropped with
  that name, when a name or `{` follows and it does not begin with `*`
  or `&` (`struct S (*f)(...)`), and not after `operator`.
- **P8** Typeref spacing is decided against the next token kept, not a
  dropped keyword: `int * restrict p` is `int *`, not `int * `.
- **P9** `} static x;` after a struct body makes `x` file-scoped.
- **P10** Functions, variables, and static members in an anonymous
  namespace are file-scoped outside headers, as static ones are.

Fortran (section 1.6 marks where each departs):

- **F1** `impure`, `non_recursive` and `simple` are subprogram prefixes
  (FTN-11, FTN-15, FTN-20) when the next token is another prefix,
  MODULE, a type specifier, FUNCTION or SUBROUTINE, so units declared
  with them are tagged, and interfaces that contain them do not end
  early. They stay identifiers otherwise, so variables may have these
  names.
- **F2** MODULE is the prefix of a separate module procedure, at the top
  level (FTN-11) and in interfaces (FTN-15), when another prefix, a type
  other than PROCEDURE, FUNCTION or SUBROUTINE follows, and the statement
  goes on after that. So `module function f(x)` no longer makes a module
  named `function` (whose dummies became its variables), and inside an
  interface, its END no longer ends the interface and with it the
  module. MODULE PROCEDURE is unchanged: its bodies in a CONTAINS part
  still end the module at their first END (FTN-21).
- **F3** A name may start with `_` (FTN-4), as flang's runtime modules
  do (`module __fortran_builtins`), so they are tagged and give scope.
- **F4** END CRITICAL, END TEAM, and END FILE (the ENDFILE statement),
  whose words are not keywords, close a construct in an execution part
  instead of ending the unit (FTN-20). The word is matched ignoring case.
- **F5** A joined END ignores case (`ENDSUBROUTINE`, FTN-4), and it ends
  a specification part (FTN-12) instead of being skipped as a statement
  function together with the next unit's header (`endmodule` after a
  module with only declarations lost the next module).
- **F6** A subprogram named by a keyword (`function sequence`,
  `subroutine inline`) is the scope of its contents (FTN-7).
- **F7** A GENERIC statement in a specification part (outside a type)
  declares a generic interface (FTN-13): its name, or the operator of
  `operator(op)` or `assignment(=)`, is tagged `i` in the enclosing
  scope, as `interface name` would be, instead of the name, a variable
  `operator`, and the specific procedures, all as variables.
- **F8** An unknown attribute in a list (FTN-13), a name with an optional
  `(…)` such as `contiguous`, `volatile`, `device` or `rank(2)`, is
  skipped instead of losing the statement's entities. The attribute
  statements BIND, CODIMENSION, and ASYNCHRONOUS or CONTIGUOUS before
  `::` or a name are skipped without ending the specification part
  (FTN-12), which made the unit skip its later declarations and the
  subprograms it contains, up to the first END.
- **R1** Subprograms, derived types, DEC structures and unions nest at
  most 1024 deep, together; a deeper one's header statement is skipped,
  and what it contains is read as part of the enclosing construct.
  Universal Ctags has no limit and runs out of stack (it crashes at
  about 20000 nested subroutines).

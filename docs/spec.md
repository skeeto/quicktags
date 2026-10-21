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

Departures are listed in section 4, and some under the rules where they
apply.

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
  - A name starting with `-` that is longer than one character is FATAL.
  - Repeating the option gives a warning, and the last value wins.
  - Output to an existing directory is FATAL; so is overwriting a file that is not a tags file (G9) [S].
- **CLI-4 [V]** `--kinds-<LANG>=SPEC`.
  - The language name is case-insensitive (`c++`, `C++`, `fortran`). There are no aliases: `--kinds-cpp` gives WARNING `Unknown language "cpp" in "kinds-cpp" option` and is ignored.
  - SPEC grammar:
    - A leading `*` enables all kinds.
    - A first character other than `+`, `-` or `*` first disables all kinds.
    - `+` and `-` switch the mode for the letters that follow (`+p-d`).
    - `{longname}` is case-sensitive.
    - An empty SPEC disables all kinds of that language.
    - An unknown letter gives WARNING `Unsupported kind: 'Q' for --kinds-C option` and is ignored.
  - The old forms `--c-kinds=` and `--c++-types=` also work. The language is everything before the first `-`.
  - `--kinds-all=` accepts only `*`, `F`, `{file}`.
- **CLI-5 [V]** C/C++ kind sharing. The letters `d e f g h l m p s t u v x z L D` are **shared** between C and C++ in both directions, so `--kinds-C=+p` turns on prototypes in `.h` files. The C++-only letters `c n A N U Z M P` are independent. CUDA is independent.
- **CLI-6 [V]** Options may follow files and apply to later files only. initOnly options after a file are FATAL: `-a -e -f -o -u -x`, `--sort`, `--pattern-length-limit`, `--tag-relative`, `--format`, `--output-format`, `--list-*`, …
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

- **DISC-1 [S]** Each argument and each recursed entry goes through `createTagsForEntry` (`main.c` 202-225), in this order:
  1. Early exclude check on the name.
  2. Symlink check. It never fires on Windows (`lstat` is `stat`).
  3. Missing: WARNING `cannot open input file`.
  4. Directory: recurse if `-R`, else silently ignore.
  5. Not a regular file: ignore.
  6. Late exclude check.
  7. Language detection, then parse.

  Command-line files and recursed files behave identically [V].
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
- **DISC-3 [V]** Default exclude list:
  `*.a *.class *.dll *.exe *.gcda *.gcno *.lib *.o *.obj *.pyc *.pyo *.so *~ .*.swp .DS_Store .arch-ids .arch-inventory .bzr .bzrignore .cvsignore .deps .dvi .git .gitattributes .gitignore .hg .hgignore .svn BitKeeper CVS EIFGEN PENDING RCS RESYNC SCCS _darcs autom4te.cache {arch}`
  - Matching tries the basename, then the full constructed path.
  - On Windows `\` becomes `/` in both pattern and name, and both are uppercased before gnulib `fnmatch(p, f, 0)`. So `*` and `?` match `/`, and a leading `.` is not special.
  - An excluded directory is not descended (early stage).
  - `--exclude=` with an empty value clears the list.
  - **[V] Bug:** any `--exclude-exception` disables the early stage, so excluded directories get walked.
  - No corpus directory matches the case-insensitive list except `.git`, which I checked with `find`.
- **DISC-4 [S][I]** `--maxdepth` defaults to `0xffffffff`; the top directory counts as depth 1. There is no loop detection on Windows.
- **DISC-5 [V]** The printed file name is the argument or constructed path **verbatim**, with only `\`→`/` at output time. `//`, `./` and `..` are kept, and so is case. Absolute `C:\…` prints as `C:/…`.

### 1.3 Language detection

- **LANG-1 [V][S]** The extension is the text after the last `.` of the basename, compared case-insensitively on this build:

  | Language | Extensions |
  |---|---|
  | C | `c` |
  | C++ | `c++ cc cp cpp cxx h h++ hh hp hpp hxx inl` |
  | Fortran | `f for ftn f77 f90 f95 f03 f08 f15` |

  - Because the build has `CASE_INSENSITIVE_FILENAMES`, `foo.C` is **C** (on Unix it would be C++). `.H`, `.CPP`, `.F90`, `.F` map case-insensitively.
- **LANG-2 [V]** If the name ends in `.in`, the suffix is stripped and the lookup repeated: `config.h.in` is C++.
- **LANG-3 [V]** There is no content sniffing without `-G`. Extension-less files, including libc++'s `vector`, and `.inc .def .ipp .tcc .cppm .ixx .cuf .fpp .f18` are not tagged.
- **LANG-4 [V]** Under the parity target, every `.h` is C++, never C or ObjC. [I] Every `.f` is Fortran.
  - Under default languages:
    - `.h` becomes ObjectiveC if any `mio_gets` chunk of up to 0x800 bytes begins with `#import`, `@interface `, `@implementation ` or `@protocol ` (`selectors.c` 181-235).
    - `.f` becomes Forth if a chunk begins with `\`, or with `:` followed by whitespace (346-361).
- **LANG-5 [S][V]** A file is a header iff its full-name extension, compared case-insensitively, is one of `h H hh hpp hxx h++ inc def` (`options.c` 131; `-h` changes the list).
  - So `.inl`, `.hp`, `.cc`, `.cpp`, `.c` and `.h.in` are **not** headers.

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
`fortran.cpp` keeps that order of reads; FTN-1 to FTN-23 describe it.

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
  - Anything else ends the part, unread.
- **FTN-13 Type declarations.** A type specifier, then `, attributes` and `::`, then a list of entities, each tagged with the kind of FTN-6.
  - Specifiers: CHARACTER `[*] (…)` or `[*] N`; BYTE, INTEGER, REAL, COMPLEX, LOGICAL, PROCEDURE and CLASS with `(…)`, `*N` or `*(…)`; DOUBLE PRECISION and DOUBLE COMPLEX (DOUBLE followed by anything else skips to the statement end); RECORD `/s/`; TYPE `(…)`, while TYPE followed by anything else defines a derived type (FTN-14); FINAL, GENERIC and ENUMERATOR are just the word.
  - Attributes: `parameter allocatable external intrinsic kind len optional private pointer protected public save target nopass abstract deferred non_overridable` alone, `dimension intent bind extends` with `(…)`, `pass` with an optional `(…)`, `codimension` with `[…]`. **Any other attribute skips to the end of the statement, losing its entities.**
  - Entities are identifiers, or keywords other than FUNCTION and SUBROUTINE. After each, `(…)`, `[…]`, `*len` or `*(…)`, a DEC `/initial value/`, and an initializer after `=` or `=>` (to the next comma outside brackets) are skipped. A list that starts with `%` (DEC `%FILL`) skips the statement.
  - So a module-level `generic :: g => a, b` tags `g` and `b` as variables.
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
  - Scope name and the typeref value: same as the name.
  - Typeref prefix: as-is.
- **FLD-3 [V]** The C++ `name:` field is never seen by default; it appears only on `A` tags.

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

### 1.9 Sorting

- **SORT-1 [S][V]** Sort whole lines (without CR or LF) with `strcmp`, i.e. unsigned bytewise. An exact duplicate of the previous line is dropped; blank lines are skipped. Pseudo-tags are sorted along with everything else.
  - TAB sorts below every name byte, so a name that is a prefix of another sorts first; then the file, then the address, then the fields.
- **SORT-2 [I]** Sorted output does not depend on traversal order. Anonymous names depend only on each file's own path and content.
- **SORT-3 [V]** `--sort=foldcase` uses `struppercmp` (`_` sorts after letters) and writes `!_TAG_FILE_SORTED 2`. Not default.

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

### Output and discovery (built in from the start)

- **line:N on every tag**, right after the kind, as `--fields=+n` writes
  it, so that Vim starts its search at the tag's line instead of jumping
  to an earlier identical line (4.5% of tags in a llama.cpp sample).
- **LF line endings**, also on Windows, where Universal Ctags writes CRLF.
- **Minimal pseudo-tags**: `!_TAG_FILE_FORMAT`, `!_TAG_FILE_SORTED`,
  `!_TAG_PROGRAM_NAME`, `!_TAG_PROGRAM_VERSION`, only in a tags file.
- **The tags file is replaced atomically** (written to `NAME.tmpN`, then
  renamed), so Vim never sees a partial file, and no temporary files leak.
- **D1** `.C` is C++ (on Windows, Universal Ctags makes it C).
- **D2** `.inl` and `.hp` are headers, and `X.h.in` is tested by the
  extension of `X.h` (no spurious `file:` fields).
- **D4** Links and junctions to directories are skipped while recursing,
  so there are no cycles. Links to files are followed.
- **D5** Paths are UTF-8 (WTF-8) with no MAX_PATH limit.
- **D7** Files over 64 MiB are skipped with a warning.
- **O1** A `$` that ends a cut pattern is escaped, so Vim cannot read it
  as an anchor.
- **O2** A lone CR ends the pattern without the `$` anchor.
- Options apply to every file wherever they appear on the command line.
  Only `-R`, `--recurse`, `-f`, `-o`, `--kinds-LANG` (and `--LANG-kinds`,
  `--LANG-types`), `--jobs`, `--version`, and `--help` are accepted.

### Lexing (built in from the start)

- **X1** `/+` does not start a comment.
- **X2** Strings end at an unescaped newline.
- **X3** A `'` digit separator applies only inside numbers, so `u8'x'`
  is a character literal.
- **X4** NUL is ordinary data, not the end of a line, and ends a pattern
  without the `$` anchor.
- **X5** Bytes 0x80 and above are identifier characters.
- **X6** `_Pragma(...)` is dropped like `__attribute__(...)`.
- **X7** `#elifdef` and `#elifndef` act as `#elif`.
- **X8** Conditionals nest 64 deep, and push and pop stay in step beyond.
- **X9** A `??` that is not a trigraph is pushed back in order.

### Parsing (after parity)

See the plan; each lands as its own commit with a pinned test.

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

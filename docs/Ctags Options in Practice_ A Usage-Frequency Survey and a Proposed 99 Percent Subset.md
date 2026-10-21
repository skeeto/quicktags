# Ctags Options in Practice: A Usage-Frequency Survey and a Proposed "99% Subset"

A small set of options covers nearly all real-world ctags invocations. Those options are `-R`/`--recurse`, `-f`/`-o` (including `-f -`), `--exclude`, `--languages`, `--fields`, `--extras`/`--extra`, `--<LANG>-kinds`/`--kinds-<LANG>` (almost always `--c-kinds=+p`/`--c++-kinds=+p[x]`), `-a`/`--append`, `-L`, `-e`, `--tag-relative`, `--sort`, `--excmd`, `--options`, `--language-force`, `--format=2` and `--output-format`. `--extras`/`--extra` is **not rare**. Among options that change tag-file contents it ranks among the most common, mainly through two idioms: `+q` (class-qualified tags, from the OmniCppComplete/kernel `--fields=+iaS --extra=+q` recipe) and `+f` (file-name tags). Plugins also pass it on every call (`--extras=+F` / `--extra=`).\[1\]\[2\]

## TL;DR

- **Core (most invocations):** `-R`, `-f`/`-o` (with `-f -` for stdout in plugins), and the zero-option form `ctags -R` / `ctags -R .`. The ctags(1) man page says ctags "is most commonly executed without any options (e.g. 'ctags *', or 'ctags -R')". That wording comes from Exuberant Ctags and Universal Ctags kept it. After those come `--exclude`, `--languages`, `--fields=+…`, `--extra[s]=+q/+f`, `--c[++]-kinds=+p[x]`, `-a`, `-L -`, `-e`, `--tag-relative=yes`.
- **`--extras`/`--extra` is common, not rare.** It shows up in the Linux kernel's `scripts/tags.sh` (`--extra=+fq`). Philip K. Gisslow's September 2021 LKML patch made that line version-aware as `--$CTAGS_EXTRA=+fq`, noting that universal-ctags "has replaced the obsolete --extra flag with --extras". It is also in the canonical C++ recipe `ctags -R --c++-kinds=+p --fields=+iaS --extra=+q`, in Kate's default command (`--extra=+q`), in Citre's recommended `--extras='*'`,\[3\] and in every tagbar/vista.vim call (`--extras=+F` / `--extra=`). The legacy singular `--extra` probably still outnumbers `--extras` in human-written configs. Universal Ctags' ctags(1) still lists it as "Equivalent to --extras". It was renamed "for making consistent with --kinds-<LANG> and --fields".
- **Proposed 99% subset:** `-R/--recurse`, `-f/-o`, `-a/--append`, `-e`, `-L`, `--exclude`, `--languages`, `--language-force`, `--langmap`, `--fields`, `--extras/--extra`, `--kinds-<LANG>/--<LANG>-kinds`, `--tag-relative`, `--sort`, `--excmd`, `--format`, `--output-format`, `--options`, `-I`, `--langdef/--regex-<LANG>`, `--version`, `--list-languages`. That is about 22 options. Confidence is medium: it rests on the default commands of the major tools plus heavily copied recipes, not on a measured corpus count.

## Key Findings

### Method and evidence quality (read this first)
No published quantitative census of ctags option usage turned up. I ran no grep.app or GitHub code-search counts, and the subagent couldn't either. The tiers below come from:
1. the hard-coded default command lines of the most widely used consumers (tagbar, gutentags, vista.vim, easytags, Citre, Sublime CTags, Kate, VS Code Ctags Companion, the Linux kernel, automake);
2. recipes that get copied across thousands of dotfiles, blogs, course pages and StackOverflow answers (OmniCppComplete, kernel, UW CSE333).

This is a **weighted-by-invocation** view. Plugin defaults run on every buffer save or open, so plugin-generated invocations far outnumber human-typed ones. Tiers (a) and (b) are robust. The exact ordering inside each tier is a judgment call.

### Concrete default command lines from major tools

| Tool | Command line / args (verbatim where possible) | Source |
|---|---|---|
| **tagbar** (Universal Ctags path, current) | `ctags --extras=+F -f - --format=2 --excmd=pattern --fields=nksSafet --sort=no --append=no -V [--options=…] --language-force=<lang> --<lang>-kinds=<kinds> <tmpfile>` | tagbar debug logs, issues #848, #776, #723\[1\]\[2\]\[4\] |
| **tagbar** (Exuberant path) | `ctags -f - --format=2 --excmd=pattern --fields=nksSaf --extra= --file-scope=yes --sort=no --append=no -V --language-force=c++ --c++-kinds=dpgetncsufmv` | tagbar issue #514 (2018); #789\[5\]\[6\] |
| **tagbar** (older Universal path, ~2017–2019) | `… --fields=nksSaf --extras= --file-scope=yes --sort=no --append=no …` (broke on Exuberant with "Unknown option: --extras") | tagbar issues #456, #552\[7\]\[8\] |
| **vista.vim** | `ctags --format=2 --excmd=pattern --fields=nksSaf --extras=+F --sort=no --append=no --extras= --output-format=json --fields=-PF -f- <file>` (copied "Refer to tagbar") | vista.vim issue #297; commit cdccc47\[9\]\[10\] |
| **vim-gutentags** | `ctags -f "tags.temp" "--options=…/res/ctags_recursive.options" [--tag-relative=yes] [extra_args] [--exclude=…] "<projroot>"`; incremental: `… --append "<file>"`; with a file-list command: `-L <tags>.files`. `ctags_recursive.options` contains only `--recurse=yes`. `g:gutentags_ctags_extra_args` defaults to `[]`. | `plat/unix/update_tags.sh`; `autoload/gutentags/ctags.vim`; issues #186, #295, #296\[11\]\[12\]\[13\]\[14\] |
| **gutentags (older, Windows trace)** | `ctags -R -f "…\tags.temp" --exclude=".git" … --exclude="node_modules" …` | gutentags issue #10\[15\] |
| **vim-easytags** | `ctags --fields=+l --c-kinds=+p --c++-kinds=+p --sort=no -f- --language-force=c <file>` | easytags issues #27, #89\[16\]\[17\] |
| **Citre (Emacs)** | `ctags --languages=c,c++,… --kinds-all='*' --fields='*' --extras='*' -R` (recipe buffer adds `-o %TAGSFILE%`) | Citre README / user manual\[18\]\[19\] |
| **Sublime Text CTags** | `ctags -R -f .tags` (the docs explicitly forbid extra flags in `command`; use settings instead) | SublimeText/CTags README & settings\[20\]\[21\]\[22\] |
| **Kate CTags plugin** | `ctags -R --c++-types=+px --extra=+q --excmd=pattern --exclude=Makefile --exclude=.` | KDE/openSUSE forum reports\[23\]\[24\] |
| **VS Code Ctags Companion** | rebuild: `ctags -R --fields=+nKz`; per-file outline: `ctags --fields=+nKz -f -`; lookups via `readtags -en` / `readtags -enpi` (since v2022.12 `+nKz` is "no longer required") | Marketplace page / GitHub releases\[25\]\[26\] |
| **Linux kernel `scripts/tags.sh`** | `xargs ctags -a -I … --$CTAGS_EXTRA=+fq --c-kinds=+px --fields=+iaS --langmap=c:+.h "${regex[@]}"`; Kconfig pass: `--langdef=kconfig --language-force=kconfig` (skipped when `--list-languages` shows Kconfig built in) | LKML patches 2016, 2021, 2022–23\[27\]\[28\]\[29\] |
| **GNU automake** | `tags`/`ctags` rules run `$(CTAGS)` with `CTAGSFLAGS`/`AM_CTAGSFLAGS` (and `ETAGS_ARGS`/`ETAGSFLAGS` for etags). Automake itself injects no ctags-specific options. | GNU automake manual §18.1, §26.5\[30\]\[31\] |
| **PostgreSQL `src/tools/make_ctags`** | Exuberant: `--c-kinds=+dfmstuv`, plus `-I` ignore lists, `-a -f`, and `ctags -e` for Emacs mode | pgsql-hackers patches\[32\]\[33\] |
| **OmniCppComplete recipe** | `ctags -R --c++-kinds=+p --fields=+iaS --extra=+q` (C: `--C-kinds=+p --fields=+aS --extra=+q`) | OmniCppComplete docs mirror; many blogs\[34\] |

## Details: Tiered list

### Tier (a): Core, in the overwhelming majority of invocations
- **`-R` / `--recurse[=yes]`**: the single most common option. It is the whole command in human use (`ctags -R`, `ctags -R .`, `ctags -R *`). It is the Sublime default, appears in Citre/Kate/Companion, and gutentags passes it through an options file. The ctags(1) man page frames `ctags -R` as the normal usage. That wording goes back to Exuberant Ctags and Universal Ctags kept it.
- **`-f <file>` / `-o <file>`, especially `-f -` / `-f-`**: every editor plugin that parses output itself uses stdout (tagbar, vista, easytags, Companion outline). Human use is mostly `-f .tags` for Sublime and git-hook recipes.\[35\] `-o` is the Universal-era spelling (Citre, U-ctags docs).\[36\]\[37\]
- **No options at all (`ctags file…`)**: this matters for the "99%" framing. A large share of human invocations rely entirely on `~/.ctags` / `~/.ctags.d/*.ctags` for configuration.\[38\]\[39\]

### Tier (b): Common, a meaningful minority (roughly in descending order)
- **`--fields=…`**: the main human idiom is `+iaS` (OmniCppComplete/kernel). Plugins use absolute sets (`nksSaf`, `nksSafet`). Other variants: `+l` (easytags, so that a `language:` field exists),\[16\] `+n` / `+nKz` (VS Code), `+niazS`, `+K` and `*` (Citre).\[40\]\[41\]
- **`--extras=` / `--extra=`**: see the dedicated section below.
- **`--c-kinds=+p` / `--c++-kinds=+p[x]`** (and old `--c++-types`): the dominant per-language kinds idiom, used to add prototypes and externs.\[37\]\[40\] The kernel uses `+px`. Reversing that to `-px` was proposed in 2009 to stop jumps landing on declarations, so the polarity is contested.\[42\] Plugins also pass a full `--<lang>-kinds=<letters>` on every call.
- **`--exclude=…`**: very common in `~/.ctags` (`.git`, `node_modules`, `vendor`, `*.min.*`).\[15\]\[38\] gutentags generates it from `wildignore` and `g:gutentags_ctags_exclude` (`-x @file` → `--exclude=@file`).\[11\]\[43\]
- **`--languages=…`**: both restrictive (`--languages=C,C++`) and subtractive (`--languages=-javascript,sql`) forms appear.\[24\]\[36\] Citre's recipe asks for it.
- **`-a` / `--append`**: build systems that feed `xargs` (kernel, PostgreSQL) and gutentags' incremental update.\[13\]\[44\] Plugins also pass `--append=no` explicitly.\[45\]\[46\]
- **`--tag-relative[=yes]`**: dotfiles and git-hook recipes (`.git/tags`).\[36\]\[39\] gutentags adds it only when the tags file sits in a subdirectory.\[11\]
- **`-L <file>` / `-L -`**: the `find … | ctags -L -` and `gcc -M … | ctags -L -` patterns, and gutentags' file-list-command mode.\[47\]\[48\]\[49\]
- **`-e`**: Emacs TAGS output, in build scripts (`ctags -e` vs `etags`) and in Emacs users' manual runs.\[32\]\[50\] Citre explicitly does not support TAGS.\[18\]
- **`--sort=no`**: almost entirely plugin-generated (tagbar, vista, easytags), because they need source order.
- **`--excmd=pattern|number` / `-n`**: plugins pin `pattern`. Some VS Code and Sublime consumers use `--excmd=number`/`n`.\[51\]\[52\]
- **`--format=2`**: hard-coded by tagbar/vista. Humans almost never type it.
- **`--language-force=<lang>`**: plugin-generated per-buffer (tagbar, easytags), plus optlib Kconfig in the kernel.
- **`--options=<file>`**: gutentags (every run), tagbar (`g:tagbar_ctags_options`), easytags (`g:easytags_opts`), and filetype plugins such as rust.vim/yats.vim that ship `.ctags` files.\[11\]\[53\]\[54\]
- **`-I <identifier-list>`**: heavy in C systems code (kernel: `__initdata`, `EXPORT_SYMBOL`, `__must_check`…; Qt: `Q_REQUIRED_RESULT`; PostgreSQL).\[29\]\[32\]\[55\]\[56\] It is rare outside C/C++.
- **`--langdef` / `--langmap` / `--regex-<LANG>`**: the classic `~/.ctags` extension mechanism (Markdown, CSS, JS patterns). The kernel uses `--langmap=c:+.h` and many `--regex-asm/c`. This is a meaningful minority in dotfiles, and these lines caused much of the E-ctags→U-ctags breakage (stricter kind letters and names; "Language … already defined").\[15\]\[29\]\[57\]\[58\]\[59\]
- **`--output-format=json|e-ctags`**: vista.vim (JSON) and LSP-ish tools.\[60\]\[61\] Some users pass `--output-format=e-ctags` to get Exuberant-compatible output from U-ctags for older consumers (seen in gutentags issue #313).\[62\]\[63\]
- **`--version`, `--list-languages`**: introspection, mostly by scripts: the kernel greps `--version` for "universal" and `--list-languages` for Kconfig, tagbar parses `--version` to choose code paths, and vista checks `--list-features` for JSON.\[28\]\[64\]\[65\]

### Tier (c): Rare / tool-author / introspection-only
`--list-kinds[-full]`, `--list-fields`, `--list-extras`, `--list-features`, `--list-maps`, `--list-output-formats`; `-V`/`--verbose` (tagbar always passes `-V` for its debug log, so it is frequent by count but meaningless to humans); `--totals`; `-x`/`--_xformat`; `--kinds-all`, `--fields-<LANG>`, `--extras-<LANG>`, `--param-<LANG>`, `--mline-regex-<LANG>`, `--map-<LANG>`; `--pseudo-tags`; `--input-encoding`/`--output-encoding`; `--filter`; `--links`; `--if0`; `--maxdepth`; `--exclude-exception`; `-D`; `--pattern-length-limit`; `-G`/`--guess-language-eagerly`; `--optlib-dir`; `--options-maybe`; `--quiet`; `-B`/`-F`; `-u`; `--put-field-prefix`; `--_interactive`; `--extras=+r` / `--fields=+r` (reference tags; GNU Global-style consumers).\[41\]\[66\] Citre's `--kinds-all='*' --fields='*' --extras='*'` is the main mainstream exposure to the "all" forms.

### Tier (d): Legacy Exuberant-only, renamed, or semantics-changed
| Exuberant / legacy | Universal Ctags status |
|---|---|
| `--extra=` | Still accepted: ctags(1) lists it as "Equivalent to `--extras=`". The rename was "for making consistent with --kinds-<LANG> and --fields". The kernel switched to `--extras` only for U-ctags in Philip K. Gisslow's September 2021 patch, which checks `--version` for "universal". |
| `--<LANG>-kinds=` | "This option is obsolete. Use `--kinds-<LANG>=...` instead". It still works "but it will be removed in the future".\[61\]\[67\] |
| `--c-types` / `--c++-types` | Pre-2003 spelling (PostgreSQL notes "--c-kinds was called --c-types before 2003"); still in Kate's default. |
| `--file-scope=yes/no` | Removed. ctags-incompatibilities(7): "Universal Ctags introduces F/fileScope extra as the replacement for --file-scope option" (`--extras=+F`/`-F`). This is why tagbar's U-ctags path passes `--extras=+F` instead of `--file-scope=yes`. |
| `--file-tags` | Removed (undocumented even in E-ctags). Use `--extras=+f` / `+{inputFile}`.\[67\] |
| `~/.ctags`, `./.ctags` | Not loaded. U-ctags reads `~/.ctags.d/*.ctags`, `./.ctags.d/*.ctags`.\[59\] This is the biggest silent behavior change for human users.\[59\] |
| `CTAGS`/`ETAGS` env vars | Not read by U-ctags.\[68\] |
| Option after file arg (`ctags -o - foo.c --list-kinds=Sh`) | E-ctags accepts it. U-ctags warns and ignores it (POSIX ordering).\[69\] |
| Kind letters/names | Stricter (alphanumeric, `F`/`file` reserved).\[68\] Breaks many old `--regex-…` definitions.\[59\] |
| Output format | Default `u-ctags` (≈ `--format=2`); `--output-format=e-ctags` ≈ `--format=1`… *note:* the U-ctags docs equate `--format=1`/`2` with `e-ctags`/`u-ctags` respectively.\[70\] Adds `TAG_OUTPUT_MODE` and other pseudo-tags.\[61\]\[71\] |

## The `--extras` / `--extra` question, answered directly

**Frequency:** high, and it is the second most common content-shaping option after `--fields`/`--c-kinds`. It appears in three distinct populations:
1. **`+q` (qualified `Class::member` tags):** the C/C++ completion recipe `--c++-kinds=+p --fields=+iaS --extra=+q`. It originated with OmniCppComplete, was adopted into the kernel's `tags.sh` in 2016 specifically "to have completion on member of structure", and was then copied into Kate's default, gutentags user configs (`--extras=+q`, `--extra=+q`)\[62\]\[72\] and countless blogs.\[29\]\[73\] The U-ctags FAQ, ctags-faq(7), still answers "Why can't I jump to class::member?" with: "If you specify the --extra=+q option, then ctags will also generate a second, class-qualified tag for each class member … in the form class::member for C++."
2. **`+f` (file-name tags, `:tag foo.c`):** in the kernel since John Kacur's 2005 LKML patches (`--extra=+f`). It was still present as `--extra=+f --c-kinds=+px` in Catalin Marinas's 2014 tags.sh patch and later became `+fq`. It also appears in git-hook recipes (`ctags -R --tag-relative --extra=+f …`) and is recommended by tools like python-cts.
3. **Plugin-generated `--extras=+F` / `--extra=` / `--extras=`:** tagbar and vista pass it on every file open. For Universal Ctags it is the mandatory replacement for the removed `--file-scope`.\[67\]

**Spelling split:** human-written configs still use `--extra=` (singular), partly because E-ctags rejects `--extras` (tagbar issue #552) and U-ctags still accepts `--extra`.\[7\] New U-ctags-only tools (Citre, ctags-code, docs) use `--extras`. For portable scripts, detect the implementation as the kernel does (`--version | grep -iq universal`) or just use `--extra`.\[28\] That works on both today, but U-ctags documents it only as an equivalence, so treat it as legacy.

## Recommendations
- **If you are building a ctags wrapper or front-end, implement these first:** `-R`, `-f/-o` (with `-`), `-a`, `-L`, `-e`, `--exclude`, `--languages`, `--language-force`, `--langmap`, `--fields`, `--extras`(+`--extra` alias), `--kinds-<LANG>`(+`--<LANG>-kinds`), `--tag-relative`, `--sort`, `--excmd`, `--format`, `--output-format`, `--options`, `-I`, `--langdef`/`--regex-<LANG>`, `--version`, `--list-languages`. Accepting the legacy aliases (`--extra`, `--<LANG>-kinds`, `--c-types`) is required for the long tail of copied recipes. Accepting `--file-scope` is needed only for E-ctags-era plugin paths.
- **Field and extra letters to support first:** fields `n k s S a f t i l K z` (and `*`); extras `q f F` (and `*`, the empty set `--extras=`). Plus the kind idiom `+p`/`+px`.
- **If you need real numbers,** run grep.app/Sourcegraph queries such as `"--extra=+q"`, `"--extras=+q"`, `"--fields=+iaS"`, `"--c++-kinds=+p"`, `"ctags -R"`, and restrict paths to `**/.ctags`, `**/.ctags.d/*.ctags`, `Makefile*`, `*.vim`, `*.el`. Report hits per file, not raw matches, to dampen vendored copies of the kernel and OmniCppComplete recipes.

## Caveats
- **No measured corpus.** The ranking is inferred from tool defaults and recipe propagation, not counted. Treat the "99%" as an informed estimate with **medium confidence for tier membership and low confidence for exact order.**
- **Plugin vs. human skew:** by invocation count, tagbar/vista/easytags-style per-buffer runs dominate, inflating `-f -`, `--format=2`, `--excmd=pattern`, `--sort=no`, `--language-force`, `-V`. By distinct config files, `-R`, `--exclude`, `--fields=+iaS`, `--extra=+q`, `--c++-kinds=+p` and `--langdef/--regex` dominate.
- **Source gaps:** tagbar's argument list comes from its own debug output quoted in issues, not from the source lines in `autoload/tagbar.vim`. gutentags' `ctags_recursive.options` content (`--recurse=yes`) comes from a user's local copy circa 2020.\[12\] The Kate default comes from forum posts.
- **Sampling bias:** GitHub-visible code over-represents Vim users, dotfiles enthusiasts and kernel-derived trees. It under-represents IDE-internal invocations (Geany, JetBrains and similar embed parsers or use their own flags) and corporate build scripts.

## Sources

1. [\[bug\] improper ctags argument ordering · Issue #776 · preservim/tagbar · GitHub](https://github.com/preservim/tagbar/issues/776)
2. [no tag found (neovim) · Issue #848 · preservim/tagbar · GitHub](https://github.com/preservim/tagbar/issues/848)
3. [GitHub - universal-ctags/citre: Ctags IDE on the True Editor - JOYK Joy of Geek, Geek News, Link all geek](https://www.joyk.com/dig/detail/1625041562396160)
4. [Tagbar doesn't populate, ctags.exe can't find file /tmp/vGCKBiZ/11.cs · Issue #723 · preservim/tagbar · GitHub](https://github.com/preservim/tagbar/issues/723)
5. [tagbar is empty · Issue #514 · preservim/tagbar · GitHub](https://github.com/preservim/tagbar/issues/514)
6. [#777 breaks the ability to add a definition for a new language or file type · Issue #789 · preservim/tagbar · GitHub](https://github.com/preservim/tagbar/issues/789)
7. [ctags: Unknown option: --extras, exit code: 1 · Issue #552 · preservim/tagbar · GitHub](https://github.com/preservim/tagbar/issues/552)
8. [\[macos\] Universal ctags errors, but exuberant ctags are fine · Issue #456 · preservim/tagbar · GitHub](https://github.com/preservim/tagbar/issues/456)
9. [Make ctags executive work with the latest u-ctags · liuchengxu/vista.vim@cdccc47 · GitHub](https://github.com/liuchengxu/vista.vim/commit/cdccc4789e1a6f41a2688e58c051a2c00fcccd5f)
10. [\[vista.vim\] Fail to decode from JSON: · Issue #297 · liuchengxu/vista.vim · GitHub](https://github.com/liuchengxu/vista.vim/issues/297)
11. [vim-gutentags/autoload/gutentags/ctags.vim at master · ludovicchabant/vim-gutentags](https://github.com/ludovicchabant/vim-gutentags/blob/master/autoload/gutentags/ctags.vim)
12. [gutentags: ctags job failed, returned: 1](https://github.com/ludovicchabant/vim-gutentags/issues/296)
13. [vim-gutentags/plat/unix/update\_tags.sh at master · ludovicchabant/vim-gutentags](https://github.com/ludovicchabant/vim-gutentags/blob/master/plat/unix/update_tags.sh)
14. [gutentags not finding existing project tag files and is regenerating them every time I open a file in project · Issue #186 · ludovicchabant/vim-gutentags · GitHub](https://github.com/ludovicchabant/vim-gutentags/issues/186)
15. [No \`tags\` file gets generated · Issue #10 · ludovicchabant/vim-gutentags · GitHub](https://github.com/ludovicchabant/vim-gutentags/issues/10)
16. [EasyTags doesn't write to dynamic tag file · Issue #89 · xolox/vim-easytags · GitHub](https://github.com/xolox/vim-easytags/issues/89)
17. [Vim locks up with easytags plugin · Issue #27 · xolox/vim-easytags · GitHub](https://github.com/xolox/vim-easytags/issues/27)
18. [citre/docs/user-manual/getting-started-on-tags-backend.md at master · universal-ctags/citre · GitHub](https://github.com/universal-ctags/citre/blob/master/docs/user-manual/getting-started-on-tags-backend.md)
19. [Citre 在Emacs 中的使用 - 一不留神的博客](https://www.skfwe.cn/p/citre-%E5%9C%A8emacs-%E4%B8%AD%E7%9A%84%E4%BD%BF%E7%94%A8/)
20. [GitHub - SublimeText/CTags: CTags support for Sublime Text · GitHub](https://github.com/SublimeText/CTags)
21. [CTags/CTags.sublime-settings at development · SublimeText/CTags · GitHub](https://github.com/SublimeText/CTags/blob/development/CTags.sublime-settings)
22. [This Sublime Text 2/3 package provides support for working with tags generated by Exuberant CTags](https://github.com/ArchyInf/CTags/blob/development/README.rst)
23. [Kate, Haskell: how to configure CTags plugin • KDE Community Forums](https://forum.kde.org/viewtopic.php%3Ff=25&t=143287.html)
24. [How to setup ctags in kde kate - Applications - openSUSE Forums](https://forums.opensuse.org/t/how-to-setup-ctags-in-kde-kate/167800)
25. [Ctags Companion - Visual Studio Marketplace](https://marketplace.visualstudio.com/items?itemName=gediminaszlatkus.ctags-companion)
26. [Releases · gediminasz/ctags-companion · GitHub](https://github.com/gediminasz/ctags-companion/releases)
27. [\[PATCH v3\] scripts/tags.sh: Fix the Kconfig tags generation when using latest ctags](https://lkml.iu.edu/hypermail/linux/kernel/2301.3/05979.html)
28. [\[PATCH\] scripts/tags.sh: Fix obsolete parameter for ctags](https://lkml.rescloud.iu.edu/2109.0/03810.html)
29. [\[PATCH\] Enable code completion in VIM](https://lkml.iu.edu/hypermail/linux/kernel/1608.3/03976.html)
30. [Tags - automake](http://www.lug.or.kr/files/docs/automake/Tags.html)
31. [Flag Variables Ordering (automake)](https://www.gnu.org/software/automake/manual/html_node/Flag-Variables-Ordering.html)
32. [0001 Enhance make ctags and make etags v15.patch](https://www.postgresql.org/message-id/attachment/194721/0001-Enhance-make_ctags-and-make_etags-v15.patch)
33. [fix make ctags v4.patch](https://postgresql.org/message-id/attachment/143692/fix-make_ctags-v4.patch)
34. [GitHub - cycnet/omnicppcomplete](https://github.com/cycnet/omnicppcomplete)
35. [Navegando com CTags no Sublime Text](https://glaucocustodio.github.io/2015/01/10/navegando-com-ctags-no-sublime-text/)
36. [Getting started with Ctags with Vim and Git. - Aaron Young](https://geekdude.github.io/tech/ctags/)
37. [Ctags Tutorial](https://courses.cs.washington.edu/courses/cse333/23wi/ctags.html)
38. [Use vim with ctags](https://kulkarniamit.github.io/whatwhyhow/howto/use-vim-ctags.html)
39. [Vim and Ctags · Andrew Stewart](https://andrew.stwrt.ca/posts/vim-ctags)
40. [Generate Ctags Files for C/C++ Source Files](https://www.centennialsoftwaresolutions.com/help/generate-ctags-files-for-cc-source-files/)
41. [Lesser-Known but Useful Universal Ctags Flags with Examples](https://medium.com/@lince-mathew/lesser-known-but-useful-universal-ctags-flags-with-examples-fbc0266f4dfe)
42. [\[PATCH\] ctags usability fix](https://lkml.iu.edu/hypermail/linux/kernel/0908.0/00040.html)
43. [Navigate ES6 projects with Vim using ctags](https://medium.com/trabe/navigate-es6-projects-with-vim-using-ctags-948d114b94f3)
44. [\[PATCH 14/26\] kbuild: Add ctags support for function prototypes and external variable declarations](https://lkml.iu.edu/hypermail/linux/kernel/0601.0/0377.html)
45. [vim gutentags](https://github.com/ludovicchabant/vim-gutentags)
46. [vim autotag](https://github.com/artem-kamyshev/vim-autotag)
47. [ctags — Universal Ctags 0.3.0 documentation](https://docs.ctags.io/en/latest/man/ctags.1.html)
48. [ctags(1) — Arch manual pages](https://man.archlinux.org/man/ctags.1.en)
49. [Generate Ctags Files for C/C++ Source Files and All of Their Included Header Files - Top Bug Net](https://www.topbug.net/blog/2012/03/17/generate-ctags-files-for-c-slash-c-plus-plus-source-files-and-all-of-their-included-header-files/)
50. [ctags command in Linux with examples - GeeksforGeeks](https://www.geeksforgeeks.org/ctags-command-in-linux-with-examples/)
51. [vs symbol by ctags](https://github.com/PrsPrsBK/vs-symbol-by-ctags)
52. [github.com](https://github.com/mshr-h/vscode-verilog-hdl-support/issues/66)
53. [Tags in tagbar are messed up · Issue #353 · rust-lang/rust.vim · GitHub](https://github.com/rust-lang/rust.vim/issues/353)
54. [vim easytags](https://github.com/Wraul/vim-easytags)
55. [scripts/tags.sh - chromiumos/third\_party/kernel - Git at Google](https://chromium.googlesource.com/chromiumos/third_party/kernel/+/dec469850077e73a60662ed76f5d80e0b6a63a06/scripts/tags.sh)
56. [Vim + omnicppcomplete + ctags \[Solved\]](https://www.qtcentre.org/threads/24646-Vim-omnicppcomplete-ctags-Solved)
57. [g:gutentags\_resolve\_symlinks=0 not working · Issue #244 · ludovicchabant/vim-gutentags · GitHub](https://github.com/ludovicchabant/vim-gutentags/issues/244)
58. [github.com](https://github.com/k-takata/ctags)
59. [Universal Ctags (abbreviated as u-ctags) is a maintained](https://github.com/helloGB20171008/ctags/blob/master/README.md)
60. [ctags-json-output — Universal Ctags 0.3.0 documentation](https://docs.ctags.io/en/latest/man/ctags-json-output.5.html)
61. [ctags-universal(1) — universal-ctags — Debian unstable — Debian Manpages](https://manpages.debian.org/unstable/universal-ctags/ctags-universal.1.en.html)
62. [cpu almost 100% · Issue #313 · ludovicchabant/vim-gutentags · GitHub](https://github.com/ludovicchabant/vim-gutentags/issues/313)
63. [github.com](https://github.com/liuchengxu/vista.vim/)
64. [tagbar/autoload/tagbar.vim at master · preservim/tagbar](https://github.com/preservim/tagbar/blob/master/autoload/tagbar.vim)
65. [\[PATCH v2\] scripts/tags.sh: Fix the Kconfig tags generation when using latest ctags](https://lkml.iu.edu/hypermail/linux/kernel/2212.3/03049.html)
66. [Changes to the tags file format — Universal Ctags 0.3.0 documentation](https://docs.ctags.io/en/latest/output-tags.html)
67. [ctags-incompatibilities — Universal Ctags 0.3.0 documentation](https://docs.ctags.io/en/latest/man/ctags-incompatibilities.7.html)
68. [ctags-incompatibilities - Incompatibilities between Universal Ctags and Exuberant Ctags - Manual Page](https://hexmos.com/freedevtools/man_pages/miscellaneous/system-standards/ctags-incompatibilities/)
69. [Linux Manpages Online - man.cx manual pages](<https://man.cx/ctags-incompatibilities(7)>)
70. [Output formats — Universal Ctags 0.3.0 documentation](https://docs.ctags.io/en/latest/output-format.html)
71. [universal-ctags/ctags](https://deepwiki.com/universal-ctags/ctags)
72. [GitHub · Where software is built](https://github.com/ludovicchabant/vim-gutentags/issues/225)
73. [Code Completion with VIM 7](https://blogs.gnome.org/lharris/2008/07/20/code-completion-with-vim-7/)

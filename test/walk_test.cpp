// Discovery: paths, exclusions, languages, and whole runs

// The names of the tags in output, in order, separated by spaces.
static Str tagnames(Arena *a, Str out)
{
    Buf b(a, 64);
    for (Cut c = cut(out, '\n'); c.ok; c = cut(c.tail, '\n')) {
        if (b.len) putbyte(&b, ' ');
        print(&b, cut(c.head, '\t').head);
    }
    return finish(&b);
}

static void test_walk(Test *t)
{
    Arena a = t->perm;

    // Joined as Universal Ctags joins them, printed with '/'
    struct { Str dir, path; } joins[] = {
        {".",       "a.c"},
        {"./",      "./a.c"},
        {"test",    "test/a.c"},
        {"test/",   "test/a.c"},
        {"x86//",   "x86//a.c"},
        {".\\test", "./test/a.c"},
        {"C:\\x",   "C:/x/a.c"},
        {"C:",      "C:a.c"},
        {"C:\\",    "C:/a.c"},
        {"..",      "../a.c"},
    };
    for (iz i = 0; i < countof(joins); i++) {
        Str p = joinpath(&a, joins[i].dir, "a.c", 1);
        expect(t, joins[i].dir, outpath(&a, p, 1), joins[i].path);
    }
    expect(t, "posix backslash", outpath(&a, joinpath(&a, "a\\b", "c.c", 0), 0),
           "a\\\\b/c.c");

    // Patterns as fnmatch() without flags matches them (DISC-8). The
    // Windows rows are Universal Ctags' results, from patterns in an
    // --exclude=@FILE list, which keeps '\' an escape there
    struct { Str pat, name; b32 windows, want; } globs[] = {
        {"*.c",           "a.c",       0, 1},
        {"*.C",           "a.c",       0, 0},
        {"*.C",           "a.c",       1, 1},
        {"a?c",           "a.c",       0, 1},
        {"*",             "",          0, 1},
        {"?",             "",          0, 0},
        {"*?",            "",          0, 0},
        {"dir*c",         "dir/a.c",   0, 1},
        {".*",            ".",         0, 1},
        {".*",            "./sub",     0, 1},
        {"*/a.c",         "dir\\a.c",  1, 1},
        {"*/a.c",         "dir\\a.c",  0, 0},
        {"*a*b*c",        "xaybzc",    0, 1},
        {"*a*b*c",        "xaybzcb",   0, 0},
        {"[ab].c",        "a.c",       1, 1},
        {"[ab].c",        "c.c",       1, 0},
        {"[!ab].c",       "c.c",       1, 1},
        {"[!ab].c",       "b.c",       1, 0},
        {"[^ab].c",       "[.c",       1, 1},
        {"[^ab].c",       "a.c",       1, 0},
        {"[a-c].c",       "b.c",       1, 1},
        {"[a-c].c",       "d.c",       1, 0},
        {"[A-C].c",       "b.c",       0, 0},
        {"[A-C].c",       "b.c",       1, 1},
        {"[]a].c",        "].c",       1, 1},
        {"[!]].c",        "].c",       1, 0},
        {"[!]].c",        "^.c",       1, 1},
        {"[a-].c",        "-.c",       0, 1},
        {"[a-].c",        "a.c",       1, 1},
        {"[a-].c",        "b.c",       1, 0},
        {"[a-",           "[a-",       0, 0},
        {"[a-",           "a",         0, 0},
        {"[ab.c",         "[ab.c",     1, 1},
        {"[ab.c",         "a.c",       1, 0},
        {"[[a",           "[[a",       0, 0},
        {"[!a",           "[!a",       0, 1},
        {"*[ab",          "x[ab",      0, 1},
        {"[[:digit:]].c", "5.c",       0, 1},
        {"[[:digit:]].c", "x.c",       0, 0},
        {"[[:digit:]].c", "5.c",       1, 0},
        {"[[:alpha:]].c", "x.c",       0, 1},
        {"[[:alpha:]].c", "\xe9.c",    0, 0},
        {"[[:alpha:]].c", "x.c",       1, 0},
        {"[[:alpha:]].c", "a].c",      1, 1},
        {"[[:alpha:]].c", "P].c",      1, 1},
        {"[[:alpha:]].c", "x].c",      1, 0},
        {"[[:punct:]x]",  "!",         0, 1},
        {"[[:space:]]",   "\v",        0, 1},
        {"[[:xdigit:]]",  "F",         0, 1},
        {"[[:xdigit:]]",  "g",         0, 0},
        {"[[:bogus:]].c", "x.c",       0, 0},
        {"[[::]]",        ":",         0, 0},
        {"[[:digit:]-z]", "-",         0, 1},
        {"[[:digit:]-z]", "y",         0, 0},
        {"[[:].c",        "[.c",       1, 1},
        {"[[:-].c",       "[.c",       1, 1},
        {"[\\]].c",       "].c",       1, 1},
        {"[a\\-c].c",     "c.c",       1, 1},
        {"[a\\-c].c",     "b.c",       1, 0},
        {"[\\a-c].c",     "b.c",       0, 1},
        {"[a-\\c].c",     "b.c",       0, 1},
        {"[a-\\",         "b",         0, 0},
        {"[\\",           "[",         0, 0},
        {"\\a.c",         "a.c",       1, 1},
        {"a\\.c",         "a.c",       1, 1},
        {"\\*.c",         "*.c",       0, 1},
        {"\\*.c",         "a.c",       0, 0},
        {"a\\",           "a",         0, 0},
        {"a\\",           "a\\",       0, 0},
        {"[x[=a=]].c",    "a].c",      1, 1},
        {"[x[=a=]].c",    "x.c",       1, 1},
        {"[x[=a=]].c",    "x].c",      1, 0},
        {"[x[=a].c",      "x.c",       0, 0},
        {"[x[.y.]].c",    "x.c",       1, 1},
        {"[x[.y.]].c",    "y].c",      1, 1},
        {"[x[.y",         "x",         1, 0},
        {"[x[:a].c",      "x.c",       0, 1},
        {"[[:x",          "[",         1, 0},
    };
    for (iz i = 0; i < countof(globs); i++) {
        Str name = concat(&a, concat(&a, globs[i].pat, " "), globs[i].name);
        expect(t, name, num(&a, globmatch(globs[i].pat, globs[i].name,
                                          globs[i].windows)),
               num(&a, globs[i].want));
    }

    // Classified patterns match as the general matcher does
    {
        Str pats[] = {"a.c", "*.c", "a*", "x*y", "*", "", ".*.swp", "?",
                      "CVS", "{arch}", "*~", "a*b*"};
        Str names[] = {"a.c", "A.C", "b.c", "a", "", "xy", "xzzy", "x\\y",
                       ".x.swp", "cvs", "{ARCH}", "~", "a\\b"};
        for (iz i = 0; i < countof(pats); i++) {
            Slice<Glob> one = {};
            one = push(&a, one, newglob(pats[i]));
            for (iz j = 0; j < countof(names); j++) {
                for (b32 w = 0; w < 2; w++) {
                    Str name = concat(&a, concat(&a, pats[i], " "), names[j]);
                    expect(t, name, num(&a, globsmatch(one, names[j], w)),
                           num(&a, globmatch(pats[i], names[j], w)));
                }
            }
        }
        struct { Str pat; i32 kind; } kinds[] = {
            {"CVS",     GLOB_EXACT},
            {"*.o",     GLOB_STAR},
            {".*.swp",  GLOB_STAR},
            {"*/x/*",   GLOB_ANY},
            {"x?",      GLOB_ANY},
            {"[x]",     GLOB_ANY},
            {"\\x",     GLOB_ANY},
        };
        for (iz i = 0; i < countof(kinds); i++) {
            expect(t, kinds[i].pat, num(&a, newglob(kinds[i].pat).kind),
                   num(&a, kinds[i].kind));
        }
        // A pattern ends at a NUL, as a C string does
        Glob g = newglob({(u8 *)"a\0*", 3});
        expect(t, "NUL", g.pat, "a");
    }

    struct { Str path; b32 windows, want; } excl[] = {
        {".git",          0, 1},
        {"src/.git",      0, 1},
        {"x.o",           0, 1},
        {"X.O",           1, 1},
        {"X.O",           0, 0},
        {"x.c",           0, 0},
        {"src/x.c",       0, 0},
        {".x.c.swp",      0, 1},
        {"./x.swp",       0, 1},  // the whole path matches .*.swp
        {"dir/{arch}",    0, 1},
        {"CVS",           0, 1},
        {"cvs",           1, 1},
        {"cvs",           0, 0},
        {"notes~",        0, 1},
    };
    for (iz i = 0; i < countof(excl); i++) {
        Arena   tmp = a;
        Options opt = {};
        Parse   p   = {};
        Buf     log(&tmp, 64);
        opt.windows = excl[i].windows;
        parseinit(&p, &opt, newos(&tmp), &tmp, &log);
        Str path = excl[i].path;
        Str base = basename(path, opt.windows);
        expect(t, path, num(&tmp, excluded(&opt, path, base)),
               num(&tmp, excl[i].want));
    }

    struct { Str path; i32 lang; b32 header; } langs[] = {
        {"a.c",         LANG_C,       0},
        {"a.C",         LANG_CXX,     0},  // u-ctags on Windows: C
        {"a.CPP",       LANG_CXX,     0},
        {"a.cc",        LANG_CXX,     0},
        {"a.h",         LANG_CXX,     1},
        {"a.H",         LANG_CXX,     1},
        {"a.hpp",       LANG_CXX,     1},
        {"a.inl",       LANG_CXX,     1},  // u-ctags: not a header
        {"a.hp",        LANG_CXX,     1},  // u-ctags: not a header
        {"a.f",         LANG_FORTRAN, 0},
        {"A.F90",       LANG_FORTRAN, 0},
        {"a.f08",       LANG_FORTRAN, 0},
        {"config.h.in", LANG_CXX,     1},
        {"x.c.in",      LANG_C,       0},
        {"x.in",        LANG_NONE,    0},
        {"x.inc",       LANG_NONE,    0},
        {"x.cu",        LANG_NONE,    0},
        {"Makefile",    LANG_NONE,    0},
        {"vector",      LANG_NONE,    0},
        {".c",          LANG_C,       0},
        {"dir.c/x",     LANG_NONE,    0},
        {"d/x.cpp",     LANG_CXX,     0},
    };
    for (iz i = 0; i < countof(langs); i++) {
        Lang l = detectlang(langs[i].path, 1);
        expect(t, langs[i].path, num(&a, l.lang), num(&a, langs[i].lang));
        expect(t, langs[i].path, num(&a, l.header), num(&a, langs[i].header));
    }

    // Whole runs
    Str x = "x\ta.c\t/^int x;$/;\"\tv\tline:1\ttyperef:typename:int\n";
    Str header =
        "!_TAG_FILE_FORMAT\t2\t/extended format; --format=1 will not append ;\" to lines/\n"
        "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/\n"
        "!_TAG_PROGRAM_NAME\tctags\t//\n"
        "!_TAG_PROGRAM_VERSION\t0.1\t//\n"
        "x\ta.c\t/^int x;$/;\"\tv\tline:1\ttyperef:typename:int\n"
        "y\tsub/b.cpp\t/^int y;$/;\"\tv\tline:1\ttyperef:typename:int\n";
    {
        Os *os = newos(&a);
        vfsfile(os, "a.c", "int x;\n");
        vfsfile(os, "sub/b.cpp", "int y;\n");
        Run r = run(os, a, "-R");
        expect(t, "run status", num(&a, r.status), "0");
        expect(t, "run err", r.err, "");
        expect(t, "run tags", vfsfind(os, "tags", 0)->data, header);
        expect(t, "run tmp", num(&a, !!vfsfind(os, "tags.tmp0", 0)), "0");

        // A tags file may be replaced
        r = run(os, a, "-R");
        expect(t, "rerun status", num(&a, r.status), "0");
        expect(t, "rerun tags", vfsfind(os, "tags", 0)->data, header);

        // Without the pseudo extra, and still a tags file to replace
        Str bare = cuthead(header, find(header, "\nx\t")+1);
        for (i32 i = 0; i < 2; i++) {
            r = run(os, a, "--extras=-p -R");
            expect(t, "-p status", num(&a, r.status), "0");
            expect(t, "-p tags", vfsfind(os, "tags", 0)->data, bare);
        }

        // D8: a directory argument is tagged recursively without -R, so
        // "." is the same as -R
        Str y = cuthead(bare, find(bare, "\ny\t")+1);
        struct { Str args, out; } dirs[] = {
            {"-f - .",              bare},
            {"-f - sub",            y},
            {"--recurse=no -f - .", bare},
            {"-f - a.c sub",        bare},
        };
        for (iz i = 0; i < countof(dirs); i++) {
            r = run(os, a, dirs[i].args);
            expect(t, dirs[i].args, num(&a, r.status), "0");
            expect(t, dirs[i].args, r.err, "");
            expect(t, dirs[i].args, r.out, dirs[i].out);
        }
    }
    {
        Os *os = newos(&a);
        Run r = run(os, a, "--help");
        Str s = r.out.len ? r.out : r.err;
        expect(t, "help status", num(&a, r.status), "0");
        expect(t, "help usage", num(&a, startswith(s, "usage: ctags ")), "1");
        expect(t, "help extras", num(&a, find(s, "\n  --extras=")>0), "1");
        expect(t, "help kinds", num(&a, find(s, "\n  Fortran: b* blockData ")>0), "1");
    }
    {
        Os *os = newos(&a);
        vfsfile(os, "a.c", "int x;\n");
        Run r = run(os, a, "-R -f -");
        expect(t, "stdout status", num(&a, r.status), "0");
        expect(t, "stdout out", r.out, x);
    }
    {
        Os *os = newos(&a);
        vfsfile(os, "tags", "hello\n");
        Run r = run(os, a, "-R");
        expect(t, "refuse status", num(&a, r.status), "1");
        expect(t, "refuse err", r.err,
               "ctags: \"tags\" doesn't look like a tag file; "
               "I refuse to overwrite it.\n");
        expect(t, "refuse tags", vfsfind(os, "tags", 0)->data, "hello\n");
    }
    {
        Os *os = newos(&a);
        vfsdir(os, "tags");
        Run r = run(os, a, "-R");
        expect(t, "dir status", num(&a, r.status), "1");
        expect(t, "dir err", r.err,
               "ctags: \"tags\" already exists as a directory; "
               "I cannot write tag entries there.\n");
    }
    // Option files
    Str src = "struct s { int m; };\nint f(void);\n"
              "int main(void) { int local; return 0; }\n";
    Str f   = "f\ta.c\t/^int f(void);$/;\"\tp\tline:2\ttyperef:typename:int\t"
              "file:\n";
    Str m   = "m\ta.c\t/^struct s { int m; };$/;\"\tm\tline:1\tstruct:s\t"
              "typeref:typename:int\tfile:\n";
    Str mn  = "main\ta.c\t/^int main(void) { int local; return 0; }$/;\"\tf\t"
              "line:3\ttyperef:typename:int\n";
    Str lc  = "local\ta.c\t/^int main(void) { int local; return 0; }$/;\"\tl\t"
              "line:3\tfunction:main\ttyperef:typename:int\tfile:\n";
    Str s   = "s\ta.c\t/^struct s { int m; };$/;\"\ts\tline:1\tfile:\n";
    Str sm  = "s::m\ta.c\t/^struct s { int m; };$/;\"\tm\tline:1\tstruct:s\t"
              "typeref:typename:int\tfile:\n";
    {
        Os *os = newos(&a);
        vfsfile(os, "a.c", src);
        vfsfile(os, "opts", "# a comment\n\n  --extras=+q  \r\n  # another\r"
                            "--kinds-C=+p\r-f\n-\n");
        Run r = run(os, a, "--options=opts a.c");
        expect(t, "options status", num(&a, r.status), "0");
        expect(t, "options err", r.err, "");
        expect(t, "options out", r.out, concat(&a, concat(&a, concat(&a,
               concat(&a, f, m), mn), s), sm));
    }
    {
        // A directory's *.ctags files in name order, not the listing's,
        // so b.ctags enables p after a.ctags disables it, and a file in
        // one ends it with a warning
        Os *os = newos(&a);
        vfsfile(os, "a.c", src);
        vfsfile(os, "od/b.ctags", "--kinds-C=+p\nfoo.c\n--kinds-C=+l\n");
        vfsfile(os, "od/a.ctags", "--extras=+q\n--kinds-C=-p\n");
        vfsfile(os, "od/.x.ctags", "--bogus\n");
        vfsfile(os, "od/c.txt", "--bogus\n");
        vfsfile(os, "od/.ctags", "--bogus\n");
        vfsdir(os, "od/d.ctags");
        vfsfile(os, "od/e.ctags", "-f   -\n");
        Run r = run(os, a, "--options=od a.c");
        expect(t, "options dir status", num(&a, r.status), "0");
        expect(t, "options dir err", r.err,
               "ctags: Warning: Ignoring non-option in od/b.ctags\n");
        expect(t, "options dir out", r.out, concat(&a, concat(&a, concat(&a,
               concat(&a, f, m), mn), s), sm));
        r = run(os, a, "--options=od/ a.c");
        expect(t, "options dir/ err", r.err,
               "ctags: Warning: Ignoring non-option in od/b.ctags\n");
    }
    {
        // On Windows, a directory ending in '\' takes no '/' before names
        Os *os = newos(&a);
        os->windows = 1;
        vfsfile(os, "a.c", src);
        vfsfile(os, "od/b.ctags", "--kinds-C=+p\nfoo.c\n");
        vfsfile(os, "od/a.ctags", "--extras=+q\n");
        Run r = run(os, a, "-f - --options=od\\ a.c");
        expect(t, "options dir\\ status", num(&a, r.status), "0");
        expect(t, "options dir\\ err", r.err,
               "ctags: Warning: Ignoring non-option in od\\b.ctags\n");
        expect(t, "options dir\\ out", r.out, concat(&a, concat(&a, concat(&a,
               concat(&a, f, m), mn), s), sm));
        r = run(os, a, "-f - --options=od a.c");
        expect(t, "options dir win err", r.err,
               "ctags: Warning: Ignoring non-option in od/b.ctags\n");
    }
    {
        // Only a file with a non-option warns, unlike Universal Ctags,
        // whose warning repeats for every later option file (OPT-14)
        Os *os = newos(&a);
        vfsfile(os, "a.c", src);
        vfsfile(os, "n2", "--kinds-C=+p\nfoo.c\n");
        vfsfile(os, "n3", "--extras=+q\n");
        Run r = run(os, a, "-f - --options=n2 --options=n3 a.c");
        expect(t, "options n2 n3 err", r.err,
               "ctags: Warning: Ignoring non-option in n2\n");
        r = run(os, a, "-f - a.c --options=n3");
        expect(t, "options after file err", r.err, "");
        expect(t, "options after file out", r.out, concat(&a, concat(&a,
               concat(&a, m, mn), s), sm));
    }
    {
        // Nested files, each read once
        Os *os = newos(&a);
        vfsfile(os, "a.c", src);
        vfsfile(os, "o1", "--options=o2\n--options=o1\n--kinds-C=+l\n");
        vfsfile(os, "o2", "--options=o1\n--extras=+q\n--options=NONE\n");
        Run r = run(os, a, "-f - --options=o1 --options=o2 a.c");
        expect(t, "options nest status", num(&a, r.status), "0");
        expect(t, "options nest err", r.err, "");
        expect(t, "options nest out", r.out, concat(&a, concat(&a, concat(&a,
               concat(&a, lc, m), mn), s), sm));
    }
    {
        Os *os = newos(&a);
        for (i32 i = 0; i < 17; i++) {
            Buf name(&a, 8);
            print(&name, "o");
            print(&name, (i64)i);
            Buf text(&a, 16);
            print(&text, "--options=o");
            print(&text, (i64)i+1);
            vfsfile(os, finish(&name), finish(&text));
        }
        Run r = run(os, a, "-f - --options=o0 a.c");
        expect(t, "options deep status", num(&a, r.status), "1");
        expect(t, "options deep err", r.err,
               "ctags: too many nested option files: \"o16\"\n");
    }
    {
        Os *os = newos(&a);
        vfsfile(os, "bad", "--kinds-C=+p\n--bogus\n");
        vfsfile(os, "help", "--help\n--bogus\n");
        struct { Str args; i32 status; Str err; } cases[] = {
            {"--options= a.c", 1,
             "ctags: no option file supplied for \"options\"\n"},
            {"--options=nosuch a.c", 1,
             "ctags: cannot stat \"nosuch\" : No such file or directory\n"},
            {"--options=bad a.c", 1, "ctags: Unknown option: --bogus\n"},
            {"--options=help a.c", 0, ""},
        };
        for (iz i = 0; i < countof(cases); i++) {
            Run r = run(os, a, cases[i].args);
            expect(t, cases[i].args, num(&a, r.status),
                   num(&a, cases[i].status));
            expect(t, cases[i].args, r.err, cases[i].err);
        }
    }
    {
        // A listing prints to standard output, warnings to standard error
        Os *os = newos(&a);
        vfsfile(os, "list",
                "--kinds-cpp=x\n--languages=-c++\n--list-languages\n");
        Run r = run(os, a, "--options=list --bogus");
        expect(t, "list status", num(&a, r.status), "0");
        expect(t, "list out", r.out, "C\nC++ [disabled]\nFortran\n");
        expect(t, "list err", r.err, "ctags: Warning: Unknown language \"cpp\" "
                                     "in \"kinds-cpp\" option\n");
    }

    {
        // Exclusions (DISC-3), as Universal Ctags makes them in this tree,
        // where it runs on Windows, and by the same rules elsewhere
        Os *os = newos(&a);
        vfsfile(os, "a.c",          "int a;\n");
        vfsfile(os, "b.cpp",        "int b;\n");
        vfsfile(os, "x.h",          "int h;\n");
        vfsfile(os, "sub/s.c",      "int s;\n");
        vfsfile(os, "sub/deep/d.c", "int d;\n");
        vfsfile(os, "other/o.c",    "int o;\n");
        vfsfile(os, "wild",         "sub\\deep\n  b.cpp  \n#a.c\n\n*.h\r\n");
        Str all     = "a b d h o s";
        Str nosuchw = "ctags: Warning: cannot open input file \"nosuch.c\" : "
                      "No such file or directory\n";
        struct { Str args; i32 windows; Str names, err; } cases[] = {
            {"-R --exclude=sub",                      2, "a b h o",   ""},
            {"-R --exclude=sub/deep",                 2, "a b h o s", ""},
            {"-R --exclude=sub\\deep",                1, "a b h o s", ""},
            {"-R --exclude=sub\\deep",                0, all,         ""},
            {"-R --exclude=*/s.c",                    2, "a b d h o", ""},
            {"-R --exclude=[ab].*",                   2, "d h o s",   ""},
            {"-R --exclude=[!a].*",                   2, "a",         ""},
            {"-R --exclude=s*",                       2, "a b h o",   ""},
            {"-R --exclude=A.C",                      1, "b d h o s", ""},
            {"-R --exclude=A.C",                      0, all,         ""},
            {"-R --exclude= --exclude=a.c",           2, "b d h o s", ""},
            {"--exclude=a.c a.c b.cpp",               2, "b",         ""},
            {"--exclude=nosuch.c nosuch.c",           2, "",          ""},
            {"-R --exclude=*.c --exclude-exception=s.c", 2, "b h s",  ""},
            {"-R --exclude-exception=x --exclude=sub --exclude-exception=",
             2, "a b h o", ""},
            {"-R --exclude=@wild",                    0, "a b d o s", ""},
            {"-R --exclude=./sub",                    2, all,         ""},
            {"-R --exclude=./sub ./sub",              2, "",          ""},
            {"--exclude=sub ./sub",                   2, "",          ""},
            {"--exclude=sub/s.c ./sub",               2, "d s",       ""},
            {"--exclude=./sub/s.c ./sub",             2, "d",         ""},
            {"--exclude=.\\sub\\s.c ./sub",           1, "d",         ""},
            {"--exclude=*b\\s.c .\\sub",              1, "d",         ""},
            {"--exclude=SUB/DEEP sub",                1, "s",         ""},
            {"--exclude=SUB/DEEP sub",                0, "d s",       ""},
            {"-R --exclude=.*",                       2, all,         ""},
            {"-R --exclude=.* .",                     2, "",          ""},
            {"--exclude=.* ./sub sub",                2, "d s",       ""},
            {"-R --exclude=sub --exclude-exception=sub",     2, all,  ""},
            {"-R --exclude=sub --exclude-exception=s*",      2, all,  ""},
            {"-R --exclude=*.c --exclude-exception=su*",     2, "b d h s", ""},
        };
        for (iz i = 0; i < countof(cases); i++) {
            for (i32 w = 0; w < 2; w++) {
                if (cases[i].windows!=2 && cases[i].windows!=w) {
                    continue;
                }
                os->windows = w;
                Str args = concat(&a, "-f - ", cases[i].args);
                Str name = concat(&a, args, w ? Str(" (windows)") : Str(""));
                Run r    = run(os, a, args);
                expect(t, name, num(&a, r.status), "0");
                expect(t, name, tagnames(&a, r.out), cases[i].names);
                expect(t, name, r.err, cases[i].err);
            }
        }

        // D11: with exceptions given, an excluded directory is still
        // pruned unless one matches it, and an excluded file is never
        // examined. Universal Ctags then walks every excluded directory,
        // and warns of a missing file it excludes.
        os->windows = 0;
        struct { Str args, names, err, uctags, uerr; } d11[] = {
            {"-R --exclude=sub --exclude-exception=nothing",
             "a b h o", "", all, ""},
            {"-R --exclude=sub --exclude-exception=s.c",
             "a b h o", "", all, ""},
            {"--exclude=nosuch.c --exclude-exception=zz nosuch.c",
             "", "", "", nosuchw},
        };
        for (iz i = 0; i < countof(d11); i++) {
            Str args = concat(&a, "-f - ", d11[i].args);
            Run r    = run(os, a, args);
            expect(t, args, num(&a, r.status), "0");
            expect(t, args, tagnames(&a, r.out), d11[i].names);
            expect(t, args, r.err, d11[i].err);
            b32 same = d11[i].names==d11[i].uctags && d11[i].err==d11[i].uerr;
            expect(t, args, num(&a, same), "0");
        }

        // D12: on Windows, '\' is '/' in the patterns of an @FILE too, so
        // "sub\deep" in it excludes sub/deep. Universal Ctags keeps the
        // '\', an escape there, so the pattern matches only "subdeep".
        os->windows = 1;
        Str uctags = "a b d o s";
        Run r      = run(os, a, "-f - -R --exclude=@wild");
        Str names  = tagnames(&a, r.out);
        expect(t, "D12", names, "a b o s");
        expect(t, "D12 departs", num(&a, names==uctags), "0");
    }

    {
        Os *os = newos(&a);
        vfsfile(os, "a.c", "int x;\n");
        Run r = run(os, a, "-f - b.c a.c c.c");
        expect(t, "missing status", num(&a, r.status), "0");
        expect(t, "missing err", r.err,
               "ctags: Warning: cannot open input file \"b.c\" : "
               "No such file or directory\n"
               "ctags: Warning: cannot open input file \"c.c\" : "
               "No such file or directory\n");
    }
}

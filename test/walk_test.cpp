// Discovery: paths, exclusions, languages, and whole runs

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
        expect(t, excl[i].path, num(&a, excluded(excl[i].path, excl[i].windows)),
               num(&a, excl[i].want));
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
        "!_TAG_OUTPUT_EXCMD\tmixed\t/number, pattern, mixed, or combineV2/\n"
        "!_TAG_OUTPUT_FILESEP\tslash\t/slash or backslash/\n"
        "!_TAG_OUTPUT_MODE\tu-ctags\t/u-ctags or e-ctags/\n"
        "!_TAG_PATTERN_LENGTH_LIMIT\t96\t/0 for no limit/\n"
        "!_TAG_PROC_CWD\t/work/\t//\n"
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
        // The header follows the options (PT-4)
        struct { Str args, cwd; b32 windows; Str header, err; } heads[] = {
            {"--format=1 --fields=+E --excmd=combine --pattern-length-limit=0x10",
             "/work", 0,
             "!_TAG_FILE_FORMAT\t1\t/original ctags format/\n"
             "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/\n"
             "!_TAG_OUTPUT_EXCMD\tcombineV2\t/number, pattern, mixed, or combineV2/\n"
             "!_TAG_OUTPUT_FILESEP\tslash\t/slash or backslash/\n"
             "!_TAG_OUTPUT_MODE\tu-ctags\t/u-ctags or e-ctags/\n"
             "!_TAG_PATTERN_LENGTH_LIMIT\t16\t/0 for no limit/\n"
             "!_TAG_PROC_CWD\t/work/\t//\n"
             "!_TAG_PROGRAM_NAME\tctags\t//\n"
             "!_TAG_PROGRAM_VERSION\t0.1\t//\n", ""},
            {"--fields=+E --output-format=e-ctags -n --format=0", "C:\\w\\x", 1,
             "!_TAG_FILE_FORMAT\t0\t/unknown format/\n"
             "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/\n"
             "!_TAG_OUTPUT_EXCMD\tnumber\t/number, pattern, mixed, or combineV2/\n"
             "!_TAG_OUTPUT_FILESEP\tbackslash\t/slash or backslash/\n"
             "!_TAG_OUTPUT_MODE\te-ctags\t/u-ctags or e-ctags/\n"
             "!_TAG_PATTERN_LENGTH_LIMIT\t96\t/0 for no limit/\n"
             "!_TAG_PROC_CWD\tC:/w/x/\t//\n"
             "!_TAG_PROGRAM_NAME\tctags\t//\n"
             "!_TAG_PROGRAM_VERSION\t0.1\t//\n", ""},
            {"--fields=+E -N --pattern-length-limit=0", "C:\\", 1,
             "!_TAG_FILE_FORMAT\t2\t/extended format; --format=1 will not append ;\" to lines/;\"\textras:pseudo\n"
             "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/;\"\textras:pseudo\n"
             "!_TAG_OUTPUT_EXCMD\tpattern\t/number, pattern, mixed, or combineV2/;\"\textras:pseudo\n"
             "!_TAG_OUTPUT_FILESEP\tslash\t/slash or backslash/;\"\textras:pseudo\n"
             "!_TAG_OUTPUT_MODE\tu-ctags\t/u-ctags or e-ctags/;\"\textras:pseudo\n"
             "!_TAG_PATTERN_LENGTH_LIMIT\t0\t/0 for no limit/;\"\textras:pseudo\n"
             "!_TAG_PROC_CWD\tC:/\t//;\"\textras:pseudo\n"
             "!_TAG_PROGRAM_NAME\tctags\t//;\"\textras:pseudo\n"
             "!_TAG_PROGRAM_VERSION\t0.1\t//;\"\textras:pseudo\n", ""},
            {"--excmd=m", "/a\\b\x01", 0,
             "!_TAG_FILE_FORMAT\t2\t/extended format; --format=1 will not append ;\" to lines/\n"
             "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/\n"
             "!_TAG_OUTPUT_EXCMD\tmixed\t/number, pattern, mixed, or combineV2/\n"
             "!_TAG_OUTPUT_FILESEP\tslash\t/slash or backslash/\n"
             "!_TAG_OUTPUT_MODE\tu-ctags\t/u-ctags or e-ctags/\n"
             "!_TAG_PATTERN_LENGTH_LIMIT\t96\t/0 for no limit/\n"
             "!_TAG_PROC_CWD\t/a\\\\b\\x01/\t//\n"
             "!_TAG_PROGRAM_NAME\tctags\t//\n"
             "!_TAG_PROGRAM_VERSION\t0.1\t//\n", ""},
            {"--output-format=e-ctags", "/a\\b\x01", 0,
             "!_TAG_FILE_FORMAT\t2\t/extended format; --format=1 will not append ;\" to lines/\n"
             "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/\n"
             "!_TAG_OUTPUT_EXCMD\tmixed\t/number, pattern, mixed, or combineV2/\n"
             "!_TAG_OUTPUT_FILESEP\tslash\t/slash or backslash/\n"
             "!_TAG_OUTPUT_MODE\te-ctags\t/u-ctags or e-ctags/\n"
             "!_TAG_PATTERN_LENGTH_LIMIT\t96\t/0 for no limit/\n"
             "!_TAG_PROC_CWD\t/a\\b\x01/\t//\n"
             "!_TAG_PROGRAM_NAME\tctags\t//\n"
             "!_TAG_PROGRAM_VERSION\t0.1\t//\n", ""},
            {"--output-format=e-ctags", "/a\nb\tc", 0,
             "!_TAG_FILE_FORMAT\t2\t/extended format; --format=1 will not append ;\" to lines/\n"
             "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/\n"
             "!_TAG_OUTPUT_EXCMD\tmixed\t/number, pattern, mixed, or combineV2/\n"
             "!_TAG_OUTPUT_FILESEP\tslash\t/slash or backslash/\n"
             "!_TAG_OUTPUT_MODE\te-ctags\t/u-ctags or e-ctags/\n"
             "!_TAG_PATTERN_LENGTH_LIMIT\t96\t/0 for no limit/\n"
             "!_TAG_PROGRAM_NAME\tctags\t//\n"
             "!_TAG_PROGRAM_VERSION\t0.1\t//\n",
             "ctags: Warning: skip priting !_TAG_PROC_CWD pseudo tag; the "
             "input field of the pseudo tag includes a tab character: "
             "/a\nb\tc/\n"},
        };
        for (iz i = 0; i < countof(heads); i++) {
            Os *os = newos(&a);
            os->cwd     = heads[i].cwd;
            os->windows = heads[i].windows;
            vfsfile(os, "a.c", "");
            Str args = concat(&a, heads[i].args, " -f tags a.c");
            Run r    = run(os, a, args);
            expect(t, args, r.err, heads[i].err);
            Str got = vfsfind(os, "tags", 0)->data;
            expect(t, args, got, heads[i].header);
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

        // The test platform finds absolute paths in its current directory
        r = run(os, a, "-f - /work/a.c");
        expect(t, "absolute out", r.out,
               "x\t/work/a.c\t/^int x;$/;\"\tv\tline:1\ttyperef:typename:int\n");
        os->windows = 1;
        os->cwd     = "C:\\work";
        r = run(os, a, "-f - C:/work\\a.c");
        expect(t, "absolute windows out", r.out,
               "x\tC:/work/a.c\t/^int x;$/;\"\tv\tline:1\ttyperef:typename:int\n");
        expect(t, "cwd", os_cwd(os, &a), "C:\\work");
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

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

    // Whole runs, which tag nothing until the parsers arrive
    Str header =
        "!_TAG_FILE_FORMAT\t2\t/extended format; --format=1 will not append ;\" to lines/\n"
        "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/\n"
        "!_TAG_PROGRAM_NAME\tctags\t//\n"
        "!_TAG_PROGRAM_VERSION\t0.1\t//\n";
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
    }
    {
        Os *os = newos(&a);
        vfsfile(os, "a.c", "int x;\n");
        Run r = run(os, a, "-R -f -");
        expect(t, "stdout status", num(&a, r.status), "0");
        expect(t, "stdout out", r.out, "");
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

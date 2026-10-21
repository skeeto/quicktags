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

// The names and file fields of the tags in output, but pseudo-tags, as
// "name=file" separated by spaces.
static Str tagfiles(Arena *a, Str out)
{
    Buf b(a, 64);
    for (Cut c = cut(out, '\n'); c.ok; c = cut(c.tail, '\n')) {
        if (startswith(c.head, "!_")) {
            continue;
        }
        Cut name = cut(c.head, '\t');
        if (b.len) putbyte(&b, ' ');
        print(&b, name.head);
        putbyte(&b, '=');
        print(&b, cut(name.tail, '\t').head);
    }
    return finish(&b);
}

struct TrelCase {
    Str out;   // the tags file, "-" for standard output
    Str args;
    i32 status;
    Str want;  // tagfiles() of the output
    Str err;
};

// Run --tag-relative cases, each writing its own tags file, which is
// then removed. A failure writes none.
static void runtrel(Test *t, Os *os, TrelCase *cases, iz n)
{
    Arena *a = os->a;
    for (iz i = 0; i < n; i++) {
        TrelCase *c    = cases + i;
        Str       args = concat(a, concat(a, concat(a, "-f ", c->out), " "),
                                c->args);
        Str       name = concat(a, args, os->windows ? Str(" (windows)")
                                                     : Str(""));
        Run       r    = run(os, *a, args);
        VNode    *f    = c->out=="-" ? 0 : vfsfind(os, c->out, 0);
        Str       out  = f ? f->data : r.out;
        expect(t, name, num(a, r.status), num(a, c->status));
        expect(t, name, r.err, c->err);
        expect(t, name, tagfiles(a, out), c->want);
        if (f) {
            vfsunlink(os, c->out);
        }
    }
}

// A --tag-relative case on a quirk of Universal Ctags, with its output,
// which ours differs from exactly where it departs (D14, D15, D16).
struct TrelQuirk {
    Str out;
    Str args;
    Str want;
    Str uctags;
    b32 departs;  // else kept as there
};

static void runquirks(Test *t, Os *os, TrelQuirk *cases, iz n)
{
    Arena *a = os->a;
    for (iz i = 0; i < n; i++) {
        TrelQuirk *q    = cases + i;
        TrelCase   c    = {q->out, q->args, 0, q->want, ""};
        Str        name = concat(a, "departs: -f ", q->out);
        name = concat(a, concat(a, name, " "), q->args);
        runtrel(t, os, &c, 1);
        expect(t, name, num(a, q->want!=q->uctags), num(a, q->departs));
    }
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
                      "CVS", "{arch}", "*~", "a*b*", "*a*", "*/*", "*.C*",
                      "**", "*X\\*"};
        Str names[] = {"a.c", "A.C", "b.c", "a", "", "xy", "xzzy", "x\\y",
                       "d/a/x.c", "x.cpp", "X*", "d\\e",
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
            {"*/x/*",   GLOB_INFIX},
            {"**",      GLOB_ANY},
            {"*x*y*",   GLOB_ANY},
            {"x*y*",    GLOB_ANY},
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

    // Languages by name, with options, as Universal Ctags 6.2.1 on
    // Windows chooses them, but where noted (LANG-2, LANG-5, LANG-6)
    i32 C   = LANG_C;
    i32 CXX = LANG_CXX;
    i32 FTN = LANG_FORTRAN;
    i32 NO  = LANG_NONE;
    struct { Str args, path; i32 lang; b32 header, windows; } langs[] = {
        {"", "a.c",         C,   0, 1},
        {"", "a.C",         CXX, 0, 1},  // D1, u-ctags: C
        {"", "a.C",         CXX, 0, 0},
        {"", "a.CPP",       CXX, 0, 1},
        {"", "a.cc",        CXX, 0, 1},
        {"", "a.h",         CXX, 1, 1},
        {"", "a.H",         CXX, 1, 1},
        {"", "a.hpp",       CXX, 1, 1},
        {"", "a.inl",       CXX, 1, 1},  // D2, u-ctags: not a header
        {"", "a.hp",        CXX, 1, 1},  // D2, u-ctags: not a header
        {"", "a.f",         FTN, 0, 1},
        {"", "A.F90",       FTN, 0, 1},
        {"", "a.f08",       FTN, 0, 1},
        {"", "config.h.in", CXX, 1, 1},  // D2, u-ctags: not a header
        {"", "x.c.in",      C,   0, 1},
        {"", "x.c.IN",      C,   0, 1},  // D2, u-ctags: none
        {"", "x.in",        NO,  0, 1},
        {"", "x.inc",       NO,  0, 1},
        {"", "x.cu",        NO,  0, 1},
        {"", "Makefile",    NO,  0, 1},
        {"", "vector",      NO,  0, 1},
        {"", ".c",          C,   0, 1},
        {"", "dir.c/x",     NO,  0, 1},
        {"", "d/x.cpp",     CXX, 0, 1},
        {"", "d\\x.cpp",    CXX, 0, 1},

        {"--langmap=c:.h",                      "a.h",    C,   1, 1},
        {"--langmap=c:.h",                      "a.c",    NO,  0, 1},
        {"--langmap=c:+.h",                     "a.c",    C,   0, 1},
        {"--langmap=c:+.h",                     "a.h",    C,   1, 1},
        {"--langmap=c++:+.xyz",                 "x.xyz",  CXX, 0, 1},
        {"--langmap=c++:+(no*)",                "noext",  CXX, 0, 1},
        {"--langmap=c++:+.",                    "noext",  CXX, 0, 1},
        {"--langmap=c++:+.",                    "d/q",    CXX, 0, 1},
        {"--langmap=c++:+.",                    "d/q.",   CXX, 0, 1},
        {"--map-C=+.h",                         "b.h",    C,   1, 1},
        {"--map-C++=-.h",                       "b.h",    NO,  0, 1},
        {"--map-C++=-.H",                       "b.h",    NO,  0, 1},
        {"--map-C=.xyz",                        "a.c",    NO,  0, 1},
        {"--map-C=.xyz",                        "x.xyz",  C,   0, 1},
        {"--map-C=+.H",                         "b.H",    C,   1, 1},
        {"--map-C=+.H",                         "b.h",    C,   1, 1},
        {"--map-C=+(*.h)",                      "z.h",    C,   1, 1},
        {"--map-C=+(*.H)",                      "z.h",    C,   1, 1},
        {"--map-C=+(*.H)",                      "z.h",    CXX, 1, 0},
        {"--langmap=c:+(*.x) --langmap=c++:+(*.X)", "p.x", CXX, 0, 1},
        {"--map-C=+(*.x) --map-c++=+(*.x)",     "p.x",    C,   0, 1},
        {"--map-Fortran=+.x --map-c++=+.x",     "p.x",    CXX, 0, 1},
        {"--map-Fortran=+(*.x) --map-c++=+.x",  "p.x",    FTN, 0, 1},
        {"--map-Fortran=+.in --map-C++=+.in",   "x.in",     CXX, 0, 1},
        {"--map-Fortran=+.in --map-C++=+.in",   "x.c.in",   C,   0, 1},
        {"--map-Fortran=+.in --map-C++=+.in",   "y.f90.in", FTN, 0, 1},
        {"--map-Fortran=+.in --map-C++=+.in",   "z.h.in",   CXX, 1, 1},
        {"--map-Fortran=+.in --map-C=+.in",     "x.in",     C,   0, 1},
        {"--map-C=+.in --map-C++=+.in",         "y.f90.in", C,   0, 1},
        {"--map-Fortran=+.in --map-C=+.in --map-C++=+.in", "y.f90.in", FTN, 0, 1},
        {"--map-Fortran=+(*.in) --map-C++=+(*.in)", "x.c.in", C,  0, 1},
        {"--map-Fortran=+(*.in) --map-C++=+(*.in)", "x.in",   CXX, 0, 1},
        {"--map-Fortran=+.c",                   "x.c.in",   C,   0, 1},
        {"--map-Fortran=+.f90 --map-C=+.f90",   "y.f90.in", C,   0, 1},
        {"--langmap=C:.X",                      "q.X",    C,   0, 1},
        {"--langmap=C:.X",                      "p.x",    C,   0, 1},
        {"--langmap=c:+.HH",                    "u.hh",   C,   1, 1},
        {"--map-C=-.c",                         "a.c",    NO,  0, 1},
        {"--map-C=-.c",                         "a.C",    CXX, 0, 1},  // D1, u-ctags: none
        {"--languages=-C",                      "a.C",    CXX, 0, 1},  // D1, u-ctags: none
        {"--map-C++=+.x",                       "a.C",    CXX, 0, 1},  // D1, u-ctags: C
        {"--map-C++=-.C",                       "a.C",    C,   0, 1},
        {"--map-C++=-.c",                       "a.C",    C,   0, 1},
        {"--map-C++=+.c --map-C++=-.c",         "a.C",    C,   0, 1},
        {"--map-C++=-.C --langmap=c++:default", "a.C",    CXX, 0, 1},  // D1, u-ctags: C
        {"--map-C++=.cpp",                      "a.C",    C,   0, 1},
        {"--langmap=c++:.C",                    "a.c",    CXX, 0, 1},
        {"--langmap=c++:+.c",                   "a.C",    CXX, 0, 1},
        {"--langmap=c:+.c",                     "a.C",    C,   0, 1},
        {"--langmap=c++:+.in",                  "y.c.in", CXX, 0, 1},
        {"--langmap=c++:+.in",                  "r.in",   CXX, 0, 1},
        {"--langmap=c:(*.c.in)",                "y.c.in", C,   0, 1},
        {"--langmap=c:(*.c.in) --langmap=c++:+(*.C.IN)", "y.c.in", CXX, 0, 1},
        {"--map-C=+.c.in",                      "y.c.in", C,   0, 1},
        {"--map-C=.c.h",                        "z.h",    CXX, 1, 1},
        {"--map-C=(x)y",                        "z.h",    CXX, 1, 1},
        {"--map-C=(x)y",                        "x",      C,   0, 1},
        {"--langmap=C++:.cpp",                  "z.h",    NO,  0, 1},
        {"--langmap=C++:+.cpp.h",               "z.h",    CXX, 1, 1},
        {"--langmap=C++:.cpp --langmap=default", "z.h",   CXX, 1, 1},
        {"--langmap=c++:default,c:.h",          "b.h",    C,   1, 1},
        {"--langmap=c:.h --langmap=c:default",  "b.h",    NO,  0, 1},
        {"--langmap=c:.h --langmap=c:default",  "a.c",    C,   0, 1},
        {"--langmap=fortran:.c",                "a.c",    FTN, 0, 1},
        {"--langmap=python:.c",                 "a.c",    NO,  0, 1},
        {"--langmap=python:+.h,c:+.x",          "a.h",    NO,  0, 1},
        {"--langmap=python:+.h,c:+.x",          "a.x",    C,   0, 1},
        {"--map-python=+.c",                    "a.c",    C,   0, 1},
        {"--languages=+python --map-python=+.c", "a.c",   C,   0, 1},
        {"--langdef=Aaa --map-Aaa=+.c",         "a.c",    NO,  0, 1},
        {"--langdef=Zzz --map-Zzz=+.c",         "a.c",    C,   0, 1},
        {"--langdef=Aaa --map-Aaa=+.h",         "a.h",    NO,  0, 1},
        {"--langdef=CC --map-CC=+.cpp",         "a.cpp",  CXX, 0, 1},
        {"--langdef=Aaa --map-Aaa=+(a*)",       "a.c",    NO,  0, 1},
        {"--langdef=Aaa{base=C} --map-Aaa=+.c", "a.c",    NO,  0, 1},
        {"--langdef=Aaa --langmap=Aaa:+.c",     "a.c",    NO,  0, 1},
        {"--langdef=Aaa --map-Aaa=+.c --languages=-Aaa", "a.c", C, 0, 1},
        {"--langdef=Aaa --map-Aaa=+.c --languages=C",    "a.c", C, 0, 1},
        {"--langdef=Aaa --map-Aaa=+.c --languages=-all,+C,+aaa", "a.c", NO, 0, 1},
        {"--langdef=Aaa --map-Aaa=+.c --languages=all",  "a.c", NO, 0, 1},
        {"--langdef=Aaa --map-Aaa=+.c --langmap=default",     "a.c", C, 0, 1},
        {"--langdef=Aaa --map-Aaa=+.c --langmap=aaa:default", "a.c", C, 0, 1},
        {"--langdef=Aaa --map-Aaa=+.c --langmap=c:+.c",  "a.c", C, 0, 1},
        {"--langdef=Aaa --map-Aaa=+.c --map-aaa=-.C",    "a.c", C, 0, 1},
        {"--langdef=Aaa --map-Aaa=.c --map-aaa=.x",      "a.c", C, 0, 1},
        {"--langdef=Aaa --map-Aaa=+.c --language-force=C", "a.c", C, 0, 1},
        {"--langdef=Aaa --map-Aaa=+.in",        "x.c.in", NO,  0, 1},
        {"--langdef=Aaa --langmap=Aaa:.in --map-C=+.in", "x.c.in", C, 0, 1},
        {"--map-C=+.c --map-C=+.c --langmap=c++:+.c", "a.c", C, 0, 1},
        {"--languages=-C",                      "a.c",    NO,  0, 1},
        {"--languages=-C",                      "a.C",    CXX, 0, 1},
        {"--languages=-C++",                    "a.C",    C,   0, 1},
        {"--languages=",                        "a.c",    NO,  0, 1},
        {"--languages=Python,C++",              "a.h",    CXX, 1, 1},
        {"--languages=-C++ --map-C=+(z*)",      "z.h",    C,   1, 1},
        {"--language-force=C++",                "x.xyz",  CXX, 0, 1},
        {"--language-force=c",                  "z.h",    C,   1, 1},
        {"--language-force=c",                  "Makefile", C, 0, 1},
        {"--language-force=Fortran",            "a.c",    FTN, 0, 1},
        {"--language-force=python",             "a.c",    NO,  0, 1},
        {"--language-force=c --language-force=auto", "a.f", FTN, 0, 1},
        {"-h .x --language-force=C++",          "p.x",    CXX, 1, 1},
        {"-h .x --language-force=C++",          "p.x.in", CXX, 1, 1},  // D2
        {"-h .xyz --langmap=c:+.xyz",           "x.xyz",  C,   1, 1},
        {"-h .xyz",                             "b.h",    CXX, 0, 1},
        {"-h +.xyz --langmap=c:+.xyz",          "b.h",    CXX, 1, 1},
        {"-h .c",                               "a.c",    C,   1, 1},
        {"-h .C",                               "a.c",    C,   1, 1},
        {"-h default",                          "b.h",    CXX, 1, 1},
        {"-h DEFAULT",                          "b.h",    CXX, 0, 1},
        {"-h h..c --map-C=+.",                  "noext",  C,   1, 1},
        {"-h c. --map-C=+.",                    "noext",  C,   1, 1},
        {"-h c --map-C=+.",                     "noext",  C,   0, 1},
        {"-h +",                                "b.h",    CXX, 1, 1},
        {"-h .x -h +.y",                        "a.h",    CXX, 0, 1},
        {"-h .x -h +.h",                        "a.h",    CXX, 1, 1},
    };
    for (iz i = 0; i < countof(langs); i++) {
        Arena tmp = a;
        Os   *os  = newos(&tmp);
        os->windows = langs[i].windows;
        Str    args = concat(&tmp, langs[i].args, " x");
        Parsed r    = parseos(&tmp, os, args);
        Str    name = concat(&tmp, concat(&tmp, args, ": "), langs[i].path);
        name = concat(&tmp, name, langs[i].windows ? Str("") : Str(" (posix)"));
        Lang   l    = detectlang(&r.opt, langs[i].path);
        expect(t, name, r.log, "");
        expect(t, name, num(&tmp, l.lang), num(&tmp, langs[i].lang));
        expect(t, name, num(&tmp, l.header), num(&tmp, langs[i].header));
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
        // Unsorted, each file's tags as its parser made them, the files
        // in the walk's order: the arguments, then the -L entries, each
        // directory depth-first in listing order, and nothing dropped
        // (SORT-4). Folded, letters as uppercase, ties by bytes (SORT-3).
        Os *os = newos(&a);
        vfsfile(os, "z.c", "int zb;\nint za;\n");
        vfsfile(os, "d/y.c", "int y;\n");
        vfsfile(os, "d/e/x.c", "int x;\n");
        vfsfile(os, "d/w.c", "int w;\n");
        vfsfile(os, "a.c", "int B, a, _c, A, b;\n");
        vfsfile(os, "list", "a.c\nd/e\n");
        struct { Str args, want; } orders[] = {
            {"-u z.c d a.c z.c",       "zb za y x w B a _c A b zb za"},
            {"--sort=no -L list z.c",  "zb za B a _c A b x"},
            {"--sort=no -R",           "zb za y x w B a _c A b"},
            {"z.c d a.c z.c",          "A B _c a b w x y za zb"},
            {"--sort=foldcase a.c z.c", "A a B b za zb _c"},
            {"--sort=f -u a.c",        "B a _c A b"},
        };
        for (iz i = 0; i < countof(orders); i++) {
            for (u64 seed = 0; seed <= 4; seed++) {
                os->seed = seed;
                Str args = concat(&a, "--jobs=3 -f - ", orders[i].args);
                Run r    = run(os, a, args);
                expect(t, args, num(&a, r.status), "0");
                expect(t, args, tagnames(&a, r.out), orders[i].want);
            }
        }
        os->seed = 0;

        // The header says how
        Str sorted[] = {"--sort=no", "-u", "--sort=yes", "--sort=foldcase"};
        Str want[]   = {"0", "0", "1", "2"};
        for (iz i = 0; i < countof(sorted); i++) {
            Str args = concat(&a, sorted[i], " a.c");
            run(os, a, args);
            Str got  = vfsfind(os, "tags", 0)->data;
            Str line = concat(&a, "!_TAG_FILE_SORTED\t", want[i]);
            expect(t, args, num(&a, find(got, line)>=0), "1");
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
        r = run(os, a, "-f - c:\\WORK/a.c");
        expect(t, "absolute windows case out", r.out,
               "x\tc:/WORK/a.c\t/^int x;$/;\"\tv\tline:1\ttyperef:typename:int\n");

        // From the root, whose path ends in a separator
        os->cwd = "C:\\";
        r = run(os, a, "-f - C:/a.c");
        expect(t, "absolute drive root out", r.out,
               "x\tC:/a.c\t/^int x;$/;\"\tv\tline:1\ttyperef:typename:int\n");
        os->windows = 0;
        os->cwd     = "/";
        r = run(os, a, "-f - /a.c");
        expect(t, "absolute root out", r.out,
               "x\t/a.c\t/^int x;$/;\"\tv\tline:1\ttyperef:typename:int\n");
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
        // -L lists (DISC-6): trimmed lines, comments and blank lines
        // skipped, options among the entries, directories recursed (D8)
        Os *os = newos(&a);
        vfsfile(os, "a.c",     "int a;\nint pa(void);\n");
        vfsfile(os, "b.cpp",   "int b;\n");
        vfsfile(os, "x.h",     "int h;\n");
        vfsfile(os, "sub/s.c", "int s;\n");
        vfsfile(os, "list",    "a.c\r\n  b.cpp  \n# x.h\n\n--kinds-C=+p\rsub\n");
        vfsfile(os, "list2",   "a.c\n-L\nlist\n");
        vfsfile(os, "list3",   "--exclude=b.cpp\na.c\n");
        vfsfile(os, "list4",   "--version\na.c\n");
        vfsfile(os, "empty",   "");
        Str again = "ctags: Warning: -L option specified more than once, "
                    "last value used\n";
        struct { Str args; i32 status; Str names, err; } cases[] = {
            {"-L list",           0, "a b pa s",   ""},
            {"-L list x.h",       0, "a b h pa s", ""},
            {"-R -L list",        0, "a b pa s",   ""},
            {"-L list2",          0, "a",          again},
            {"-L empty",          0, "",           ""},
            {"-R -L empty",       0, "",           ""},
            {"-L nosuch a.c",     1, "",           "ctags: cannot open list "
                                                   "file \"nosuch\" : No such "
                                                   "file or directory\n"},
            {"-L sub",            1, "",           "ctags: cannot open list "
                                                   "file \"sub\" : Is a "
                                                   "directory\n"},
            // Options apply to every file wherever they are: Universal
            // Ctags excludes nothing here, as b.cpp comes first
            {"-L list3 b.cpp",    0, "a",          ""},
            {"-L - b.cpp",        0, "a b h",      ""},
        };
        os->input = "x.h\n\na.c";
        for (iz i = 0; i < countof(cases); i++) {
            Str args = concat(&a, "-f - ", cases[i].args);
            Run r    = run(os, a, args);
            expect(t, args, num(&a, r.status), num(&a, cases[i].status));
            if (!cases[i].status) {
                expect(t, args, tagnames(&a, r.out), cases[i].names);
            }
            expect(t, args, r.err, cases[i].err);
        }
        Run r = run(os, a, "-L list4");
        expect(t, "-L list4", r.out, "ctags 0.1\n");

        // D13: a -L after a file name is read as any other. Universal
        // Ctags parses it but never reads the list, so it tags x.h alone,
        // and a list that cannot be opened is no error.
        Str nosuch = "ctags: cannot open list file \"nosuch\" : No such "
                     "file or directory\n";
        struct {
            Str args;
            i32 status;
            Str names, err;
            i32 ustatus;
            Str unames, uerr;
        } d13[] = {
            {"x.h -L list",   0, "a b h pa s", "",     0, "h", ""},
            {"x.h -L -",      0, "a h",        "",     0, "h", ""},
            {"x.h -L nosuch", 1, "",           nosuch, 0, "h", ""},
        };
        for (iz i = 0; i < countof(d13); i++) {
            Str args = concat(&a, "-f - ", d13[i].args);
            r = run(os, a, args);
            Str names = tagnames(&a, r.out);
            expect(t, args, num(&a, r.status), num(&a, d13[i].status));
            expect(t, args, names, d13[i].names);
            expect(t, args, r.err, d13[i].err);
            b32 same = r.status==d13[i].ustatus && names==d13[i].unames &&
                       r.err==d13[i].uerr;
            expect(t, args, num(&a, same), "0");
        }
    }

    {
        // Languages in whole runs (LANG-6): forced on every input, walked
        // or given, disabled, mapped, and -h deciding which are headers
        Os *os = newos(&a);
        vfsfile(os, "a.c",   "static int sa;\n");
        vfsfile(os, "b.h",   "static int sb;\n");
        vfsfile(os, "noext", "static int sn;\n");
        vfsfile(os, "x.o",   "static int so;\n");
        vfsfile(os, "d/q",   "static int sq;\n");
        vfsfile(os, "f.f90", "module m\nend module\n");
        Str off = "ctags: C parser specified with --language-force is "
                  "disabled\n";
        struct { Str args; i32 status; Str names, err; } cases[] = {
            {"-R",                            0, "m sa sb",        ""},
            {"-R --language-force=C",         0, "sa sb sn sq",    ""},
            {"--language-force=C f.f90 d",    0, "sq",             ""},
            {"-R --language-force=Python",    0, "",               ""},
            {"-R --languages=-C",             0, "m sb",           ""},
            {"-R --languages=Fortran",        0, "m",              ""},
            {"-R --languages=",               0, "",               ""},
            {"-R --langmap=c:+.",             0, "m sa sb sn sq",  ""},
            {"-R --langmap=fortran:+.h.c",    0, "m",              ""},
            {"--languages=-C --language-force=C a.c", 1, "", off},
            {"--language-force=C --languages=-C nosuch.c", 1, "", off},
        };
        for (iz i = 0; i < countof(cases); i++) {
            Str args = concat(&a, "-f - ", cases[i].args);
            Run r    = run(os, a, args);
            expect(t, args, num(&a, r.status), num(&a, cases[i].status));
            expect(t, args, tagnames(&a, r.out), cases[i].names);
            expect(t, args, r.err, cases[i].err);
        }

        // Only headers' static variables lack file: (LANG-5)
        Str sa = "sa\ta.c\t/^static int sa;$/;\"\tv\tline:1\t"
                 "typeref:typename:int";
        Str sb = "sb\tb.h\t/^static int sb;$/;\"\tv\tline:1\t"
                 "typeref:typename:int";
        struct { Str args, out; } hs[] = {
            {"a.c b.h",              "%a\tfile:\n%b\n"},
            {"-h .c a.c b.h",        "%a\n%b\tfile:\n"},
            {"-h +.c a.c b.h",       "%a\n%b\n"},
            {"-h c.H a.c b.h",       "%a\n%b\n"},
            {"-h x -h default a.c",  "%a\tfile:\n"},
        };
        for (iz i = 0; i < countof(hs); i++) {
            Buf want(&a, 256);
            for (iz j = 0; j < hs[i].out.len; j++) {
                u8 c = hs[i].out[j];
                if (c == '%') {
                    print(&want, hs[i].out[++j]=='a' ? sa : sb);
                } else {
                    putbyte(&want, c);
                }
            }
            Str args = concat(&a, "-f - ", hs[i].args);
            Run r    = run(os, a, args);
            expect(t, args, r.out, finish(&want));
            expect(t, args, r.err, "");
        }
    }

    {
        // --links=no skips links to files, walked or given (DISC-7)
        Os *os = newos(&a);
        vfsfile(os, "a.c",     "int a;\n");
        vfslink(os, "l.c",     "int l;\n");
        vfslink(os, "sub/m.c", "int m;\n");
        vfsfile(os, "sub/n.c", "int n;\n");
        struct { Str args, names; } cases[] = {
            {"-R",                         "a l m n"},
            {"-R --links=no",              "a n"},
            {"--links=no a.c l.c sub",     "a n"},
            {"--links=no --links a.c l.c", "a l"},
        };
        for (iz i = 0; i < countof(cases); i++) {
            Str args = concat(&a, "-f - ", cases[i].args);
            Run r    = run(os, a, args);
            expect(t, args, num(&a, r.status), "0");
            expect(t, args, tagnames(&a, r.out), cases[i].names);
            expect(t, args, r.err, "");
        }
    }

    {
        // -I files, a token to a line (OPT-17), named on Windows also
        // by a drive letter
        Os *os = newos(&a);
        vfsfile(os, "m.c", "DECL x;\nWRAP(1) y;\n");
        vfsfile(os, "list", "DECL=int\r\n  \nWRAP+=long \n");
        vfsfile(os, "C:list", "DECL=char\n");
        Str x = "x\tm.c\t/^DECL x;$/;\"\tv\tline:1\ttyperef:typename:";
        Str y = "y\tm.c\t/^WRAP(1) y;$/;\"\tv\tline:2\ttyperef:typename:";
        Run r = run(os, a, "-f - -I @list m.c");
        expect(t, "-I @list err", r.err, "");
        expect(t, "-I @list out", r.out,
               concat(&a, concat(&a, concat(&a, x, "int\n"), y), "long\n"));
        r = run(os, a, "-f - -I C:list m.c");
        expect(t, "-I C:list out", r.out, concat(&a, x, "DECL\n"));
        os->windows = 1;
        r = run(os, a, "-f - -I C:list m.c");
        expect(t, "-I C:list windows out", r.out, concat(&a, x, "char\n"));
        r = run(os, a, "-f - -I @ m.c");
        expect(t, "-I @ windows err", r.err,
               "ctags: cannot open \"\" : Invalid argument\n");

        // Read as text on Windows, a list ends at a Ctrl-Z
        vfsfile(os, "z.c", "FOO int a;\nBAZ int b;\n");
        vfsfile(os, "z.txt", "FOO\x1a" "BAR+\n\x1a" "BAZ\n");
        Str za = "a\tz.c\t/^FOO int a;$/;\"\tv\tline:1\ttyperef:typename:";
        Str zb = "b\tz.c\t/^BAZ int b;$/;\"\tv\tline:2\ttyperef:typename:BAZ int\n";
        r = run(os, a, "-f - -I @z.txt z.c");
        expect(t, "-I Ctrl-Z windows", r.out,
               concat(&a, concat(&a, za, "int\n"), zb));
        os->windows = 0;
        r = run(os, a, "-f - -I @z.txt z.c");
        expect(t, "-I Ctrl-Z", r.out,
               concat(&a, concat(&a, za, "FOO int\n"), zb));
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

    {
        // Workers, as many as there are files to tag, without directories
        // to walk, however many arguments are examined, by the workers or
        // not: each worker takes a share of the memory
        Os *os = newos(&a);
        os->ncpu = 4;
        Str srcs[] = {"a.c", "b.c", "c.c", "d.c", "e.c", "f.c", "sub/s.c"};
        for (iz i = 0; i < countof(srcs); i++) {
            vfsfile(os, srcs[i], "int x;\n");
        }
        Str docs[] = {"README", "Makefile", "notes.txt", "x.o", "y.txt"};
        Buf list(&a, 256);
        print(&list, "a.c\n");
        for (iz i = 0; i < countof(docs); i++) {
            vfsfile(os, docs[i], "text\n");
            print(&list, docs[i]);
            putbyte(&list, '\n');
        }
        vfsfile(os, "list", finish(&list));
        struct { Str args, jobs; } cases[] = {
            {"a.c",                                       "1"},
            {"a.c b.c",                                   "2"},
            {"a.c README nosuch.c",                       "1"},
            {"a.c README notes.txt Makefile nosuch.c",    "1"},
            {"-L list",                                   "1"},
            {"a.c README notes.txt Makefile y.txt sub",   "4"},
            {"a.c b.c README notes.txt Makefile",         "2"},
            {"a.c b.c c.c d.c e.c f.c",                   "4"},
            {"--jobs=2 a.c b.c c.c d.c",                  "2"},
            {"",                                          "4"},
        };
        for (iz i = 0; i < countof(cases); i++) {
            Str args = concat(&a, "--_stats -R -f - ", cases[i].args);
            Run r    = run(os, a, args);
            Str jobs = {};
            for (Cut c = cut(r.err, '\n'); c.ok; c = cut(c.tail, '\n')) {
                if (find(c.head, " jobs, ") > 0) {
                    jobs = cut(cuthead(c.head, 7), ' ').head;
                }
            }
            expect(t, args, jobs, cases[i].jobs);
        }
    }

    {
        // The workers' warnings, more than the log first holds, sorted
        // after those of missing arguments, and each message intact
        // however full the log is when the tags file cannot be written
        Os *os = newos(&a);
        vfsfile(os, "a.c", "int a;\n");
        for (i32 i = 0; i < 100; i++) {
            Buf name(&a, 16);
            print(&name, "tags.tmp");
            print(&name, (i64)i);
            vfsfile(os, finish(&name), "");
        }
        Str nosuch = "ctags: Warning: cannot open input file \"nosuch.c\" : "
                     "No such file or directory\n";
        Str nowrite = "ctags: cannot open tag file \"tags.tmp99\" : "
                      "File exists\n";
        Buf want(&a, 1<<14);
        print(&want, nosuch);
        for (i32 i = 0; i < 120; i++) {
            Buf name(&a, 16);
            print(&name, "d/u");
            printpad(&name, i, 3);
            print(&name, ".c");
            vfsunreadable(os, finish(&name));
            print(&want, "ctags: Warning: cannot open input file \"");
            print(&want, finish(&name));
            print(&want, "\" : Permission denied\n");

            Str  args = num(&a, i);
            Run  r    = run(os, a, "a.c nosuch.c d");
            Str  err  = concat(&a, finish(&want), nowrite);
            expect(t, concat(&a, "warnings, tags ", args), r.err, err);
            r = run(os, a, "-f - a.c nosuch.c d");
            expect(t, concat(&a, "warnings ", args), r.err, finish(&want));
            expect(t, concat(&a, "warnings out ", args), tagnames(&a, r.out),
                   "a");
        }
    }

    // --tag-relative (OPT-18): file fields as Universal Ctags writes them
    // in this tree, here from /home/u/w, and on Windows from C:\Users\u\w
    Str here    = "a=a.c b=sub/b.c d=sub/deep/d.c o=other/o.c";
    Str insub   = "a=../a.c b=b.c d=deep/d.c o=../other/o.c";
    Str indeep  = "a=../../a.c b=../b.c d=d.c o=../../other/o.c";
    Str inother = "a=../a.c b=../sub/b.c d=../sub/deep/d.c o=o.c";
    Str above   = "a=w/a.c b=w/sub/b.c d=w/sub/deep/d.c o=w/other/o.c";
    {
        Os *os = newos(&a);
        os->cwd = "/home/u/w";
        vfsfile(os, "a.c",          "int a;\n");
        vfsfile(os, "sub/b.c",      "int b;\n");
        vfsfile(os, "sub/deep/d.c", "int d;\n");
        vfsfile(os, "other/o.c",    "int o;\n");
        vfsfile(os, "list",         "./a.c\nsub/b.c\n/home/u/w/other/o.c\n");
        vfsfile(os, "opts",         "--tag-relative\n");
        Str abs    = "a=/home/u/w/a.c b=/home/u/w/sub/b.c "
                     "d=/home/u/w/sub/deep/d.c o=/home/u/w/other/o.c";
        Str a1     = "a=/home/u/w/a.c";
        Str b1     = "b=/home/u/w/sub/b.c";
        Str nosuch = "ctags: Warning: cannot open input file \"./nosuch.c\" "
                     ": No such file or directory\n";
        TrelCase cases[] = {
            // Each value, with the tags file anywhere
            {"sub/tags",           "-R",                       0, here,    ""},
            {"sub/tags",           "--tag-relative=no -R",     0, here,    ""},
            {"sub/tags",           "--tag-relative -R",        0, insub,   ""},
            {"sub/tags",           "--tag-relative= -R",       0, insub,   ""},
            {"sub/tags",           "--tag-relative=yes .",     0, insub,   ""},
            {"sub/tags",           "--tag-relative=always -R", 0, insub,   ""},
            {"sub/tags",           "--tag-relative=never -R",  0, abs,     ""},
            {"sub/tags",           "--options=opts -R",        0, insub,   ""},
            {"sub/deep/tags",      "--tag-relative -R",        0, indeep,  ""},
            {"other/tags",         "--tag-relative -R",        0, inother, ""},
            {"./sub/tags",         "--tag-relative -R",        0, insub,   ""},
            {"sub/./tags",         "--tag-relative -R",        0, insub,   ""},
            {"sub/../other/tags",  "--tag-relative -R",        0, inother, ""},
            {"../tags",            "--tag-relative -R",        0, above,   ""},
            {"../tags",            "--tag-relative=always -R", 0, above,   ""},
            {"/home/u/w/sub/tags", "--tag-relative -R",        0, insub,   ""},
            {"/home/u/w/tags",     "--tag-relative=always -R", 0, here,    ""},
            {"tags",               "--tag-relative -R",        0, here,    ""},
            {"-",                  "--tag-relative -R",        0, here,    ""},
            {"-",                  "--tag-relative=always -R", 0, here,    ""},
            {"-",                  "--tag-relative=never -R",  0, abs,     ""},

            // Roots with "." and "..", and what is walked under them
            {"sub/tags", "--tag-relative ./",          0, insub,              ""},
            {"sub/tags", "--tag-relative sub/",        0, "b=b.c d=deep/d.c", ""},
            {"sub/tags", "--tag-relative sub/deep/..", 0, "b=b.c d=deep/d.c", ""},
            {"sub/tags", "--tag-relative sub/.",       0, "b=b.c d=deep/d.c", ""},
            {"sub/tags", "--tag-relative ./a.c sub/./b.c sub/deep/../b.c "
                         "../w/a.c",                   0, "a=../a.c b=b.c",   ""},
            {"-",        "--tag-relative ./a.c sub/deep/../b.c "
                         "../w/sub/deep/d.c",
                         0, "a=a.c b=sub/b.c d=sub/deep/d.c", ""},
            {"-",        "--tag-relative=never ./a.c ../w/a.c", 0, a1, ""},

            // Absolute inputs, written as constructed only by yes
            {"sub/tags", "--tag-relative /home/u/w/a.c /home/u/w/sub/../sub/b.c",
                         0, "a=/home/u/w/a.c b=/home/u/w/sub/../sub/b.c", ""},
            {"sub/tags", "--tag-relative=always /home/u/w/a.c "
                         "/home/u/w/sub/../sub/b.c", 0, "a=../a.c b=b.c", ""},
            {"sub/tags", "--tag-relative=never /home/u/w/a.c "
                         "/home/u/w/sub/../sub/b.c",
                         0, concat(&a, concat(&a, a1, " "), b1), ""},
            {"../tags",  "--tag-relative=always /home/u/w/sub/b.c",
                         0, "b=w/sub/b.c", ""},

            // -L entries are as arguments
            {"sub/tags", "--tag-relative -L list",
                         0, "a=../a.c b=b.c o=/home/u/w/other/o.c", ""},
            {"sub/tags", "--tag-relative=always -L list",
                         0, "a=../a.c b=b.c o=../other/o.c", ""},
            {"-",        "--tag-relative=never -L list",
                         0, concat(&a, concat(&a, concat(&a, a1, " "), b1),
                                   " o=/home/u/w/other/o.c"), ""},

            // Warnings keep the path as constructed
            {"-",        "--tag-relative=never ./nosuch.c a.c", 0, a1, nosuch},
        };
        runtrel(t, os, cases, countof(cases));

        // Anonymous names hash the path as constructed, whatever the file
        // field holds
        vfsfile(os, "anon.c", "struct { int v; } g;\n");
        Run r1 = run(os, a, "-f - anon.c");
        Run r2 = run(os, a, "-f - --tag-relative=never anon.c");
        Run r3 = run(os, a, "-f - ./anon.c");
        Str n1 = tagnames(&a, r1.out);
        expect(t, "trel anon", tagnames(&a, r2.out), n1);
        expect(t, "trel anon file",
               num(&a, find(r2.out, "\t/home/u/w/anon.c\t")>0), "1");
        expect(t, "trel anon ./", num(&a, tagnames(&a, r3.out)==n1), "0");
        vfsunlink(os, "anon.c");

        // The header names the current directory whatever the base, and
        // the lines follow the format with the paths made relative
        {
            Run r = run(os, a, "-f sub/tags --tag-relative=always "
                               "--excmd=combine --fields=+K a.c sub/b.c");
            expect(t, "trel header status", num(&a, r.status), "0");
            expect(t, "trel header", vfsfind(os, "sub/tags", 0)->data,
                "!_TAG_FILE_FORMAT\t2\t/extended format; --format=1 will not append ;\" to lines/\n"
                "!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/\n"
                "!_TAG_OUTPUT_EXCMD\tcombineV2\t/number, pattern, mixed, or combineV2/\n"
                "!_TAG_OUTPUT_FILESEP\tslash\t/slash or backslash/\n"
                "!_TAG_OUTPUT_MODE\tu-ctags\t/u-ctags or e-ctags/\n"
                "!_TAG_PATTERN_LENGTH_LIMIT\t96\t/0 for no limit/\n"
                "!_TAG_PROC_CWD\t/home/u/w/\t//\n"
                "!_TAG_PROGRAM_NAME\tctags\t//\n"
                "!_TAG_PROGRAM_VERSION\t0.1\t//\n"
                "a\t../a.c\t1;/^int a;$/;\"\tvariable\tline:1\ttyperef:typename:int\n"
                "b\tb.c\t1;/^int b;$/;\"\tvariable\tline:1\ttyperef:typename:int\n");
            vfsunlink(os, "sub/tags");
        }

        // D14: doubled separators are no name of their own where Universal
        // Ctags' counting them goes wrong, and stay where they lead to the
        // file, as there
        TrelQuirk d14[] = {
            {"-",           "--tag-relative .//a.c",          "a=a.c", "a=/a.c", 1},
            {"sub/tags",    "--tag-relative sub//b.c sub///deep/d.c",
                            "b=b.c d=deep/d.c",       "b=/b.c d=//deep/d.c", 1},
            {"sub//tags",   "--tag-relative a.c sub/b.c",
                            "a=../a.c b=b.c",         "a=../../a.c b=../b.c", 1},
            {".//sub/tags", "--tag-relative=always a.c sub/b.c",
                            "a=../a.c b=../sub/b.c",  "a=../../a.c b=../../sub/b.c", 1},
            {"-",           "--tag-relative=never x//../a.c",
                            a1,                       "a=/home/u/w/x/a.c", 1},
            {"sub/tags",    "--tag-relative .//a.c",          "a=..//a.c", "a=..//a.c", 0},
            {"-",           "--tag-relative=never .//a.c",
                            "a=/home/u/w//a.c",       "a=/home/u/w//a.c", 0},
        };
        runquirks(t, os, d14, countof(d14));

        // Without a current directory, --tag-relative fails, giving no
        // reason, and the default works, where Universal Ctags fails at
        // every start
        os->cwd = {};
        Run r = run(os, a, "-f - --tag-relative a.c");
        expect(t, "trel nocwd status", num(&a, r.status), "1");
        expect(t, "trel nocwd err", r.err, "ctags: failed in getcwd()\n");
        r = run(os, a, "-f - --tag-relative=no a.c");
        expect(t, "trel nocwd no", tagfiles(&a, r.out), "a=a.c");
        r = run(os, a, "a.c");
        VNode *tags = vfsfind(os, "tags", 0);
        expect(t, "trel nocwd default", num(&a, r.status), "0");
        expect(t, "trel nocwd default", tagfiles(&a, tags ? tags->data : Str{}),
               "a=a.c");
        expect(t, "trel nocwd header",
               num(&a, find(tags ? tags->data : Str{}, "!_TAG_PROC_CWD")), "-1");
        vfsunlink(os, "tags");
    }
    {
        // From a root, which the system names with its separator, "/" or
        // "C:\", unlike any other directory
        Os *os = newos(&a);
        os->cwd = "/";
        vfsfile(os, "home/u/w/a.c",     "int a;\n");
        vfsfile(os, "home/u/w/sub/b.c", "int b;\n");
        TrelCase cases[] = {
            {"-", "--tag-relative=never home/u/w/a.c",   0, "a=/home/u/w/a.c", ""},
            {"-", "--tag-relative ./home/u/w/a.c",       0, "a=home/u/w/a.c",  ""},
            {"-", "--tag-relative=always /home/u/w/a.c", 0, "a=home/u/w/a.c",  ""},
            {"home/u/w/sub/tags", "--tag-relative=always /home/u/w/a.c",
                                  0, "a=../a.c", ""},
        };
        runtrel(t, os, cases, countof(cases));

        os = newos(&a);
        os->windows = 1;
        os->cwd     = "C:\\";
        vfsfile(os, "Users/u/w/a.c",     "int a;\n");
        vfsfile(os, "Users/u/w/sub/b.c", "int b;\n");
        TrelCase wcases[] = {
            {"-", "--tag-relative=never Users\\u\\w\\a.c",
                  0, "a=C:/Users/u/w/a.c", ""},
            {"-", "--tag-relative=never \\Users\\u\\w\\a.c",
                  0, "a=C:/Users/u/w/a.c", ""},
            {"-", "--tag-relative .\\Users\\u\\w\\a.c",
                  0, "a=Users/u/w/a.c", ""},
            {"-", "--tag-relative=always C:\\Users\\u\\w\\a.c",
                  0, "a=Users/u/w/a.c", ""},
            {"Users\\u\\w\\sub\\tags", "--tag-relative=always C:/Users/u/w/a.c",
                                       0, "a=../a.c", ""},
        };
        runtrel(t, os, wcases, countof(wcases));
    }
    {
        Os *os = newos(&a);
        os->windows = 1;
        os->cwd     = "C:\\Users\\u\\w";
        vfsfile(os, "a.c",          "int a;\n");
        vfsfile(os, "sub/b.c",      "int b;\n");
        vfsfile(os, "sub/deep/d.c", "int d;\n");
        vfsfile(os, "other/o.c",    "int o;\n");
        Str abs = "a=C:/Users/u/w/a.c b=C:/Users/u/w/sub/b.c "
                  "d=C:/Users/u/w/sub/deep/d.c o=C:/Users/u/w/other/o.c";
        TrelCase cases[] = {
            {"sub\\tags",                  "--tag-relative -R",        0, insub,   ""},
            {"sub\\deep\\tags",            "--tag-relative=always -R", 0, indeep,  ""},
            {"C:\\Users\\u\\w\\sub\\tags", "--tag-relative -R",        0, insub,   ""},
            {"c:/USERS/U/W/OTHER/tags",    "--tag-relative -R",        0, inother, ""},
            {"-",                          "--tag-relative=never -R",  0, abs,     ""},
            {"..\\tags",                   "--tag-relative -R",        0, above,   ""},
            {"sub/tags", "--tag-relative .\\a.c sub\\b.c .\\sub\\deep\\d.c",
                         0, "a=../a.c b=b.c d=deep/d.c", ""},

            // Absolute, with either separator, a drive in either case, or
            // none, compared with the base regardless of case
            {"sub/tags", "--tag-relative C:\\Users\\u\\w\\a.c "
                         "c:/users/U/W/sub/b.c",
                         0, "a=C:/Users/u/w/a.c b=c:/users/U/W/sub/b.c", ""},
            {"sub/tags", "--tag-relative=always C:\\Users\\u\\w\\a.c "
                         "c:/users/U/W/sub/b.c", 0, "a=../a.c b=b.c", ""},
            {"sub/tags", "--tag-relative=never C:\\Users\\u\\w\\a.c "
                         "c:/users/U/W/sub/b.c",
                         0, "a=C:/Users/u/w/a.c b=C:/users/U/W/sub/b.c", ""},
            {"sub/tags", "--tag-relative \\Users\\u\\w\\a.c",
                         0, "a=/Users/u/w/a.c", ""},
            {"sub/tags", "--tag-relative=always /Users/u/w/a.c",
                         0, "a=../a.c", ""},
            {"-",        "--tag-relative=never \\Users\\u\\w\\a.c",
                         0, "a=C:/Users/u/w/a.c", ""},
        };
        runtrel(t, os, cases, countof(cases));

        // e-ctags writes a path made relative or absolute with '/', and
        // only one written as constructed keeps its '\' (FLD-6, OPT-18)
        struct { Str args, out; } ectags[] = {
            {"--tag-relative=always sub\\deep\\d.c",
             "d\tsub/deep/d.c\t1;\"\tv\tline:1\ttyperef:typename:int\n"},
            {"--tag-relative=never sub\\deep\\d.c",
             "d\tC:/Users/u/w/sub/deep/d.c\t1;\"\tv\tline:1\ttyperef:typename:int\n"},
            {"--tag-relative C:\\Users\\u\\w\\sub\\b.c",
             "b\tC:\\Users\\u\\w\\sub\\b.c\t1;\"\tv\tline:1\ttyperef:typename:int\n"},
            {"sub\\deep\\d.c",
             "d\tsub\\deep\\d.c\t1;\"\tv\tline:1\ttyperef:typename:int\n"},
        };
        for (iz i = 0; i < countof(ectags); i++) {
            Str args = concat(&a, "-f - --output-format=e-ctags -n ",
                              ectags[i].args);
            Run r    = run(os, a, args);
            expect(t, args, r.err, "");
            expect(t, args, r.out, ectags[i].out);
        }

        // D15: ".." at a drive's root stays there, where Universal Ctags
        // takes the drive's ':' with it
        TrelQuirk d15[] = {
            {"-",        "--tag-relative=never C:\\..\\Users\\u\\w\\a.c",
                         "a=C:/Users/u/w/a.c", "a=C/Users/u/w/a.c", 1},
            {"-",        "--tag-relative=never /../../Users/u/w/a.c",
                         "a=C:/Users/u/w/a.c", "a=C/Users/u/w/a.c", 1},
            {"sub/tags", "--tag-relative=always C:/../Users/u/w/a.c",
                         "a=../a.c",           "a=C/Users/u/w/a.c", 1},
            {"-",        "--tag-relative ..\\..\\..\\..\\Users\\u\\w\\a.c",
                         "a=a.c",              "a=C/Users/u/w/a.c", 1},
        };
        runquirks(t, os, d15, countof(d15));

        // The current directory's drive letter in lowercase, as cmd.exe
        // may have it, is uppercased in absolute paths
        os->cwd = "c:\\users\\u\\w";
        TrelCase lower[] = {
            {"-", "--tag-relative=never a.c sub\\b.c",
                  0, "a=C:/users/u/w/a.c b=C:/users/u/w/sub/b.c", ""},
            {"-", "--tag-relative=always C:\\Users\\u\\w\\a.c sub\\b.c",
                  0, "a=a.c b=sub/b.c", ""},
        };
        runtrel(t, os, lower, countof(lower));
    }
    {
        // Paths on another drive, and on a drive but not from its root
        Os *os = newos(&a);
        os->windows = 1;
        os->cwd     = "C:\\Users\\u\\w";
        vfsfile(os, "a.c",     "int a;\n");
        vfsfile(os, "C:x.c",   "int x;\n");
        vfsfile(os, "D:\\y.c", "int y;\n");
        Str drive  = "ctags: C:x.c: relative file names with drive letters "
                     "not supported\n";
        Str nosuch = "ctags: Warning: cannot open input file \"C:nosuch.c\" "
                     ": No such file or directory\n";
        Str subdir = "ctags: C:sub/: relative file names with drive letters "
                     "not supported\n";
        TrelCase cases[] = {
            {"-",          "--tag-relative a.c D:\\y.c",       0, "a=a.c y=D:/y.c", ""},
            {"sub/tags",   "--tag-relative=always a.c D:/y.c", 0, "a=../a.c y=D:/y.c", ""},
            {"sub/tags",   "--tag-relative=never D:\\y.c",     0, "y=D:/y.c", ""},
            {"-",          "C:x.c",                            0, "x=C:x.c", ""},
            {"-",          "--tag-relative C:x.c a.c",         1, "", drive},
            {"-",          "--tag-relative=always a.c C:x.c",  1, "", drive},
            {"tags",       "--tag-relative=never C:x.c",       1, "", drive},
            // No tags file, where Universal Ctags leaves a's tags in one
            {"tags",       "--tag-relative a.c C:x.c",         1, "", drive},
            {"-",          "--tag-relative C:nosuch.c a.c",    0, "a=a.c", nosuch},
            {"C:sub/tags", "a.c",                              0, "a=a.c", ""},
            {"C:sub/tags", "--tag-relative a.c",               1, "", subdir},
            {"C:tags",     "--tag-relative a.c",               0, "a=/Users/u/w/a.c", ""},
        };
        runtrel(t, os, cases, countof(cases));
    }
    {
        // D16: a UNC path is rooted at its share, as a path at its drive,
        // where Universal Ctags puts the current drive before it, or
        // before a path rooted without a drive, from a share, "@:"
        Os *os = newos(&a);
        os->windows = 1;
        os->cwd     = "C:\\Users\\u\\w";
        vfsfile(os, "//host/share/x/a.c", "int a;\n");
        Str unc = "a=//host/share/x/a.c";
        TrelQuirk d16[] = {
            {"-",        "--tag-relative=never \\\\host\\share\\x\\a.c",
                         unc, "a=C://host/share/x/a.c", 1},
            {"sub/tags", "--tag-relative=always //host/share/x/a.c",
                         unc, "a=../../../..//host/share/x/a.c", 1},
            {"-",        "--tag-relative=never //host/share/../x/a.c",
                         unc, "a=C://host/x/a.c", 1},
            {"-",        "--tag-relative //host/share/x/a.c", unc, unc, 0},
        };
        runquirks(t, os, d16, countof(d16));

        os->cwd = "\\\\host\\share\\w";
        vfsfile(os, "a.c",          "int a;\n");
        vfsfile(os, "sub/b.c",      "int b;\n");
        vfsfile(os, "\\x\\c.c",     "int c;\n");
        vfsfile(os, "//host/other/d.c", "int d;\n");
        Str share = "a=//host/share/w/a.c b=//host/share/w/sub/b.c";
        TrelQuirk cwd16[] = {
            // The current directory's own paths, as Universal Ctags has them
            {"-",        "--tag-relative=never a.c sub\\b.c", share, share, 0},
            {"sub/tags", "--tag-relative a.c sub\\b.c",
                         "a=../a.c b=b.c",              "a=../a.c b=b.c", 0},
            {"-",        "--tag-relative=never ..\\..\\w\\a.c",
                         "a=//host/share/w/a.c",        "a=//host/w/a.c", 1},
            {"-",        "--tag-relative=never \\x\\c.c",
                         "c=//host/share/x/c.c",        "c=@:/x/c.c", 1},
            {"-",        "--tag-relative=always \\\\HOST\\Share\\w\\sub\\b.c",
                         "b=sub/b.c",                   "b=@://HOST/Share/w/sub/b.c", 1},
            {"sub/tags", "--tag-relative=always //host/other/d.c",
                         "d=//host/other/d.c",          "d=@://host/other/d.c", 1},
        };
        runquirks(t, os, cwd16, countof(cwd16));
    }
}

// Command line: options and the --kinds-LANG grammar

// Enabled kind letters, e.g. "C:defghmstuv C++:defghmstuvcnMP F:...".
static Str showkinds(Arena *a, Kinds *k)
{
    Buf b(a, 64);
    i32 langs[] = {LANG_C, LANG_CXX, LANG_FORTRAN};
    Str names[] = {"C:", " C++:", " F:"};
    for (i32 i = 0; i < countof(langs); i++) {
        print(&b, names[i]);
        KindTable kt = kindtable(k, langs[i]);
        for (i32 j = 0; j < kt.len; j++) {
            if (kindon(k, langs[i], j)) {
                putbyte(&b, kt.kinds[j].letter);
            }
        }
    }
    return finish(&b);
}

// Enabled extras as letters, after "{anonymous}" if it is enabled.
static Str showextras(Arena *a, u32 bits)
{
    Buf b(a, 32);
    for (i32 i = 0; i < countof(extras); i++) {
        if (bits>>i & 1) {
            if (extras[i].letter) {
                putbyte(&b, extras[i].letter);
            } else {
                putbyte(&b, '{');
                print(&b, extras[i].name);
                putbyte(&b, '}');
            }
        }
    }
    return finish(&b);
}

struct Parsed {
    Options opt;
    Str     log;
    i32     status;
};

// Parse a command line, split by words(), on the given file system.
static Parsed parseos(Arena *a, Os *os, Str cmd)
{
    Parsed     r    = {};
    Slice<Str> args = words(a, cmd);
    Buf        log(a, 256);
    r.status = parseargs(&r.opt, os, args.data, (i32)args.len, a, &log);
    r.log    = finish(&log);
    return r;
}

static Parsed parsecmd(Arena *a, Str cmd)
{
    return parseos(a, newos(a), cmd);
}

static Str joinlines(Arena *a, Slice<Str> lines)
{
    Buf b(a, 64);
    for (iz i = 0; i < lines.len; i++) {
        print(&b, lines[i]);
        putbyte(&b, '|');
    }
    return finish(&b);
}

static void test_cli(Test *t)
{
    Arena a = t->perm;

    struct { Str text; u32 flags; Str want; } lines[] = {
        {"",                    0,                         ""},
        {"a\nb",                0,                         "a|b|"},
        {"a\r\nb\r\n",          0,                         "a|b|"},
        {"a\rb\r\r\nc",         0,                         "a|b||c|"},
        {"a\n\n b \n",          0,                         "a|| b |"},
        {"a\n\n b \n",          LINES_TRIM,                "a|b|"},
        {" \t\n\v\f\r",         LINES_TRIM,                ""},
        {"# x\n  #y\na#\n",     LINES_COMMENTS,            "a#|"},
        {" # x\r\r--a  \r",     LINES_TRIM|LINES_COMMENTS, "--a|"},
    };
    for (iz i = 0; i < countof(lines); i++) {
        Arena tmp = a;
        Str   got = joinlines(&tmp, splitlines(&tmp, lines[i].text, lines[i].flags));
        expect(t, lines[i].text, got, lines[i].want);
    }

    struct { Str args, kinds, log; } kinds[] = {
        {"x.c", "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-C=+p x.c",
         "C:defghmpstuv C++:defghmpstuvcnMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-C++=p x.c", "C:p C++:p F:bceEfiklmMnNpstvS", ""},
        {"--kinds-C=f x.c", "C:f C++:fcnMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-c=-d+x x.c",
         "C:efghmstuvx C++:efghmstuvxcnMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-C= x.c", "C: C++:cnMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-C=* x.c",
         "C:defghlmpstuvxzLD C++:defghlmpstuvxzLDcnMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-C=*-l x.c",
         "C:defghmpstuvxzLD C++:defghmpstuvxzLDcnMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-C++=+{prototype}-{macro} x.c",
         "C:efghmpstuv C++:efghmpstuvcnMP F:bceEfiklmMnNpstvS", ""},
        {"--C++-kinds=+p x.c",
         "C:defghmpstuv C++:defghmpstuvcnMP F:bceEfiklmMnNpstvS", ""},
        {"--c++-types=+A x.c",
         "C:defghmstuv C++:defghmstuvcnAMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-fortran=+L-l x.f",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfikLmMnNpstvS", ""},
        {"--kinds-C=+Q x.c",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS",
         "ctags: Warning: Unsupported kind: 'Q' for --kinds-C option\n"},
        {"--kinds-C=+{Prototype} x.c",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS",
         "ctags: Warning: Unsupported kind: 'Prototype' for --kinds-C option\n"},
        {"--kinds-cpp=+p x.c",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS",
         "ctags: Warning: Unknown language \"cpp\" in \"kinds-cpp\" option\n"},
    };
    for (iz i = 0; i < countof(kinds); i++) {
        Arena  tmp = a;
        Parsed r   = parsecmd(&tmp, kinds[i].args);
        expect(t, kinds[i].args, num(&tmp, r.status), num(&tmp, ARGS_OK));
        expect(t, kinds[i].args, showkinds(&tmp, &r.opt.kinds), kinds[i].kinds);
        expect(t, kinds[i].args, r.log, kinds[i].log);
    }

    // As Universal Ctags 6.2.1 parses them
    struct { Str args, extras, log; } xs[] = {
        {"x.c",                    "{anonymous}Fps", ""},
        {"--extras=+q x.c",        "{anonymous}Fpqs", ""},
        {"--extras=q x.c",         "q", ""},
        {"--extras= x.c",          "", ""},
        {"--extras=* x.c",         "{anonymous}Ffgpqrsz", ""},
        {"--extras=*-p x.c",       "{anonymous}Ffgqrsz", ""},
        {"--extras=-F+q x.c",      "{anonymous}pqs", ""},
        {"--extras=+{qualified}-{anonymous} x.c", "Fpqs", ""},
        {"--extras=+{qual x.c",    "{anonymous}Fps", ""},
        {"--extras=+q --extras=-s x.c", "{anonymous}Fpq", ""},
        {"--extra=+q x.c",         "{anonymous}Fpqs",
         "ctags: Warning: --extra option is obsolete; use --extras instead\n"},
        {"--extras=+Q x.c",        "{anonymous}Fps",
         "ctags: Warning: Unsupported parameter 'Q' for \"extras\" option\n"},
        {"--extra=+Qq x.c",        "{anonymous}Fpqs",
         "ctags: Warning: --extra option is obsolete; use --extras instead\n"
         "ctags: Warning: Unsupported parameter 'Q' for \"extra\" option\n"},
        {"--extras=+{bogus} x.c",  "{anonymous}Fps",
         "ctags: Warning: Unsupported parameter '{bogus}' for \"extras\" option\n"},
        {"--file-scope=no x.c",    "{anonymous}ps",
         "ctags: Warning: \"--file-scope\" option is obsolete; "
         "use \"--extras=-F\" instead\n"},
        {"--extras=-F --file-scope x.c", "{anonymous}Fps",
         "ctags: Warning: \"--file-scope\" option is obsolete; "
         "use \"--extras=+F\" instead\n"},
    };
    for (iz i = 0; i < countof(xs); i++) {
        Arena  tmp = a;
        Parsed r   = parsecmd(&tmp, xs[i].args);
        expect(t, xs[i].args, num(&tmp, r.status), num(&tmp, ARGS_OK));
        expect(t, xs[i].args, showextras(&tmp, r.opt.extras), xs[i].extras);
        expect(t, xs[i].args, r.log, xs[i].log);
    }

    struct { Str args; i32 status; Str output, files, log; } opts[] = {
        {"-R",               ARGS_OK, "tags", ".", ""},
        {"-Rf- x.c",         ARGS_OK, "-", "x.c", ""},
        {"-R -f out.tags",   ARGS_OK, "out.tags", ".", ""},
        {"-o out a.c b.f",   ARGS_OK, "out", "a.c b.f", ""},
        {"-f /dev/stdout a", ARGS_OK, "-", "a", ""},
        {"--recurse=yes",    ARGS_OK, "tags", ".", ""},
        {"--recurse=OFF a",  ARGS_OK, "tags", "a", ""},
        {"--jobs=4 a",       ARGS_OK, "tags", "a", ""},
        {"-f a -f b x",      ARGS_OK, "b", "x",
         "ctags: Warning: -f option specified more than once, last value used\n"},
        {"",                 ARGS_FATAL, "", "",
         "ctags: No files specified. Try \"ctags --help\".\n"},
        {"--recurse=maybe",  ARGS_FATAL, "", "",
         "ctags: Invalid value for \"recurse\" option\n"},
        {"--jobs=x a",       ARGS_FATAL, "", "",
         "ctags: Invalid value for \"jobs\" option\n"},
        {"-x a",             ARGS_FATAL, "", "", "ctags: Unknown option: -x\n"},
        {"--bogus a",        ARGS_FATAL, "", "",
         "ctags: Unknown option: --bogus\n"},
        {"-f -x a",          ARGS_FATAL, "", "",
         "ctags: output file name may not begin with a '-'\n"},
        {"a -f",             ARGS_FATAL, "", "",
         "ctags: Missing parameter for \"f\" option\n"},
        {"-f a -o b x",      ARGS_OK, "b", "x",
         "ctags: Warning: -o option specified more than once, last value used\n"},
        {"-f a -f -x x",     ARGS_OK, "-x", "x",
         "ctags: Warning: -f option specified more than once, last value used\n"},
        {"- a",              ARGS_FATAL, "", "", "ctags: Unknown option: -\n"},
        {"-- a",             ARGS_FATAL, "", "", "ctags: Unknown option: --\n"},
        {"--=x a",           ARGS_FATAL, "", "", "ctags: Unknown option: --\n"},
        {"--foo-bar-kinds=x a", ARGS_FATAL, "", "",
         "ctags: Unknown option: --foo-bar-kinds\n"},
        {"--kinds-C={x a",   ARGS_FATAL, "", "",
         "ctags: unexpected character in kind specification\n"},
        {"--extras=+} a",    ARGS_FATAL, "", "",
         "ctags: unexpected character in extra specification: '}'\n"},
        {"--extras=+{a{b} a", ARGS_FATAL, "", "",
         "ctags: unexpected character in extra specification: '{'\n"},
        {"--file-scope=maybe a", ARGS_FATAL, "", "",
         "ctags: Invalid value for \"file-scope\" option\n"},
    };
    for (iz i = 0; i < countof(opts); i++) {
        Arena  tmp = a;
        Parsed r   = parsecmd(&tmp, opts[i].args);
        expect(t, opts[i].args, num(&tmp, r.status), num(&tmp, opts[i].status));
        expect(t, opts[i].args, r.log, opts[i].log);
        if (r.status == ARGS_OK) {
            Buf files(&tmp, 64);
            for (iz j = 0; j < r.opt.files.len; j++) {
                if (j) putbyte(&files, ' ');
                print(&files, r.opt.files[j]);
            }
            expect(t, opts[i].args, r.opt.output, opts[i].output);
            expect(t, opts[i].args, finish(&files), opts[i].files);
        }
    }
}

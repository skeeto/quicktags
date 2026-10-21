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

// Enabled fields: common letters, then the parsers', e.g.
// "NFPTfknst C++:name".
static Str showfields(Arena *a, u64 bits)
{
    Buf b(a, 64);
    for (i32 i = 0; i < NFIELDS; i++) {
        if (fieldon(bits, i)) {
            if (i < NCOMMONFIELDS) {
                putbyte(&b, fields[i].letter);
            } else {
                print(&b, i<FIELD_CXX ? Str(" C:") : Str(" C++:"));
                print(&b, fields[i].name);
            }
        }
    }
    return finish(&b);
}

// The -I macro for a name as a token would give it, "NAME[+][=TEXT]",
// or "" for none.
static Str showmacro(Arena *a, Macros *t, Str name)
{
    Map<Macro> *map = t->map;
    Macro      *m   = map ? upsert(&map, name, (Arena *)0) : 0;
    if (!m) {
        return "";
    }
    Buf b(a, 32);
    print(&b, m->name);
    print(&b, m->rule.parens ? Str("+") : Str(""));
    if (m->rule.repl.data) {
        putbyte(&b, '=');
        print(&b, m->rule.repl);
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
        {"--kinds-C={x x.c", "C: C++:cnMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-C=+p{x x.c",
         "C:defghmpstuv C++:defghmpstuvcnMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-C=+{} x.c",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS",
         "ctags: Warning: Unsupported kind: '' for --kinds-C option\n"},
        {"--kinds-C=F x.c", "C: C++:cnMP F:bceEfiklmMnNpstvS", ""},
        {"--kinds-Fortran=+F-{file} x.c",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS", ""},
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
        {"--extras=* --kinds-C=-F x.c",   "{anonymous}Ffgpqrsz", ""},
        {"--extras=*-p --kinds-C=-F x.c", "{anonymous}Ffgqrsz", ""},
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
        {"--extras=+{} x.c",       "{anonymous}Fps",
         "ctags: Warning: Unsupported parameter '{}' for \"extras\" option\n"},
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

    Str notyet = "ctags: --fields: not yet supported\n";
    Str fixedN = "ctags: Warning: Cannot disable fixed field: 'N'{name} in "
                 "ctags output mode\n";
    struct { Str args; i32 status; Str fields, log; } fs[] = {
        {"x.c",                ARGS_OK, "NFPTfknst C++:name", ""},
        {"--fields=+n x.c",    ARGS_OK, "NFPTfknst C++:name", ""},
        {"--fields=-N x.c",    ARGS_OK, "NFPTfknst C++:name", fixedN},
        {"--fields=-NFP x.c",  ARGS_OK, "NFPTfknst C++:name",
         "ctags: Warning: Cannot disable fixed field: 'N'{name} in "
         "ctags output mode\n"
         "ctags: Warning: Cannot disable fixed field: 'F'{input} in "
         "ctags output mode\n"
         "ctags: Warning: Cannot disable fixed field: 'P'{pattern} in "
         "ctags output mode\n"},
        {"--fields=-N --fields=+N x.c", ARGS_OK, "NFPTfknst C++:name", ""},
        {"--fields=k x.c",     ARGS_FATAL, "NFPk C++:name", notyet},
        {"--fields=k --fields=-N x.c", ARGS_FATAL, "NFPk C++:name", notyet},
        {"--fields=+z-k-K x.c", ARGS_FATAL, "NFPKTfnstz C++:name",
         "ctags: Warning: though z/kind field is enabled, neither k nor K "
         "field is not enabled\n"
         "ctags: Warning: enable the K field to make the z/kind field "
         "printable\n"
         "ctags: --fields: not yet supported\n"},
        {"--fields=+Z-s x.c",  ARGS_FATAL, "NFPTZfknst C++:name",
         "ctags: Warning: though Z/scope field is enabled, s field is not "
         "enabled\n"
         "ctags: Warning: enable the s field to make the Z/scope field "
         "printable\n"
         "ctags: --fields: not yet supported\n"},
        {"--fields=+Q x.c",    ARGS_OK, "NFPTfknst C++:name",
         "ctags: Warning: Unsupported parameter 'Q' for \"fields\" option\n"},
        {"--fields=+{name x.c", ARGS_OK, "NFPTfknst C++:name", ""},
        {"--fields=-{line}+{line} x.c", ARGS_OK, "NFPTfknst C++:name", ""},
        {"--fields=+{line}{kind}{scope} x.c", ARGS_FATAL,
         "NFPTZfknstz C++:name", notyet},
        {"--fields=* x.c",     ARGS_FATAL,
         "NFPCEKRSTZaefiklmnoprstxz C++:name", notyet},
        {"--fields=+{bogus} x.c", ARGS_FATAL, "NFPTfknst C++:name",
         "ctags: no such field: 'bogus'\n"},
        {"--fields=+{K} x.c",  ARGS_FATAL, "NFPTfknst C++:name",
         "ctags: no such field: 'K'\n"},
        {"--fields=+{} x.c",   ARGS_FATAL, "NFPTfknst C++:name",
         "ctags: no such field: ''\n"},
        {"--fields=+} x.c",    ARGS_FATAL, "NFPTfknst C++:name",
         "ctags: unexpected character in field specification: '}'\n"},
        {"--fields=+{a{b} x.c", ARGS_FATAL, "NFPTfknst C++:name",
         "ctags: unexpected character in field specification: '{'\n"},
    };
    for (iz i = 0; i < countof(fs); i++) {
        Arena  tmp = a;
        Parsed r   = parsecmd(&tmp, fs[i].args);
        expect(t, fs[i].args, num(&tmp, r.status), num(&tmp, fs[i].status));
        expect(t, fs[i].args, showfields(&tmp, r.opt.fields), fs[i].fields);
        expect(t, fs[i].args, r.log, fs[i].log);
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
        {"-q a",             ARGS_FATAL, "", "", "ctags: Unknown option: -q\n"},
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
        {"--kinds-C=+} a",   ARGS_FATAL, "", "",
         "ctags: unexpected character in kind specification: '}'\n"},
        {"--kinds-C=+{a{b} a", ARGS_FATAL, "", "",
         "ctags: unexpected character in kind specification: '{'\n"},
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

    // Every option's grammar, with the messages of Universal Ctags 6.2.1,
    // ours for what it accepts and this program refuses, and "not yet
    // supported" for what this program will do
    i32 OK    = ARGS_OK;
    i32 FATAL = ARGS_FATAL;
    struct { Str args; i32 status; Str log; } gs[] = {
        // Short options
        {"-x a",          FATAL, "ctags: -x: not supported\n"},
        {"-B a",          FATAL, "ctags: -B: not supported\n"},
        {"-D X=1 a",      FATAL, "ctags: -D: not supported\n"},
        {"a -D",          FATAL, "ctags: Missing parameter for \"D\" option\n"},
        {"-i x a",        FATAL, "ctags: Unknown option: -i\n"},
        {"-ix a",         FATAL, "ctags: Unknown option: -i\n"},
        {"-p x a",        FATAL, "ctags: Unknown option: -p\n"},
        {"-d 1 a",        FATAL, "ctags: Unknown option: -d\n"},
        {"-b 1 a",        FATAL, "ctags: Unknown option: -b\n"},
        {"-V -F -VF a",   OK,    ""},
        {"-RV",           OK,    ""},
        {"-w -wR",        OK,    ""},
        {"-G a",          FATAL, "ctags: -G: not supported\n"},
        {"-RG",           FATAL, "ctags: -G: not supported\n"},
        {"--guess-language-eagerly a", FATAL,
         "ctags: --guess-language-eagerly: not supported\n"},
        {"--guess-language-eagerly=no a", OK, ""},
        {"--guess-language-eagerly=x a", FATAL,
         "ctags: Invalid value for \"guess-language-eagerly\" option\n"},
        {"-a a",          FATAL, "ctags: --append: not yet supported\n"},
        {"-a -f - a",     FATAL, "ctags: append mode is not compatible with "
                                 "tags to stdout\n"},
        {"-e a",          FATAL, "ctags: -e: not yet supported\n"},
        {"-n a",          FATAL, "ctags: --excmd: not yet supported\n"},
        {"-N a",          FATAL, "ctags: --excmd: not yet supported\n"},
        {"-u a",          FATAL, "ctags: --sort: not yet supported\n"},
        {"-L x",          FATAL, "ctags: -L: not yet supported\n"},
        {"-L x -L y",     FATAL, "ctags: Warning: -L option specified more "
                                 "than once, last value used\n"
                                 "ctags: -L: not yet supported\n"},
        {"-I X a",        OK,    ""},
        {"-IX,Y+=Z -I - a", OK,  ""},
        {"-I @nosuch a",  FATAL, "ctags: cannot open \"nosuch\" : No such "
                                 "file or directory\n"},
        {"-I ./nosuch a", FATAL, "ctags: cannot open \"./nosuch\" : No such "
                                 "file or directory\n"},
        {"-I /nosuch a",  FATAL, "ctags: cannot open \"/nosuch\" : No such "
                                 "file or directory\n"},
        {"-I \\nosuch a", FATAL, "ctags: cannot open \"\\nosuch\" : No such "
                                 "file or directory\n"},
        {"-I @ a",        FATAL, "ctags: cannot open \"\" : No such file or "
                                 "directory\n"},
        {"-I C:x a",      OK,    ""},  // a file only on Windows
        {"-h default a",  OK,    ""},
        {"-h .x a",       FATAL, "ctags: -h: not yet supported\n"},

        // Values
        {"--excmd=mixed a",     OK,    ""},
        {"--excmd=m a",         OK,    ""},
        {"--excmd=numberwang a", FATAL, "ctags: --excmd: not yet supported\n"},
        {"--excmd=combine a",   FATAL, "ctags: --excmd: not yet supported\n"},
        {"--excmd=x a",         FATAL, "ctags: Invalid value for \"excmd\" "
                                       "option: x\n"},
        {"--excmd= a",          FATAL, "ctags: Invalid value for \"excmd\" "
                                       "option: \n"},
        {"--excmd=combinex a",  FATAL, "ctags: Invalid value for \"excmd\" "
                                       "option: combinex\n"},
        {"--sort=yes a",        OK,    ""},
        {"--sort=1 a",          OK,    ""},
        {"--sort=no a",         FATAL, "ctags: --sort: not yet supported\n"},
        {"--sort=FOLD a",       FATAL, "ctags: --sort: not yet supported\n"},
        {"--sort a",            FATAL, "ctags: Invalid value for \"sort\" "
                                       "option\n"},
        {"--sort=x a",          FATAL, "ctags: Invalid value for \"sort\" "
                                       "option\n"},
        {"--format=2 a",        OK,    ""},
        {"--format=+2 a",       OK,    ""},
        {"--format=1 a",        FATAL, "ctags: --format: not yet supported\n"},
        {"--format=1x a",       FATAL, "ctags: --format: not yet supported\n"},
        {"--format=0 a",        FATAL, "ctags: --format: not yet supported\n"},
        {"--format=3 a",        FATAL, "ctags: Unsupported value for "
                                       "\"format\" option\n"},
        {"--format=-1 a",       FATAL, "ctags: Unsupported value for "
                                       "\"format\" option\n"},
        {"--format=99999999999 a", FATAL, "ctags: Unsupported value for "
                                          "\"format\" option\n"},
        {"--format=x a",        FATAL, "ctags: Invalid value for \"format\" "
                                       "option\n"},
        {"--format= a",         FATAL, "ctags: Invalid value for \"format\" "
                                       "option\n"},
        {"--pattern-length-limit=96 a",   OK, ""},
        {"--pattern-length-limit=0x60 a", OK, ""},
        {"--pattern-length-limit=0140 a", OK, ""},
        {"--pattern-length-limit=+96 a",  OK, ""},
        {"--pattern-length-limit=0 a",    FATAL,
         "ctags: --pattern-length-limit: not yet supported\n"},
        {"--pattern-length-limit=-1 a",   FATAL,
         "ctags: --pattern-length-limit: not yet supported\n"},
        {"--pattern-length-limit= a",     FATAL,
         "ctags: A parameter is needed after \"pattern-length-limit\" "
         "option\n"},
        {"--pattern-length-limit=x a",    FATAL,
         "ctags: -pattern-length-limit: Invalid pattern length limit\n"},
        {"--pattern-length-limit=10x a",  FATAL,
         "ctags: -pattern-length-limit: Invalid pattern length limit\n"},
        {"--pattern-length-limit=08 a",   FATAL,
         "ctags: -pattern-length-limit: Invalid pattern length limit\n"},
        {"--pattern-length-limit=99999999999 a", FATAL,
         "ctags: -pattern-length-limit: Invalid pattern length limit\n"},
        {"--output-format=u-ctags a", OK, ""},
        {"-e --output-format=u-ctags a", FATAL,
         "ctags: -e: not yet supported\n"},
        {"--output-format=e-ctags --output-format=u-ctags a", FATAL,
         "ctags: --output-format=e-ctags: not yet supported\n"},
        {"--output-format=e-ctags a", FATAL,
         "ctags: --output-format=e-ctags: not yet supported\n"},
        {"--output-format=etags a",   FATAL, "ctags: -e: not yet supported\n"},
        {"--output-format=xref a",    FATAL,
         "ctags: --output-format=xref: not supported\n"},
        {"--output-format=json a",    FATAL,
         "ctags: the output format \"json\" is not available on this "
         "platform\n"},
        {"--output-format= a",        FATAL,
         "ctags: no output format name supplied for \"output-format\"\n"},
        {"--output-format=bogus a",   FATAL,
         "ctags: unknown output format name supplied for "
         "\"output-format=bogus\"\n"},
        {"--tag-relative=no a",     OK,    ""},
        {"--tag-relative=NEVER a",  FATAL,
         "ctags: --tag-relative: not yet supported\n"},
        {"--tag-relative a",        FATAL,
         "ctags: --tag-relative: not yet supported\n"},
        {"--tag-relative=x a",      FATAL,
         "ctags: Invalid value for \"tag-relative\" option\n"},
        {"--totals --totals=extra --totals=no a", OK, ""},
        {"--totals=x a",       FATAL, "ctags: Invalid value for \"totals\" "
                                      "option\n"},
        {"--quiet --verbose=no --recurse=no a", OK, ""},
        {"--quiet=x a",        FATAL, "ctags: Invalid value for \"quiet\" "
                                      "option\n"},
        {"--append=no a",      OK,    ""},
        {"--append a",         FATAL, "ctags: --append: not yet supported\n"},
        {"--append=x a",       FATAL, "ctags: Invalid value for \"append\" "
                                      "option\n"},
        {"--links a",          OK,    ""},
        {"--links=no a",       FATAL, "ctags: --links: not yet supported\n"},
        {"--if0=no a",         OK,    ""},
        {"--if0 a",            FATAL, "ctags: --if0: not supported\n"},
        {"--if0=x a",          FATAL, "ctags: Invalid value for \"if0\" "
                                      "option\n"},
        {"--line-directives=no a", OK, ""},
        {"--line-directives a", FATAL, "ctags: --line-directives: not "
                                       "supported\n"},
        {"--exclude=x a",      FATAL, "ctags: --exclude: not yet supported\n"},
        {"--exclude= a",       FATAL, "ctags: --exclude: not yet supported\n"},
        {"--exclude-exception=x a", FATAL,
         "ctags: --exclude-exception: not yet supported\n"},
        {"--file-tags a",      FATAL, "ctags: Warning: \"--file-tags\" option "
                                      "is obsolete; use \"--extras=+f\" "
                                      "instead\n"
                                      "ctags: --extras=+f: not yet supported\n"},
        {"--file-tags=no a",   OK,    "ctags: Warning: \"--file-tags\" option "
                                      "is obsolete; use \"--extras=-f\" "
                                      "instead\n"},
        {"--extras=+f a",      FATAL, "ctags: --extras=+f: not yet supported\n"},
        {"--extras=* a",       FATAL, "ctags: --extras=+f: not yet supported\n"},
        {"--extra=+fq a",      FATAL, "ctags: Warning: --extra option is "
                                      "obsolete; use --extras instead\n"
                                      "ctags: --extras=+f: not yet supported\n"},
        {"--extras-all=+{inputFile} a", FATAL,
         "ctags: --extras=+f: not yet supported\n"},
        {"--kinds-C=-F --extras=+f a", FATAL,
         "ctags: --extras=+f: not yet supported\n"},
        {"--extras=+f --kinds-python=-F a", OK, ""},
        {"--extras=+f --kinds-all=-F a",    OK, ""},
        {"--kinds-C=+F a",     OK,    ""},
        {"--__anything=x a",   OK,    ""},

        // Languages
        {"--languages=C,C++,Fortran a",        OK, ""},
        {"--languages=all a",                  OK, ""},
        {"--languages=C,C++,Fortran,Python a", OK, ""},
        {"--languages=python,+C,C++,fortran a", OK, ""},
        {"--languages=-Python a",              OK, ""},
        {"--languages=C,-C++ a",  FATAL, "ctags: --languages: not yet "
                                         "supported\n"},
        {"--languages= a",        FATAL, "ctags: --languages: not yet "
                                         "supported\n"},
        {"--languages=cpp a",     FATAL, "ctags: Warning: Unknown language "
                                         "\"cpp\" in \"languages\" option\n"
                                         "ctags: --languages: not yet "
                                         "supported\n"},
        {"--languages=ALL a",     FATAL, "ctags: Warning: Unknown language "
                                         "\"ALL\" in \"languages\" option\n"
                                         "ctags: --languages: not yet "
                                         "supported\n"},
        {"--language-force=auto --language-force=AUTO a", OK, ""},
        {"--language-force=C a",  FATAL, "ctags: --language-force: not yet "
                                         "supported\n"},
        {"--language-force=python a", FATAL, "ctags: --language-force: not "
                                             "yet supported\n"},
        {"--language-force=cpp a", FATAL, "ctags: Unknown language \"cpp\" "
                                          "in \"language-force\" option\n"},
        {"--language-force= a",   FATAL, "ctags: Unknown language \"\" in "
                                         "\"language-force\" option\n"},
        {"--language=auto a",     OK,    "ctags: Warning: \"--language\" "
                                         "option is obsolete; use "
                                         "\"--language-force\" instead\n"},
        {"--lang=cpp a",          FATAL, "ctags: Warning: \"--lang\" option "
                                         "is obsolete; use "
                                         "\"--language-force\" instead\n"
                                         "ctags: Unknown language \"cpp\" in "
                                         "\"lang\" option\n"},
        {"--langdef=foo --kinds-foo=+x --regex-FOO=/x/y/ a", OK, ""},

        // Subparsers of ours, which our parsers run on their files
        {"--langdef=Foo{base=C} --kinddef-Foo=y,yy,yys a", FATAL,
         "ctags: --kinddef-Foo: not supported\n"},
        {"--langdef=Foo{base=c++}{shared} --regex-foo=/x/y/ a", FATAL,
         "ctags: --regex-foo: not supported\n"},
        {"--langdef=Foo{dedicated}{base=fortran}{bidirectional} "
         "--mline-regex-Foo=/x/y/ a", FATAL,
         "ctags: --mline-regex-Foo: not supported\n"},
        {"--langdef=Foo{base=C}{dedicated} --kinddef-Foo=y,yy,yys "
         "--regex-Foo=/x/y/ a", OK, ""},
        {"--langdef=Foo{base=C} --langdef=Bar{base=Foo} --regex-Bar=/x/y/ a",
         FATAL, "ctags: --regex-Bar: not supported\n"},
        {"--langdef=Foo{base=C}{dedicated} --langdef=Bar{base=Foo} "
         "--regex-Bar=/x/y/ a", OK, ""},
        {"--langdef=Foo{base=Python} --regex-Foo=/x/y/ a",       OK, ""},
        {"--langdef=Foo{base=QtMoc} --regex-Foo=/x/y/ a",        OK, ""},
        {"--langdef=Foo{base=C}{base=Python} --regex-Foo=/x/y/ a", OK, ""},
        {"--langdef=Foo{base=Python}{base=C} --regex-Foo=/x/y/ a", FATAL,
         "ctags: --regex-Foo: not supported\n"},
        {"--langdef=Foo{version=1.0}x{base=C} --regex-Foo=/x/y/ a", FATAL,
         "ctags: --regex-Foo: not supported\n"},
        {"--langdef=Foo{{base=C}} --regex-Foo=/x/y/ a",          OK, ""},
        {"--langdef=Foo{base=C}{base=cpp} --regex-Foo=/x/y/ a",  FATAL,
         "ctags: Warning: Unknown language(cpp) is specified for \"base\" "
         "flag of --langdef option\n"
         "ctags: --regex-Foo: not supported\n"},
        {"--langdef=Foo{base=} a", OK, "ctags: Warning: No base parser "
                                       "specified for \"base\" flag of "
                                       "--langdef option\n"},
        {"--langdef=Foo{base} a",  OK, "ctags: Warning: No base parser "
                                       "specified for \"base\" flag of "
                                       "--langdef option\n"},
        {"--langdef=Foo{base=cpreprocessor} --regex-Foo=/x/y/ a", OK,
         "ctags: Warning: Because of an internal limitation, Making a sub "
         "parser based on the CPreProcessor parser is not allowed: "
         "cpreprocessor\n"},
        {"--langdef=Foo{base=}{shared} a", OK,
         "ctags: Warning: No base parser specified for \"base\" flag of "
         "--langdef option\n"
         "ctags: Warning: Ignore the direction of subparser because "
         "\"{base=}\" is not given\n"},
        {"--langdef=Foo{base=C --regex-Foo=/x/y/ a", OK,
         "ctags: Warning: long flags specifier opened with `{' is not "
         "closed `}': \"{base=C\"\n"},
        {"--langdef=Foo{base=C}{dedicated --regex-Foo=/x/y/ a", FATAL,
         "ctags: Warning: long flags specifier opened with `{' is not "
         "closed `}': \"{base=C}{dedicated\"\n"
         "ctags: --regex-Foo: not supported\n"},
        {"--langdef=a{x} --map-A=+.c a", FATAL, "ctags: --langmap: not yet "
                                                "supported\n"},
        {"--langdef=a#+ --langdef=All a", OK, ""},
        {"--langdef=foo --langdef=FOO a", FATAL, "ctags: Language \"FOO\" "
                                                 "already defined\n"},
        {"--langdef=c a",   FATAL, "ctags: Language \"c\" already defined\n"},
        {"--langdef=Vim a", FATAL, "ctags: Language \"Vim\" already defined\n"},
        {"--langdef= a",    FATAL, "ctags: No language specified for "
                                   "\"langdef\" option\n"},
        {"--langdef=all a", FATAL, "ctags: \"all\" is reserved; don't use "
                                   "it as the name for defining a new "
                                   "language\n"},
        {"--langdef=NONE a", FATAL, "ctags: \"NONE\" is reserved; don't use "
                                    "it as the name for defining a new "
                                    "language\n"},
        {"--langdef=a_b a", FATAL, "ctags: don't use `_' in a language name "
                                   "(a_b)\n"},
        {"--langdef=a'b a", FATAL, "ctags: don't use \"'\" in a language "
                                   "name (a'b)\n"},
        {"--langmap=default a",           OK, ""},
        {"--langmap= a",                  OK, ""},
        {"--langmap=python:.py a",        OK, ""},
        {"--langmap=python:+.pyx.pyi,ruby:.rb a", OK, ""},
        {"--langmap=python:+(*.py3) a", FATAL, "ctags: --langmap: not yet "
                                               "supported\n"},
        {"--langmap=c:default,C++:DEFAULT a", OK, ""},
        {"--langmap=python:.C a", FATAL, "ctags: --langmap: not yet "
                                         "supported\n"},
        {"--langmap=make:(Makefile) a", FATAL, "ctags: --langmap: not yet "
                                               "supported\n"},
        {"--langmap=c:.c a",      FATAL, "ctags: --langmap: not yet "
                                         "supported\n"},
        {"--langmap=fortran: a",  FATAL, "ctags: --langmap: not yet "
                                         "supported\n"},
        {"--langmap=c:.c --langmap=default a", OK, ""},
        {"--langmap=cpp:.x a",    OK,    "ctags: Warning: Unknown language "
                                         "\"cpp:.x\" in \"langmap\" option\n"},
        {"--langmap=python:.py,cpp:.x a", OK, "ctags: Warning: Unknown "
                                              "language \"python:.py,cpp:.x\" "
                                              "in \"langmap\" option\n"},
        {"--langmap=c a",         OK,    "ctags: Warning: Unknown language "
                                         "\"c\" in \"langmap\" option\n"},
        {"--langmap=c:x a",       FATAL, "ctags: Badly formed language map "
                                         "for C language\n"},
        {"--langmap=c:(x a",      FATAL, "ctags: Unterminated file name "
                                         "pattern for C language\n"},
        {"--langmap=python:x a",  FATAL, "ctags: Badly formed language map "
                                         "for Python language\n"},
        {"--map-python=+.pyx --map-PYTHON=-.c --map-python=-(x\\)y) a", OK, ""},
        {"--map-python=(x\\)y) a", FATAL, "ctags: --langmap: not yet "
                                          "supported\n"},
        {"--map-python=+.c a",    FATAL, "ctags: --langmap: not yet "
                                         "supported\n"},
        {"--map-C=+.x a",         FATAL, "ctags: --langmap: not yet "
                                         "supported\n"},
        {"--map-C= a",            FATAL, "ctags: no parameter is given for "
                                         "map-C\n"},
        {"--map-cpp=+.x a",       FATAL, "ctags: Unknown language \"cpp\" in "
                                         "\"map-cpp\" option\n"},
        {"--map- a",              FATAL, "ctags: Unknown option: --map-\n"},
        {"--map-C=x a",           FATAL, "ctags: Badly formed language map "
                                         "for C language\n"},
        {"--map-c=+(x a",         FATAL, "ctags: Unterminated file name "
                                         "pattern for C language\n"},
        {"--map-Python:x=+.y a",  OK,    "ctags: Warning: using `:' as a "
                                         "separator is obsolete; use `.' "
                                         "instead: --map-Python:x\n"},

        // Kinds of other languages, and every language
        {"--kinds-python=+x --Python-kinds=x --vim-types=* a", OK, ""},
        {"--kinds-python=+Q{x --kinds-C=+{ghost}-{ghost} a", OK, ""},
        {"--kinds-python=+} a", FATAL, "ctags: unexpected character in kind "
                                       "specification: '}'\n"},
        {"--kinds-python=+{a{b} a", FATAL, "ctags: unexpected character in "
                                           "kind specification: '{'\n"},
        {"--kinds-foo=+x a",  OK, "ctags: Warning: Unknown language \"foo\" "
                                  "in \"kinds-foo\" option\n"},
        {"--kinds- a",        OK, "ctags: Warning: No language given in "
                                  "\"kinds-\" option\n"},
        {"--kinds-all=p a",   FATAL, "ctags: only '*', 'F', \"{file}\" or "
                                     "their combination is acceptable as kind "
                                     "letter for --kinds-all\n"},
        {"--kinds-all={x} a", FATAL, "ctags: only '*', 'F', \"{file}\" or "
                                     "their combination is acceptable as kind "
                                     "letter for --kinds-all\n"},
        {"--kinds-all=++ a",  FATAL, "ctags: don't repeat + (nor -) in "
                                     "--kinds-all option\n"},
        {"--kinds-all=+* a",  FATAL, "ctags: don't use '*' after + (nor -) "
                                     "in --kinds-all option\n"},
        {"--kinds-all=*+ a",  FATAL, "ctags: no kind specification after + "
                                     "(or -) in --kinds-all option\n"},
        {"--all-kinds=p a",   FATAL, "ctags: Warning: \"--all-kinds\" option "
                                     "is obsolete; use \"--kinds-all\" "
                                     "instead\n"
                                     "ctags: only '*', 'F', \"{file}\" or "
                                     "their combination is acceptable as kind "
                                     "letter for --all-kinds\n"},
        {"--kinddef-C=x,xx,desc a", FATAL, "ctags: --kinddef-C: not "
                                           "supported\n"},
        {"--kinddef-python=x,xx,desc a", OK, ""},
        {"--kinddef-foo=x a",  FATAL, "ctags: Unknown language \"foo\" in "
                                      "\"kinddef-foo\" option\n"},
        {"--kinddef-=x a",     FATAL, "ctags: Unknown option: --kinddef-\n"},
        {"--regex-c++=/x/y/ a", FATAL, "ctags: --regex-c++: not supported\n"},
        {"--mline-regex-Fortran=/x/y/ a", FATAL, "ctags: "
                                                 "--mline-regex-Fortran: not "
                                                 "supported\n"},
        {"--regex-python=/x/y/ a", OK, ""},
        {"--regex-foo=/x/y/ a", FATAL, "ctags: Unknown language \"foo\" in "
                                       "\"regex-foo\" option\n"},

        // Parameters
        {"--param-C.x=1 a",    FATAL, "ctags: no such parameter in C: x\n"},
        {"--param-c++.x=1 a",  FATAL, "ctags: no such parameter in C++: x\n"},
        {"--param-C:x=1 a",    FATAL, "ctags: Warning: using `:' as a "
                                      "separator is obsolete; use `.' "
                                      "instead: --param-C:x\n"
                                      "ctags: no such parameter in C: x\n"},
        {"--param-C=1 a",      FATAL, "ctags: no separator(.) is given for "
                                      "param-C=1\n"},
        {"--param-foo.x=1 a",  FATAL, "ctags: Unknown language \"foo\" in "
                                      "\"param-foo.x\" option\n"},
        {"--param-python.x=1 a", OK, ""},
        {"--param-CPreProcessor._expand=no --param-cpreprocessor.if0=0 a",
         OK, ""},
        {"--param-CPreProcessor._expand=1 a", FATAL,
         "ctags: --param-CPreProcessor._expand: not supported\n"},
        {"--param-cpreprocessor.if0=yes a", FATAL,
         "ctags: --param-cpreprocessor.if0: not supported\n"},
        {"--param-CPreProcessor.if0=x a", FATAL,
         "ctags: Invalid value for \"if0\" parameter\n"},
        {"--param-CPreProcessor.if0= a", FATAL,
         "ctags: no value is given for param-CPreProcessor.if0\n"},
        {"--param-CPreProcessor.define=X a", FATAL,
         "ctags: --param-CPreProcessor.define: not supported\n"},
        {"--param-CPreProcessor.ignore=X a", OK, ""},
        {"--param-CPreProcessor.ignore= a", FATAL,
         "ctags: no value is given for param-CPreProcessor.ignore\n"},
        {"--param-CPreProcessor.bogus=1 a", FATAL,
         "ctags: no such parameter in CPreProcessor: bogus\n"},

        // Roles, which only reference tags have
        {"--roles-C.d=+{undef} --roles-C++.M=-{partOwner} a", OK, ""},
        {"--roles-C= --roles-C=* --roles-C.* --roles-all.*=* a", OK, ""},
        {"--roles-C.{macro}=*+{undef}-{condition} --roles-foo.d=x a", OK,
         "ctags: Warning: unknown language \"foo\" in --roles-foo.d "
         "option\n"},
        {"--roles-C.Q=+{undef} a", OK, "ctags: Warning: no such kind letter "
                                       "as specified in --roles-C.Q "
                                       "option\n"},
        {"--roles-C.M=+{undef} a", OK, "ctags: Warning: no such kind letter "
                                       "as specified in --roles-C.M "
                                       "option\n"},
        {"--roles-C.{bogus}x=+{undef} a", OK, "ctags: Warning: no such kind "
                                              "name as specified in "
                                              "--roles-C.{bogus}x option\n"},
        {"--roles-C.d=+{bogus} a", OK, "ctags: Warning: no such role: "
                                       "\"bogus\" in kind 'd' in language "
                                       "\"C\"\n"},
        {"--roles-C.F=+{x} a",    OK, "ctags: Warning: don't enable/disable "
                                      "a role in F/file kind; it has no "
                                      "role: --roles-C.F\n"},
        {"--roles- a",            OK, "ctags: Warning: no language given in "
                                      "\"roles-\" option\n"},
        {"--roles-C=x a",         FATAL, "ctags: only '*' or '' (empty "
                                         "string) is acceptable as an "
                                         "argument for --roles-C: x\n"},
        {"--roles-C.d=x a",       FATAL, "ctags: unexpected character x in "
                                         "--roles-C.d=x option\n"},
        {"--roles-C.d={x a",      FATAL, "ctags: no '}' representing the end "
                                         "of role name in --roles-C.d "
                                         "option: x\n"},
        {"--roles-C.d={} a",      FATAL, "ctags: empty role for the kind "
                                         "letter: d\n"},
        {"--roles-C.{macro}x=+{undef} a", FATAL, "ctags: garbage after the "
                                                 "kind specification {macro} "
                                                 "in --roles-C.{macro}x "
                                                 "option\n"},
        {"--roles-C.dx=+{undef} a", FATAL, "ctags: garbage after the kind "
                                           "specification 'd' in "
                                           "--roles-C.dx option\n"},
        {"--roles-C.1=+{undef} a", FATAL, "ctags: '1', unexpected character "
                                          "in --roles-C.1\n"},
        {"--roles-C.{macro=+{undef} a", FATAL, "ctags: no '}' representing "
                                               "the end of kind name in "
                                               "--roles-C.{macro option: "
                                               "{macro\n"},
        {"--roles-all.d= a",     FATAL, "ctags: only '*' or '' (empty "
                                        "string) is acceptable as a kind "
                                        "spec for --roles-all: "
                                        "--roles-all.d\n"},
        {"--roles-all=x a",      FATAL, "ctags: only '*' or '' (empty "
                                        "string) is acceptable as an "
                                        "argument for --roles-all: x\n"},

        // Per-language extras and fields
        {"--extras-C= --extras-C++=* --extras-Fortran=-{linkName} a", OK, ""},
        {"--extras-python=+{x} a", OK, ""},
        {"--extras-C=+{bogus} a",  FATAL, "ctags: no such extra: 'bogus'\n"},
        {"--extras-C=q a",         FATAL, "ctags: Warning: Wrong per language "
                                          "extra specification: q\n"
                                          "ctags: only long name can be used "
                                          "in per language extra spec: 'q'\n"},
        {"--extras-foo= a",        OK,    "ctags: Warning: Unknown language: "
                                          "foo (ignoring \"--extras-foo\")\n"},
        {"--extras- a",            FATAL, "ctags: No language given in "
                                          "\"extras-\" option\n"},
        {"--extras-Fortran=+{linkName} a", FATAL,
         "ctags: --extras-Fortran=+{linkName}: not supported\n"},
        {"--extras-all=* a",       FATAL,
         "ctags: --extras-Fortran=+{linkName}: not supported\n"},
        {"--fields-C++=+{template} --fields-C=* --fields-all=+{line} a",
         OK, ""},
        {"--fields-python=+{x} a", OK, ""},
        {"--fields-C++=-{name} a", FATAL, "ctags: --fields: not yet "
                                          "supported\n"},
        {"--fields-C++= a",        FATAL, "ctags: --fields: not yet "
                                          "supported\n"},
        {"--fields-C++=+{bogus} a", FATAL, "ctags: no such field: 'bogus'\n"},
        {"--fields-C++=+n a",      FATAL, "ctags: only long name can be used "
                                          "in per language field spec: "
                                          "'n'\n"},
        {"--fields-C++=n a",       FATAL, "ctags: Warning: Wrong per language "
                                          "field specification: n\n"
                                          "ctags: only long name can be used "
                                          "in per language field spec: "
                                          "'n'\n"},
        {"--fields-C++=+} a",      FATAL, "ctags: unexpected character in "
                                          "field specification: '}'\n"},
        {"--fields-C=+{name} a",   FATAL, "ctags: no such field: 'name'\n"},
        {"--fields-Fortran=+{alias} a", FATAL, "ctags: no such field: "
                                               "'alias'\n"},
        {"--fields-cpp=+{name} a", OK,    "ctags: Warning: Unknown language: "
                                          "cpp (ignoring \"--fields-cpp\")\n"},
        {"--fields- a",            FATAL, "ctags: No language given in "
                                          "\"fields-\" option\n"},
        {"--fields-all= a",        FATAL, "ctags: --fields: not yet "
                                          "supported\n"},
    };
    for (iz i = 0; i < countof(gs); i++) {
        Arena  tmp = a;
        Parsed r   = parsecmd(&tmp, gs[i].args);
        expect(t, gs[i].args, num(&tmp, r.status), num(&tmp, gs[i].status));
        expect(t, gs[i].args, r.log, gs[i].log);
    }

    // The state options leave
    struct { Str args, kinds, extras, fields; } st[] = {
        {"--kinds-all=* a",
         "C:defghlmpstuvxzLD C++:defghlmpstuvxzLDcnANUZMP "
         "F:bceEfiklLmMnNpPstvS", "{anonymous}Fps", "NFPTfknst C++:name"},
        {"--kinds-all= a", "C: C++: F:", "{anonymous}Fps",
         "NFPTfknst C++:name"},
        {"--kinds-all=F a", "C: C++: F:", "{anonymous}Fps",
         "NFPTfknst C++:name"},
        {"--kinds-all=+F-{file} a",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS",
         "{anonymous}Fps", "NFPTfknst C++:name"},
        {"--all-types=* a",
         "C:defghlmpstuvxzLD C++:defghlmpstuvxzLDcnANUZMP "
         "F:bceEfiklLmMnNpPstvS", "{anonymous}Fps", "NFPTfknst C++:name"},
        {"--extras-all={qualified} a",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS", "q",
         "NFPTfknst C++:name"},
        {"--fields-all={name} a",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS",
         "{anonymous}Fps", "NFP C++:name"},
        {"--fields-all={alias} a",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS",
         "{anonymous}Fps", "NFP C:alias C++:alias"},
        {"--fields-C++={template} a",
         "C:defghmstuv C++:defghmstuvcnMP F:bceEfiklmMnNpstvS",
         "{anonymous}Fps", "NFPTfknst C++:template"},
    };
    for (iz i = 0; i < countof(st); i++) {
        Arena  tmp = a;
        Parsed r   = parsecmd(&tmp, st[i].args);
        expect(t, st[i].args, showkinds(&tmp, &r.opt.kinds), st[i].kinds);
        expect(t, st[i].args, showextras(&tmp, r.opt.extras), st[i].extras);
        expect(t, st[i].args, showfields(&tmp, r.opt.fields), st[i].fields);
    }

    // The -I table (OPT-17): macros by name, and how many there are. A
    // NUL ends a line's token (X4), where Universal Ctags joins the next
    // line on, to make the tokens nul#c and "sp last".
    {
        Os *os = newos(&a);
        vfsfile(os, "m.txt", "  lead\nx+=int y\r\n\n z, w \t\nnul\0" "x=y\n"
                             "#c\n" "sp \0" "junk\n" "last");
        Str ls = "-I a,,b+\tc=d\ne+=f( a";
        struct { Str args, name, want; } ms[] = {
            {ls, "a", "a"},
            {ls, "b", "b+"},
            {ls, "c", "c=d"},
            {ls, "e", "e+=f("},
            {"-I x+junk=y+z a",  "x", "x+=y+z"},
            {"-I x=,y+= a",      "x", "x"},
            {"-I x=,y+= a",      "y", "y+"},
            {"-I a=1 -I a=2 a",  "a", "a=2"},
            {"-I a -I - -I b a", "a", ""},
            {"-I a -I - -I b a", "b", "b"},
            {"--param-CPreProcessor.ignore=a=b,c a", "a", "a=b,c"},
            {"--param-CPreProcessor.ignore=a=b,c a", "c", ""},
            {"-I @m.txt a",  "  lead", "  lead"},
            {"-I @m.txt a",  "x",      "x+=int y"},
            {"-I @m.txt a",  " z, w",  " z, w"},
            {"-I @m.txt a",  "nul",    "nul"},
            {"-I ./m.txt a", "#c",     "#c"},
            {"-I ./m.txt a", "sp",     "sp"},
            {"-I ./m.txt a", "last",   "last"},
        };
        for (iz i = 0; i < countof(ms); i++) {
            Arena  tmp = a;
            Parsed r   = parseos(&tmp, os, ms[i].args);
            expect(t, ms[i].args, r.log, "");
            expect(t, ms[i].args, showmacro(&tmp, &r.opt.macros, ms[i].name),
                   ms[i].want);
        }

        struct { Str args; i32 len; } ns[] = {
            {"a",                         0},
            {"-I +x,=y,+=z a",            0},
            {"-I a=1,b -I a=2 a",         2},
            {"-I a,b -I - -I c a",        1},
            {"-I @m.txt a",               7},
        };
        for (iz i = 0; i < countof(ns); i++) {
            Arena  tmp = a;
            Parsed r   = parseos(&tmp, os, ns[i].args);
            expect(t, ns[i].args, num(&tmp, r.opt.macros.all.len),
                   num(&tmp, ns[i].len));
        }

        // Of A=int, A=char and n more tokens, the table finds A=int
        // after it grows at 821 tokens, until it grows again at 1090.
        // Emptying it, after pre other tokens, starts the count again.
        struct { i32 pre; i32 n; Str want; } gs[] = {
            {   0,  818, "A=char"},
            {   0,  819, "A=int"},
            {   0, 1087, "A=int"},
            {   0, 1088, "A=char"},
            {1000,  100, "A=char"},
        };
        for (iz i = 0; i < countof(gs); i++) {
            Arena tmp = a;
            Buf   cmd(&tmp, 1<<14);
            if (gs[i].pre) {
                print(&cmd, "-I g");
                for (i32 j = 1; j < gs[i].pre; j++) {
                    print(&cmd, ",g");
                    print(&cmd, (i64)j);
                }
                print(&cmd, " -I - ");
            }
            print(&cmd, "-I A=int,A=char");
            for (i32 j = 0; j < gs[i].n; j++) {
                print(&cmd, ",f");
                print(&cmd, (i64)j);
            }
            print(&cmd, " a");
            Parsed r = parseos(&tmp, os, finish(&cmd));
            expect(t, num(&tmp, gs[i].n), showmacro(&tmp, &r.opt.macros, "A"),
                   gs[i].want);
        }
    }

    // The file kind, which every language shares, and which the inputFile
    // extra sets, the last option winning
    struct { Str args; b32 on; } fk[] = {
        {"a",                                0},
        {"--kinds-C=+F a",                   1},
        {"--kinds-C=F --kinds-C=* a",        1},  // resets leave it
        {"--kinds-C=+F --kinds-C= a",        1},
        {"--kinds-python=+{file} a",         1},
        {"--Python-kinds=+F --kinds-c=-F a", 0},
        {"--kinds-foo=+F a",                 0},  // unknown, ignored
        {"--langdef=foo --kinds-foo=+F a",   1},
        {"--kinds-all=+F a",                 1},
        {"--all-kinds=*F-F a",               0},
        {"--extras=+f a",                    1},
        {"--kinds-C=-F --extras=+f a",       1},
        {"--extras=+f --kinds-C=-F a",       0},
        {"--extras=+f --kinds-Fortran=-F a", 0},
        {"--extras=+f --extras=+q a",        1},
        {"--extras=+f --extras=q a",         0},
        {"--kinds-C=+F --extras=* a",        1},
        {"--kinds-C=+F --extras=-f a",       0},
        {"--extras=+f --extras-all= a",      0},
        {"--extras-all=+{inputFile} a",      1},
        {"--extras=+f --extras-Fortran= a",  1},
        {"--file-tags a",                    1},
        {"--kinds-C=+F --file-tags=no a",    0},
        {"--kinds-C=+F --file-scope=no a",   1},
    };
    for (iz i = 0; i < countof(fk); i++) {
        Arena  tmp = a;
        Parsed r   = parsecmd(&tmp, fk[i].args);
        expect(t, fk[i].args, num(&tmp, r.opt.filekind), num(&tmp, fk[i].on));
    }
    {
        // The placeholder kind's letter, a space, which words() splits
        Arena tmp  = a;
        Buf   log(&tmp, 64);
        u64   bits = 0;
        Spec  s    = flagspec(&bits, cxxkinds, NCKINDS, "+ -{ghost}", SPEC_KINDS,
                              "kinds-C", &log);
        expect(t, "ghost ok", num(&tmp, s.ok), "1");
        expect(t, "ghost log", finish(&log), "");
        expect(t, "ghost bits", num(&tmp, (i64)bits), "0");
    }
    // Listings, as Universal Ctags 6.2.1 prints them for these languages,
    // with the state the options before them leave
    struct { Str args; i32 status; Str print, log; } ls[] = {
        {"--list-languages", ARGS_EXIT, "C\nC++\nFortran\n", ""},
        {"--languages=C --list-languages=x", ARGS_EXIT,
         "C\nC++ [disabled]\nFortran [disabled]\n", ""},
        {"--list-languages=_packcc", ARGS_EXIT, "", ""},
        {"--kinds-C=+p-d --list-kinds=C", ARGS_EXIT,
         "d  macro definitions [off]\n"
         "e  enumerators (values inside an enumeration)\n"
         "f  function definitions\n"
         "g  enumeration names\n"
         "h  included header files\n"
         "l  local variables [off]\n"
         "m  struct, and union members\n"
         "p  function prototypes\n"
         "s  structure names\n"
         "t  typedefs\n"
         "u  union names\n"
         "v  variable definitions\n"
         "x  external and forward variable declarations [off]\n"
         "z  function parameters inside function or prototype definitions [off]\n"
         "L  goto labels [off]\n"
         "D  parameters inside macro definitions [off]\n", ""},
        {"--languages=-Fortran --kinds-Fortran= --list-kinds", ARGS_EXIT,
         "C\n"
         "    d  macro definitions\n"
         "    e  enumerators (values inside an enumeration)\n"
         "    f  function definitions\n"
         "    g  enumeration names\n"
         "    h  included header files\n"
         "    l  local variables [off]\n"
         "    m  struct, and union members\n"
         "    p  function prototypes [off]\n"
         "    s  structure names\n"
         "    t  typedefs\n"
         "    u  union names\n"
         "    v  variable definitions\n"
         "    x  external and forward variable declarations [off]\n"
         "    z  function parameters inside function or prototype definitions [off]\n"
         "    L  goto labels [off]\n"
         "    D  parameters inside macro definitions [off]\n"
         "C++\n"
         "    d  macro definitions\n"
         "    e  enumerators (values inside an enumeration)\n"
         "    f  function definitions\n"
         "    g  enumeration names\n"
         "    h  included header files\n"
         "    l  local variables [off]\n"
         "    m  class, struct, and union members\n"
         "    p  function prototypes [off]\n"
         "    s  structure names\n"
         "    t  typedefs\n"
         "    u  union names\n"
         "    v  variable definitions\n"
         "    x  external and forward variable declarations [off]\n"
         "    z  function parameters inside function or prototype definitions [off]\n"
         "    L  goto labels [off]\n"
         "    D  parameters inside macro definitions [off]\n"
         "    c  classes\n"
         "    n  namespaces\n"
         "    A  namespace aliases [off]\n"
         "    N  names imported via using scope::symbol [off]\n"
         "    U  using namespace statements [off]\n"
         "    Z  template parameters [off]\n"
         "    M  modules\n"
         "    P  partitions\n"
         "Fortran [disabled]\n"
         "    b  block data [off]\n"
         "    c  common blocks [off]\n"
         "    e  entry points [off]\n"
         "    E  enumerations [off]\n"
         "    f  functions [off]\n"
         "    i  interface contents, generic names, and operators [off]\n"
         "    k  type and structure components [off]\n"
         "    l  labels [off]\n"
         "    L  local, common block, and namelist variables [off]\n"
         "    m  modules [off]\n"
         "    M  type bound procedures [off]\n"
         "    n  namelists [off]\n"
         "    N  enumeration values [off]\n"
         "    p  programs [off]\n"
         "    P  subprogram prototypes [off]\n"
         "    s  subroutines [off]\n"
         "    t  derived types and structures [off]\n"
         "    v  program (global) and module variables [off]\n"
         "    S  submodules [off]\n", ""},
        {"--list-kinds=python", ARGS_EXIT, "", ""},
        {"--list-kinds-full=C++", ARGS_EXIT,
         "#LETTER NAME       ENABLED REFONLY NROLES MASTER DESCRIPTION\n"
         "A       alias      no      no      0      NONE   namespace aliases\n"
         "D       macroparam no      no      0      C      parameters inside macro definitions\n"
         "L       label      no      no      0      C      goto labels\n"
         "M       module     yes     no      2      NONE   modules\n"
         "N       name       no      no      0      NONE   names imported via using scope::symbol\n"
         "P       partition  yes     no      1      NONE   partitions\n"
         "U       using      no      no      0      NONE   using namespace statements\n"
         "Z       tparam     no      no      0      NONE   template parameters\n"
         "c       class      yes     no      0      NONE   classes\n"
         "d       macro      yes     no      2      C      macro definitions\n"
         "e       enumerator yes     no      0      C      enumerators (values inside an enumeration)\n"
         "f       function   yes     no      0      C      function definitions\n"
         "g       enum       yes     no      0      C      enumeration names\n"
         "h       header     yes     yes     4      C      included header files\n"
         "l       local      no      no      0      C      local variables\n"
         "m       member     yes     no      0      C      class, struct, and union members\n"
         "n       namespace  yes     no      0      NONE   namespaces\n"
         "p       prototype  no      no      0      C      function prototypes\n"
         "s       struct     yes     no      0      C      structure names\n"
         "t       typedef    yes     no      0      C      typedefs\n"
         "u       union      yes     no      0      C      union names\n"
         "v       variable   yes     no      0      C      variable definitions\n"
         "x       externvar  no      no      0      C      external and forward variable declarations\n"
         "z       parameter  no      no      0      C      function parameters inside function or prototype definitions\n",
         ""},
        {"--kinds-Fortran=+L-l --list-kinds-full", ARGS_EXIT, {}, ""},
        {"--extras=+q-p --list-extras", ARGS_EXIT,
         "#LETTER NAME      ENABLED LANGUAGE FIXED DESCRIPTION\n"
         "-       anonymous yes     NONE     no    Include tags for non-named objects like lambda\n"
         "F       fileScope yes     NONE     no    Include tags of file scope\n"
         "f       inputFile no      NONE     no    Include an entry for the base file name of every input file\n"
         "g       guest     no      NONE     no    Include tags generated by guest parsers\n"
         "p       pseudo    no      NONE     no    Include pseudo tags\n"
         "q       qualified yes     NONE     no    Include an extra class-qualified tag entry for each tag\n"
         "r       reference no      NONE     no    Include reference tags\n"
         "s       subparser yes     NONE     no    Include tags generated by subparsers\n"
         "z       nulltag   no      NONE     no    Include tags with empty strings as their names\n"
         "-       linkName  no      Fortran  no    Linking name used in foreign languages\n",
         ""},
        {"--list-extras=fortran", ARGS_EXIT,
         "#LETTER NAME     ENABLED LANGUAGE FIXED DESCRIPTION\n"
         "-       linkName no      Fortran  no    Linking name used in foreign languages\n",
         ""},
        {"--list-extras=C", ARGS_EXIT,
         "#LETTER NAME ENABLED LANGUAGE FIXED DESCRIPTION\n", ""},
        {"--fields-C++=-{name}+{template} --list-fields=C++", ARGS_EXIT,
         "#LETTER NAME           ENABLED LANGUAGE JSTYPE FIXED OP DESCRIPTION\n"
         "-       alias          no      C++      s--    no    -- the name of the alias target specified in __attribute__((alias(...)))\n"
         "-       captures       no      C++      s--    no    -- lambda capture list\n"
         "-       macrodef       no      C++      s--    no    -- macro definition\n"
         "-       name           no      C++      s--    no    -- aliased names\n"
         "-       properties     no      C++      s--    no    -- properties (static, inline, mutable, export,...)\n"
         "-       section        no      C++      s--    no    -- the place where the object is placed\n"
         "-       specialization no      C++      s--    no    -- template specialization parameters\n"
         "-       template       yes     C++      s--    no    -- template parameters\n",
         ""},
        {"--fields=-N-n+Z-s --list-fields=NONE", ARGS_EXIT,
         "#LETTER NAME           ENABLED LANGUAGE JSTYPE FIXED OP DESCRIPTION\n"
         "N       name           yes     NONE     s--    yes   rw tag name\n"
         "F       input          yes     NONE     s--    yes   r- input file\n"
         "P       pattern        yes     NONE     s-b    yes   -- pattern\n"
         "C       compact        no      NONE     s--    no    -- compact input line (used only in xref output)\n"
         "E       extras         no      NONE     s--    no    r- Extra tag type information\n"
         "K       NONE           no      NONE     s--    no    -- Kind of tag in long-name form\n"
         "R       NONE           no      NONE     s--    no    -- Marker (R or D) representing whether tag is definition or reference\n"
         "S       signature      no      NONE     s--    no    rw Signature of routine (e.g. prototype or parameter list)\n"
         "T       epoch          yes     NONE     -i-    no    -- the last modified time of the input file (only for F/file kind tag)\n"
         "Z       scope          yes     NONE     s--    no    rw [tags output] prepend \"scope:\" key to s/scope field output, [xref and json output] the same as s/ field\n"
         "a       access         no      NONE     s--    no    rw Access (or export) of class members\n"
         "e       end            no      NONE     -i-    no    rw end lines of various items\n"
         "f       file           yes     NONE     --b    no    -- File-restricted scoping\n"
         "i       inherits       no      NONE     s-b    no    -w Inheritance information\n"
         "k       NONE           yes     NONE     s--    no    -- Kind of tag in one-letter form\n"
         "l       language       no      NONE     s--    no    r- Language of input file containing tag\n"
         "m       implementation no      NONE     s--    no    -- Implementation information\n"
         "n       line           no      NONE     -i-    no    rw Line number of tag definition\n"
         "o       nth            no      NONE     -i-    no    -- the order in the parent scope\n"
         "p       scopeKind      no      NONE     s--    no    -- [tags output] no effect, [xref and json output] kind of scope in long-name form\n"
         "r       roles          no      NONE     s--    no    r- Roles\n"
         "s       NONE           yes     NONE     s--    no    -- [tags output] scope (kind:name) of tag definition, [xref and json output] name of scope\n"
         "t       typeref        yes     NONE     s--    no    rw Type and name of a variable or typedef\n"
         "x       xpath          no      NONE     s--    no    -- xpath for the tag\n"
         "z       kind           no      NONE     s--    no    r- [tags output] prepend \"kind:\" to k/ (or K/) field output, [xref and json output] kind in long-name form\n",
         "ctags: Warning: though Z/scope field is enabled, s field is not "
         "enabled\n"
         "ctags: Warning: enable the s field to make the Z/scope field "
         "printable\n"
         "ctags: Warning: Cannot disable fixed field: 'N'{name} in ctags "
         "output mode\n"},
        {"-e --fields=-N --list-fields=NONE", ARGS_EXIT,
         "#LETTER NAME           ENABLED LANGUAGE JSTYPE FIXED OP DESCRIPTION\n"
         "C       compact        no      NONE     s--    no    -- compact input line (used only in xref output)\n"
         "E       extras         no      NONE     s--    no    r- Extra tag type information\n"
         "F       input          yes     NONE     s--    no    r- input file\n"
         "K       NONE           no      NONE     s--    no    -- Kind of tag in long-name form\n"
         "N       name           no      NONE     s--    no    rw tag name\n"
         "P       pattern        yes     NONE     s-b    no    -- pattern\n"
         "R       NONE           no      NONE     s--    no    -- Marker (R or D) representing whether tag is definition or reference\n"
         "S       signature      no      NONE     s--    no    rw Signature of routine (e.g. prototype or parameter list)\n"
         "T       epoch          yes     NONE     -i-    no    -- the last modified time of the input file (only for F/file kind tag)\n"
         "Z       scope          no      NONE     s--    no    rw [tags output] prepend \"scope:\" key to s/scope field output, [xref and json output] the same as s/ field\n"
         "a       access         no      NONE     s--    no    rw Access (or export) of class members\n"
         "e       end            no      NONE     -i-    no    rw end lines of various items\n"
         "f       file           yes     NONE     --b    no    -- File-restricted scoping\n"
         "i       inherits       no      NONE     s-b    no    -w Inheritance information\n"
         "k       NONE           yes     NONE     s--    no    -- Kind of tag in one-letter form\n"
         "l       language       no      NONE     s--    no    r- Language of input file containing tag\n"
         "m       implementation no      NONE     s--    no    -- Implementation information\n"
         "n       line           yes     NONE     -i-    no    rw Line number of tag definition\n"
         "o       nth            no      NONE     -i-    no    -- the order in the parent scope\n"
         "p       scopeKind      no      NONE     s--    no    -- [tags output] no effect, [xref and json output] kind of scope in long-name form\n"
         "r       roles          no      NONE     s--    no    r- Roles\n"
         "s       NONE           yes     NONE     s--    no    -- [tags output] scope (kind:name) of tag definition, [xref and json output] name of scope\n"
         "t       typeref        yes     NONE     s--    no    rw Type and name of a variable or typedef\n"
         "x       xpath          no      NONE     s--    no    -- xpath for the tag\n"
         "z       kind           no      NONE     s--    no    r- [tags output] prepend \"kind:\" to k/ (or K/) field output, [xref and json output] kind in long-name form\n",
         ""},
        {"--list-fields=fortran", ARGS_EXIT,
         "#LETTER NAME ENABLED LANGUAGE JSTYPE FIXED OP DESCRIPTION\n", ""},
        {"--list-kinds=cpp", ARGS_FATAL, {},
         "ctags: Unknown language \"cpp\" in \"list-kinds\" option\n"},
        {"--list-kinds-full=NONE", ARGS_FATAL, {},
         "ctags: Unknown language \"NONE\" in \"list-kinds-full\" option\n"},
        {"--list-extras=cpp", ARGS_FATAL, {},
         "ctags: Unknown language \"cpp\" in \"list-extras\" option\n"},
        {"--list-fields=cpp", ARGS_FATAL, {},
         "ctags: Unknown language \"cpp\" in \"list-fields\" option\n"},
    };
    for (iz i = 0; i < countof(ls); i++) {
        Arena  tmp = a;
        Parsed r   = parsecmd(&tmp, ls[i].args);
        expect(t, ls[i].args, num(&tmp, r.status), num(&tmp, ls[i].status));
        if (ls[i].print.data) {
            expect(t, ls[i].args, r.opt.print, ls[i].print);
        }
        expect(t, ls[i].args, r.log, ls[i].log);
    }

    // The pseudo extra: until an option sets it, enabled where the writer
    // has pseudo-tags and the output is a file; under etags, fixed
    struct { Str args, row; } ps[] = {
        {"--list-extras",                           "yes     NONE     no "},
        {"-f - --list-extras",                      "no      NONE     no "},
        {"-o /dev/stdout --list-extras",            "no      NONE     no "},
        {"-f - --extras=+p --list-extras",          "yes     NONE     no "},
        {"-f - --extras=+q --list-extras",          "no      NONE     no "},
        {"-f - --extras=* --list-extras",           "yes     NONE     no "},
        {"--extras=-p --list-extras",               "no      NONE     no "},
        {"-e --list-extras",                        "no      NONE     yes"},
        {"-e --extras=+p --list-extras",            "no      NONE     yes"},
        {"--extras=+p -e --list-extras",            "yes     NONE     yes"},
        {"-e --output-format=e-ctags --list-extras", "yes     NONE     no "},
        {"-e --extras-all=* --output-format=e-ctags --list-extras",
         "no      NONE     no "},
    };
    for (iz i = 0; i < countof(ps); i++) {
        Arena  tmp = a;
        Parsed r   = parsecmd(&tmp, ps[i].args);
        Str    row = concat(&tmp, "\np       pseudo    ", ps[i].row);
        expect(t, ps[i].args, num(&tmp, find(r.opt.print, row) > 0), "1");
    }

    {
        // -? is --help, at once
        Arena  tmp  = a;
        Parsed help = parsecmd(&tmp, "--help");
        Parsed q    = parsecmd(&tmp, "-R? --bogus");
        expect(t, "-? status", num(&tmp, q.status), num(&tmp, ARGS_EXIT));
        expect(t, "-? print", q.opt.print, help.opt.print);
        expect(t, "-? log", q.log, "");
    }

    {
        // Listing all kinds in full, sorted by language and letter
        Arena  tmp  = a;
        Parsed r    = parsecmd(&tmp, "--kinds-Fortran=+L-l --list-kinds-full");
        Str    want =
            "#LANGUAGE LETTER NAME       ENABLED REFONLY NROLES MASTER DESCRIPTION\n"
            "C         D      macroparam no      no      0      C      parameters inside macro definitions\n";
        expect(t, "full kinds head", takehead(r.opt.print, want.len), want);
        Str fortran =
            "Fortran   L      local      yes     no      0      NONE   local, common block, and namelist variables\n";
        expect(t, "full kinds L", num(&tmp, find(r.opt.print, fortran) > 0), "1");
        Str last =
            "Fortran   v      variable   yes     no      0      NONE   program (global) and module variables\n";
        expect(t, "full kinds tail", num(&tmp, endswith(r.opt.print, last)), "1");
    }
    {
        Arena tmp = a;
        Os   *os  = newos(&tmp);
        vfsfile(os, "x.h", "");
        vfsdir(os, "dir");
        Parsed r = parseos(&tmp, os, "-h x.h x.c");
        expect(t, "-h FILE", r.log, "ctags: -h: Invalid list\n");
        r = parseos(&tmp, os, "-h dir x.c");
        expect(t, "-h DIR", r.log, "ctags: -h: Invalid list\n");
    }
}

// Command line: the few Universal Ctags options that matter to Vim
// This is free and unencumbered software released into the public domain.

struct Options {
    Str   output;   // "-" is standard output
    Str  *files;
    iz    nfiles;
    Kinds kinds;
    i32   jobs;     // 0: one per processor
    b32   recurse;
    b32   stats;
    b32   nofast;
    b32   windows;  // '\' and ':' separate paths
};

static Str usage =
"usage: ctags [options] [file(s)]\n"
"  -R, --recurse[=yes|no]  descend into directories (default: no)\n"
"  -f FILE, -o FILE        write tags to FILE, - for standard output\n"
"                          (default: tags)\n"
"  --kinds-LANG=[+|-]KINDS enable or disable tag kinds for C, C++, or\n"
"                          Fortran, as letters or {names}\n"
"  --jobs=N                threads, 0 for one per processor (default: 0)\n"
"  --version               print the version\n"
"  --help                  print this message\n";

enum : i32 {
    ARGS_OK,
    ARGS_EXIT,   // printed what was asked, exit 0
    ARGS_FATAL,  // printed the problem, exit 1
};

// Parse a --flag[=value] boolean, as Universal Ctags accepts them.
static i32 parsebool(Str s)  // -1 if invalid
{
    Str yes[] = {"", "1", "y", "yes", "on", "true"};
    Str no[]  = {"0", "n", "no", "off", "false"};
    for (iz i = 0; i < countof(yes); i++) {
        if (equalsfold(s, yes[i])) return 1;
    }
    for (iz i = 0; i < countof(no); i++) {
        if (equalsfold(s, no[i])) return 0;
    }
    return -1;
}

// Parse a non-negative decimal no larger than max.
static i32 parsenum(Str s, i32 max)  // -1 if invalid
{
    if (!s.len) {
        return -1;
    }
    i32 r = 0;
    for (iz i = 0; i < s.len; i++) {
        i32 d = s[i] - '0';
        if (d<0 || d>9 || r>(max-d)/10) {
            return -1;
        }
        r = r*10 + d;
    }
    return r;
}

static void fatal(Buf *log, Str msg, Str arg, Str tail)
{
    print(log, "ctags: ");
    print(log, msg);
    print(log, arg);
    print(log, tail);
    putbyte(log, '\n');
}

// Name the language of a --kinds-LANG option, or LANG_NONE.
static i32 langnamed(Str name)
{
    if (equalsfold(name, "c")) {
        return LANG_C;
    } else if (equalsfold(name, "c++")) {
        return LANG_CXX;
    } else if (equalsfold(name, "fortran")) {
        return LANG_FORTRAN;
    }
    return LANG_NONE;
}

static i32 setoutput(Options *opt, Str name, b32 *seen, Buf *log)
{
    if (name.len>1 && name[0]=='-') {
        fatal(log, "output file name may not begin with a '-'", "", "");
        return ARGS_FATAL;
    }
    if (*seen) {
        print(log, "ctags: Warning: -f option specified more than once, "
                   "last value used\n");
    }
    *seen = 1;
    opt->output = name=="/dev/stdout" ? Str("-") : name;
    return ARGS_OK;
}

static i32 parseargs(Options *opt, Str *args, i32 nargs, Arena *perm, Buf *log)
{
    opt->output = "tags";
    opt->kinds  = defaultkinds();
    opt->files  = alloc<Str>(perm, nargs);

    b32 seen = 0;  // -f given
    for (i32 i = 1; i < nargs; i++) {
        Str a = args[i];

        if (!startswith(a, "-") || a=="-") {
            opt->files[opt->nfiles++] = a;

        } else if (!startswith(a, "--")) {
            for (iz j = 1; j < a.len; j++) {
                switch (a[j]) {
                case 'R':
                    opt->recurse = 1;
                    break;
                case 'f':
                case 'o': {
                    Str v = cuthead(a, j+1);
                    if (!v.len) {
                        if (i+1 == nargs) {
                            fatal(log, "-", takehead(cuthead(a, j), 1),
                                  ": Parameter missing");
                            return ARGS_FATAL;
                        }
                        v = args[++i];
                    }
                    if (setoutput(opt, v, &seen, log)) {
                        return ARGS_FATAL;
                    }
                    j = a.len;
                } break;
                default:
                    fatal(log, "Unknown option: -", takehead(cuthead(a, j), 1), "");
                    return ARGS_FATAL;
                }
            }

        } else {
            Cut c     = cut(cuthead(a, 2), '=');
            Str name  = c.head;
            Str value = c.ok ? c.tail : Str{(u8 *)"", 0};

            if (name == "recurse") {
                i32 b = parsebool(value);
                if (b < 0) {
                    fatal(log, "Invalid value for \"recurse\" option", "", "");
                    return ARGS_FATAL;
                }
                opt->recurse = b;

            } else if (name == "jobs") {
                i32 n = parsenum(value, 1024);
                if (n < 0) {
                    fatal(log, "Invalid value for \"jobs\" option", "", "");
                    return ARGS_FATAL;
                }
                opt->jobs = n;

            } else if (name=="kinds-all" || name=="all-kinds") {
                fatal(log, "Unknown option: --", name, "");
                return ARGS_FATAL;

            } else if (startswith(name, "kinds-") ||
                       endswith(name, "-kinds") || endswith(name, "-types")) {
                // --kinds-LANG, or the old --LANG-kinds and --LANG-types
                // where the language runs up to the first '-'
                Str lname = {};
                if (startswith(name, "kinds-")) {
                    lname = cuthead(name, 6);
                } else {
                    lname = cut(name, '-').head;
                }
                i32 lang = langnamed(lname);
                if (lang == LANG_NONE) {
                    print(log, "ctags: Warning: Unknown language \"");
                    print(log, lname);
                    print(log, "\" in \"");
                    print(log, name);
                    print(log, "\" option\n");
                } else if (!kindspec(&opt->kinds, lang, value, name, log)) {
                    return ARGS_FATAL;
                }

            } else if (name == "version") {
                print(log, "ctags ");
                print(log, version);
                putbyte(log, '\n');
                return ARGS_EXIT;

            } else if (name == "help") {
                print(log, usage);
                return ARGS_EXIT;

            } else if (name == "_stats") {
                opt->stats = 1;

            } else if (name == "_nofast") {
                opt->nofast = 1;

            } else {
                fatal(log, "Unknown option: ", a, "");
                return ARGS_FATAL;
            }
        }
    }

    if (!opt->nfiles) {
        if (!opt->recurse) {
            fatal(log, "No files specified. Try \"ctags --help\".", "", "");
            return ARGS_FATAL;
        }
        opt->files[opt->nfiles++] = ".";
    }
    return ARGS_OK;
}

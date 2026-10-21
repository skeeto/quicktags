// Command line: options as Universal Ctags parses them, from arguments
// and from option files
// This is free and unencumbered software released into the public domain.

struct Options {
    Str        output;       // "-" is standard output
    Slice<Str> files;
    Kinds      kinds;
    u32        extras;       // EXTRA_*
    u64        fields;       // one bit per FIELD_*
    i32        jobs;         // 0: one per processor
    Str        print;        // for ARGS_EXIT: what to print
    b32        recurse;
    b32        fieldsreset;  // --fields reset them: no fixed-field warnings
    b32        stats;
    b32        nofast;
    b32        windows;      // '\' and ':' separate paths
};

static Str usage =
"usage: ctags [options] [file(s)]\n"
"  -R, --recurse[=yes|no]  descend into directories (default: no)\n"
"  -f FILE, -o FILE        write tags to FILE, - for standard output\n"
"                          (default: tags)\n"
"  --kinds-LANG=[+|-]KINDS enable or disable tag kinds for C, C++, or\n"
"                          Fortran, as letters or {names}\n"
"  --extras=[+|-]EXTRAS    q: also tag C and C++ names qualified by\n"
"                          scope (ns::Class::f), F: file-scoped tags,\n"
"                          p: pseudo-tags (default: Fp)\n"
"  --jobs=N                threads, 0 for one per processor (default: 0)\n"
"  --version               print the version\n"
"  --help                  print this message\n";

enum : i32 {
    ARGS_OK,
    ARGS_EXIT,   // print opt->print, exit 0
    ARGS_FATAL,  // printed the problem, exit 1
};

// The state of parsing, shared by the arguments and the option files.
struct Parse {
    Options *opt;
    Os      *os;
    Arena   *perm;
    Buf     *log;
    b32      seenf;  // -f or -o given
};


// Messages

static void message(Buf *log, Str level, Str a, Str b, Str c, Str d, Str e,
                    Str f, Str g)
{
    print(log, "ctags: ");
    print(log, level);
    Str parts[] = {a, b, c, d, e, f, g};
    for (iz i = 0; i < countof(parts); i++) {
        print(log, parts[i]);
    }
    putbyte(log, '\n');
}

// Report a problem that stops the program, in pieces.
static i32 fatal(Parse *p, Str a, Str b = {}, Str c = {}, Str d = {},
                 Str e = {}, Str f = {}, Str g = {})
{
    message(p->log, "", a, b, c, d, e, f, g);
    return ARGS_FATAL;
}

static void warning(Parse *p, Str a, Str b = {}, Str c = {}, Str d = {},
                    Str e = {}, Str f = {}, Str g = {})
{
    message(p->log, "Warning: ", a, b, c, d, e, f, g);
}


// Values

static b32 istrue(Str s)
{
    Str yes[] = {"1", "y", "yes", "on", "true"};
    for (iz i = 0; i < countof(yes); i++) {
        if (equalsfold(s, yes[i])) return 1;
    }
    return 0;
}

static b32 isfalse(Str s)
{
    Str no[] = {"0", "n", "no", "off", "false"};
    for (iz i = 0; i < countof(no); i++) {
        if (equalsfold(s, no[i])) return 1;
    }
    return 0;
}

// Parse a --flag[=value] boolean, as Universal Ctags accepts them.
static i32 parsebool(Str s)  // -1 if invalid
{
    return !s.len || istrue(s) ? 1 : isfalse(s) ? 0 : -1;
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

static i32 badvalue(Parse *p, Str option)
{
    return fatal(p, "Invalid value for \"", option, "\" option");
}

enum : u32 {
    LINES_TRIM     = 1u<<0,  // trim whitespace, dropping blank lines
    LINES_COMMENTS = 1u<<1,  // drop lines whose first non-blank is '#'
};

// Split text into lines at LF, CR, or CRLF. A final line break ends the
// last line rather than starting another.
static Slice<Str> splitlines(Arena *a, Str s, u32 flags)
{
    Slice<Str> r = {};
    for (iz i = 0; i < s.len;) {
        iz end = i;
        for (; end<s.len && s[end]!='\n' && s[end]!='\r'; end++) {}
        Str line = span(s.data+i, s.data+end);
        i = end + (end<s.len);
        if (end<s.len && s[end]=='\r' && i<s.len && s[i]=='\n') {
            i++;
        }
        if (flags & LINES_TRIM) {
            line = trim(line);
            if (!line.len) continue;
        }
        if (flags&LINES_COMMENTS && startswith(trimleft(line), "#")) {
            continue;
        }
        r = push(a, r, line);
    }
    return r;
}


// Languages

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


// Long options

// --kinds-LANG, or the old --LANG-kinds and --LANG-types where the
// language runs up to the first '-'.
static i32 kindsopt(Parse *p, Str name, Str value)
{
    Cut dash  = cut(name, '-');
    Str lname = dash.tail=="kinds" || dash.tail=="types" ? dash.head
                                                          : cuthead(name, 6);
    i32 lang  = langnamed(lname);
    if (lang == LANG_NONE) {
        warning(p, "Unknown language \"", lname, "\" in \"", name, "\" option");
        return ARGS_OK;
    }
    KindTable t    = kindtable(&p->opt->kinds, lang);
    u64       bits = *t.bits;
    Spec      s    = flagspec(&bits, t.kinds, t.len, value, SPEC_KINDS, name,
                              p->log);
    *t.bits = (u32)bits;
    return s.ok ? ARGS_OK : ARGS_FATAL;
}

// Enable the fields that others need, and the fixed fields, which no
// option disables, warning as Universal Ctags does for its tags output.
static void fixfields(Parse *p)
{
    u64 *f = &p->opt->fields;
    if (fieldon(*f, FIELD_KINDKEY) && !fieldon(*f, FIELD_KIND) &&
        !fieldon(*f, FIELD_KINDLONG)) {
        warning(p, "though z/kind field is enabled, neither k nor K field "
                   "is not enabled");
        warning(p, "enable the K field to make the z/kind field printable");
        *f |= (u64)1 << FIELD_KINDLONG;
    }
    if (fieldon(*f, FIELD_SCOPEKEY) && !fieldon(*f, FIELD_SCOPE)) {
        warning(p, "though Z/scope field is enabled, s field is not enabled");
        warning(p, "enable the s field to make the Z/scope field printable");
        *f |= (u64)1 << FIELD_SCOPE;
    }
    for (i32 i = FIELD_NAME; i <= FIELD_PATTERN; i++) {
        if (!fieldon(*f, i)) {
            *f |= (u64)1 << i;
            if (!p->opt->fieldsreset) {
                warning(p, "Cannot disable fixed field: '",
                        {&fields[i].letter, 1}, "'{", fields[i].name,
                        "} in ctags output mode");
            }
        }
    }
}

enum : i32 {
    OPT_EXTRAS, OPT_FIELDS, OPT_FILESCOPE, OPT_HELP, OPT_JOBS, OPT_RECURSE,
    OPT_VERSION, OPT_STATS, OPT_NOFAST,
};

static struct { Str name; i32 id; } longopts[] = {
    {"extra",      OPT_EXTRAS},
    {"extras",     OPT_EXTRAS},
    {"fields",     OPT_FIELDS},
    {"file-scope", OPT_FILESCOPE},
    {"help",       OPT_HELP},
    {"jobs",       OPT_JOBS},
    {"recurse",    OPT_RECURSE},
    {"version",    OPT_VERSION},
    {"_nofast",    OPT_NOFAST},
    {"_stats",     OPT_STATS},
};

// Families of options that name a language, by prefix.
enum : i32 { LOPT_KINDS };

static struct { Str prefix; i32 id; } langopts[] = {
    {"kinds-", LOPT_KINDS},
};

static i32 longopt(Parse *p, Str name, Str value)
{
    Options *opt = p->opt;

    i32 id = -1;
    for (iz i = 0; i<countof(longopts) && id<0; i++) {
        if (longopts[i].name == name) {
            id = longopts[i].id;
        }
    }

    switch (id) {
    case OPT_EXTRAS: {
        if (name == "extra") {
            warning(p, "--extra option is obsolete; use --extras instead");
        }
        u64  bits = opt->extras;
        Spec s    = flagspec(&bits, extras, countof(extras), value,
                             SPEC_EXTRAS, name, p->log);
        opt->extras = (u32)bits;
        return s.ok ? ARGS_OK : ARGS_FATAL;
    }

    case OPT_FIELDS: {
        Spec s = flagspec(&opt->fields, fields, NCOMMONFIELDS, value,
                          SPEC_FIELDS, name, p->log);
        opt->fieldsreset |= s.reset;
        return s.ok ? ARGS_OK : ARGS_FATAL;
    }

    case OPT_FILESCOPE: {
        i32 b = parsebool(value);
        if (b < 0) {
            return badvalue(p, name);
        }
        warning(p, "\"--file-scope\" option is obsolete; use \"--extras=",
                b ? Str("+F") : "-F", "\" instead");
        opt->extras &= ~EXTRA_FILESCOPE;
        opt->extras |= b ? EXTRA_FILESCOPE : 0u;
        return ARGS_OK;
    }

    case OPT_HELP: {
        Buf b(p->perm);
        print(&b, usage);
        printkinds(&b);
        opt->print = finish(&b);
        return ARGS_EXIT;
    }

    case OPT_JOBS:
        opt->jobs = parsenum(value, 1024);
        return opt->jobs<0 ? badvalue(p, name) : ARGS_OK;

    case OPT_RECURSE:
        opt->recurse = parsebool(value);
        return opt->recurse<0 ? badvalue(p, name) : ARGS_OK;

    case OPT_VERSION: {
        Buf b(p->perm, 32);
        print(&b, "ctags ");
        print(&b, version);
        putbyte(&b, '\n');
        opt->print = finish(&b);
        return ARGS_EXIT;
    }

    case OPT_STATS:
        opt->stats = 1;
        return ARGS_OK;

    case OPT_NOFAST:
        opt->nofast = 1;
        return ARGS_OK;
    }

    // Universal Ctags tries the old --LANG-kinds form after its table of
    // exact names, which include --list-kinds
    Cut dash = cut(name, '-');
    if (dash.tail=="kinds" || dash.tail=="types") {
        return kindsopt(p, name, value);
    }
    for (iz i = 0; i < countof(langopts); i++) {
        if (startswith(name, langopts[i].prefix)) {
            switch (langopts[i].id) {
            case LOPT_KINDS: return kindsopt(p, name, value);
            }
        }
    }
    return fatal(p, "Unknown option: --", name);
}


// Short options

// Letters that take a parameter, the rest of the cluster or the next
// argument, as in Universal Ctags.
static b32 takesparam(u8 c)
{
    switch (c) {
    case 'f': case 'o': case 'h': case 'i': case 'I': case 'L': case 'p':
    case 'd': case 'D': case 'b':
        return 1;
    }
    return 0;
}

static i32 setoutput(Parse *p, u8 letter, Str name)
{
    if (p->seenf) {
        warning(p, "-", {&letter, 1}, " option specified more than once, "
                "last value used");
    } else if (name.len>1 && name[0]=='-') {
        return fatal(p, "output file name may not begin with a '-'");
    }
    p->seenf = 1;
    p->opt->output = name=="/dev/stdout" ? Str("-") : name;
    return ARGS_OK;
}

static i32 shortopt(Parse *p, u8 c, Str param)
{
    switch (c) {
    case 'R':
        p->opt->recurse = 1;
        return ARGS_OK;
    case 'f':
    case 'o':
        return setoutput(p, c, param);
    }
    return fatal(p, "Unknown option: -", {&c, 1});
}


// The loop

static void parseinit(Parse *p, Options *opt, Os *os, Arena *perm, Buf *log)
{
    p->opt  = opt;
    p->os   = os;
    p->perm = perm;
    p->log  = log;
    opt->kinds  = defaultkinds();
    opt->extras = defaultextras();
    opt->fields = defaultfields();
}

// Parse arguments: options, and files to tag.
static i32 parseloop(Parse *p, Str *args, iz nargs)
{
    for (iz i = 0; i < nargs; i++) {
        Str a = args[i];
        i32 r = ARGS_OK;

        if (!startswith(a, "-")) {
            p->opt->files = push(p->perm, p->opt->files, a);

        } else if (startswith(a, "--")) {
            Cut c = cut(cuthead(a, 2), '=');
            r = longopt(p, c.head, c.ok ? c.tail : Str(""));

        } else if (a.len == 1) {
            r = fatal(p, "Unknown option: -");

        } else {
            for (iz j = 1; !r && j<a.len; j++) {
                u8  c     = a[j];
                Str param = {};
                if (takesparam(c)) {
                    param = cuthead(a, j+1);
                    if (!param.len && i+1<nargs) {
                        param = args[++i];
                    }
                    for (; param.len && param[0]==' '; param = cuthead(param, 1)) {}
                    if (!param.len) {
                        return fatal(p, "Missing parameter for \"", {&c, 1},
                                     "\" option");
                    }
                    j = a.len;
                }
                r = shortopt(p, c, param);
            }
        }

        if (r) {
            return r;
        }
    }
    return ARGS_OK;
}

static i32 notyet(Parse *p, Str option)
{
    return fatal(p, option, ": not yet supported");
}

// Check and complete the options once all are parsed.
static i32 parsefinish(Parse *p)
{
    Options *opt = p->opt;
    fixfields(p);
    if (!opt->output.data) {
        opt->output = "tags";
    }
    if (!opt->files.len) {
        if (!opt->recurse) {
            return fatal(p, "No files specified. Try \"ctags --help\".");
        }
        opt->files = push(p->perm, opt->files, Str("."));
    }

    // Parsed, but not yet implemented: each check goes when its
    // milestone lands
    if (opt->fields != defaultfields()) return notyet(p, "--fields");  // M3a
    return ARGS_OK;
}

static i32 parseargs(Options *opt, Os *os, Str *args, i32 nargs, Arena *perm,
                     Buf *log)
{
    Parse p = {};
    parseinit(&p, opt, os, perm, log);
    i32 r = parseloop(&p, args+1, nargs-1);
    return r ? r : parsefinish(&p);
}

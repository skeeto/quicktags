// Command line: options as Universal Ctags parses them, from arguments
// and from option files
// This is free and unencumbered software released into the public domain.

enum : i32 { EXCMD_MIXED, EXCMD_NUMBER, EXCMD_PATTERN, EXCMD_COMBINE };
enum : i32 { SORT_NO, SORT_YES, SORT_FOLD };
enum : i32 { WRITER_UCTAGS, WRITER_ECTAGS, WRITER_ETAGS };
enum : i32 { TREL_NO, TREL_YES, TREL_ALWAYS, TREL_NEVER };

// Every option is parsed into its value here. Some are not acted on yet,
// and parsefinish refuses them.
struct Options {
    Str        output;       // "-" is standard output
    Slice<Str> files;
    Kinds      kinds;
    b32        filekind;     // the file kind, F, which all languages share
    u32        extras;       // EXTRA_*
    u64        fields;       // one bit per FIELD_*
    i32        jobs;         // 0: one per processor
    i32        excmd;        // EXCMD_*
    i32        sort;         // SORT_*
    i32        format;       // tags file format, 2 by default
    u32        patmax;       // pattern length limit, 0 for none
    i32        writer;       // WRITER_*
    i32        tagrelative;  // TREL_*
    i32        force;        // forced language, LANG_NONE for none
    u32        langs;        // enabled languages, one bit per LANG_*
    Slice<Str> excludes;     // --exclude values, as given
    Slice<Str> exceptions;   // --exclude-exception values, as given
    Slice<Str> ignores;      // -I lists, as given
    Str        listfile;     // -L, null for none
    Str        print;        // for ARGS_EXIT: what to print
    b32        append;
    b32        recurse;
    b32        nolinks;      // --links=no
    b32        mapped;       // --langmap or --map-LANG changed our maps
    b32        headers;      // -h changed the header extensions
    b32        fieldsreset;  // --fields reset them: no fixed-field warnings
    b32        stats;
    b32        nofast;
    b32        windows;      // '\' and ':' separate paths
};

static Str usage =
"usage: ctags [options] [file(s)]\n"
"Tags C, C++, and Fortran files, and directories recursively.\n"
"  -R, --recurse[=yes|no]  with no files, tag the current directory\n"
"  -f FILE, -o FILE        write tags to FILE, - for standard output\n"
"                          (default: tags)\n"
"  --kinds-LANG=[+|-]KINDS enable or disable tag kinds for C, C++, or\n"
"                          Fortran, as letters or {names}\n"
"  --extras=[+|-]EXTRAS    q: also tag C and C++ names qualified by\n"
"                          scope (ns::Class::f), F: file-scoped tags,\n"
"                          p: pseudo-tags (default: Fp)\n"
"  --options=FILE|DIR      read options from FILE, one per line, or from\n"
"                          the *.ctags files in DIR\n"
"  --jobs=N                threads, 0 for one per processor (default: 0)\n"
"  --list-languages, --list-kinds[=LANG], --list-kinds-full[=LANG],\n"
"  --list-extras[=LANG], --list-fields[=LANG]\n"
"                          list them as Universal Ctags does\n"
"  --version               print the version\n"
"  --help                  print this message\n"
"Universal Ctags' other options are accepted where they change nothing\n"
"here, as are those naming its other languages, and refused otherwise.\n";

enum : i32 {
    ARGS_OK,
    ARGS_EXIT,   // print opt->print, exit 0
    ARGS_FATAL,  // printed the problem, exit 1
};

// The state of parsing, shared by the arguments and the option files.
struct Parse {
    Options   *opt;
    Os        *os;
    Arena     *perm;
    Buf       *log;
    Str        optfile;   // the option file being read, null for arguments
    Map<b32>  *read;      // option files read, by path
    i32        depth;     // option files being read
    Slice<Str> langdefs;  // languages defined by --langdef
    b32        seenf;     // -f or -o given
};

enum : i32 { OPTFILEDEPTH = 16 };

static Str errstr(i32 err);  // walk.cpp


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

static i32 badvalue(Parse *p, Str option)
{
    return fatal(p, "Invalid value for \"", option, "\" option");
}

// An option Universal Ctags has that this program never will, because
// it would change which tags exist or write another kind of output.
static i32 unsupported(Parse *p, Str a, Str b = {})
{
    return fatal(p, a, b, ": not supported");
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

struct Ulong {
    u32 value;
    b32 digits;    // any were read
    b32 overflow;
    b32 whole;     // the number ran to the end
};

// Parse a number as the Windows C runtime's strtoul() and scanf("%u")
// do, with 32-bit longs: leading whitespace, a sign, and for base 0 a
// "0x" or "0" prefix. A negative number wraps around.
static Ulong parseulong(Str s, i32 base)
{
    Ulong r = {};
    iz    i = 0;
    for (; i<s.len && whitespace(s[i]); i++) {}
    b32 neg = i<s.len && s[i]=='-';
    i += i<s.len && (s[i]=='-' || s[i]=='+');
    if (!base) {
        base = i<s.len && s[i]=='0' ? 8 : 10;
        if (base==8 && i+2<s.len && (s[i+1]=='x' || s[i+1]=='X')) {
            u8 c = lowercase(s[i+2]);
            if (digit(c) || (c>='a' && c<='f')) {
                base = 16;
                i += 2;
            }
        }
    }
    u64 v = 0;
    for (; i < s.len; i++) {
        u8  c = lowercase(s[i]);
        i32 d = digit(c) ? c-'0' : c>='a' && c<='z' ? c-'a'+10 : 99;
        if (d >= base) {
            break;
        }
        r.digits = 1;
        v = v*(u64)base + (u64)d;
        if (v > 0xffffffff) {
            r.overflow = 1;
            v = 0xffffffff;
        }
    }
    r.value = neg ? 0 - (u32)v : (u32)v;
    r.whole = i == s.len;
    return r;
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

// The languages Universal Ctags 6.2.1 has besides ours: those it lists,
// and two it hides. Options naming them are accepted and do nothing.
static Str foreignlangs[] = {
    "Abaqus", "Abc", "Ada", "Ant", "Asciidoc", "Asm", "Asp", "Autoconf",
    "AutoIt", "Automake", "Awk", "Basic", "Bats", "BETA", "BibLaTeX",
    "BibTeX", "C#", "Cargo", "Clojure", "CMake", "Cobol", "CobolFree",
    "CobolVariable", "CPreProcessor", "CSS", "Ctags", "CUDA", "D", "Diff",
    "DosBatch", "DTD", "DTS", "Eiffel", "Elixir", "Elm", "EmacsLisp",
    "Erlang", "Falcon", "Flex", "Forth", "FrontMatter",
    "FunctionParameters", "Fypp", "Gdbinit", "GDScript", "GemSpec", "Go",
    "GPerf", "Haskell", "Haxe", "HTML", "Iniconf", "Inko", "IPythonCell",
    "ITcl", "Java", "JavaProperties", "JavaScript", "JNI", "JSON", "Julia",
    "Kconfig", "Kotlin", "LdScript", "LEX", "Lisp", "LiterateHaskell",
    "Lua", "M4", "Make", "Man", "Markdown", "MatLab", "Meson",
    "MesonOptions", "Moose", "Myrddin", "NSIS", "ObjectiveC", "OCaml",
    "Org", "Pascal", "Passwd", "Perl", "Perl6", "PHP", "PkgConfig", "Pod",
    "PowerShell", "Protobuf", "PuppetManifest", "Python",
    "PythonEntryPoints", "PythonLoggingConfig", "QemuHX", "QML", "QtMoc",
    "Quarto", "R", "R6Class", "Rake", "Raku", "ReStructuredText", "REXX",
    "RMarkdown", "Robot", "RpmMacros", "RpmSpec", "RSpec", "Ruby", "Rust",
    "S4Class", "Scdoc", "Scheme", "SCSS", "SELinuxInterface",
    "SELinuxTypeEnforcement", "Sh", "SLang", "SML", "SQL", "SystemdUnit",
    "SystemTap", "SystemVerilog", "Tcl", "TclOO", "Terraform",
    "TerraformVariables", "Tex", "TeXBeamer", "Thrift", "TOML", "TTCN",
    "Txt2tags", "TypeScript", "TypeSpec", "Unknown", "V", "Varlink",
    "Vera", "Verilog", "VHDL", "Vim", "WindRes", "YACC", "YumRepo",
    "Zephir", "Zsh", "UniversalCtags", "CTagsSelfTest",
};

struct Named {
    i32 lang;  // LANG_*
    Str name;  // as Universal Ctags spells it, or as given if unknown
};

// Find a language by name, ignoring case: ours, one we don't parse
// (including those defined by --langdef), or LANG_NONE.
static Named langnamed(Parse *p, Str name)
{
    Named r = {};
    r.name = name;
    for (i32 i = LANG_C; i <= LANG_FORTRAN; i++) {
        if (equalsfold(name, langnames[i])) {
            r.lang = i;
            r.name = langnames[i];
            return r;
        }
    }
    for (iz i = 0; i < countof(foreignlangs); i++) {
        if (equalsfold(name, foreignlangs[i])) {
            r.lang = LANG_FOREIGN;
            r.name = foreignlangs[i];
            return r;
        }
    }
    for (iz i = 0; i < p->langdefs.len; i++) {
        if (equalsfold(name, p->langdefs[i])) {
            r.lang = LANG_FOREIGN;
            r.name = p->langdefs[i];
            return r;
        }
    }
    return r;
}

// The language named after an option's prefix, up to a '.' or ':', as in
// --regex-LANG and --param-LANG.NAME.
static Named langpart(Parse *p, Str option, iz prefix)
{
    Str rest = cuthead(option, prefix);
    iz  end  = 0;
    for (; end<rest.len && rest[end]!='.' && rest[end]!=':'; end++) {}
    if (end<rest.len && rest[end]==':') {
        warning(p, "using `:' as a separator is obsolete; use `.' instead: --",
                option);
    }
    return langnamed(p, end ? takehead(rest, end) : rest);
}

static i32 unknownlang(Parse *p, Str lang, Str option)
{
    return fatal(p, "Unknown language \"", lang, "\" in \"", option,
                 "\" option");
}

// Whether an extension, ignoring case, is one of our languages', which
// a map for another language would take (an approximation until the
// maps are ours to edit).
static b32 ourext(Str ext)
{
    static Str exts[] = {
        "c", "c++", "cc", "cp", "cpp", "cxx", "h", "h++", "hh", "hp", "hpp",
        "hxx", "inl", "f", "for", "ftn", "f77", "f90", "f95", "f03", "f08",
        "f15", "in",
    };
    for (iz i = 0; i < countof(exts); i++) {
        if (equalsfold(ext, exts[i])) return 1;
    }
    return 0;
}

enum : i32 { MAP_OK, MAP_BADLY, MAP_UNTERMINATED };

struct MapItem {
    Str item;  // an extension, or a pattern
    Str tail;  // after it
    i32 err;   // MAP_*
    b32 pattern;
};

// Parse the first item of a language map: ".ext", ending at '.', '(',
// ',' or the end, or "(pattern)", in which "\)" does not end it.
static MapItem mapitem(Str s)
{
    MapItem r = {};
    if (startswith(s, ".")) {
        iz end = 1;
        for (; end<s.len && s[end]!='.' && s[end]!='(' && s[end]!=','; end++) {}
        r.item = span(s.data+1, s.data+end);
        r.tail = cuthead(s, end);
    } else if (startswith(s, "(")) {
        iz end = 1;
        for (; end<s.len && s[end]!=')'; end++) {
            end += s[end]=='\\' && end+1<s.len && s[end+1]==')';
        }
        if (end == s.len) {
            r.err = MAP_UNTERMINATED;
            return r;
        }
        r.item    = span(s.data+1, s.data+end);
        r.tail    = cuthead(s, end+1);
        r.pattern = 1;
    } else {
        r.err = MAP_BADLY;
    }
    return r;
}

static i32 badmap(Parse *p, i32 err, Named lang)
{
    Str msg = err==MAP_UNTERMINATED ? Str("Unterminated file name pattern for ")
                                    : Str("Badly formed language map for ");
    return fatal(p, msg, lang.name, " language");
}

// Note whether mapping an item to a language would change ours.
static void mapchange(Parse *p, Named lang, MapItem m)
{
    p->opt->mapped |= lang.lang!=LANG_FOREIGN || m.pattern || ourext(m.item);
}

// --langmap=LANG:[+]MAPS[,LANG:...], each item taken from every other
// language, or default for all or for one language.
static i32 langmapopt(Parse *p, Str name, Str value)
{
    if (value == "default") {
        p->opt->mapped = 0;
        return ARGS_OK;
    }
    for (Str maps = value; maps.len;) {
        Cut   c    = cut(maps, ':');
        Named lang = c.ok ? langnamed(p, c.head) : Named{};
        if (!lang.lang) {
            warning(p, "Unknown language \"", value, "\" in \"", name,
                    "\" option");
            return ARGS_OK;
        }
        Str list = c.tail;
        b32 add  = startswith(list, "+");
        list = cuthead(list, add);
        Str first = cut(list, ',').head;
        if (equalsfold(first, "default")) {
            list = cuthead(list, first.len);
        } else {
            p->opt->mapped |= lang.lang!=LANG_FOREIGN && !add;
            while (list.len && list[0]!=',') {
                MapItem m = mapitem(list);
                if (m.err) {
                    return badmap(p, m.err, lang);
                }
                mapchange(p, lang, m);
                list = m.tail;
            }
        }
        maps = cuthead(list, startswith(list, ","));
    }
    return ARGS_OK;
}

// --map-LANG=[+|-]MAP: one item added, removed, or made the whole map.
static i32 mapopt(Parse *p, Str name, Str value)
{
    Named lang = langpart(p, name, 4);
    if (!lang.lang) {
        return unknownlang(p, lang.name, name);
    } else if (!value.len) {
        return fatal(p, "no parameter is given for ", name);
    }
    b32     remove = value[0] == '-';
    MapItem m      = mapitem(cuthead(value, remove || value[0]=='+'));
    if (m.err) {
        return badmap(p, m.err, lang);
    }
    if (!remove) {
        mapchange(p, lang, m);
    }
    p->opt->mapped |= lang.lang != LANG_FOREIGN;
    return ARGS_OK;
}

// --languages=[+|-]LIST: replace, add to, or remove from the enabled
// languages, the mode carrying on through the list.
static i32 languagesopt(Parse *p, Str name, Str value)
{
    u32 *langs = &p->opt->langs;
    u8   mode  = 0;  // 0 to replace
    Cut  c     = {};
    c.tail = value;
    do {
        c = cut(c.tail, ',');
        Str lang = c.head;
        if (startswith(lang, "+") || startswith(lang, "-")) {
            mode = lang[0];
            lang = cuthead(lang, 1);
        }
        if (!mode) {
            *langs = 0;
        }
        if (!lang.len) {
            continue;
        }
        if (lang == "all") {
            *langs = mode=='-' ? 0u : (u32)LANGS_ALL;
        } else {
            Named l = langnamed(p, lang);
            if (!l.lang) {
                warning(p, "Unknown language \"", lang, "\" in \"", name,
                        "\" option");
            } else if (l.lang != LANG_FOREIGN) {
                *langs &= ~(1u << l.lang);
                *langs |= (u32)(mode!='-') << l.lang;
            }
        }
        mode = mode ? mode : '+';
    } while (c.ok);
    return ARGS_OK;
}

// --language-force=LANG|auto, and the old --lang and --language.
static i32 forceopt(Parse *p, Str name, Str value)
{
    if (name != "language-force") {
        warning(p, "\"--", name, "\" option is obsolete; use "
                "\"--language-force\" instead");
    }
    if (equalsfold(value, "auto")) {
        p->opt->force = LANG_NONE;
        return ARGS_OK;
    }
    Named lang = langnamed(p, value);
    if (!lang.lang) {
        return unknownlang(p, value, name);
    }
    p->opt->force = lang.lang;
    return ARGS_OK;
}

// --langdef=NAME[{flags}] defines a language, which this program does
// not parse, so that options may name it.
static i32 langdefopt(Parse *p, Str name, Str value)
{
    Str lang = cut(value, '{').head;
    if (!lang.len) {
        return fatal(p, "No language specified for \"", name, "\" option");
    } else if (langnamed(p, lang).lang) {
        return fatal(p, "Language \"", lang, "\" already defined");
    } else if (lang=="all" || lang=="NONE") {
        return fatal(p, "\"", lang, "\" is reserved; don't use it as the "
                     "name for defining a new language");
    }
    for (iz i = 0; i < lang.len; i++) {
        u8 c = lang[i];
        if (!alnum(c) && c!='+' && c!='#') {
            if (c=='`' || c=='\'') {
                return fatal(p, "don't use \"", {&c, 1},
                             "\" in a language name (", lang, ")");
            }
            return fatal(p, "don't use `", {&c, 1}, "' in a language name (",
                         lang, ")");
        }
    }
    p->langdefs = push(p->perm, p->langdefs, lang);
    return ARGS_OK;
}


// Kinds, extras, and fields

// Apply a language's kinds specification. For one not parsed here, only
// the file kind, which every language shares, changes anything.
static i32 kindspec(Parse *p, i32 lang, Str spec, Str option)
{
    KindTable t    = kindtable(&p->opt->kinds, lang);
    u64       bits = t.bits ? *t.bits : 0;
    i32       d    = t.bits ? SPEC_KINDS : SPEC_FOREIGNKINDS;
    Spec      s    = flagspec(&bits, t.kinds, t.len, spec, d, option, p->log);
    if (t.bits) {
        *t.bits = (u32)bits;
    }
    if (s.file) {
        p->opt->filekind = s.file > 0;
    }
    return s.ok ? ARGS_OK : ARGS_FATAL;
}

// Store the extras a specification left, with those it set: setting the
// inputFile extra sets the file kind to the same state.
static void setextras(Parse *p, u64 bits, u64 set)
{
    Options *opt = p->opt;
    if (set & EXTRA_INPUTFILE) {
        opt->filekind = (bits & EXTRA_INPUTFILE) != 0;
    }
    opt->extras = (u32)bits;
}

// --kinds-all, which takes only "*", "F", "{file}" (the file kind), and
// a "+" or "-" before either of the last two, for every language.
static i32 kindsall(Parse *p, Str name, Str value)
{
    b32 afterop = 0;
    for (iz i = 0; i < value.len;) {
        u8 c = value[i];
        if (c=='+' || c=='-') {
            if (afterop) {
                return fatal(p, "don't repeat + (nor -) in --", name,
                             " option");
            }
            afterop = 1;
            i++;
        } else if (c == '*') {
            if (afterop) {
                return fatal(p, "don't use '*' after + (nor -) in --", name,
                             " option");
            }
            i++;
        } else if (c=='F' || startswith(cuthead(value, i), "{file}")) {
            afterop = 0;
            i += c=='F' ? 1 : 6;
        } else {
            return fatal(p, "only '*', 'F', \"{file}\" or their combination "
                         "is acceptable as kind letter for --", name);
        }
    }
    if (afterop) {
        return fatal(p, "no kind specification after + (or -) in --", name,
                     " option");
    }
    for (i32 lang = LANG_C; lang <= LANG_FORTRAN; lang++) {
        if (kindspec(p, lang, value, name)) {
            return ARGS_FATAL;
        }
    }
    return ARGS_OK;
}

// --kinds-LANG, or the old --LANG-kinds and --LANG-types where the
// language runs up to the first '-'.
static i32 kindsopt(Parse *p, Str name, Str value)
{
    Cut dash  = cut(name, '-');
    b32 old   = dash.tail=="kinds" || dash.tail=="types";
    Str lname = old ? dash.head : cuthead(name, 6);
    if (lname == "all") {
        if (old) {
            warning(p, "\"--", name, "\" option is obsolete; use "
                    "\"--kinds-all\" instead");
        }
        return kindsall(p, name, value);
    } else if (!lname.len && !old) {
        warning(p, "No language given in \"", name, "\" option");
        return ARGS_OK;
    }
    Named lang = langnamed(p, lname);
    if (!lang.lang) {
        warning(p, "Unknown language \"", lname, "\" in \"", name, "\" option");
        return ARGS_OK;
    }
    return kindspec(p, lang.lang, value, name);
}

// --fields-LANG and --extras-LANG, by name only, where LANG may be "all"
// for every common and per-language entry.
static i32 langspec(Parse *p, Str name, Str value, b32 isfields)
{
    Options *opt   = p->opt;
    Str      lname = cuthead(name, 7);
    if (!lname.len) {
        return fatal(p, "No language given in \"", name, "\" option");
    }

    // The language's own entries: C and C++ have fields, Fortran an extra
    i32 beg = 0;
    i32 end = 0;
    if (isfields) {
        end = NFIELDS;
    } else {
        end = NEXTRAS;
    }
    if (lname != "all") {
        Named lang = langnamed(p, lname);
        if (!lang.lang) {
            warning(p, "Unknown language: ", lname, " (ignoring \"--", name,
                    "\")");
            return ARGS_OK;
        } else if (lang.lang == LANG_FOREIGN) {
            return ARGS_OK;
        } else if (!isfields) {
            beg = lang.lang==LANG_FORTRAN ? NCOMMONEXTRAS : NEXTRAS;
        } else if (lang.lang == LANG_C) {
            beg = FIELD_C;
            end = FIELD_CXX;
        } else {
            beg = lang.lang==LANG_CXX ? FIELD_CXX : NFIELDS;
        }
    }

    Kind *table = isfields ? fields : extras;
    u64   bits  = isfields ? opt->fields : opt->extras;
    u64   mask  = (((u64)1 << (end - beg)) - 1) << beg;
    u64   part  = bits >> beg;
    i32   kind  = isfields ? SPEC_LANGFIELDS : SPEC_LANGEXTRAS;
    Spec  s     = flagspec(&part, table+beg, end-beg, value, kind, name,
                           p->log);
    bits = (bits & ~mask) | (part<<beg & mask);
    if (isfields) {
        opt->fields = bits;
        opt->fieldsreset |= s.reset && lname=="all";
    } else {
        setextras(p, bits, s.set<<beg & mask);
    }
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

// --roles-LANG[.KIND]=[+|-]{role}...: roles only mark reference tags,
// which are not written, so this checks its arguments, then does nothing.
static i32 rolesopt(Parse *p, Str name, Str value)
{
    Str lname = cuthead(name, 6);
    b32 any   = !value.len || value=="*";
    if (!lname.len) {
        warning(p, "no language given in \"", name, "\" option");
        return ARGS_OK;
    } else if (startswith(lname, "all")) {
        Str rest = cuthead(lname, 3);
        if (!rest.len || rest==".*") {
            return any ? ARGS_OK
                       : fatal(p, "only '*' or '' (empty string) is "
                               "acceptable as an argument for --", name, ": ",
                               value);
        } else if (rest[0] == '.') {
            return fatal(p, "only '*' or '' (empty string) is acceptable as "
                         "a kind spec for --roles-all: --", name);
        }
    }

    Cut   dot  = cut(lname, '.');
    Named lang = langnamed(p, dot.head);
    if (!lang.lang) {
        warning(p, "unknown language \"", dot.head, "\" in --", name,
                " option");
        return ARGS_OK;
    } else if (!dot.ok || dot.tail=="*") {
        return any ? ARGS_OK
                   : fatal(p, "only '*' or '' (empty string) is acceptable "
                           "as an argument for --", name, ": ", value);
    } else if (lang.lang == LANG_FOREIGN) {
        return ARGS_OK;
    }

    KindTable t    = kindtable(&p->opt->kinds, lang.lang);
    Str       spec = dot.tail;
    Kind     *kind = 0;
    if (!spec.len || (spec[0]!='{' && !letter(spec[0]))) {
        Str c = spec.len ? takehead(spec, 1) : Str("");
        return fatal(p, "'", c, "', unexpected character in --", name);
    }
    Cut kname = cut(cuthead(spec, 1), '}');
    if (spec[0]=='{' && !kname.ok) {
        return fatal(p, "no '}' representing the end of kind name in --",
                     name, " option: ", spec);
    } else if (spec[0]=='F' || (spec[0]=='{' && kname.head=="file")) {
        warning(p, "don't enable/disable a role in F/file kind; it has no "
                "role: --", name);
        return ARGS_OK;
    }
    for (i32 i = 0; i<t.len && !kind; i++) {
        Kind *k     = t.kinds + i;
        b32   match = spec[0]=='{' ? k->name==kname.head : k->letter==spec[0];
        kind = match ? k : 0;
    }
    if (!kind) {
        warning(p, "no such kind ", spec[0]=='{' ? Str("name") : Str("letter"),
                " as specified in --", name, " option");
        return ARGS_OK;
    } else if (spec[0]=='{' && kname.tail.len) {
        return fatal(p, "garbage after the kind specification {", kname.head,
                     "} in --", name, " option");
    } else if (spec[0]!='{' && spec.len>1) {
        return fatal(p, "garbage after the kind specification '",
                     takehead(spec, 1), "' in --", name, " option");
    }

    Str kl = {&kind->letter, 1};
    for (iz i = 0; i < value.len;) {
        u8 c = value[i];
        if (c=='+' || c=='-' || c=='*') {
            i++;
            continue;
        } else if (c != '{') {
            return fatal(p, "unexpected character ", {&value[i], 1}, " in --",
                         name, "=", value, " option");
        }
        Cut role = cut(cuthead(value, i+1), '}');
        if (!role.ok) {
            return fatal(p, "no '}' representing the end of role name in --",
                         name, " option: ", cuthead(value, i+1));
        } else if (!role.head.len) {
            return fatal(p, "empty role for the kind letter: ", kl);
        } else if (!countroles(lang.lang, kind->letter, role.head)) {
            warning(p, "no such role: \"", role.head, "\" in kind '", kl,
                    "' in language \"", lang.name, "\"");
        }
        i = value.len - role.tail.len;
    }
    return ARGS_OK;
}

// --param-LANG.NAME=VALUE: our languages have no parameters, and of the
// C preprocessor's, ignore is -I, and the others would change which
// tags exist, but for _expand and if0 off, the defaults.
static i32 paramopt(Parse *p, Str name, Str value)
{
    Named lang = langpart(p, name, 6);
    if (!lang.lang) {
        return unknownlang(p, lang.name, name);
    }
    Str rest = cuthead(name, 6+lang.name.len);
    if (!rest.len || (rest[0]!='.' && rest[0]!=':')) {
        return fatal(p, "no separator(.) is given for ", name, "=", value);
    } else if (!value.len) {
        return fatal(p, "no value is given for ", name);
    }
    Str param = cuthead(rest, 1);
    if (lang.lang!=LANG_FOREIGN) {
        return fatal(p, "no such parameter in ", lang.name, ": ", param);
    } else if (lang.name != "CPreProcessor") {
        return ARGS_OK;
    }
    if (param == "ignore") {
        p->opt->ignores = push(p->perm, p->opt->ignores, value);
        return ARGS_OK;
    } else if (param=="_expand" || param=="if0") {
        i32 b = parsebool(value);
        if (b < 0) {
            return fatal(p, "Invalid value for \"", param, "\" parameter");
        }
        return b ? unsupported(p, "--", name) : ARGS_OK;
    } else if (param == "define") {
        return unsupported(p, "--", name);
    }
    return fatal(p, "no such parameter in ", lang.name, ": ", param);
}


// Option files

static i32 parseloop(Parse *, Str *args, iz nargs);

// Read an option file, one option per line, and blank lines and those
// starting with '#' skipped. A file already read is skipped.
static i32 readoptions(Parse *p, Str path)
{
    b32 *seen = upsert(&p->read, path, p->perm);
    if (*seen) {
        return ARGS_OK;
    }
    *seen = 1;
    if (p->depth == OPTFILEDEPTH) {
        return fatal(p, "too many nested option files: \"", path, "\"");
    }

    Read r = os_read(p->os, p->perm, path, (iz)1<<30);
    if (r.err) {
        return fatal(p, "cannot open option file \"", path, "\" : ",
                     errstr(r.err));
    }
    Slice<Str> lines = splitlines(p->perm, r.data, LINES_TRIM|LINES_COMMENTS);

    Str outer = p->optfile;
    p->optfile = path;
    p->depth++;
    i32 status = parseloop(p, lines.data, lines.len);
    p->depth--;
    p->optfile = outer;
    return status;
}

// --options=FILE|DIR|NONE: read a file, or a directory's *.ctags files
// in name order. NONE, which Universal Ctags takes only before other
// options, reads nothing, as no option files are read by default here.
static i32 optionsopt(Parse *p, Str name, Str value)
{
    if (!value.len) {
        return fatal(p, "no option file supplied for \"", name, "\"");
    } else if (value == "NONE") {
        return ARGS_OK;
    }

    Stat s = os_stat(p->os, *p->perm, value);
    if (s.type == STAT_NONE) {
        return fatal(p, "cannot stat \"", value, "\" : ", errstr(s.err));
    } else if (s.type != STAT_DIR) {
        return readoptions(p, value);
    }

    Listing l = os_list(p->os, p->perm, value);
    if (l.err) {
        return fatal(p, "cannot open option directory \"", value, "\" : ",
                     errstr(l.err));
    }
    Slice<Str> names = {};
    for (Dirent *d = l.head; d; d = d->next) {
        Str n = d->name;
        if (!d->isdir && n.len>6 && n[0]!='.' && endswith(n, ".ctags")) {
            names = push(p->perm, names, n);
        }
    }
    sort(names.data, names.len, strless, *p->perm);

    u8  last = value[value.len-1];
    b32 sep  = last=='/' || (p->opt->windows && last=='\\');
    for (iz i = 0; i < names.len; i++) {
        Str path = concat(p->perm, value, sep ? Str("") : Str("/"));
        path = concat(p->perm, path, names[i]);
        i32 r = readoptions(p, path);
        if (r) {
            return r;
        }
    }
    return ARGS_OK;
}


// Listings, as Universal Ctags writes them, of our languages

// Print a table as the --list-* commands do: each column but the last
// padded to its widest cell, then a space, and the header row marked
// with a '#'. The cells are row by row, the header first.
static void printtable(Buf *b, Slice<Str> cells, i32 ncols)
{
    iz width[8] = {};
    assert(ncols <= countof(width));
    for (iz i = 0; i < cells.len; i++) {
        iz c = i % ncols;
        width[c] = cells[i].len>width[c] ? cells[i].len : width[c];
    }
    width[0]++;  // the '#'
    for (iz i = 0; i < cells.len; i++) {
        iz c   = i % ncols;
        iz len = cells[i].len;
        if (!i) {
            putbyte(b, '#');
            len++;
        }
        print(b, cells[i]);
        if (c+1 == ncols) {
            putbyte(b, '\n');
        } else {
            for (; len < width[c]; len++) {
                putbyte(b, ' ');
            }
            putbyte(b, ' ');
        }
    }
}

static Str yesno(b32 b)
{
    return b ? Str("yes") : Str("no");
}

static Str letterof(Kind *k)
{
    return k->letter ? Str{&k->letter, 1} : Str("-");
}

static Str kinddesc(i32 lang, i32 kind)
{
    if (lang==LANG_C && kind==K_MEMBER) {
        return "struct, and union members";
    }
    return lang==LANG_FORTRAN ? ftnkinds[kind].desc : cxxkinds[kind].desc;
}

// The language a --list-* option names: LANG_NONE for all of them, or
// with ok cleared, one Universal Ctags does not know.
struct Listed {
    i32 lang;
    b32 ok;
};

static Listed listed(Parse *p, Str value)
{
    Listed r = {};
    r.ok = 1;
    if (value.len && !equalsfold(value, "all")) {
        r.lang = langnamed(p, value).lang;
        r.ok   = r.lang != LANG_NONE;
    }
    return r;
}

static void listkinds(Buf *b, Options *opt, i32 lang, b32 indent)
{
    KindTable t = kindtable(&opt->kinds, lang);
    for (i32 i = 0; i < t.len; i++) {
        print(b, indent ? Str("    ") : Str(""));
        putbyte(b, t.kinds[i].letter);
        print(b, "  ");
        print(b, kinddesc(lang, i));
        print(b, kindon(&opt->kinds, lang, i) ? Str("") : Str(" [off]"));
        putbyte(b, '\n');
    }
}

static b32 letterless(Kind **a, Kind **b)
{
    return (*a)->letter < (*b)->letter;
}

// Add a language's kinds as --list-kinds-full lists them, by letter.
static Slice<Str> fullkinds(Arena *a, Slice<Str> cells, Options *opt,
                            i32 lang, b32 named)
{
    KindTable t     = kindtable(&opt->kinds, lang);
    Kind    **kinds = alloc<Kind *>(a, t.len);
    for (i32 i = 0; i < t.len; i++) {
        kinds[i] = t.kinds + i;
    }
    sort(kinds, t.len, letterless, *a);
    for (i32 i = 0; i < t.len; i++) {
        Kind *k     = kinds[i];
        i32   kind  = (i32)(k - t.kinds);
        Buf   roles(a, 4);
        print(&roles, (i64)countroles(lang, k->letter, {}));
        if (named) {
            cells = push(a, cells, langnames[lang]);
        }
        cells = push(a, cells, letterof(k));
        cells = push(a, cells, k->name);
        cells = push(a, cells, yesno(kindon(&opt->kinds, lang, kind)));
        cells = push(a, cells, yesno(k==cxxkinds+K_HEADER));
        cells = push(a, cells, finish(&roles));
        b32 shared = lang!=LANG_FORTRAN && kind<NCKINDS;
        cells = push(a, cells, shared ? Str("C") : Str("NONE"));
        cells = push(a, cells, kinddesc(lang, kind));
    }
    return cells;
}

// --list-kinds[=LANG] and --list-kinds-full[=LANG]. Other languages'
// kinds are unknown here, so they list none.
static i32 listkindsopt(Parse *p, Str name, Str value)
{
    Options *opt  = p->opt;
    Listed   l    = listed(p, value);
    b32      full = name == "list-kinds-full";
    if (!l.ok) {
        return unknownlang(p, value, name);
    }

    Buf b(p->perm);
    if (!full) {
        for (i32 lang = LANG_C; lang <= LANG_FORTRAN; lang++) {
            if (!l.lang) {
                print(&b, langnames[lang]);
                print(&b, opt->langs>>lang&1 ? Str("") : Str(" [disabled]"));
                putbyte(&b, '\n');
            }
            if (!l.lang || l.lang==lang) {
                listkinds(&b, opt, lang, !l.lang);
            }
        }
    } else {
        Slice<Str> cells = {};
        Str header[] = {
            "LANGUAGE", "LETTER", "NAME", "ENABLED", "REFONLY", "NROLES",
            "MASTER", "DESCRIPTION",
        };
        for (iz i = !!l.lang; i < countof(header); i++) {
            cells = push(p->perm, cells, header[i]);
        }
        for (i32 lang = LANG_C; lang <= LANG_FORTRAN; lang++) {
            if (!l.lang || l.lang==lang) {
                cells = fullkinds(p->perm, cells, opt, lang, !l.lang);
            }
        }
        printtable(&b, cells, (i32)countof(header) - !!l.lang);
    }
    opt->print = finish(&b);
    return ARGS_EXIT;
}

// --list-extras[=LANG|NONE]: the common extras, and Fortran's.
static i32 listextrasopt(Parse *p, Str name, Str value)
{
    Options *opt  = p->opt;
    b32      none = equalsfold(value, "NONE");
    Listed   l    = none ? Listed{} : listed(p, value);
    if (!none && !l.ok) {
        return unknownlang(p, value, name);
    }

    Slice<Str> cells = {};
    Str header[] = {"LETTER", "NAME", "ENABLED", "LANGUAGE", "FIXED",
                    "DESCRIPTION"};
    for (iz i = 0; i < countof(header); i++) {
        cells = push(p->perm, cells, header[i]);
    }
    for (i32 i = 0; i < NEXTRAS; i++) {
        b32 common = i < NCOMMONEXTRAS;
        if (l.lang ? l.lang==LANG_FORTRAN && !common : common || !none) {
            Kind *x = extras + i;
            cells = push(p->perm, cells, letterof(x));
            cells = push(p->perm, cells, x->name);
            cells = push(p->perm, cells, yesno(opt->extras>>i & 1));
            cells = push(p->perm, cells, common ? Str("NONE") : Str("Fortran"));
            cells = push(p->perm, cells, Str("no"));
            cells = push(p->perm, cells, x->desc);
        }
    }
    Buf b(p->perm);
    printtable(&b, cells, (i32)countof(header));
    opt->print = finish(&b);
    return ARGS_EXIT;
}

// --list-fields[=LANG|NONE]: the common fields, and C's and C++'s, after
// enabling those that must be.
static i32 listfieldsopt(Parse *p, Str name, Str value)
{
    Options *opt  = p->opt;
    b32      none = equalsfold(value, "NONE");
    Listed   l    = none ? Listed{} : listed(p, value);
    if (!none && !l.ok) {
        return unknownlang(p, value, name);
    }
    if (opt->writer != WRITER_ETAGS) {
        fixfields(p);
    }

    Slice<Str> cells = {};
    Str header[] = {"LETTER", "NAME", "ENABLED", "LANGUAGE", "JSTYPE",
                    "FIXED", "OP", "DESCRIPTION"};
    for (iz i = 0; i < countof(header); i++) {
        cells = push(p->perm, cells, header[i]);
    }
    for (i32 i = 0; i < NFIELDS; i++) {
        i32 lang = i<FIELD_C ? LANG_NONE : i<FIELD_CXX ? LANG_C : LANG_CXX;
        if (l.lang ? l.lang==lang : !lang || !none) {
            Kind *f      = fields + i;
            b32   common = lang == LANG_NONE;
            cells = push(p->perm, cells, letterof(f));
            cells = push(p->perm, cells, f->name.data ? f->name : Str("NONE"));
            cells = push(p->perm, cells, yesno(fieldon(opt->fields, i)));
            cells = push(p->perm, cells, langnames[lang]);
            cells = push(p->perm, cells, common ? fieldtypes[i] : Str("s--"));
            cells = push(p->perm, cells, yesno(i <= FIELD_PATTERN));
            cells = push(p->perm, cells, common ? fieldops[i] : Str("--"));
            cells = push(p->perm, cells, f->desc);
        }
    }
    Buf b(p->perm);
    printtable(&b, cells, (i32)countof(header));
    opt->print = finish(&b);
    return ARGS_EXIT;
}

// --list-languages, and its categories, which have none of ours.
static i32 listlangsopt(Parse *p, Str value)
{
    Options *opt = p->opt;
    Buf      b(p->perm, 64);
    if (value!="_libxml" && value!="_libyaml" && value!="_packcc") {
        for (i32 lang = LANG_C; lang <= LANG_FORTRAN; lang++) {
            print(&b, langnames[lang]);
            print(&b, opt->langs>>lang&1 ? Str("") : Str(" [disabled]"));
            putbyte(&b, '\n');
        }
    }
    opt->print = finish(&b);
    return ARGS_EXIT;
}


// Long options

static void etagsmode(Options *opt)
{
    opt->writer      = WRITER_ETAGS;
    opt->sort        = SORT_NO;
    opt->tagrelative = TREL_YES;
}

enum : i32 {
    OPT_APPEND, OPT_EXCLUDE, OPT_EXCEPTION, OPT_EXCMD, OPT_EXTRAS,
    OPT_FIELDS, OPT_FILESCOPE, OPT_FILETAGS, OPT_FORMAT, OPT_HELP, OPT_IF0,
    OPT_JOBS, OPT_FORCE, OPT_LANGUAGES, OPT_LANGDEF, OPT_LANGMAP,
    OPT_LINEDIRS, OPT_LINKS, OPT_LISTEXTRAS, OPT_LISTFIELDS, OPT_LISTKINDS,
    OPT_LISTLANGS, OPT_OPTIONS, OPT_OUTFORMAT, OPT_PATLIMIT, OPT_QUIET,
    OPT_RECURSE, OPT_SORT, OPT_TAGRELATIVE, OPT_TOTALS, OPT_VERBOSE,
    OPT_VERSION, OPT_STATS, OPT_NOFAST,
};

static struct { Str name; i32 id; } longopts[] = {
    {"append",               OPT_APPEND},
    {"exclude",              OPT_EXCLUDE},
    {"exclude-exception",    OPT_EXCEPTION},
    {"excmd",                OPT_EXCMD},
    {"extra",                OPT_EXTRAS},
    {"extras",               OPT_EXTRAS},
    {"fields",               OPT_FIELDS},
    {"file-scope",           OPT_FILESCOPE},
    {"file-tags",            OPT_FILETAGS},
    {"format",               OPT_FORMAT},
    {"help",                 OPT_HELP},
    {"if0",                  OPT_IF0},
    {"jobs",                 OPT_JOBS},
    {"lang",                 OPT_FORCE},
    {"language",             OPT_FORCE},
    {"language-force",       OPT_FORCE},
    {"languages",            OPT_LANGUAGES},
    {"langdef",              OPT_LANGDEF},
    {"langmap",              OPT_LANGMAP},
    {"line-directives",      OPT_LINEDIRS},
    {"links",                OPT_LINKS},
    {"list-extras",          OPT_LISTEXTRAS},
    {"list-fields",          OPT_LISTFIELDS},
    {"list-kinds",           OPT_LISTKINDS},
    {"list-kinds-full",      OPT_LISTKINDS},
    {"list-languages",       OPT_LISTLANGS},
    {"options",              OPT_OPTIONS},
    {"output-format",        OPT_OUTFORMAT},
    {"pattern-length-limit", OPT_PATLIMIT},
    {"quiet",                OPT_QUIET},
    {"recurse",              OPT_RECURSE},
    {"sort",                 OPT_SORT},
    {"tag-relative",         OPT_TAGRELATIVE},
    {"totals",               OPT_TOTALS},
    {"verbose",              OPT_VERBOSE},
    {"version",              OPT_VERSION},
    {"_nofast",              OPT_NOFAST},
    {"_stats",               OPT_STATS},
};

// Families of options that name a language, by prefix, in the order
// Universal Ctags tries them: the first two before its exact names, the
// rest after.
enum : i32 {
    LOPT_FIELDS, LOPT_EXTRAS, LOPT_DEFINE, LOPT_KINDS, LOPT_MAP, LOPT_PARAM,
    LOPT_ROLES,
};

static struct { Str prefix; i32 id; } langopts[] = {
    {"fields-",      LOPT_FIELDS},
    {"extras-",      LOPT_EXTRAS},
    {"kinddef-",     LOPT_DEFINE},
    {"kinds-",       LOPT_KINDS},   // also the old --LANG-kinds
    {"regex-",       LOPT_DEFINE},
    {"mline-regex-", LOPT_DEFINE},
    {"map-",         LOPT_MAP},
    {"param-",       LOPT_PARAM},
    {"roles-",       LOPT_ROLES},
};

// The family of an option, an index into langopts, or -1. Some families
// do not take an option without a language, which is then unknown.
static i32 langfamily(Str name)
{
    for (i32 i = 0; i < countof(langopts); i++) {
        Str prefix = langopts[i].prefix;
        i32 id     = langopts[i].id;
        Cut dash   = cut(name, '-');
        if (id==LOPT_KINDS && (dash.tail=="kinds" || dash.tail=="types")) {
            return i;
        } else if (startswith(name, prefix) &&
                   (name.len>prefix.len || id==LOPT_FIELDS ||
                    id==LOPT_EXTRAS || id==LOPT_KINDS || id==LOPT_ROLES)) {
            return i;
        }
    }
    return -1;
}

static i32 langopt(Parse *p, i32 family, Str name, Str value)
{
    switch (langopts[family].id) {
    case LOPT_FIELDS: return langspec(p, name, value, 1);
    case LOPT_EXTRAS: return langspec(p, name, value, 0);
    case LOPT_KINDS:  return kindsopt(p, name, value);
    case LOPT_MAP:    return mapopt(p, name, value);
    case LOPT_PARAM:  return paramopt(p, name, value);
    case LOPT_ROLES:  return rolesopt(p, name, value);
    case LOPT_DEFINE: {
        // --regex-LANG, --mline-regex-LANG, and --kinddef-LANG, which
        // would change which tags exist
        Named lang = langpart(p, name, langopts[family].prefix.len);
        if (!lang.lang) {
            return unknownlang(p, lang.name, name);
        }
        return lang.lang==LANG_FOREIGN ? ARGS_OK : unsupported(p, "--", name);
    }
    }
    return ARGS_OK;
}

static i32 exactopt(Parse *p, i32 id, Str name, Str value)
{
    Options *opt = p->opt;

    i32 b = 0;
    switch (id) {
    case OPT_APPEND: case OPT_FILESCOPE: case OPT_FILETAGS: case OPT_IF0:
    case OPT_LINEDIRS: case OPT_LINKS: case OPT_QUIET: case OPT_RECURSE:
    case OPT_VERBOSE:
        b = parsebool(value);
        if (b < 0) {
            return badvalue(p, name);
        }
    }

    switch (id) {
    case OPT_APPEND:
        opt->append = b;
        return ARGS_OK;

    case OPT_EXCLUDE:
        opt->excludes = push(p->perm, opt->excludes, value);
        return ARGS_OK;

    case OPT_EXCEPTION:
        opt->exceptions = push(p->perm, opt->exceptions, value);
        return ARGS_OK;

    case OPT_EXCMD:
        switch (value.len ? value[0] : 0) {
        case 'm': opt->excmd = EXCMD_MIXED;   return ARGS_OK;
        case 'n': opt->excmd = EXCMD_NUMBER;  return ARGS_OK;
        case 'p': opt->excmd = EXCMD_PATTERN; return ARGS_OK;
        }
        if (value != "combine") {
            return fatal(p, "Invalid value for \"", name, "\" option: ", value);
        }
        opt->excmd = EXCMD_COMBINE;
        return ARGS_OK;

    case OPT_EXTRAS: {
        if (name == "extra") {
            warning(p, "--extra option is obsolete; use --extras instead");
        }
        u64  bits = opt->extras;
        Spec s    = flagspec(&bits, extras, NCOMMONEXTRAS, value,
                             SPEC_EXTRAS, name, p->log);
        setextras(p, bits, s.set);
        return s.ok ? ARGS_OK : ARGS_FATAL;
    }

    case OPT_FIELDS: {
        Spec s = flagspec(&opt->fields, fields, NCOMMONFIELDS, value,
                          SPEC_FIELDS, name, p->log);
        opt->fieldsreset |= s.reset;
        return s.ok ? ARGS_OK : ARGS_FATAL;
    }

    case OPT_FILESCOPE:
    case OPT_FILETAGS: {
        u8  sign  = b ? '+' : '-';
        u8  x     = id==OPT_FILESCOPE ? 'F' : 'f';
        u32 extra = id==OPT_FILESCOPE ? EXTRA_FILESCOPE : EXTRA_INPUTFILE;
        warning(p, "\"--", name, "\" option is obsolete; use \"--extras=",
                {&sign, 1}, {&x, 1}, "\" instead");
        setextras(p, (opt->extras & ~extra) | (b ? extra : 0u), extra);
        return ARGS_OK;
    }

    case OPT_FORMAT: {
        // As scanf("%u") reads it
        Ulong n = parseulong(value, 10);
        if (!n.digits) {
            return badvalue(p, name);
        } else if (n.overflow || n.value>2) {
            return fatal(p, "Unsupported value for \"", name, "\" option");
        }
        opt->format = (i32)n.value;
        return ARGS_OK;
    }

    case OPT_HELP: {
        Buf b(p->perm);
        print(&b, usage);
        printkinds(&b);
        opt->print = finish(&b);
        return ARGS_EXIT;
    }

    case OPT_IF0:
    case OPT_LINEDIRS:
        return b ? unsupported(p, "--", name) : ARGS_OK;

    case OPT_JOBS:
        opt->jobs = parsenum(value, 1024);
        return opt->jobs<0 ? badvalue(p, name) : ARGS_OK;

    case OPT_FORCE:     return forceopt(p, name, value);
    case OPT_LANGUAGES: return languagesopt(p, name, value);
    case OPT_LANGDEF:   return langdefopt(p, name, value);
    case OPT_LANGMAP:   return langmapopt(p, name, value);

    case OPT_LINKS:
        opt->nolinks = !b;
        return ARGS_OK;

    case OPT_LISTEXTRAS: return listextrasopt(p, name, value);
    case OPT_LISTFIELDS: return listfieldsopt(p, name, value);
    case OPT_LISTKINDS:  return listkindsopt(p, name, value);
    case OPT_LISTLANGS:  return listlangsopt(p, value);
    case OPT_OPTIONS:    return optionsopt(p, name, value);

    case OPT_OUTFORMAT:
        if (!value.len) {
            return fatal(p, "no output format name supplied for \"", name,
                         "\"");
        } else if (value == "u-ctags") {
            opt->writer = WRITER_UCTAGS;
        } else if (value == "e-ctags") {
            opt->writer = WRITER_ECTAGS;
        } else if (value == "etags") {
            etagsmode(opt);
        } else if (value == "xref") {
            return unsupported(p, "--output-format=xref");
        } else if (value == "json") {
            return fatal(p, "the output format \"", value, "\" is not "
                         "available on this platform");
        } else {
            return fatal(p, "unknown output format name supplied for \"", name,
                         "=", value, "\"");
        }
        return ARGS_OK;

    case OPT_PATLIMIT: {
        // As strtoul() reads it, in any base
        Ulong n = parseulong(value, 0);
        if (!value.len) {
            return fatal(p, "A parameter is needed after \"", name,
                         "\" option");
        } else if (!n.digits || n.overflow || !n.whole) {
            return fatal(p, "-", name, ": Invalid pattern length limit");
        }
        opt->patmax = n.value;
        return ARGS_OK;
    }

    case OPT_QUIET:
    case OPT_VERBOSE:
        return ARGS_OK;

    case OPT_RECURSE:
        opt->recurse = b;
        return ARGS_OK;

    case OPT_SORT:
        if (isfalse(value)) {
            opt->sort = SORT_NO;
        } else if (istrue(value)) {
            opt->sort = SORT_YES;
        } else if (equalsfold(value, "f") || equalsfold(value, "fold") ||
                   equalsfold(value, "foldcase")) {
            opt->sort = SORT_FOLD;
        } else {
            return badvalue(p, name);
        }
        return ARGS_OK;

    case OPT_TAGRELATIVE:
        if (isfalse(value)) {
            opt->tagrelative = TREL_NO;
        } else if (!value.len || istrue(value)) {
            opt->tagrelative = TREL_YES;
        } else if (equalsfold(value, "always")) {
            opt->tagrelative = TREL_ALWAYS;
        } else if (equalsfold(value, "never")) {
            opt->tagrelative = TREL_NEVER;
        } else {
            return badvalue(p, name);
        }
        return ARGS_OK;

    case OPT_TOTALS:
        if (value.len && !istrue(value) && !isfalse(value) &&
            !equalsfold(value, "extra")) {
            return badvalue(p, name);
        }
        return ARGS_OK;

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
    return ARGS_OK;
}

static i32 longopt(Parse *p, Str name, Str value)
{
    i32 family = langfamily(name);
    if (family>=0 && langopts[family].id<=LOPT_EXTRAS) {
        return langopt(p, family, name, value);
    }
    for (iz i = 0; i < countof(longopts); i++) {
        if (longopts[i].name == name) {
            return exactopt(p, longopts[i].id, name, value);
        }
    }
    if (family >= 0) {
        return langopt(p, family, name, value);
    } else if (startswith(name, "__")) {
        return ARGS_OK;  // ignored, as Universal Ctags does
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
    Options *opt = p->opt;
    switch (c) {
    case 'a': opt->append  = 1;             return ARGS_OK;
    case 'e': etagsmode(opt);               return ARGS_OK;
    case 'n': opt->excmd   = EXCMD_NUMBER;  return ARGS_OK;
    case 'N': opt->excmd   = EXCMD_PATTERN; return ARGS_OK;
    case 'R': opt->recurse = 1;             return ARGS_OK;
    case 'u': opt->sort    = SORT_NO;       return ARGS_OK;
    case 'F': return ARGS_OK;  // forward patterns, the only kind
    case 'V': return ARGS_OK;  // verbose

    case 'B':  // backward patterns
    case 'D':  // macro definitions
    case 'x':  // a cross reference
        return unsupported(p, "-", {&c, 1});

    case 'f':
    case 'o':
        return setoutput(p, c, param);

    case 'h':
        // A list that names a file is likely a mistake, "-h *.h"
        if (os_stat(p->os, *p->perm, param).type != STAT_NONE) {
            return fatal(p, "-h: Invalid list");
        }
        opt->headers = param != "default";
        return ARGS_OK;

    case 'I':
        opt->ignores = push(p->perm, opt->ignores, param);
        return ARGS_OK;

    case 'L':
        if (opt->listfile.data) {
            warning(p, "-L option specified more than once, last value used");
        }
        opt->listfile = param;
        return ARGS_OK;
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
    opt->sort   = SORT_YES;
    opt->format = 2;
    opt->patmax = 96;  // as Universal Ctags
    opt->langs  = LANGS_ALL;
}

// Parse arguments, or the lines of an option file: options, and files
// to tag, though a file in an option file ends it.
static i32 parseloop(Parse *p, Str *args, iz nargs)
{
    for (iz i = 0; i < nargs; i++) {
        Str a = args[i];
        i32 r = ARGS_OK;

        if (!startswith(a, "-") && p->optfile.data) {
            warning(p, "Ignoring non-option in ", p->optfile);
            return ARGS_OK;

        } else if (!startswith(a, "-")) {
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
                    iz blanks = 0;
                    for (; blanks<param.len && param[blanks]==' '; blanks++) {}
                    param = cuthead(param, blanks);
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

// Whether fields differ from the defaults where it matters: the parsers'
// fields but C++'s name are accepted and do nothing.
static b32 fieldschanged(u64 bits)
{
    u64 mask = ((u64)1<<NCOMMONFIELDS) - 1;
    mask |= (u64)1 << FIELD_CXXNAME;
    return ((bits ^ defaultfields()) & mask) != 0;
}

// Check and complete the options once all are parsed.
static i32 parsefinish(Parse *p)
{
    Options *opt = p->opt;
    if (opt->append && opt->output=="-") {
        return fatal(p, "append mode is not compatible with tags to stdout");
    }
    if (opt->writer != WRITER_ETAGS) {
        fixfields(p);
    }
    if (!opt->output.data) {
        opt->output = opt->writer==WRITER_ETAGS ? Str("TAGS") : Str("tags");
    }
    if (!opt->files.len && !opt->listfile.data) {
        if (!opt->recurse) {
            return fatal(p, "No files specified. Try \"ctags --help\".");
        }
        opt->files = push(p->perm, opt->files, Str("."));
    }
    if (opt->extras & EXTRA_LINKNAME) {
        return unsupported(p, "--extras-Fortran=+{linkName}");
    }

    // Parsed, but not yet implemented: each check goes when its
    // milestone lands
    b32 filetags = opt->extras&EXTRA_INPUTFILE && opt->filekind;
    if (opt->writer == WRITER_ETAGS)  return notyet(p, "-e");                      // M10
    if (opt->writer == WRITER_ECTAGS) return notyet(p, "--output-format=e-ctags"); // M3a
    if (fieldschanged(opt->fields))   return notyet(p, "--fields");                // M3a
    if (opt->excmd != EXCMD_MIXED)    return notyet(p, "--excmd");                 // M3a
    if (opt->format != 2)             return notyet(p, "--format");                // M3a
    if (opt->patmax != 96)            return notyet(p, "--pattern-length-limit");  // M3a
    if (opt->tagrelative != TREL_NO)  return notyet(p, "--tag-relative");          // M3b
    if (opt->sort != SORT_YES)        return notyet(p, "--sort");                  // M4
    if (opt->append)                  return notyet(p, "--append");                // M5
    if (filetags)                     return notyet(p, "--extras=+f");             // M7
    if (opt->excludes.len)            return notyet(p, "--exclude");               // M2a
    if (opt->exceptions.len)          return notyet(p, "--exclude-exception");     // M2a
    if (opt->listfile.data)           return notyet(p, "-L");                      // M2a
    if (opt->nolinks)                 return notyet(p, "--links");                 // M2a
    if (opt->langs != LANGS_ALL)      return notyet(p, "--languages");             // M2b
    if (opt->force)                   return notyet(p, "--language-force");        // M2b
    if (opt->mapped)                  return notyet(p, "--langmap");               // M2b
    if (opt->headers)                 return notyet(p, "-h");                      // M2b
    if (opt->ignores.len)             return notyet(p, "-I");                      // M9
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

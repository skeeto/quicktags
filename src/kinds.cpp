// Tag kinds, extras, and fields: their tables, and the grammar of the
// options that enable them
// This is free and unencumbered software released into the public domain.

enum : i32 {
    LANG_NONE, LANG_C, LANG_CXX, LANG_FORTRAN,
    LANG_FOREIGN,  // one Universal Ctags knows and this program does not
};

enum : u32 { LANGS_ALL = 1u<<LANG_C | 1u<<LANG_CXX | 1u<<LANG_FORTRAN };

static Str langnames[] = {"NONE", "C", "C++", "Fortran"};

// An entry of a table of kinds, extras, or fields, as Universal Ctags
// lists it.
struct Kind {
    u8  letter;  // 0 for none
    Str name;    // null for none
    b32 on;      // enabled by default
    Str desc;
};

// C and C++ share the first NCKINDS kinds, including whether each is
// enabled. A kind's index also numbers it in anonymous names, so the
// order is fixed.
enum : i32 {
    K_MACRO, K_ENUMERATOR, K_FUNCTION, K_ENUM, K_HEADER, K_LOCAL,
    K_MEMBER, K_PROTOTYPE, K_STRUCT, K_TYPEDEF, K_UNION, K_VARIABLE,
    K_EXTERNVAR, K_PARAMETER, K_LABEL, K_MACROPARAM,
    K_CLASS, K_NAMESPACE, K_ALIAS, K_NAME, K_USING, K_TPARAM, K_MODULE,
    K_PARTITION,
    NCKINDS   = K_CLASS,
    NCXXKINDS = K_PARTITION + 1,
};

static Kind cxxkinds[NCXXKINDS] = {
    {'d', "macro",      1, "macro definitions"},
    {'e', "enumerator", 1, "enumerators (values inside an enumeration)"},
    {'f', "function",   1, "function definitions"},
    {'g', "enum",       1, "enumeration names"},
    {'h', "header",     1, "included header files"},  // never written
    {'l', "local",      0, "local variables"},
    {'m', "member",     1, "class, struct, and union members"},
    {'p', "prototype",  0, "function prototypes"},
    {'s', "struct",     1, "structure names"},
    {'t', "typedef",    1, "typedefs"},
    {'u', "union",      1, "union names"},
    {'v', "variable",   1, "variable definitions"},
    {'x', "externvar",  0, "external and forward variable declarations"},
    {'z', "parameter",  0, "function parameters inside function or "
                           "prototype definitions"},
    {'L', "label",      0, "goto labels"},
    {'D', "macroparam", 0, "parameters inside macro definitions"},
    {'c', "class",      1, "classes"},
    {'n', "namespace",  1, "namespaces"},
    {'A', "alias",      0, "namespace aliases"},
    {'N', "name",       0, "names imported via using scope::symbol"},
    {'U', "using",      0, "using namespace statements"},
    {'Z', "tparam",     0, "template parameters"},
    {'M', "module",     1, "modules"},
    {'P', "partition",  1, "partitions"},
};

enum : i32 {
    F_BLOCKDATA, F_COMMON, F_ENTRY, F_ENUM, F_FUNCTION, F_INTERFACE,
    F_COMPONENT, F_LABEL, F_LOCAL, F_MODULE, F_METHOD, F_NAMELIST,
    F_ENUMERATOR, F_PROGRAM, F_PROTOTYPE, F_SUBROUTINE, F_TYPE,
    F_VARIABLE, F_SUBMODULE,
    NFKINDS,
};

static Kind ftnkinds[NFKINDS] = {
    {'b', "blockData",  1, "block data"},
    {'c', "common",     1, "common blocks"},
    {'e', "entry",      1, "entry points"},
    {'E', "enum",       1, "enumerations"},
    {'f', "function",   1, "functions"},
    {'i', "interface",  1, "interface contents, generic names, and operators"},
    {'k', "component",  1, "type and structure components"},
    {'l', "label",      1, "labels"},
    {'L', "local",      0, "local, common block, and namelist variables"},
    {'m', "module",     1, "modules"},
    {'M', "method",     1, "type bound procedures"},
    {'n', "namelist",   1, "namelists"},
    {'N', "enumerator", 1, "enumeration values"},
    {'p', "program",    1, "programs"},
    {'P', "prototype",  0, "subprogram prototypes"},
    {'s', "subroutine", 1, "subroutines"},
    {'t', "type",       1, "derived types and structures"},
    {'v', "variable",   1, "program (global) and module variables"},
    {'S', "submodule",  1, "submodules"},
};

// Enabled kinds, one bit per table index.
struct Kinds {
    u32 cxx;  // C uses the low NCKINDS bits
    u32 ftn;
};

struct KindTable {
    Kind *kinds;
    i32   len;
    u32  *bits;
};

static KindTable kindtable(Kinds *k, i32 lang)
{
    KindTable r = {};
    switch (lang) {
    case LANG_C:       r = {cxxkinds, NCKINDS,   &k->cxx}; break;
    case LANG_CXX:     r = {cxxkinds, NCXXKINDS, &k->cxx}; break;
    case LANG_FORTRAN: r = {ftnkinds, NFKINDS,   &k->ftn}; break;
    }
    return r;
}

static Kinds defaultkinds()
{
    Kinds r = {};
    for (i32 i = 0; i < NCXXKINDS; i++) {
        r.cxx |= (u32)cxxkinds[i].on << i;
    }
    for (i32 i = 0; i < NFKINDS; i++) {
        r.ftn |= (u32)ftnkinds[i].on << i;
    }
    return r;
}

static b32 kindon(Kinds *k, i32 lang, i32 kind)
{
    u32 bits = lang==LANG_FORTRAN ? k->ftn : k->cxx;
    return bits>>kind & 1;
}

static u8 kindletter(i32 lang, i32 kind)
{
    return lang==LANG_FORTRAN ? ftnkinds[kind].letter : cxxkinds[kind].letter;
}

static Str kindname(i32 lang, i32 kind)
{
    return lang==LANG_FORTRAN ? ftnkinds[kind].name : cxxkinds[kind].name;
}

// List kinds for --help, four to a line, each marked with '*' if enabled
// by default. The header kind is left out: it makes no tags here.
static void printkinds(Buf *b)
{
    struct {
        Str   label;
        Kind *kinds;
        i32   beg;
        i32   end;
    } groups[] = {
        {"C, C++:",  cxxkinds, 0,       NCKINDS},
        {"C++:",     cxxkinds, NCKINDS, NCXXKINDS},
        {"Fortran:", ftnkinds, 0,       NFKINDS},
    };
    print(b, "\nkinds, * if enabled by default:\n");
    for (iz g = 0; g < countof(groups); g++) {
        i32 column = 0;
        for (i32 i = groups[g].beg; i < groups[g].end; i++) {
            Kind *k = groups[g].kinds + i;
            if (k == cxxkinds+K_HEADER) {
                continue;
            }
            if (!column) {
                Str label = column || i>groups[g].beg ? Str("") : groups[g].label;
                print(b, "  ");
                print(b, label);
                for (iz n = label.len; n < 9; n++) {
                    putbyte(b, ' ');
                }
            }
            putbyte(b, k->letter);
            putbyte(b, k->on ? '*' : ' ');
            putbyte(b, ' ');
            print(b, k->name);
            if (++column == 4 || i+1 == groups[g].end) {
                putbyte(b, '\n');
                column = 0;
            } else {
                for (iz n = k->name.len; n < 13; n++) {
                    putbyte(b, ' ');
                }
            }
        }
    }
}


// Extras

// Universal Ctags' extras, one bit per table index: the common ones,
// then Fortran's. Only anonymous, fileScope, pseudo, and qualified
// change the output here. The other common ones are accepted and do
// nothing, as their tags come from features not ported, and linkName is
// refused.
enum : u32 {
    EXTRA_ANONYMOUS = 1u<<0,
    EXTRA_FILESCOPE = 1u<<1,
    EXTRA_INPUTFILE = 1u<<2,
    EXTRA_GUEST     = 1u<<3,
    EXTRA_PSEUDO    = 1u<<4,
    EXTRA_QUALIFIED = 1u<<5,
    EXTRA_REFERENCE = 1u<<6,
    EXTRA_SUBPARSER = 1u<<7,
    EXTRA_NULLTAG   = 1u<<8,
    EXTRA_LINKNAME  = 1u<<9,  // Fortran
};

enum : i32 { NCOMMONEXTRAS = 9, NEXTRAS = 10 };

static Kind extras[NEXTRAS] = {
    {0,   "anonymous", 1, "Include tags for non-named objects like lambda"},
    {'F', "fileScope", 1, "Include tags of file scope"},
    {'f', "inputFile", 0, "Include an entry for the base file name of "
                          "every input file"},
    {'g', "guest",     0, "Include tags generated by guest parsers"},
    {'p', "pseudo",    1, "Include pseudo tags"},
    {'q', "qualified", 0, "Include an extra class-qualified tag entry for "
                          "each tag"},
    {'r', "reference", 0, "Include reference tags"},
    {'s', "subparser", 1, "Include tags generated by subparsers"},
    {'z', "nulltag",   0, "Include tags with empty strings as their names"},
    {0,   "linkName",  0, "Linking name used in foreign languages"},
};

static u32 defaultextras()
{
    u32 r = 0;
    for (i32 i = 0; i < NEXTRAS; i++) {
        r |= (u32)extras[i].on << i;
    }
    return r;
}


// Roles

// Universal Ctags' roles of C and C++ kinds, which only reference tags
// have, to check --roles-LANG.KIND options.
static struct { i32 lang; u8 kind; Str name; } roles[] = {
    {LANG_C,   'd', "condition"},
    {LANG_C,   'd', "undef"},
    {LANG_C,   'f', "foreigndecl"},
    {LANG_C,   'h', "local"},
    {LANG_C,   'h', "system"},
    {LANG_C,   's', "foreigndecl"},
    {LANG_CXX, 'M', "imported"},
    {LANG_CXX, 'M', "partOwner"},
    {LANG_CXX, 'P', "imported"},
    {LANG_CXX, 'd', "condition"},
    {LANG_CXX, 'd', "undef"},
    {LANG_CXX, 'h', "exported"},
    {LANG_CXX, 'h', "imported"},
    {LANG_CXX, 'h', "local"},
    {LANG_CXX, 'h', "system"},
};

// The number of roles of a kind, or with a name, whether it has it.
static i32 countroles(i32 lang, u8 kind, Str name)
{
    i32 n = 0;
    for (iz i = 0; i < countof(roles); i++) {
        n += roles[i].lang==lang && roles[i].kind==kind &&
             (!name.data || roles[i].name==name);
    }
    return n;
}


// Fields

// Universal Ctags' fields, in the order it lists them, one bit per table
// index: the fixed fields, the others by letter, then the C and C++
// parsers' own. Only the defaults are written for now.
enum : i32 {
    FIELD_NAME, FIELD_INPUT, FIELD_PATTERN, FIELD_COMPACT, FIELD_EXTRAS,
    FIELD_KINDLONG, FIELD_REFMARK, FIELD_SIGNATURE, FIELD_EPOCH,
    FIELD_SCOPEKEY, FIELD_ACCESS, FIELD_END, FIELD_FILE, FIELD_INHERITS,
    FIELD_KIND, FIELD_LANGUAGE, FIELD_IMPL, FIELD_LINE, FIELD_NTH,
    FIELD_SCOPEKIND, FIELD_ROLES, FIELD_SCOPE, FIELD_TYPEREF, FIELD_XPATH,
    FIELD_KINDKEY,
    NCOMMONFIELDS,
    FIELD_C = NCOMMONFIELDS,  // C: alias, macrodef, properties, section
    FIELD_CXX = FIELD_C + 4,
    FIELD_CXXNAME = FIELD_CXX + 3,
    NFIELDS = FIELD_CXX + 8,
};

static Kind fields[NFIELDS] = {
    {'N', "name",           1, "tag name"},
    {'F', "input",          1, "input file"},
    {'P', "pattern",        1, "pattern"},
    {'C', "compact",        0, "compact input line (used only in xref output)"},
    {'E', "extras",         0, "Extra tag type information"},
    {'K', {},               0, "Kind of tag in long-name form"},
    {'R', {},               0, "Marker (R or D) representing whether tag is "
                               "definition or reference"},
    {'S', "signature",      0, "Signature of routine (e.g. prototype or "
                               "parameter list)"},
    {'T', "epoch",          1, "the last modified time of the input file "
                               "(only for F/file kind tag)"},
    {'Z', "scope",          0, "[tags output] prepend \"scope:\" key to "
                               "s/scope field output, [xref and json output] "
                               "the same as s/ field"},
    {'a', "access",         0, "Access (or export) of class members"},
    {'e', "end",            0, "end lines of various items"},
    {'f', "file",           1, "File-restricted scoping"},
    {'i', "inherits",       0, "Inheritance information"},
    {'k', {},               1, "Kind of tag in one-letter form"},
    {'l', "language",       0, "Language of input file containing tag"},
    {'m', "implementation", 0, "Implementation information"},
    {'n', "line",           1, "Line number of tag definition"},  // on here
    {'o', "nth",            0, "the order in the parent scope"},
    {'p', "scopeKind",      0, "[tags output] no effect, [xref and json "
                               "output] kind of scope in long-name form"},
    {'r', "roles",          0, "Roles"},
    {'s', {},               1, "[tags output] scope (kind:name) of tag "
                               "definition, [xref and json output] name of "
                               "scope"},
    {'t', "typeref",        1, "Type and name of a variable or typedef"},
    {'x', "xpath",          0, "xpath for the tag"},
    {'z', "kind",           0, "[tags output] prepend \"kind:\" to k/ (or "
                               "K/) field output, [xref and json output] "
                               "kind in long-name form"},

    {0, "alias",          0, "the name of the alias target specified in "
                             "__attribute__((alias(...)))"},
    {0, "macrodef",       0, "macro definition"},
    {0, "properties",     0, "properties (static, inline, mutable, "
                             "export,...)"},
    {0, "section",        0, "the place where the object is placed"},

    {0, "alias",          0, "the name of the alias target specified in "
                             "__attribute__((alias(...)))"},
    {0, "captures",       0, "lambda capture list"},
    {0, "macrodef",       0, "macro definition"},
    {0, "name",           1, "aliased names"},
    {0, "properties",     0, "properties (static, inline, mutable, "
                             "export,...)"},
    {0, "section",        0, "the place where the object is placed"},
    {0, "specialization", 0, "template specialization parameters"},
    {0, "template",       0, "template parameters"},
};

// Universal Ctags' data type and access columns for the common fields.
// The parsers' are all "s--" and "--".
static Str fieldtypes[NCOMMONFIELDS] = {
    "s--", "s--", "s-b", "s--", "s--", "s--", "s--", "s--", "-i-", "s--",
    "s--", "-i-", "--b", "s-b", "s--", "s--", "s--", "-i-", "-i-", "s--",
    "s--", "s--", "s--", "s--", "s--",
};
static Str fieldops[NCOMMONFIELDS] = {
    "rw", "r-", "--", "--", "r-", "--", "--", "rw", "--", "rw",
    "rw", "rw", "--", "-w", "--", "r-", "--", "rw", "--", "--",
    "r-", "--", "rw", "--", "r-",
};

static u64 defaultfields()
{
    u64 r = 0;
    for (i32 i = 0; i < NFIELDS; i++) {
        r |= (u64)fields[i].on << i;
    }
    return r;
}

static b32 fieldon(u64 bits, i32 field)
{
    return bits>>field & 1;
}


// Specifications

enum : i32 {
    SPEC_KINDS,         // --kinds-LANG
    SPEC_FOREIGNKINDS,  // --kinds-LANG, a language not parsed here
    SPEC_EXTRAS,        // --extras
    SPEC_FIELDS,        // --fields
    SPEC_LANGEXTRAS,    // --extras-LANG, by {name} only
    SPEC_LANGFIELDS,    // --fields-LANG, by {name} only
};

struct Spec {
    b32 ok;
    b32 reset;  // all were first disabled
    u64 set;    // entries enabled or disabled, by a reset or by name
    i32 file;   // the file kind: 0 if not named, +1 enabled, -1 disabled
};

static Spec badspec(Buf *log, Str what, u8 c)
{
    print(log, "ctags: unexpected character in ");
    print(log, what);
    print(log, " specification: '");
    putbyte(log, c);
    print(log, "'\n");
    return {};
}

// Apply a specification of kinds, extras, or fields to a table's bits,
// as Universal Ctags parses each: a leading "*" enables every entry and
// any other first character but "+" or "-" disables them all, "+" and
// "-" switch between enabling and disabling the entries that follow,
// named by letter or by {name}, and an unclosed {name is ignored. The
// dialects differ in their messages, in which unknown entries are fatal,
// and in the per-language forms taking names only. A name sets every
// entry with that name. Every language's kinds also include the file
// kind, F or {file}, which they share and resets leave alone, and the
// placeholder, ' ' or {ghost}, which makes no tags.
static Spec flagspec(u64 *bits, Kind *table, i32 len, Str spec, i32 dialect,
                     Str option, Buf *log)
{
    Spec r      = {};
    u64  all    = len<64 ? ((u64)1<<len) - 1 : ~(u64)0;
    b32  bylang = dialect >= SPEC_LANGEXTRAS;
    b32  kinds  = dialect <= SPEC_FOREIGNKINDS;
    Str  what   = "field";
    if (kinds) {
        what = "kind";
    } else if (dialect==SPEC_EXTRAS || dialect==SPEC_LANGEXTRAS) {
        what = "extra";
    }

    iz i = 0;
    if (i<spec.len && spec[i]=='*') {
        *bits |= all;
        r.set  = all;
        i++;
    } else if (i==spec.len || spec[i]=='{' ||
               (!bylang && spec[i]!='+' && spec[i]!='-')) {
        *bits  &= ~all;
        r.set   = all;
        r.reset = 1;
    } else if (bylang && spec[i]!='+' && spec[i]!='-') {
        print(log, "ctags: Warning: Wrong per language ");
        print(log, what);
        print(log, " specification: ");
        print(log, spec);
        putbyte(log, '\n');
    }

    b32 on = 1;
    for (; i < spec.len; i++) {
        u8  c    = spec[i];
        Str name = {};
        if (c=='+' || c=='-') {
            on = c == '+';
            continue;
        } else if (c == '{') {
            iz end = i + 1;
            for (; end<spec.len && spec[end]!='}' && spec[end]!='{'; end++) {}
            if (end == spec.len) {
                break;
            } else if (spec[end] == '{') {
                return badspec(log, what, '{');
            }
            name = span(spec.data+i+1, spec.data+end);
            i = end;
        } else if (c == '}') {
            return badspec(log, what, c);
        } else if (bylang) {
            print(log, "ctags: only long name can be used in per language ");
            print(log, what);
            print(log, " spec: '");
            putbyte(log, c);
            print(log, "'\n");
            return r;
        }

        if (kinds && (name.data ? name=="file" : c=='F')) {
            r.file = on ? 1 : -1;
            continue;
        } else if (kinds && (name.data ? name=="ghost" : c==' ')) {
            continue;
        }

        b32 found = 0;
        for (i32 j = 0; j < len; j++) {
            Kind *k     = table + j;
            b32   match = name.data ? k->name.data && k->name==name
                                    : k->letter && k->letter==c;
            if (match) {
                found  = 1;
                r.set |= (u64)1 << j;
                *bits &= ~((u64)1 << j);
                *bits |= (u64)on << j;
            }
        }
        if (found) {
            continue;
        }

        switch (dialect) {
        case SPEC_KINDS:
            print(log, "ctags: Warning: Unsupported kind: '");
            print(log, name.data ? name : Str{&c, 1});
            print(log, "' for --");
            print(log, option);
            print(log, " option\n");
            break;
        case SPEC_FOREIGNKINDS:
            break;  // its kinds are unknown here
        case SPEC_EXTRAS:
        case SPEC_FIELDS:
            if (dialect==SPEC_FIELDS && name.data) {
                print(log, "ctags: no such field: '");
                print(log, name);
                print(log, "'\n");
                return r;
            }
            print(log, "ctags: Warning: Unsupported parameter '");
            if (name.data) {
                putbyte(log, '{');
                print(log, name);
                putbyte(log, '}');
            } else {
                putbyte(log, c);
            }
            print(log, "' for \"");
            print(log, option);
            print(log, "\" option\n");
            break;
        case SPEC_LANGEXTRAS:
        case SPEC_LANGFIELDS:
            print(log, "ctags: no such ");
            print(log, what);
            print(log, ": '");
            print(log, name);
            print(log, "'\n");
            return r;
        }
    }
    r.ok = 1;
    return r;
}

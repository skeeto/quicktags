// Tag kinds and extras: their tables, and the --kinds-<LANG> and
// --extras options
// This is free and unencumbered software released into the public domain.

enum : i32 { LANG_NONE, LANG_C, LANG_CXX, LANG_FORTRAN };

struct Kind {
    u8  letter;
    Str name;
    b32 on;  // enabled by default
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
    {'d', "macro",      1},
    {'e', "enumerator", 1},
    {'f', "function",   1},
    {'g', "enum",       1},
    {'h', "header",     1},  // reference tags only, never written
    {'l', "local",      0},
    {'m', "member",     1},
    {'p', "prototype",  0},
    {'s', "struct",     1},
    {'t', "typedef",    1},
    {'u', "union",      1},
    {'v', "variable",   1},
    {'x', "externvar",  0},
    {'z', "parameter",  0},
    {'L', "label",      0},
    {'D', "macroparam", 0},
    {'c', "class",      1},
    {'n', "namespace",  1},
    {'A', "alias",      0},
    {'N', "name",       0},
    {'U', "using",      0},
    {'Z', "tparam",     0},
    {'M', "module",     1},
    {'P', "partition",  1},
};

enum : i32 {
    F_BLOCKDATA, F_COMMON, F_ENTRY, F_ENUM, F_FUNCTION, F_INTERFACE,
    F_COMPONENT, F_LABEL, F_LOCAL, F_MODULE, F_METHOD, F_NAMELIST,
    F_ENUMERATOR, F_PROGRAM, F_PROTOTYPE, F_SUBROUTINE, F_TYPE,
    F_VARIABLE, F_SUBMODULE,
    NFKINDS,
};

static Kind ftnkinds[NFKINDS] = {
    {'b', "blockData",  1},
    {'c', "common",     1},
    {'e', "entry",      1},
    {'E', "enum",       1},
    {'f', "function",   1},
    {'i', "interface",  1},
    {'k', "component",  1},
    {'l', "label",      1},
    {'L', "local",      0},
    {'m', "module",     1},
    {'M', "method",     1},
    {'n', "namelist",   1},
    {'N', "enumerator", 1},
    {'p', "program",    1},
    {'P', "prototype",  0},
    {'s', "subroutine", 1},
    {'t', "type",       1},
    {'v', "variable",   1},
    {'S', "submodule",  1},
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

// Apply a --kinds-<LANG> specification as Universal Ctags does: a
// leading "*" enables every kind and a leading letter first disables
// them all, "+" and "-" switch between enabling and disabling the
// letters that follow, and "{name}" names a kind in full. Unknown kinds
// are reported to the log and ignored.
static b32 kindspec(Kinds *k, i32 lang, Str spec, Str option, Buf *log)
{
    KindTable t   = kindtable(k, lang);
    u32       all = (u32)(((u64)1 << t.len) - 1);
    iz        i   = 0;
    if (i<spec.len && spec[i]=='*') {
        *t.bits |= all;
        i++;
    } else if (i==spec.len || (spec[i]!='+' && spec[i]!='-')) {
        *t.bits &= ~all;
    }

    b32 on = 1;
    for (; i < spec.len; i++) {
        u8  c    = spec[i];
        Str name = {};
        if (c == '+') {
            on = 1;
            continue;
        } else if (c == '-') {
            on = 0;
            continue;
        } else if (c == '{') {
            iz end = i + 1;
            for (; end<spec.len && spec[end]!='}' && spec[end]!='{'; end++) {}
            if (end==spec.len || spec[end]=='{') {
                print(log, "ctags: unexpected character in kind specification\n");
                return 0;
            }
            name = span(spec.data+i+1, spec.data+end);
            i = end;
        } else if (c == '}') {
            print(log, "ctags: unexpected character in kind specification\n");
            return 0;
        }

        i32 kind = -1;
        for (i32 j = 0; j<t.len && kind<0; j++) {
            if (name.data ? t.kinds[j].name==name : t.kinds[j].letter==c) {
                kind = j;
            }
        }
        if (kind < 0) {
            print(log, "ctags: Warning: Unsupported kind: '");
            if (name.data) {
                print(log, name);
            } else {
                putbyte(log, c);
            }
            print(log, "' for --");
            print(log, option);
            print(log, " option\n");
        } else if (on) {
            *t.bits |= (u32)1 << kind;
        } else {
            *t.bits &= ~((u32)1 << kind);
        }
    }
    return 1;
}


// Extras

// Universal Ctags' extras, one bit per table index. Only fileScope,
// pseudo, and qualified change the output here. The rest are accepted
// and do nothing, as their tags come from features not ported.
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
};

static Kind extras[] = {
    {0,   "anonymous", 1},  // named only in full
    {'F', "fileScope", 1},
    {'f', "inputFile", 0},
    {'g', "guest",     0},
    {'p', "pseudo",    1},
    {'q', "qualified", 0},
    {'r', "reference", 0},
    {'s', "subparser", 1},
    {'z', "nulltag",   0},
};

static u32 defaultextras()
{
    u32 r = 0;
    for (i32 i = 0; i < countof(extras); i++) {
        r |= (u32)extras[i].on << i;
    }
    return r;
}

static b32 badextra(Buf *log, u8 c)
{
    print(log, "ctags: unexpected character in extra specification: '");
    putbyte(log, c);
    print(log, "'\n");
    return 0;
}

// Apply an --extras specification, which has the kinds grammar with
// small differences, as in Universal Ctags: "+" and "-" within braces
// are part of the name, an unclosed name is ignored, and the messages
// differ. The option names itself in warnings, "extras" or "extra".
static b32 extraspec(u32 *bits, Str spec, Str option, Buf *log)
{
    u32 all = ((u32)1 << countof(extras)) - 1;
    iz  i   = 0;
    if (i<spec.len && spec[i]=='*') {
        *bits |= all;
        i++;
    } else if (i==spec.len || (spec[i]!='+' && spec[i]!='-')) {
        *bits &= ~all;
    }

    b32 on = 1;
    for (; i < spec.len; i++) {
        u8  c    = spec[i];
        Str name = {};
        if (c == '+') {
            on = 1;
            continue;
        } else if (c == '-') {
            on = 0;
            continue;
        } else if (c == '{') {
            iz end = i + 1;
            for (; end<spec.len && spec[end]!='}' && spec[end]!='{'; end++) {}
            if (end == spec.len) {
                break;
            } else if (spec[end] == '{') {
                return badextra(log, '{');
            }
            name = span(spec.data+i+1, spec.data+end);
            i = end;
        } else if (c == '}') {
            return badextra(log, c);
        }

        i32 extra = -1;
        for (i32 j = 0; j<countof(extras) && extra<0; j++) {
            if (name.data ? extras[j].name==name : extras[j].letter==c) {
                extra = j;
            }
        }
        if (extra < 0) {
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
        } else if (on) {
            *bits |= (u32)1 << extra;
        } else {
            *bits &= ~((u32)1 << extra);
        }
    }
    return 1;
}

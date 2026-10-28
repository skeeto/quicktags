// Fuzzing in process: inputs that once failed, generated token soup, and
// mutated tag-case sources, each tagged with default and with all kinds,
// and in other formats, checking that the output is well formed, and
// that fields added to the default ones change nothing else. Debug
// builds also trap on undefined behavior.

static u32 rand32(u64 *rng)
{
    *rng = *rng*0x3243f6a8885a308d + 1;
    return (u32)(*rng >> 32);
}

static Str fuzzc[] = {
    "int", "char", "void", "struct", "union", "enum", "typedef", "static",
    "extern", "const", "volatile", "unsigned", "long", "return", "if",
    "else", "for", "while", "do", "switch", "case", "default", "goto",
    "sizeof", "inline", "__attribute__((x))", "__declspec(y)", "_Pragma(\"z\")",
    "class", "namespace", "template", "typename", "public:", "private",
    "using", "operator", "virtual", "friend", "explicit", "constexpr",
    "decltype", "auto", "new", "delete", "try", "catch", "throw",
    "module", "import", "export", "final", "override", "noexcept",
    "a", "b", "f", "g", "T", "S", "x", "MACRO", "ns", "\xce\xbb", "$v",
    "0", "1", "0x1f", "1'000", "1.5e3", "077uLL", "'c'", "u8'x'", "'\\''",
    "\"s\"", "\"a\\\"b\"", "R\"d(raw)d\"", "L\"w\"", "\"open",
    "(", ")", "[", "]", "{", "}", "<", ">", ";", ",", ":", "::", "=", "==",
    "*", "&", "&&", "->", ".", "...", "~", "!", "?", "+", "-", "/", "%",
    "^", "|", "<<", ">>", "<=>", "[[nodiscard]]", "?\?=", "?\?/", "<:", ":>",
    "<%", "%>", "%:", "/* c */", "/*", "*/", "// c\n", "/+", "\\\n",
    "\n", "\n", "\n", "\r\n", "\r", "\t", " ", "\v", "\f", "\0", "\x80",
    "\n#define X(a, b) a b\n", "\n#define Y 1\n", "\n#if 0\n", "\n#if 1\n",
    "\n#ifdef Q\n", "\n#elif 1\n", "\n#elifdef Q\n", "\n#else\n",
    "\n#endif\n", "\n#include <x.h>\n", "\n#pragma weak w\n", "\n#undef Y\n",
    "\n# \n", "\n#\n", "#",
};

static Str fuzzf[] = {
    "program", "module", "submodule", "subroutine", "function", "end",
    "contains", "interface", "type", "integer", "real", "character",
    "logical", "parameter", "dimension", "allocatable", "pointer",
    "common", "block", "data", "entry", "enum", "enumerator", "namelist",
    "procedure", "generic", "abstract", "pure", "elemental", "recursive",
    "impure", "use", "only", "result", "bind(c)", "kind=8", "len=*",
    "extends(t)", "deferred", "non_overridable", "type, extends(t) ::",
    "type, abstract ::", "procedure(p), deferred ::",
    "procedure, non_overridable ::", "submodule (m)",
    "a", "b", "x", "_u", "100", "1.5d0", "'s'", "\"t\"", "::", ",", "=",
    "(", ")", "(/", "/)", "%", "&", "&\n", "!c", "\n", "\n", "\n", ";",
    "\nC comment\n", "\n      ", "\n     &", "\n100   ", "*", "\0", "\x80",
    "\n#if 0\n", "\n#endif\n",
};

static Str fuzzexts[] = {".c", ".cpp", ".h", ".hpp", ".f90", ".f"};

// Generate an input from the vocabulary for the extension.
static Str fuzzsoup(Arena *a, u64 *rng, i32 ext)
{
    b32  ftn   = ext >= 4;
    Str *words = ftn ? fuzzf : fuzzc;
    iz   n     = ftn ? countof(fuzzf) : countof(fuzzc);
    Buf  b(a, 1<<10);
    i32  len = (i32)(rand32(rng) % 400);
    for (i32 i = 0; i < len; i++) {
        print(&b, words[rand32(rng) % (u32)n]);
        putbyte(&b, rand32(rng)%4 ? ' ' : '\n');
    }
    return finish(&b);
}

// Mutate a source: flip, delete, duplicate, or truncate.
static Str fuzzmutate(Arena *a, u64 *rng, Str src)
{
    Buf b(a, src.len*2 + 64);
    print(&b, src);
    i32 edits = 1 + (i32)(rand32(rng) % 8);
    for (i32 i = 0; i < edits && b.len; i++) {
        iz at = (iz)(rand32(rng) % (u32)b.len);
        switch (rand32(rng) % 4) {
        case 0:  // replace a byte
            b.data[at] = (u8)"{}()[]<>;,:=*&#'\"\\/\n x0"[rand32(rng) % 24];
            break;
        case 1:  // delete a span
            for (iz j = at; j+1 < b.len; j++) {
                b.data[j] = b.data[j+1];
            }
            b.len--;
            break;
        case 2: {  // duplicate a span
            iz  n   = (iz)(rand32(rng) % 32);
            n = n < b.len-at ? n : b.len-at;
            Str dup = clone(a, {b.data+at, n});
            Buf c(a, b.len + n);
            print(&c, {b.data, at});
            print(&c, dup);
            print(&c, {b.data+at, b.len-at});
            b = c;
        } break;
        case 3:  // truncate
            b.len = at;
            break;
        }
    }
    return finish(&b);
}

// The order of a run's lines.
enum : i32 {
    ORDER_BYTES,  // sorted by bytes, each line once
    ORDER_NONE,   // unsorted, as the parser made them
    ORDER_FOLD,   // sorted ignoring case, ties by bytes, each line once
};

// A run's options, and what they promise of each line.
struct Lint {
    Str opts;      // before "-f - FILE"
    i32 excmd;     // the address: EXCMD_*
    b32 nofields;  // --format=1: nothing after the address
    b32 deflt;     // the kind letter first, then line:
    b32 strip;     // the default output once stripped (fuzzstrip)
    i32 order = ORDER_BYTES;
    b32 filetag = 0;  // one file tag (EX-7), last if unsorted
    b32 etags   = 0;  // Emacs tags, checked by etagslint
    b32 twin    = 0;  // also without the fast paths, the same
};

static Lint lints[] = {
    {"",                                                EXCMD_MIXED,   0, 1, 0,
     ORDER_BYTES, 0, 0, 1},
    {"--kinds-C=* --kinds-C++=* --kinds-Fortran=* ",    EXCMD_MIXED,   0, 1, 0,
     ORDER_BYTES, 0, 0, 1},
    // -I macros, with loops (T leads to one, and S and R bring themselves
    // back after other tokens, also at the end) and replacements that
    // open groups they do not close
    {"-I x=x,y=z,z=y,a+,b+=c(,T=y,MACRO+=f(,S+=;S,R+=)R ",
                                                        EXCMD_MIXED,   0, 1, 0,
     ORDER_BYTES, 0, 0, 1},
    // One of the following too, in turn
    {"--fields=* --extras=+q --kinds-C++=* ",           EXCMD_MIXED,   0, 0, 0},
    {"--excmd=number --fields=+lKzZEr --extras=+qf ",   EXCMD_NUMBER,  0, 0, 0,
     ORDER_BYTES, 1},
    {"--excmd=combine --kinds-Fortran=+L --fields=+E ", EXCMD_COMBINE, 0, 1, 0},
    {"-N --pattern-length-limit=10 --fields=+Zr --extras=+f ",
                                                        EXCMD_PATTERN, 0, 0, 0,
     ORDER_BYTES, 1},
    {"--format=1 -n ",                                  EXCMD_NUMBER,  1, 0, 0},
    {"--output-format=e-ctags --fields=+zES --file-tags ",
                                                        EXCMD_MIXED,   0, 0, 0,
     ORDER_BYTES, 1},
    {"--fields=+lzZEriaSm ",                            EXCMD_MIXED,   0, 0, 1,
     ORDER_BYTES, 0, 0, 1},
    // Unsorted and folded, which sorted are the default output (but for
    // the file tag)
    {"-u --fields=+lzZEr --extras=+f ",                 EXCMD_MIXED,   0, 0, 1,
     ORDER_NONE, 1},
    {"--sort=foldcase --fields=+iaSm ",                 EXCMD_MIXED,   0, 1, 1,
     ORDER_FOLD},
    // Emacs tags, of the default tags
    {"-e ",                                             EXCMD_MIXED,   0, 0, 0,
     ORDER_NONE, 0, 1},
    {"-e --pattern-length-limit=7 ",                    EXCMD_MIXED,   0, 0, 0,
     ORDER_NONE, 0, 1},
};

// Do the lines come in this order, each once? Folded, struppercmp()
// orders them, then bytes.
static b32 lintorder(Str a, Str b, i32 order)
{
    if (order == ORDER_FOLD) {
        iz len = a.len<b.len ? a.len : b.len;
        for (iz i = 0; i < len; i++) {
            u8 x = a[i]>='a' && a[i]<='z' ? (u8)(a[i] - 32) : a[i];
            u8 y = b[i]>='a' && b[i]<='z' ? (u8)(b[i] - 32) : b[i];
            if (x != y) return x < y;
        }
        if (a.len != b.len) {
            return a.len < b.len;
        }
    }
    return order==ORDER_NONE || compare(a, b)<0;
}

// Universal Ctags' order of the fields written here (FLD-4), by key,
// where a scope's key is its kind, and the kind may have none.
static i32 fieldrank(Str key)
{
    static Str keys[] = {
        "kind", "line", "language", "scope", "typeref", "file", "inherits",
        "access", "implementation", "signature", "roles", "extras", "epoch",
        "name",
    };
    for (i32 i = 0; i < countof(keys); i++) {
        if (key == keys[i]) {
            return i;
        }
    }
    return 3;
}

// Is it a line number in [1, max]?
static b32 lintnum(Str s, iz max)
{
    iz n = 0;
    for (iz i = 0; i < s.len; i++) {
        if (!digit(s[i]) || n>max) {
            return 0;
        }
        n = n*10 + s[i] - '0';
    }
    return n>=1 && n<=max;
}

static b32 lintpattern(Str s)
{
    return s.len>=3 && startswith(s, "/^") && endswith(s, "/");
}

// Is the address what the excmd asks for? Digits are a line number.
static b32 lintaddress(Str addr, i32 excmd, iz lines)
{
    iz digits = 0;
    for (; digits<addr.len && digit(addr[digits]); digits++) {}
    Str number = takehead(addr, digits);
    Str rest   = cuthead(addr, digits);
    if (digits && !lintnum(number, lines)) {
        return 0;
    }
    switch (excmd) {
    case EXCMD_MIXED:   return digits ? !rest.len : lintpattern(addr);
    case EXCMD_NUMBER:  return digits && !rest.len;
    case EXCMD_PATTERN: return lintpattern(addr);
    case EXCMD_COMBINE:
        return digits && (!rest.len || (startswith(rest, ";") &&
                                        lintpattern(cuthead(rest, 1))));
    }
    return 0;
}

// Are extras a subsequence of those written here, in their order?
static b32 lintextras(Str s)
{
    Str names[] = {"fileScope", "inputFile", "qualified", "anonymous"};
    iz  next    = 0;
    Cut c       = {};
    c.tail = s;
    do {
        c = cut(c.tail, ',');
        for (; next<countof(names) && !(names[next]==c.head); next++) {}
        if (next++ == countof(names)) {
            return 0;
        }
    } while (c.ok);
    return 1;
}

// Offset of the ;" and tab just after the address that begins s, or -1.
// The address is read as written, a line number, a pattern, or both
// (N;/^.../), whose escapes hide each '/' and '\' within, so a field
// value may hold ;" and a tab, as readers allow.
static iz fieldsat(Str s)
{
    iz i = 0;
    for (; i<s.len && digit(s[i]); i++) {}
    if (i && i+1<s.len && s[i]==';' && s[i+1]=='/') {
        i++;
    }
    if (i<s.len && s[i]=='/') {
        for (i++; i<s.len && s[i]!='/'; i++) {
            i += s[i] == '\\';
        }
        if (i >= s.len) {
            return -1;
        }
        i++;
    }
    b32 ok = i+2<s.len && s[i]==';' && s[i+1]=='"' && s[i+2]=='\t';
    return ok ? i : -1;
}

// Is the tags output well formed for the source and options? In order,
// each address as the excmd says, the fields in order, and the line
// field in range, and equal to a line number in the address. A file tag,
// named for the file, is addressed by line 1 whatever the excmd.
static Str fuzzlint(Str out, Str path, Str src, Lint *x)
{
    iz lines = 1;
    for (iz i = 0; i < src.len; i++) {
        lines += src[i] == '\n';
    }
    iz  nfile = 0;
    Str prev  = {};
    for (Cut c = cut(out, '\n'); c.ok; c = cut(c.tail, '\n')) {
        Str l = c.head;
        if (prev.data && !lintorder(prev, l, x->order)) {
            return x->order==ORDER_FOLD ? Str("lines not folded and unique")
                                        : Str("lines not sorted and unique");
        }
        prev = l;
        if (cut(l, 0).ok) {
            return "a NUL byte";  // a reader would end the line there
        }
        Cut name = cut(l, '\t');
        Cut file = cut(name.tail, '\t');
        if (!name.ok || !name.head.len) {
            return "empty name";
        } else if (!file.ok) {
            return "no address";
        }

        Str addr   = file.tail;
        Str fields = {};
        if (!x->nofields) {
            iz at = fieldsat(addr);
            if (at < 0) {
                return "no ;\" before the fields";
            }
            fields = cuthead(addr, at+3);
            addr   = takehead(addr, at);
        }
        if (x->filetag && name.head==path) {
            nfile++;
            if (!(addr == "1")) {
                return "file tag not at line 1";
            } else if (x->order==ORDER_NONE && c.tail.len) {
                return "file tag not last";
            }
        } else if (!lintaddress(addr, x->excmd, lines)) {
            return "bad address";
        }
        iz digits = 0;
        for (; digits<addr.len && digit(addr[digits]); digits++) {}

        i32 rank = -1;
        b32 line = 0;
        Cut f    = {};
        f.tail = fields;
        while (f.tail.len) {
            f = cut(f.tail, '\t');
            Cut kv = cut(f.head, ':');
            i32 r  = kv.ok ? fieldrank(kv.head) : 0;
            if (r <= rank) {
                return "fields out of order";
            } else if (x->deflt && rank<0 && (kv.ok || kv.head.len!=1)) {
                return "the kind letter is not first";
            }
            rank = r;
            if (kv.ok && kv.head=="line") {
                line = 1;
                if (!lintnum(kv.tail, lines)) {
                    return "line number out of range";
                } else if (digits && !(takehead(addr, digits) == kv.tail)) {
                    return "address and line differ";
                }
            } else if (kv.ok && kv.head=="extras" && !lintextras(kv.tail)) {
                return "bad extras";
            }
        }
        if (x->deflt && !line) {
            return "no line field";
        }
    }
    if (nfile != x->filetag) {
        return "not one file tag";
    }
    return {};
}

// Output with --fields=+lzZEriaSm, in any order, as the default options
// would write it: without the file tag, each line without its language,
// inherits, access, implementation, signature, roles, and extras fields,
// and without the kind and scope keys, then sorted, each line once.
static Str fuzzstrip(Arena *a, Str out)
{
    Slice<Str> lines = {};
    for (Cut c = cut(out, '\n'); c.ok; c = cut(c.tail, '\n')) {
        Str l    = c.head;
        if (find(l, "\textras:inputFile") >= 0) {
            continue;
        }
        Cut name = cut(l, '\t');
        Cut file = cut(name.tail, '\t');
        iz  at   = fieldsat(file.tail);
        if (at < 0) {
            lines = push(a, lines, l);
            continue;
        }
        at += l.len - file.tail.len;
        Buf b(a, l.len);  // only shrinks
        print(&b, takehead(l, at+2));
        Cut f = {};
        f.tail = cuthead(l, at+3);
        while (f.tail.len) {
            f = cut(f.tail, '\t');
            Cut kv  = cut(f.head, ':');
            Str key = kv.ok ? kv.head : Str{};
            if (key=="language" || key=="inherits" || key=="access" ||
                key=="implementation" || key=="signature" ||
                key=="roles" || key=="extras") {
                continue;
            }
            putbyte(&b, '\t');
            print(&b, key=="kind" || key=="scope" ? kv.tail : f.head);
        }
        lines = push(a, lines, finish(&b));
    }

    sort(lines.data, lines.len, strless, *a);
    Buf b(a, out.len+1);
    for (iz i = 0; i < lines.len; i++) {
        if (!i || !(lines[i] == lines[i-1])) {
            print(&b, lines[i]);
            putbyte(&b, '\n');
        }
    }
    return finish(&b);
}

// The checks of one input: the first problem found, if any, with the
// options that showed it, and what the outputs held.
struct Fuzzed {
    Str   bad;       // null for none
    Lint *lint;
    iz    ntags;
    iz    inherits;  // Fortran's inherits: fields
    iz    impl;      // Fortran's implementation: fields
};

// Tag the file with each option set in turn, the first the default
// options, checking each output, and stopping at the first problem.
static Fuzzed fuzzcheck(Arena a, Str file, Str src, Lint **runs, iz nruns)
{
    Fuzzed r   = {};
    Os    *os  = newos(&a);
    b32    ftn = endswith(file, ".f90") || endswith(file, ".f");
    vfsfile(os, file, src);
    Str deflt = {};
    for (iz o = 0; o<nruns && !r.bad.data; o++) {
        Buf cmd(&a, 128);
        print(&cmd, runs[o]->opts);
        print(&cmd, "-f - ");
        print(&cmd, file);
        Run got = runcap(os, a, finish(&cmd), (iz)8<<20);
        r.bad  = runs[o]->etags ? etagslint(a, got.out, file, src, deflt)
                                : fuzzlint(got.out, file, src, runs[o]);
        r.lint = runs[o];
        if (!o) {
            deflt = got.out;
        } else if (runs[o]->strip && !(fuzzstrip(&a, got.out) == deflt)) {
            r.bad = "not the default output once stripped";
        }
        for (Cut c = cut(got.out, '\n'); c.ok; c = cut(c.tail, '\n')) {
            r.ntags++;
            if (ftn) {
                r.inherits += find(c.head, "\tinherits:") >= 0;
                r.impl     += find(c.head, "\timplementation:") >= 0;
            }
        }
        if (got.status) {
            r.bad = "nonzero exit status";
        }

        if (!r.bad.data && runs[o]->twin) {
            Buf slow(&a, 128);
            print(&slow, "--_nofast ");
            print(&slow, finish(&cmd));
            Run twin = runcap(os, a, finish(&slow), (iz)8<<20);
            if (!(twin.out == got.out)) {
                r.bad = "not the output without the fast paths";
            }
        }
    }
    return r;
}

// Expect no problem, else print the input, which and how it was tagged.
static b32 fuzzexpect(Test *t, Fuzzed r, char const *what, i32 i, Str file,
                      Str src)
{
    if (expect(t, "fuzz", r.bad, {})) {
        return 1;
    }
    fprintf(stderr, "  %s %d (%.*s%.*s):\n[", what, i,
            (int)r.lint->opts.len, (char *)r.lint->opts.data,
            (int)file.len, (char *)file.data);
    printstr(stderr, src);
    fprintf(stderr, "]\n");
    return 0;
}

// Inputs the checks once got wrong, each tagged with every option set:
// field values holding ;" before a tab, and a NUL written as it is.
static struct {
    Str file;
    Str src;
} fuzzseeds[] = {
    {"f.h",   "class K : Y<\"x;\" {};\n"},
    {"f.c",   "typedef struct node\"nde, *nodeptr;\" z;\n"},
    {"f.cpp", "void f(char c = '\0', int d) {}\n"},
};

#ifndef FUZZRUNS
#  define FUZZRUNS 3000  // more for a longer campaign: -DFUZZRUNS=1000000
#endif

static void test_fuzz(Test *t)
{
    TagCase *sets[]  = {ccases, cxxcases, fortrancases};
    iz       sizes[] = {countof(ccases), countof(cxxcases), countof(fortrancases)};

    Lint *all[countof(lints)] = {};
    for (iz i = 0; i < countof(lints); i++) {
        all[i] = lints + i;
    }
    for (i32 i = 0; i < countof(fuzzseeds); i++) {
        Str    file = fuzzseeds[i].file;
        Str    src  = fuzzseeds[i].src;
        Fuzzed r    = fuzzcheck(t->perm, file, src, all, countof(all));
        if (!fuzzexpect(t, r, "seed", i, file, src)) {
            return;
        }
    }

    // GCC 16 places u64 locals below their alignof (8) on i686 Windows,
    // which UBSan traps through the pointer: a regression w64devkit
    // patched in 0de8f07, explicit here for unpatched compilers
    alignas(8) u64 rng      = 1;
    iz             ntags    = 0;
    iz             inherits = 0;
    iz             impl     = 0;
    for (i32 i = 0; i < FUZZRUNS; i++) {
        Arena a   = t->perm;
        i32   ext = (i32)(rand32(&rng) % countof(fuzzexts));
        Str   src = {};
        if (rand32(&rng) % 2) {
            src = fuzzsoup(&a, &rng, ext);
        } else {
            i32      set = ext>=4 ? 2 : (i32)(rand32(&rng) % 2);
            TagCase *tc  = sets[set] + rand32(&rng)%(u32)sizes[set];
            src = fuzzmutate(&a, &rng, tc->src);
        }

        Buf path(&a, 16);
        print(&path, "f");
        print(&path, fuzzexts[ext]);
        Str file = finish(&path);

        // Default options, all kinds, -I macros, and a format option set
        // in turn
        Lint  *runs[] = {
            lints, lints+1, lints+2, lints + 3 + i%(countof(lints)-3)
        };
        Fuzzed r = fuzzcheck(a, file, src, runs, countof(runs));
        ntags    += r.ntags;
        inherits += r.inherits;
        impl     += r.impl;
        if (!fuzzexpect(t, r, "input", i, file, src)) {
            return;
        }
    }
    expect(t, "fuzz found tags", num(&t->perm, ntags > FUZZRUNS), "1");
    expect(t, "fuzz found Fortran's inherits:",
           num(&t->perm, inherits > FUZZRUNS/200), "1");
    expect(t, "fuzz found Fortran's implementation:",
           num(&t->perm, impl > FUZZRUNS/200), "1");
}

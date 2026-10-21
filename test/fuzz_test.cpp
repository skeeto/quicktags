// Fuzzing in process: generated token soup and mutated tag-case sources,
// each tagged with default and with all kinds, checking that the output
// is well formed. Debug builds also trap on undefined behavior.

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

// Is the tags output well formed for the source?
static Str fuzzlint(Str out, Str src)
{
    iz lines = 1;
    for (iz i = 0; i < src.len; i++) {
        lines += src[i] == '\n';
    }
    Str prev = {};
    for (Cut c = cut(out, '\n'); c.ok; c = cut(c.tail, '\n')) {
        Str l = c.head;
        if (prev.data && compare(prev, l) >= 0) {
            return "lines not sorted and unique";
        }
        prev = l;
        Cut name = cut(l, '\t');
        if (!name.ok || !name.head.len) {
            return "empty name";
        }
        iz at = -1;
        for (iz i = 0; i+2 < l.len; i++) {
            if (l[i]==';' && l[i+1]=='"' && l[i+2]=='\t') {
                at = i + 3;
            }
        }
        if (at < 0) {
            return "no ;\" before the kind";
        }
        Cut kind = cut(cuthead(l, at), '\t');
        if (kind.head.len!=1 || !startswith(kind.tail, "line:")) {
            return "bad kind or line field";
        }
        Cut num  = cut(cuthead(kind.tail, 5), '\t');
        iz  line = 0;
        for (iz i = 0; i < num.head.len; i++) {
            if (num.head[i]<'0' || num.head[i]>'9') {
                return "bad line number";
            }
            line = line*10 + num.head[i] - '0';
        }
        if (line<1 || line>lines) {
            return "line number out of range";
        }
        Cut addr = cut(cut(l, '\t').tail, '\t');
        Str a    = addr.tail;
        if (!startswith(a, "/^") && !(a.len && a[0]>='0' && a[0]<='9')) {
            return "bad address";
        }
    }
    return {};
}

#ifndef FUZZRUNS
#  define FUZZRUNS 3000  // more for a longer campaign: -DFUZZRUNS=1000000
#endif

static void test_fuzz(Test *t)
{
    TagCase *sets[]  = {ccases, cxxcases, fortrancases};
    iz       sizes[] = {countof(ccases), countof(cxxcases), countof(fortrancases)};

    // GCC 16 places u64 locals below their alignof (8) on i686 Windows,
    // which UBSan traps through the pointer: a regression w64devkit
    // patched in 0de8f07, explicit here for unpatched compilers
    alignas(8) u64 rng   = 1;
    iz             ntags = 0;
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

        Os *os = newos(&a);
        Buf path(&a, 16);
        print(&path, "f");
        print(&path, fuzzexts[ext]);
        Str file = finish(&path);
        vfsfile(os, file, src);

        Str opts[] = {"", "--kinds-C=* --kinds-C++=* --kinds-Fortran=* "};
        for (iz o = 0; o < countof(opts); o++) {
            Buf cmd(&a, 128);
            print(&cmd, opts[o]);
            print(&cmd, "-f - ");
            print(&cmd, file);
            Run r = runcap(os, a, finish(&cmd), (iz)8<<20);
            Str bad = fuzzlint(r.out, src);
            for (iz j = 0; j < r.out.len; j++) {
                ntags += r.out[j] == '\n';
            }
            if (r.status) {
                bad = "nonzero exit status";
            }
            if (!expect(t, "fuzz", bad, {})) {
                fprintf(stderr, "  input %d (%.*s):\n[", i,
                        (int)file.len, (char *)file.data);
                printstr(stderr, src);
                fprintf(stderr, "]\n");
                return;
            }
        }
    }
    expect(t, "fuzz found tags", num(&t->perm, ntags > FUZZRUNS), "1");
}

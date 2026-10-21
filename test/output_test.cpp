// Tag formatting: escapes, patterns, fields, anonymous names, merging

// Format one tag found in a file of the given text.
static Str fmttag(Arena *a, Str path, i32 lang, Str text, Tag tag)
{
    Source src = {};
    src.path     = path;
    src.outpath  = outpath(a, path, 1);
    src.text     = text;
    src.anonhash = anonhash(path);
    src.lang     = lang;
    Kinds k = {0xffffffff, 0xffffffff};
    Buf   b(a, 256);
    File  f = {};
    f.src   = &src;
    f.kinds = &k;
    f.out   = &b;
    tagemit(&f, &tag);
    return finish(&b);
}

// The address alone, for a tag on the first line of the text.
static Str fmtpattern(Arena *a, Str text, Str name, b32 truncate)
{
    Tag t = {};
    t.name     = name;
    t.truncate = truncate;
    Buf b(a, 256);
    printpattern(&b, text, &t);
    return finish(&b);
}

static Str repeat(Arena *a, Str head, u8 c, iz n, Str tail)
{
    Buf b(a, n+tail.len);
    print(&b, head);
    for (iz i = 0; i < n; i++) {
        putbyte(&b, c);
    }
    print(&b, tail);
    return finish(&b);
}

static void test_output(Test *t)
{
    Arena a = t->perm;

    // djb2 over the raw path names anonymous tags (Universal Ctags
    // vectors, including a path with backslashes, hashed as written)
    struct { Str path; u32 hash; } hashes[] = {
        {"./Units/parser-c.r/bit_field.c.d/input.c", 0x1719c4a5},
        {"ggml/include/ggml.h",                       0x1031cceb},
        {"ggml\\src\\ggml-cpu\\amx\\common.h",        0xdbcbb705},
    };
    for (iz i = 0; i < countof(hashes); i++) {
        Buf got(&a, 16);
        Buf want(&a, 16);
        printhex(&got,  anonhash(hashes[i].path), 8);
        printhex(&want, hashes[i].hash, 8);
        expect(t, hashes[i].path, finish(&got), finish(&want));
    }

    {
        Source src = {};
        src.anonhash = 0x1719c4a5;
        File f = {};
        f.src = &src;
        expect(t, "anon 1", anonname(&f, &a, K_STRUCT), "__anon1719c4a50108");
        expect(t, "anon 2", anonname(&f, &a, K_ENUM),   "__anon1719c4a50203");
        f.anon = 0xfe;
        expect(t, "anon ff",  anonname(&f, &a, K_FUNCTION), "__anon1719c4a5ff02");
        expect(t, "anon 100", anonname(&f, &a, K_FUNCTION), "__anon1719c4a510002");
    }

    // Patterns, as Universal Ctags writes them unless noted
    struct { Str text, name; b32 truncate; Str want; } pats[] = {
        {"int x;\n",            "x",   0, "/^int x;$/"},
        {"a/b\\c\n",            "a",   0, "/^a\\/b\\\\c$/"},
        {"p$\n",                "p",   0, "/^p\\$$/"},
        {"p$q\n",               "p",   0, "/^p$q$/"},
        {"int x;\r\n",          "x",   0, "/^int x;$/"},
        {"int x;",              "x",   0, "/^int x;/"},
        {"\ttab\n",             "tab", 0, "/^\ttab$/"},
        {"^x\n",                "x",   0, "/^^x$/"},
        {"#define FOO 1\n",     "FOO", 1, "/^#define FOO /"},
        {"#define FOO\n",       "FOO", 1, "/^#define FOO$/"},
        {"#define FOO",         "FOO", 1, "/^#define FOO/"},
        {"#define A(x) A(x)\n", "A",   1, "/^#define A(x) A(/"},
        {"#define B 1\n",       "Z",   1, "/^#define B 1$/"},
        // Departures: unmatchable anchors are not written
        {"a\rb\n",              "a",   0, "/^a/"},          // u-ctags: /^a$/
        {"ab\0cd\n",            "a",   0, "/^ab/"},         // u-ctags: drops cd
        {"x$",                  "x",   0, "/^x\\$/"},       // u-ctags: /^x$/
        {"#define FOO$\n",      "FOO", 1, "/^#define FOO\\$/"},  // u-ctags: FOO$
    };
    for (iz i = 0; i < countof(pats); i++) {
        Str text = pats[i].text;  // literals keep embedded NULs
        expect(t, text, fmtpattern(&a, text, pats[i].name, pats[i].truncate),
               pats[i].want);
    }

    // The length limit counts escapes and finishes a UTF-8 sequence
    expect(t, "limit",
        fmtpattern(&a, repeat(&a, "", 'a', 100, "\n"), "a", 0),
        repeat(&a, "/^", 'a', 96, "/"));
    expect(t, "limit exact",
        fmtpattern(&a, repeat(&a, "", 'a', 96, "\n"), "a", 0),
        repeat(&a, "/^", 'a', 96, "$/"));
    expect(t, "limit escape",
        fmtpattern(&a, repeat(&a, "", 'a', 95, "/bcd\n"), "a", 0),
        repeat(&a, "/^", 'a', 95, "\\//"));
    expect(t, "limit utf-8",
        fmtpattern(&a, repeat(&a, "", 'a', 95, "\xc3\xa9z\n"), "a", 0),
        repeat(&a, "/^", 'a', 95, "\xc3\xa9/"));
    expect(t, "limit dollar",  // u-ctags leaves the '$' bare
        fmtpattern(&a, repeat(&a, "", 'a', 95, "$b\n"), "a", 0),
        repeat(&a, "/^", 'a', 95, "\\$/"));

    // Whole lines, fields in Universal Ctags' order
    {
        Str text = "struct A {\n  int *f() {}\n};\n";
        Tag tag = {};
        tag.name      = "f";
        tag.kind      = K_FUNCTION;
        tag.line      = 2;
        tag.linepos   = 11;
        tag.scopekind = "struct";
        tag.scope     = "A";
        tag.typekind  = "typename";
        tag.type      = "int *";
        tag.filescope = 1;
        expect(t, "line", fmttag(&a, "src\\x.cpp", LANG_CXX, text, tag),
               "f\tsrc/x.cpp\t/^  int *f() {}$/;\"\tf\tline:2\tstruct:A"
               "\ttyperef:typename:int *\tfile:\n");
    }
    {
        Tag tag = {};
        tag.name     = "cb";
        tag.kind     = F_COMMON;
        tag.line     = 63;
        tag.lineaddr = 1;
        expect(t, "line address", fmttag(&a, "x.f90", LANG_FORTRAN, "", tag),
               "cb\tx.f90\t63;\"\tc\tline:63\n");
    }

    struct { Str name, want; } names[] = {
        {" x",    "\\x20x"},
        {"!x",    "\\x21x"},
        {"x!",    "x!"},
        {"a\tb",  "a\\tb"},
        {"a\\b",  "a\\\\b"},
        {"\x01",  "\\x01"},
        {"\x7f",  "\\x7F"},
        {"\x1b",  "\\x1B"},
    };
    for (iz i = 0; i < countof(names); i++) {
        Buf b(&a, 16);
        printescaped(&b, names[i].name, 1);
        expect(t, names[i].want, finish(&b), names[i].want);
    }

    // Merging sorted streams: strcmp() order, each line once
    {
        Str lines[] = {
            "ab\tx", "a\tx", "a_b\tx", "Z\tx", "\xc3\xa9\tx", "b\tx",
            "a\tx", "!_TAG\tx", "a\ty",
        };
        i32  n   = countof(lines);
        Line *ls = alloc<Line>(&a, n);
        for (i32 i = 0; i < n; i++) {
            ls[i].data = lines[i].data;
            ls[i].len  = lines[i].len;
            ls[i].key  = linekey(ls[i].data, ls[i].len);
        }
        // Three streams, each sorted
        sort(ls, 3, lineless, a);
        sort(ls+3, 3, lineless, a);
        sort(ls+6, 3, lineless, a);
        Stream s[] = {{ls, ls+3}, {ls+3, ls+6}, {ls+6, ls+9}};

        // Lines must end in newlines, as they do in a worker's buffer
        Buf joined(&a, 256);
        for (i32 i = 0; i < n; i++) {
            iz at = joined.len;
            print(&joined, {ls[i].data, ls[i].len});
            putbyte(&joined, '\n');
            ls[i].data = joined.data + at;
        }

        Os  *os = newos(&a);
        Out *o  = newout(os, &a, 1);
        iz   count = mergelines(o, s, 3, a);
        flush(o);
        expect(t, "merge", finish(&os->out),
               "!_TAG\tx\nZ\tx\na\tx\na\ty\na_b\tx\nab\tx\nb\tx\n\xc3\xa9\tx\n");
        expect(t, "merge count", num(&a, count), "8");
    }
}

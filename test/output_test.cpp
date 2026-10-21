// Tag formatting: escapes, patterns, fields, anonymous names, merging

// Format one tag found in a file of the given text.
static Str fmttag(Arena *a, Str path, i32 lang, Str text, Tag tag,
                  Format *fmt = 0)
{
    Source src = {};
    src.path     = path;
    src.outpath  = fmt && fmt->ectags ? path : outpath(a, path, 1);
    src.text     = text;
    src.anonhash = anonhash(path);
    src.lang     = lang;
    Kinds k = {0xffffffff, 0xffffffff};
    Buf   b(a, 256);
    File  f = {};
    f.src    = &src;
    f.kinds  = &k;
    f.fmt    = fmt;
    f.extras = defaultextras();
    f.out    = &b;
    tagemit(&f, &tag);
    return finish(&b);
}

// The address alone, for a tag on the first line of the text.
static Str fmtpattern(Arena *a, Str text, Str name, b32 truncate,
                      iz max = PATTERNLIMIT)
{
    Tag t = {};
    t.name     = name;
    t.truncate = truncate;
    Buf b(a, 256);
    printpattern(&b, text, &t, max);
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
    expect(t, "limit 4",
        fmtpattern(&a, "int x;\n", "x", 0, 4), "/^int /");
    expect(t, "limit 4 dollar",
        fmtpattern(&a, "abc$d\n", "x", 0, 4), "/^abc\\$/");
    expect(t, "no limit",
        fmtpattern(&a, repeat(&a, "", 'a', 200, "$\n"), "a", 0,
                   (iz)(~(uz)0>>1)),
        repeat(&a, "/^", 'a', 200, "\\$$/"));

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

    // Other formats: every field written, in order (FLD-4, FLD-5)
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
        tag.anon      = 1;
        tag.qualified = 1;
        tag.aliased   = "a\\b";
        tag.inherits  = "B<\"\t\">";
        tag.access    = "public";
        tag.impl      = "deferred";
        tag.signature = "(char c='\\\\')";
        Format fmt = {};
        fmt.patmax = PATTERNLIMIT;
        fmt.fields = ~(u64)0;
        expect(t, "all fields", fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt),
               "f\tx.cpp\t/^  int *f() {}$/;\"\tkind:function\tline:2"
               "\tlanguage:C++\tscope:struct:A\ttyperef:typename:int *\tfile:"
               "\tinherits:B<\"\\t\">\taccess:public\timplementation:deferred"
               "\tsignature:(char c='\\\\\\\\')"
               "\troles:def\textras:fileScope,qualified,anonymous"
               "\tname:a\\\\b\n");
        tag.inherits  = {};
        tag.access    = {};
        tag.impl      = {};
        tag.signature = {};

        // Only a pattern, with the original format
        fmt.nofields = 1;
        expect(t, "format 1", fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt),
               "f\tx.cpp\t/^  int *f() {}$/\n");

        // Names, paths, and scopes as they are under e-ctags, but those
        // with a tab or newline dropped (FLD-6)
        tag.name      = "f\\";
        tag.scope     = "A\\";
        tag.type      = "B\\";
        tag.aliased   = {};
        fmt.nofields  = 0;
        fmt.ectags    = 1;
        fmt.fields    = (u64)1<<FIELD_KIND | (u64)1<<FIELD_SCOPE |
                        (u64)1<<FIELD_TYPEREF;
        expect(t, "e-ctags", fmttag(&a, "s\\x.cpp", LANG_CXX, text, tag, &fmt),
               "f\\\ts\\x.cpp\t/^  int *f() {}$/;\"\tf\tstruct:A\\"
               "\ttyperef:typename:B\\\\\n");
        tag.scope = "A\tB";
        expect(t, "e-ctags scope tab",
               fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt), "");
        fmt.nofields = 1;
        expect(t, "e-ctags scope tab, format 1",
               fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt),
               "f\\\tx.cpp\t/^  int *f() {}$/\n");
        expect(t, "e-ctags file newline",
               fmttag(&a, "x\n.cpp", LANG_CXX, text, tag, &fmt), "");

        // Signatures as they are too, so one with a tab drops the tag
        // while written, but base classes escaped (FLD-8, FLD-9)
        tag.name      = "f";
        tag.scope     = "A";
        tag.inherits  = "B\\\t";
        tag.signature = "(c='\\\\')";
        fmt.nofields  = 0;
        fmt.fields    = (u64)1<<FIELD_INHERITS | (u64)1<<FIELD_SIGNATURE;
        expect(t, "e-ctags signature",
               fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt),
               "f\tx.cpp\t/^  int *f() {}$/;\"\tinherits:B\\\\\\t"
               "\tsignature:(c='\\\\')\n");
        tag.signature = "(c='\0')";  // literals keep embedded NULs
        expect(t, "e-ctags signature NUL",
               fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt), "");
        tag.signature = "(s=\"\t\")";
        expect(t, "e-ctags signature tab",
               fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt), "");
        fmt.fields = (u64)1<<FIELD_INHERITS;
        expect(t, "e-ctags signature tab, not written",
               fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt),
               "f\tx.cpp\t/^  int *f() {}$/;\"\tinherits:B\\\\\\t\n");
        fmt.fields   = (u64)1<<FIELD_SIGNATURE;
        fmt.nofields = 1;
        expect(t, "e-ctags signature tab, format 1",
               fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt),
               "f\tx.cpp\t/^  int *f() {}$/\n");
        tag.inherits  = {};
        tag.signature = {};

        // Addresses by excmd (OUT-6)
        tag.name     = "f";
        fmt.ectags   = 0;
        fmt.nofields = 1;
        struct { i32 excmd; b32 lineaddr; Str want; } addrs[] = {
            {EXCMD_MIXED,   0, "/^  int *f() {}$/"},
            {EXCMD_MIXED,   1, "2"},
            {EXCMD_NUMBER,  0, "2"},
            {EXCMD_NUMBER,  1, "2"},
            {EXCMD_PATTERN, 0, "/^  int *f() {}$/"},
            {EXCMD_PATTERN, 1, "/^  int *f() {}$/"},
            {EXCMD_COMBINE, 0, "2;/^  int *f() {}$/"},
            {EXCMD_COMBINE, 1, "2"},
        };
        for (iz i = 0; i < countof(addrs); i++) {
            fmt.excmd    = addrs[i].excmd;
            tag.lineaddr = addrs[i].lineaddr;
            Str want = concat(&a, "f\tx.cpp\t", addrs[i].want);
            expect(t, addrs[i].want,
                   fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt),
                   concat(&a, want, "\n"));
        }
    }

    // Escapes in other formats, as by default: given the default fields,
    // tagline writes the same lines as tagemit
    {
        Str text = "struct A {\n  int *f() {}\n};\n";
        Str head = "\tx.cpp\t/^  int *f() {}$/;\"\tf\tline:2\tstruct:";
        struct { Str name, scope, type, aliased, want; } tags[] = {
            {" x",   "A",    "int *", {},
             "\\x20x|A\ttyperef:typename:int *\n"},
            {"!x",   "A\\B", "int *", {},
             "\\x21x|A\\\\B\ttyperef:typename:int *\n"},
            {"a\\b", "A\tB", "B\\",   "c\x01",
             "a\\\\b|A\\tB\ttyperef:typename:B\\\\\tname:c\\x01\n"},
            {"x!",   "\x7f", "int",   " y",
             "x!|\\x7F\ttyperef:typename:int\tname: y\n"},
        };
        Format fmt = {};
        fmt.fields = defaultfields();
        fmt.patmax = PATTERNLIMIT;
        for (iz i = 0; i < countof(tags); i++) {
            Tag tag = {};
            tag.name      = tags[i].name;
            tag.kind      = K_FUNCTION;
            tag.line      = 2;
            tag.linepos   = 11;
            tag.scopekind = "struct";
            tag.scope     = tags[i].scope;
            tag.typekind  = "typename";
            tag.type      = tags[i].type;
            tag.aliased   = tags[i].aliased;
            Cut parts = cut(tags[i].want, '|');
            Str want  = concat(&a, concat(&a, parts.head, head), parts.tail);
            expect(t, want, fmttag(&a, "x.cpp", LANG_CXX, text, tag), want);
            expect(t, want, fmttag(&a, "x.cpp", LANG_CXX, text, tag, &fmt),
                   want);
        }
    }

    // Whether a file's first bytes make it a tags file Universal Ctags
    // replaces (CLI-3), each checked against it
    struct { Str head; b32 want; } heads[] = {
        {"a\tb\t/x/\n",        1}, {"a\tb\t?x?\n",       1},
        {"a\tb\t12\n",         1}, {"a\tb\t12;\"\tv\n",   1},
        {"a\tb\t12x\n",        0}, {"a\tb\t;12\n",        0},
        {"a\tb\t\n",           0}, {"a\tb\t\r\n",         0},
        {"a\tb\t\t/x/\n",      0}, {"a\t\tb\t/x/\n",      0},
        {"a\tb;\t/x/\n",       0}, {"#a\tb\t/x/\n",       0},
        {"\tb\t/x/\n",         0}, {"a\tb\n",             0},
        {"hello\n",            0}, {"\f\nx,1\n",          1},
        {"\f\rx",              1}, {"\fx",                0},
        {"\f",                 0}, {"a\0\tb\t/x/\n",      0},
        {"a\tb\t1\r2\n",       1}, {"a\tb\t12\r\n",       1},
        {"int a;\t/* x */\n",  0}, {"#define X\t1\n",     0},
        {"a\tb\t/\n",          1}, {"a b\tc d\t 1\n",     0},
        {"a\tb\t1 2;x\n",      0},
        {"!_TAG_FILE_FORMAT\t2\t/x/\n", 1},
    };
    for (iz i = 0; i < countof(heads); i++) {
        expect(t, heads[i].head, num(&a, istagsline(heads[i].head)),
               num(&a, heads[i].want));
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
            ls[i].key  = linekey<0>(ls[i].data, ls[i].len);
        }
        // Three streams, each sorted
        sort(ls, 3, lineless<0>, a);
        sort(ls+3, 3, lineless<0>, a);
        sort(ls+6, 3, lineless<0>, a);
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
        iz   count = mergelines<0>(o, s, 3, a);
        flush(o);
        expect(t, "merge", finish(&os->out),
               "!_TAG\tx\nZ\tx\na\tx\na\ty\na_b\tx\nab\tx\nb\tx\n\xc3\xa9\tx\n");
        expect(t, "merge count", num(&a, count), "8");
    }

    // Merging folded streams: struppercmp() order, ties by bytes, so
    // that duplicates meet and are written once
    {
        Str lines[] = {
            "ab\tx", "a\tx", "_\tx", "B\tx", "\xc3\xa9\tx", "AB\tx",
            "a\tx", "!_TAG\tx", "Z\tx", "aB\tx", "b\tx", "A\tx",
        };
        i32  n   = countof(lines);
        Buf  joined(&a, 256);
        Line *ls = alloc<Line>(&a, n);
        for (i32 i = 0; i < n; i++) {
            iz at = joined.len;
            print(&joined, lines[i]);
            putbyte(&joined, '\n');
            ls[i].data = joined.data + at;
            ls[i].len  = lines[i].len;
            ls[i].key  = linekey<1>(ls[i].data, ls[i].len);
        }
        sort(ls, 4, lineless<1>, a);
        sort(ls+4, 4, lineless<1>, a);
        sort(ls+8, 4, lineless<1>, a);
        Stream s[] = {{ls, ls+4}, {ls+4, ls+8}, {ls+8, ls+12}};

        Os  *os = newos(&a);
        Out *o  = newout(os, &a, 1);
        iz   count = mergelines<1>(o, s, 3, a);
        flush(o);
        expect(t, "fold merge", finish(&os->out),
               "!_TAG\tx\nA\tx\na\tx\nAB\tx\naB\tx\nab\tx\nB\tx\nb\tx\n"
               "Z\tx\n_\tx\n\xc3\xa9\tx\n");
        expect(t, "fold merge count", num(&a, count), "11");
    }
}

// Unit tests: a platform layer with an in-memory file system
// $ cmake --build build && ./build/ctags_test [GROUP]
// This is free and unencumbered software released into the public domain.
#include <stdio.h>
#include <stdlib.h>
#include "ctags.cpp"
#include "test/vfs.cpp"

struct Test {
    Arena perm;
    i32   run;
    i32   failed;
};

static void printstr(FILE *f, Str s)
{
    fwrite(s.data, 1, (uz)s.len, f);
}

// Compare strings, printing both on mismatch.
static b32 expect(Test *t, Str name, Str got, Str want)
{
    t->run++;
    if (got == want) {
        return 1;
    }
    t->failed++;
    fprintf(stderr, "FAIL: ");
    printstr(stderr, name);
    fprintf(stderr, "\n  got:  [");
    printstr(stderr, got);
    fprintf(stderr, "]\n  want: [");
    printstr(stderr, want);
    fprintf(stderr, "]\n");
    return 0;
}

static Str num(Arena *a, i64 v)
{
    Buf b(a, 24);
    print(&b, v);
    return finish(&b);
}

// Split a command line on spaces, for tests that run the program.
static Slice<Str> words(Arena *a, Str s)
{
    Slice<Str> r = {};
    r = push(a, r, Str("ctags"));
    Cut c = {};
    for (c.tail = s; c.tail.len;) {
        c = cut(c.tail, ' ');
        if (c.head.len) {
            r = push(a, r, c.head);
        }
    }
    return r;
}

struct Run {
    Str out;
    Str err;
    i32 status;
};

// Run the program on the file system with the given arguments, in
// memory poisoned to expose reads of what was never written.
static Run run(Os *os, Arena a, Str cmd)
{
    Slice<Str> args = words(&a, cmd);
    iz    cap = (iz)32<<20;
    byte *mem = (byte *)allocbytes(&a, cap);
    __builtin_memset(mem, 0x55, (uz)cap);
    Arena m = {mem, mem+cap};

    os->out = Buf(os->a, 1<<12);
    os->err = Buf(os->a, 1<<12);
    Run r = {};
    r.status = ctags_main(os, m, args.data, (i32)args.len);
    r.out    = finish(&os->out);
    r.err    = finish(&os->err);
    return r;
}

#include "test/wide_test.cpp"
#include "test/output_test.cpp"
#include "test/cli_test.cpp"
#include "test/walk_test.cpp"

int main(int argc, char **argv)
{
    iz    cap = (iz)1<<30;
    byte *mem = (byte *)malloc((uz)cap);
    Test  t   = {};
    t.perm = {mem, mem+cap};

    struct {
        char const *name;
        void      (*fn)(Test *);
    } groups[] = {
        {"wide",   test_wide},
        {"output", test_output},
        {"cli",    test_cli},
        {"walk",   test_walk},
    };
    i32 matched = 0;
    for (iz i = 0; i < countof(groups); i++) {
        Str name = {(u8 *)groups[i].name, 0};
        for (; name.data[name.len]; name.len++) {}
        Str want = {};
        if (argc > 1) {
            want.data = (u8 *)argv[1];
            for (; want.data[want.len]; want.len++) {}
        }
        if (!want.data || want==name) {
            fprintf(stderr, "%s\n", groups[i].name);
            groups[i].fn(&t);
            matched++;
        }
    }

    if (!matched) {
        fprintf(stderr, "ctags_test: no group %s\n", argv[1]);
        return 1;
    }
    fprintf(stderr, "%d tests, %d failed\n", t.run, t.failed);
    return t.failed ? 1 : 0;
}

// Unit tests: a platform layer with an in-memory file system
// $ cmake --build build && ./build/ctags_test [GROUP]
// This is free and unencumbered software released into the public domain.
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "ctags.cpp"
#include "test/vfs.cpp"

struct Test {
    Arena       perm;
    i32         run;
    i32         failed;
    char const *exportdir;  // export tag cases here instead of running
    i32         exported;
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

// Tag cases: one file, tagged alone with some options, and the exact
// lines expected on standard output (no pseudo-tags). SHARED cases
// match Universal Ctags exactly. IMPROVED cases depart from it, which
// uctags records. OURS cases test what it cannot run, such as options
// it lacks. Exported (--export DIR), a CTest checks the SHARED and
// IMPROVED expectations against the real Universal Ctags.
enum { SHARED, IMPROVED, OURS };

struct TagCase {
    i32 kind;
    Str name;
    Str file;    // its extension picks the language
    Str args;    // options before "-f - FILE"
    Str src;
    Str want;
    Str uctags;  // for IMPROVED cases: Universal Ctags' output
};

static void writefile(char const *path, Str s)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "ctags_test: cannot write %s\n", path);
        exit(1);
    }
    fwrite(s.data, 1, (uz)s.len, f);
    fclose(f);
}

static void makedir(char const *path)
{
    #ifdef _WIN32
    mkdir(path);
    #else
    mkdir(path, 0777);
    #endif
}

// Write DIR/NNNN/FILE, the source; DIR/NNNN/case, its kind and file,
// name, and options on three lines; and DIR/NNNN/want.tags, Universal
// Ctags' expected output.
static void exportcase(Test *t, Arena a, TagCase *c)
{
    char dir[512];
    char path[1024];
    makedir(t->exportdir);
    snprintf(dir, sizeof(dir), "%s/%04d", t->exportdir, t->exported++);
    makedir(dir);
    Buf b(&a, 64);
    print(&b, c->file);
    putbyte(&b, 0);
    snprintf(path, sizeof(path), "%s/%s", dir, (char *)b.data);
    writefile(path, c->src);
    snprintf(path, sizeof(path), "%s/case", dir);
    Buf info(&a, 256);
    print(&info, c->kind==SHARED ? Str("shared ") : c->kind==IMPROVED ? Str("improved ") : Str("ours "));
    print(&info, c->file);
    putbyte(&info, '\n');
    print(&info, c->name);
    putbyte(&info, '\n');
    print(&info, c->args);
    putbyte(&info, '\n');
    writefile(path, finish(&info));
    snprintf(path, sizeof(path), "%s/want.tags", dir);
    writefile(path, c->kind==IMPROVED ? c->uctags : c->want);
}

static void runcases(Test *t, TagCase *cases, iz n)
{
    for (iz i = 0; i < n; i++) {
        TagCase *c = cases + i;
        Arena    a = t->perm;
        if (t->exportdir) {
            if (c->kind != OURS) {
                exportcase(t, a, c);
            }
            continue;
        }
        Os *os = newos(&a);
        vfsfile(os, c->file, c->src);
        Buf cmd(&a, 128);
        print(&cmd, c->args);
        print(&cmd, " -f - ");
        print(&cmd, c->file);
        Run r = run(os, a, finish(&cmd));
        expect(t, c->name, r.out, c->want);
        expect(t, c->name, r.err, "");
    }
}

#include "test/wide_test.cpp"
#include "test/output_test.cpp"
#include "test/cli_test.cpp"
#include "test/walk_test.cpp"
#include "test/cxx_test.cpp"
#include "test/fortran_test.cpp"

int main(int argc, char **argv)
{
    iz    cap = (iz)1<<30;
    byte *mem = (byte *)malloc((uz)cap);
    Test  t   = {};
    t.perm = {mem, mem+cap};

    struct {
        char const *name;
        void      (*fn)(Test *);
        b32         cases;  // has tag cases to export
    } groups[] = {
        {"wide",    test_wide,    0},
        {"output",  test_output,  0},
        {"cli",     test_cli,     0},
        {"walk",    test_walk,    0},
        {"c",       test_c,       1},
        {"cxx",     test_cxx,     1},
        {"fortran", test_fortran, 1},
    };

    Str want = {};
    if (argc==3 && !__builtin_strcmp(argv[1], "--export")) {
        t.exportdir = argv[2];
    } else if (argc > 1) {
        want.data = (u8 *)argv[1];
        for (; want.data[want.len]; want.len++) {}
    }

    i32 matched = 0;
    for (iz i = 0; i < countof(groups); i++) {
        Str name = {(u8 *)groups[i].name, 0};
        for (; name.data[name.len]; name.len++) {}
        if (t.exportdir ? groups[i].cases : !want.data || want==name) {
            if (!t.exportdir) {
                fprintf(stderr, "%s\n", groups[i].name);
            }
            groups[i].fn(&t);
            matched++;
        }
    }

    if (t.exportdir) {
        fprintf(stderr, "%d cases exported\n", t.exported);
        return 0;
    }
    if (!matched) {
        fprintf(stderr, "ctags_test: no group %s\n", argv[1]);
        return 1;
    }
    fprintf(stderr, "%d tests, %d failed\n", t.run, t.failed);
    return t.failed ? 1 : 0;
}

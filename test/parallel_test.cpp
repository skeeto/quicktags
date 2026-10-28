// Determinism: the output depends only on the input, never on the number
// of workers, the order they run in, or what ran before in the process.

static Str parallelcmd(Arena *a, Str mode, Str jobs)
{
    Buf b(a, 64);
    print(&b, mode);
    putbyte(&b, ' ');
    print(&b, jobs);
    print(&b, " -R -f - src");
    return finish(&b);
}

static void test_parallel(Test *t)
{
    Arena a  = t->perm;
    Os   *os = newos(&a);
    os->cwd   = "/home/u";    // for --tag-relative
    os->mtime = 1600000000;  // for --extras=+f
    vfsfile(os, "src/main.c",
        "#include \"util.h\"\n"
        "static int count;\n"
        "struct { int x; } anon;\n"
        "int main(void) { return helper(count); }\n");
    vfsfile(os, "src/util.h",
        "#define HELPER 1\n"
        "int helper(int);\n"
        "enum { A, B };\n");
    vfsfile(os, "src/lib/vec.cpp",
        "namespace { struct V { float x, y; }; }\n"
        "template<class T> T dot(T a, T b) { return a*b; }\n"
        "auto f = [](int n) { return n; };\n");
    vfsfile(os, "src/lib/vec.hpp",
        "class Vec { public: Vec(); int n; };\n");
    vfsfile(os, "src/num/solve.f90",
        "module solve\n"
        "contains\n"
        "  subroutine step(x)\n"
        "    real :: x\n"
        "  end subroutine\n"
        "end module\n");
    vfsfile(os, "src/num/old.f",
        "      PROGRAM OLD\n"
        "      END\n");
    vfsdir(os, "src/empty");
    vfsfile(os, "src/README", "of no language: a section under etags\n");
    vfsfile(os, "src/lib/notes.txt", "");
    for (i32 i = 0; i < 40; i++) {
        // Spread over directories, some nested, for the walk to interleave
        Buf name(&a, 32);
        print(&name, "src/gen/d");
        print(&name, (i64)(i%4));
        print(&name, i%3 ? Str("/") : Str("/sub/"));
        print(&name, "f");
        print(&name, (i64)i);
        print(&name, ".c");
        Buf src(&a, 64);
        print(&src, "struct s");
        print(&src, (i64)i);
        print(&src, " { int v; } g");
        print(&src, (i64)i);
        print(&src, ";\nstruct { int w; } h");
        print(&src, (i64)i);
        print(&src, ";\n");
        vfsfile(os, finish(&name), finish(&src));
    }

    Run first = run(os, a, "-R -f - src");
    expect(t, "parallel status", num(&a, first.status), "0");
    expect(t, "parallel tags", num(&a, first.out.len > 2000), "1");

    Str jobs[] = {"--jobs=1", "--jobs=2", "--jobs=3", "--jobs=8", "--jobs=32"};
    for (iz i = 0; i < countof(jobs); i++) {
        for (i32 reverse = 0; reverse < 2; reverse++) {
            os->ncpu    = 4;
            os->reverse = reverse;
            Run r = run(os, a, parallelcmd(&a, "", jobs[i]));
            expect(t, jobs[i], r.out, first.out);
        }
    }

    // Default threads, and again in the same process
    os->reverse = 0;
    os->ncpu    = 24;
    for (i32 i = 0; i < 3; i++) {
        Run r = run(os, a, "-R -f - src");
        expect(t, "repeated", r.out, first.out);
    }

    // Workers taking turns at random, in each mode, must write what one
    // worker alone writes, and more than one must have tagged files
    Str modes[] = {
        "",
        "--extras=+q",
        "--tag-relative=never",
        "--fields=+iaSmlKzZEr --extras=+q",
        "--excmd=combine --output-format=e-ctags",
        "--tag-relative=never --fields=+iaSK -n",
        "--sort=no",
        "-u --extras=+q --fields=+K",
        "--sort=foldcase",
        "--sort=foldcase --extras=+q",
        "--extras=+f",
        "-u --extras=+fq --fields=+KE",
        "-e",
        "-e --extras=+fq --kinds-C=* --tag-relative=never",
        "--_bigfile=48",  // most files, but not f0.c to f9.c, tagged first
        "--_bigfile=48 --sort=no --extras=+f",
        "-e --_bigfile=1",
    };
    for (iz m = 0; m < countof(modes); m++) {
        os->seed = 0;
        Run one = run(os, a, parallelcmd(&a, modes[m], "--jobs=1"));
        expect(t, "seeded status", num(&a, one.status), "0");
        for (u64 seed = 1; seed <= 8; seed++) {
            for (iz i = 1; i < countof(jobs); i++) {
                Buf name(&a, 64);
                print(&name, "seed ");
                print(&name, (i64)seed);
                putbyte(&name, ' ');
                print(&name, jobs[i]);
                putbyte(&name, ' ');
                print(&name, modes[m]);

                os->seed = seed;
                Run r = run(os, a, parallelcmd(&a, modes[m], jobs[i]));
                expect(t, finish(&name), r.out, one.out);
                expect(t, finish(&name), num(&a, os->nworked > 1), "1");
            }
        }
    }

    // Appending to a tags file, in order or not, in each order
    Str olds[] = {
        "!_TAG_FILE_SORTED\t1\t/x/\nA\tsrc/main.c\t1\ng3\tsrc/gone.c\t1\n"
        "zz\tsrc/gen/d1/f1.c\t9\n",
        "zz\tsrc/gen/d1/f1.c\t9\r\ng3\tsrc/gone.c\t1\r\nA\tsrc/main.c\t1\r\n",
    };
    Str appends[] = {"-a", "-a --sort=no", "-a --sort=foldcase",
                     "-a -e -f tags"};
    for (iz m = 0; m < countof(appends); m++) {
        for (iz o = 0; o < countof(olds); o++) {
            os->seed = 0;
            vfsfile(os, "tags", olds[o]);
            Str  cmd  = concat(&a, appends[m], " -R src --jobs=");
            Run  one  = run(os, a, concat(&a, cmd, "1"));
            Str  want = vfsfind(os, "tags", 0)->data;
            expect(t, "append status", num(&a, one.status), "0");
            expect(t, "append tags", num(&a, want.len > 2000), "1");
            for (u64 seed = 0; seed <= 8; seed++) {
                for (i32 j = 2; j <= 32; j *= 2) {
                    Buf name(&a, 64);
                    print(&name, appends[m]);
                    print(&name, " seed ");
                    print(&name, (i64)seed);
                    print(&name, " jobs ");
                    print(&name, (i64)j);
                    print(&name, " old ");
                    print(&name, (i64)o);

                    os->seed = seed;
                    vfsfile(os, "tags", olds[o]);
                    Run r = run(os, a, concat(&a, cmd, num(&a, j)));
                    expect(t, finish(&name), num(&a, r.status), "0");
                    expect(t, finish(&name), vfsfind(os, "tags", 0)->data, want);
                }
            }
        }
    }
    vfsunlink(os, "tags");

    // Arguments, more than there are workers, which the workers then
    // examine: missing ones are reported in their order
    Buf list(&a, 1<<10);
    print(&list, "src/zz.c\n");
    for (i32 i = 0; i < 40; i++) {
        print(&list, "src/gen/d");
        print(&list, (i64)(i%4));
        print(&list, i%3 ? Str("/") : Str("/sub/"));
        print(&list, "f");
        print(&list, (i64)i);
        print(&list, ".c\n");
    }
    print(&list, "src/aa.c\nsrc/main.c\nsrc/util.h\nsrc/lib\nsrc/num\n");
    vfsfile(os, "list", finish(&list));
    Str missing = "ctags: Warning: cannot open input file \"src/zz.c\" : "
                  "No such file or directory\n"
                  "ctags: Warning: cannot open input file \"src/aa.c\" : "
                  "No such file or directory\n";
    os->seed = 0;
    Run unsorted = run(os, a, "--jobs=1 -u -L list -f -");
    for (u64 seed = 0; seed <= 8; seed++) {
        os->seed = seed;
        Run r = run(os, a, "--jobs=3 -L list -f -");
        expect(t, "list", r.out, first.out);
        expect(t, "list", r.err, missing);
        r = run(os, a, "--jobs=3 -u -L list -f -");
        expect(t, "unsorted list", r.out, unsorted.out);
        expect(t, "unsorted list", r.err, missing);
    }
    os->seed = 0;
}

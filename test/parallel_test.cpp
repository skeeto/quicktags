// Determinism: the output depends only on the input, never on the number
// of workers, the order they run in, or what ran before in the process.

static void test_parallel(Test *t)
{
    Arena a  = t->perm;
    Os   *os = newos(&a);
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
    for (i32 i = 0; i < 40; i++) {
        Buf name(&a, 32);
        print(&name, "src/gen/f");
        print(&name, (i64)i);
        print(&name, ".c");
        Buf src(&a, 64);
        print(&src, "struct s");
        print(&src, (i64)i);
        print(&src, " { int v; } g");
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
            Buf cmd(&a, 64);
            print(&cmd, jobs[i]);
            print(&cmd, " -R -f - src");
            Run r = run(os, a, finish(&cmd));
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
}

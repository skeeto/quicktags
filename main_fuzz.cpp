// Fuzz test layer for AFL++, over the in-memory file system: the first
// byte picks a language and kinds, the rest is a file to tag. Undefined
// behavior traps, and the output is checked for the format's invariants.
// $ afl-clang-fast++ -std=c++20 -g3 -fsanitize=undefined -fsanitize-trap -o fuzz main_fuzz.cpp
// $ mkdir i && printf 'xint x;' >i/x && afl-fuzz -i i -o o ./fuzz
// Without AFL++ it tags one input from standard input.
// This is free and unencumbered software released into the public domain.
#include <stdio.h>
#include <stdlib.h>
#include "src/ctags.cpp"
#include "test/vfs.cpp"

#ifndef __AFL_FUZZ_TESTCASE_LEN
static u8  fuzzbuf[1<<20];
static i32 fuzzlen;
#  define __AFL_FUZZ_INIT()
#  define __AFL_INIT() fuzzlen = (i32)fread(fuzzbuf, 1, sizeof(fuzzbuf), stdin)
#  define __AFL_FUZZ_TESTCASE_BUF fuzzbuf
#  define __AFL_FUZZ_TESTCASE_LEN fuzzlen
#  define __AFL_LOOP(n) !fuzzdone++
static i32 fuzzdone;
#endif

__AFL_FUZZ_INIT();

int main()
{
    __AFL_INIT();
    iz    cap = (iz)1<<26;
    byte *mem = (byte *)malloc((uz)cap);
    u8   *buf = __AFL_FUZZ_TESTCASE_BUF;
    while (__AFL_LOOP(10000)) {
        i32 len = __AFL_FUZZ_TESTCASE_LEN;
        if (len < 1) {
            continue;
        }

        Str names[] = {"f.c", "f.cpp", "f.h", "f.hpp", "f.f90", "f.f"};
        Str name    = names[buf[0] % countof(names)];
        Str kinds   = buf[0]&0x80 ? Str("--kinds-C=*") : Str("--kinds-C=+p");
        Str ckinds  = buf[0]&0x80 ? Str("--kinds-C++=*") : Str("--kinds-C++=+p");
        Str fkinds  = buf[0]&0x80 ? Str("--kinds-Fortran=*") : Str("--kinds-Fortran=+L");

        Arena a  = {mem, mem+cap/2};
        Os   *os = newos(&a);
        vfsfile(os, name, {buf+1, len-1});
        Str   args[] = {"ctags", kinds, ckinds, fkinds, "-f", "-", name};
        Arena m      = {mem+cap/2, mem+cap};
        if (ctags_main(os, m, args, countof(args))) {
            __builtin_trap();
        }

        // Each line has a name, and lines are sorted and unique
        Str out  = finish(&os->out);
        Str prev = {};
        for (Cut c = cut(out, '\n'); c.ok; c = cut(c.tail, '\n')) {
            if (!c.head.len || c.head[0]=='\t') {
                __builtin_trap();
            }
            if (prev.data && compare(prev, c.head) >= 0) {
                __builtin_trap();
            }
            prev = c.head;
        }
    }
    return 0;
}

// Fuzz test layer for AFL++, over the in-memory file system: the first
// byte picks a language, kinds, -I macros and the order, the rest is a
// file to tag.
// Undefined behavior traps, and the output is checked for the format's
// invariants.
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
        Str macros  = buf[0]&0x40 ? Str("-Ix=x,y=z,z=y,a+,b+=c(,T=y,MACRO+=f(,S+=;S,R+=)R")
                                  : Str("-I-");
        Str sorts[] = {"--sort=no", "--sort=yes", "--sort=foldcase", "--sort=yes"};
        i32 order   = buf[0]>>4 & 3;  // SORT_*, then yes again

        // Tagged in the chosen order, then unsorted, by default too
        Arena a = {mem, mem+cap/2};
        Arena m = {mem+cap/2, mem+cap};
        Str   out[2] = {};
        for (i32 run = 0; run < 1+(order==SORT_NO); run++) {
            Os *os = newos(&a);
            vfsfile(os, name, {buf+1, len-1});
            Str sort   = run ? Str("--sort=yes") : sorts[order];
            Str args[] = {"ctags", kinds, ckinds, fkinds, macros, sort, "-f",
                          "-", name};
            if (ctags_main(os, m, args, countof(args))) {
                __builtin_trap();
            }
            out[run] = finish(&os->out);
        }

        // Each line has a name, and lines are sorted and unique, by bytes
        // or folded, or unsorted, those sorted are the default output
        Str        prev  = {};
        Slice<Str> lines = {};
        for (Cut c = cut(out[0], '\n'); c.ok; c = cut(c.tail, '\n')) {
            if (!c.head.len || c.head[0]=='\t') {
                __builtin_trap();
            }
            Line x = {linekey<1>(prev.data, prev.len), prev.data, prev.len};
            Line y = {linekey<1>(c.head.data, c.head.len), c.head.data, c.head.len};
            if (prev.data && order==SORT_FOLD && !lineless<1>(&x, &y)) {
                __builtin_trap();
            } else if (prev.data && order!=SORT_NO && order!=SORT_FOLD &&
                       compare(prev, c.head)>=0) {
                __builtin_trap();
            }
            prev  = c.head;
            lines = push(&a, lines, c.head);
        }
        if (order == SORT_NO) {
            sort(lines.data, lines.len, strless, a);
            Buf b(&a, out[0].len+1);
            for (iz i = 0; i < lines.len; i++) {
                if (!i || !(lines[i] == lines[i-1])) {
                    print(&b, lines[i]);
                    putbyte(&b, '\n');
                }
            }
            if (!(finish(&b) == out[1])) {
                __builtin_trap();
            }
        }
    }
    return 0;
}

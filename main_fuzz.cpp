// Fuzz test layer for AFL++, over the in-memory file system: the first
// byte picks a language, kinds, -I macros, file tags, and the order or
// Emacs tags, the rest is a file to tag.
// Undefined behavior traps, and the output is checked for the format's
// invariants, and against the output without the fast paths.
// $ afl-clang-fast++ -std=c++20 -g3 -fsanitize=undefined -fsanitize-trap -o fuzz main_fuzz.cpp
// $ mkdir i && printf 'xint x;' >i/x && afl-fuzz -i i -o o ./fuzz
// Without AFL++ it tags one input from standard input.
// This is free and unencumbered software released into the public domain.
#include <stdio.h>
#include <stdlib.h>
#include "src/ctags.cpp"
#include "test/vfs.cpp"
#include "test/etagslint.cpp"

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
        Str extras  = buf[0]&0x08 ? Str("--extras=+f") : Str("--extras=-f");
        Str sorts[] = {"--sort=no", "--sort=yes", "--sort=foldcase", "-e"};
        i32 order   = buf[0]>>4 & 3;  // SORT_*, then Emacs tags
        b32 etags   = order == 3;

        // Tagged in the chosen order or as Emacs tags, then sorted too,
        // then as first without the fast paths
        Arena a = {mem, mem+cap/2};
        Arena m = {mem+cap/2, mem+cap};
        Str   out[3] = {};
        b32   twice  = order==SORT_NO || etags;
        for (i32 run = 0; run < 3; run++) {
            if (run==1 && !twice) {
                continue;
            }
            Os *os = newos(&a);
            vfsfile(os, name, {buf+1, len-1});
            Str sort   = run==1 ? Str("--sort=yes") : sorts[order];
            Str fast   = run==2 ? Str("--_nofast") : Str("--_bigfile=0");
            Str args[] = {"ctags", kinds, ckinds, fkinds, macros, extras, sort,
                          fast, "-f", "-", name};
            if (ctags_main(os, m, args, countof(args))) {
                __builtin_trap();
            }
            out[run] = finish(&os->out);
        }
        if (!(out[2] == out[0])) {
            __builtin_trap();
        }

        // Emacs tags are well formed, and of the sorted output's tags
        if (etags) {
            if (etagslint(a, out[0], name, {buf+1, len-1}, out[1]).data) {
                __builtin_trap();
            }
            continue;
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

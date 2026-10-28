// Scanning: searches over bytes, by vector where the processor has them
//
// A search has up to three forms. The scalar form is always compiled and
// is the reference. The SSE2 form takes 16 bytes at a time, on x86 built
// for SSE2: every x86-64, and w64devkit's i686. The AVX2 form takes 32, on
// x86-64 only, compiled for AVX2 alone and used where the processor and
// system support it. A run's level (Options.simd, SIMD_*) is passed to
// each search, which runs its forms from the widest down: a vector form
// examines whole blocks while they fit, never loading past the end, and
// returns the first match, or else where the tail it leaves begins, from
// which the next form continues.
//
// The vector forms use GCC's vector extensions and builtins, so the core
// still needs no headers. AVX2 forms keep their 32-byte values in
// registers, since Windows aligns the stack only to 16 bytes.
//
// This is free and unencumbered software released into the public domain.

enum : i32 { SIMD_NONE, SIMD_SSE2, SIMD_AVX2 };

// The widest level the processor and system support.
static i32 simdlevel()
{
    #if __x86_64__
    __builtin_cpu_init();  // a constructor's job, but there may be no CRT
    return __builtin_cpu_supports("avx2") ? SIMD_AVX2 : SIMD_SSE2;
    #elif __SSE2__
    return SIMD_SSE2;
    #else
    return SIMD_NONE;
    #endif
}

#if __SSE2__
typedef char V16 __attribute((vector_size(16)));

static V16 load16(u8 *p)
{
    V16 r;
    __builtin_memcpy(&r, p, 16);
    return r;
}

static V16 splat16(u8 c)
{
    return V16{} + (char)c;
}

// Bit i set where lane i of a comparison holds.
static u32 bits16(V16 v)
{
    return (u32)__builtin_ia32_pmovmskb128(v);
}
#endif

#if __x86_64__
typedef char V32 __attribute((vector_size(32)));
#  define AVX2 [[gnu::target("avx2")]]
#  define load32(v, p) __builtin_memcpy(&v, p, 32)
#  define bits32(v)    (u32)__builtin_ia32_pmovmskb256(v)
#endif


// One byte value: the first a in [s, end), or end.

static u8 *findbyte_scalar(u8 *s, u8 *end, u8 a)
{
    for (; s<end && *s!=a; s++) {}
    return s;
}

#if __SSE2__
static u8 *findbyte_sse2(u8 *s, u8 *end, u8 a)
{
    V16 va = splat16(a);
    for (; end-s >= 16; s += 16) {
        u32 m = bits16((V16)(load16(s) == va));
        if (m) {
            return s + __builtin_ctz(m);
        }
    }
    return s;
}
#endif

#if __x86_64__
AVX2 static u8 *findbyte_avx2(u8 *s, u8 *end, u8 a)
{
    V32 va = V32{} + (char)a;
    for (; end-s >= 32; s += 32) {
        V32 x;
        load32(x, s);
        u32 m = bits32((V32)(x == va));
        if (m) {
            return s + __builtin_ctz(m);
        }
    }
    return s;
}
#endif

static u8 *findbyte(i32 simd, u8 *s, u8 *end, u8 a)
{
    #if __x86_64__
    if (simd >= SIMD_AVX2) {
        s = findbyte_avx2(s, end, a);
    }
    #endif
    #if __SSE2__
    if (simd >= SIMD_SSE2) {
        s = findbyte_sse2(s, end, a);
    }
    #endif
    return findbyte_scalar(s, end, a);
}


// Two byte values: the first a or b in [s, end), or end.

static u8 *findbyte2_scalar(u8 *s, u8 *end, u8 a, u8 b)
{
    for (; s<end && *s!=a && *s!=b; s++) {}
    return s;
}

#if __SSE2__
static u8 *findbyte2_sse2(u8 *s, u8 *end, u8 a, u8 b)
{
    V16 va = splat16(a);
    V16 vb = splat16(b);
    for (; end-s >= 16; s += 16) {
        V16 x = load16(s);
        u32 m = bits16((V16)((x == va) | (x == vb)));
        if (m) {
            return s + __builtin_ctz(m);
        }
    }
    return s;
}
#endif

#if __x86_64__
AVX2 static u8 *findbyte2_avx2(u8 *s, u8 *end, u8 a, u8 b)
{
    V32 va = V32{} + (char)a;
    V32 vb = V32{} + (char)b;
    for (; end-s >= 32; s += 32) {
        V32 x;
        load32(x, s);
        u32 m = bits32((V32)((x == va) | (x == vb)));
        if (m) {
            return s + __builtin_ctz(m);
        }
    }
    return s;
}
#endif

static u8 *findbyte2(i32 simd, u8 *s, u8 *end, u8 a, u8 b)
{
    #if __x86_64__
    if (simd >= SIMD_AVX2) {
        s = findbyte2_avx2(s, end, a, b);
    }
    #endif
    #if __SSE2__
    if (simd >= SIMD_SSE2) {
        s = findbyte2_sse2(s, end, a, b);
    }
    #endif
    return findbyte2_scalar(s, end, a, b);
}


// Four byte values: the first a, b, c, or d in [s, end), or end.

static u8 *findbyte4_scalar(u8 *s, u8 *end, u8 a, u8 b, u8 c, u8 d)
{
    for (; s<end && *s!=a && *s!=b && *s!=c && *s!=d; s++) {}
    return s;
}

#if __SSE2__
static u8 *findbyte4_sse2(u8 *s, u8 *end, u8 a, u8 b, u8 c, u8 d)
{
    V16 va = splat16(a);
    V16 vb = splat16(b);
    V16 vc = splat16(c);
    V16 vd = splat16(d);
    for (; end-s >= 16; s += 16) {
        V16 x = load16(s);
        u32 m = bits16((V16)((x == va) | (x == vb) | (x == vc) | (x == vd)));
        if (m) {
            return s + __builtin_ctz(m);
        }
    }
    return s;
}
#endif

#if __x86_64__
AVX2 static u8 *findbyte4_avx2(u8 *s, u8 *end, u8 a, u8 b, u8 c, u8 d)
{
    V32 va = V32{} + (char)a;
    V32 vb = V32{} + (char)b;
    V32 vc = V32{} + (char)c;
    V32 vd = V32{} + (char)d;
    for (; end-s >= 32; s += 32) {
        V32 x;
        load32(x, s);
        u32 m = bits32((V32)((x == va) | (x == vb) | (x == vc) | (x == vd)));
        if (m) {
            return s + __builtin_ctz(m);
        }
    }
    return s;
}
#endif

static u8 *findbyte4(i32 simd, u8 *s, u8 *end, u8 a, u8 b, u8 c, u8 d)
{
    #if __x86_64__
    if (simd >= SIMD_AVX2) {
        s = findbyte4_avx2(s, end, a, b, c, d);
    }
    #endif
    #if __SSE2__
    if (simd >= SIMD_SSE2) {
        s = findbyte4_sse2(s, end, a, b, c, d);
    }
    #endif
    return findbyte4_scalar(s, end, a, b, c, d);
}


// A pair of bytes: the first a followed by b in [s, end), or end.

static u8 *findpair_scalar(u8 *s, u8 *end, u8 a, u8 b)
{
    for (; end-s>=2 && !(s[0]==a && s[1]==b); s++) {}
    return end-s>=2 ? s : end;
}

#if __SSE2__
static u8 *findpair_sse2(u8 *s, u8 *end, u8 a, u8 b)
{
    V16 va = splat16(a);
    V16 vb = splat16(b);
    for (; end-s >= 17; s += 16) {
        u32 m = bits16((V16)((load16(s) == va) & (load16(s+1) == vb)));
        if (m) {
            return s + __builtin_ctz(m);
        }
    }
    return s;
}
#endif

#if __x86_64__
AVX2 static u8 *findpair_avx2(u8 *s, u8 *end, u8 a, u8 b)
{
    V32 va = V32{} + (char)a;
    V32 vb = V32{} + (char)b;
    for (; end-s >= 33; s += 32) {
        V32 x, y;
        load32(x, s);
        load32(y, s+1);
        u32 m = bits32((V32)((x == va) & (y == vb)));
        if (m) {
            return s + __builtin_ctz(m);
        }
    }
    return s;
}
#endif

static u8 *findpair(i32 simd, u8 *s, u8 *end, u8 a, u8 b)
{
    #if __x86_64__
    if (simd >= SIMD_AVX2) {
        s = findpair_avx2(s, end, a, b);
    }
    #endif
    #if __SSE2__
    if (simd >= SIMD_SSE2) {
        s = findpair_sse2(s, end, a, b);
    }
    #endif
    return findpair_scalar(s, end, a, b);
}


// Newlines: how many in [s, end), and just past the last one, or null.

struct Lines {
    iz  count;
    u8 *after;
};

static Lines countlines_scalar(Lines r, u8 *s, u8 *end)
{
    for (; s < end; s++) {
        if (*s == '\n') {
            r.count++;
            r.after = s + 1;
        }
    }
    return r;
}

#if __SSE2__
static u8 *countlines_sse2(Lines *r, u8 *s, u8 *end)
{
    V16 nl = splat16('\n');
    for (; end-s >= 16; s += 16) {
        u32 m = bits16((V16)(load16(s) == nl));
        if (m) {
            r->after = s + 32 - __builtin_clz(m);
            for (; m; m &= m-1) {
                r->count++;
            }
        }
    }
    return s;
}
#endif

#if __x86_64__
AVX2 static u8 *countlines_avx2(Lines *r, u8 *s, u8 *end)
{
    V32 nl = V32{} + '\n';
    for (; end-s >= 32; s += 32) {
        V32 x;
        load32(x, s);
        u32 m = bits32((V32)(x == nl));
        if (m) {
            r->after = s + 32 - __builtin_clz(m);
            for (; m; m &= m-1) {
                r->count++;
            }
        }
    }
    return s;
}
#endif

static Lines countlines(i32 simd, u8 *s, u8 *end)
{
    Lines r = {};
    #if __x86_64__
    if (simd >= SIMD_AVX2) {
        s = countlines_avx2(&r, s, end);
    }
    #endif
    #if __SSE2__
    if (simd >= SIMD_SSE2) {
        s = countlines_sse2(&r, s, end);
    }
    #endif
    return countlines_scalar(r, s, end);
}


// The index of the first byte where a and b differ, or len. Used in
// comparators without a run's context, so only SSE2.

static iz firstdiff(u8 *a, u8 *b, iz len)
{
    iz i = 0;
    #if __SSE2__
    for (; len-i >= 16; i += 16) {
        u32 m = bits16((V16)(load16(a+i) != load16(b+i)));
        if (m) {
            return i + __builtin_ctz(m);
        }
    }
    #endif
    for (; i<len && a[i]==b[i]; i++) {}
    return i;
}


// Tables of numbers: 64 bytes as masks of the classes such a table holds,
// bit i for byte i, by vector on x86-64, where such masks are native.

struct Tableblock {
    u64 num;    // within a number: . 0-9 A-F H L U X a-f h l u x
    u64 digit;
    u64 blank;  // space, tab, LF, and CR, which must precede a LF
    u64 cr;
    u64 lf;
    u64 comma;
};

// Bit i holds the parity of bits 0 through i.
static u64 prefixxor(u64 x)
{
    x ^= x << 1;
    x ^= x << 2;
    x ^= x << 4;
    x ^= x << 8;
    x ^= x << 16;
    x ^= x << 32;
    return x;
}

#if __x86_64__
static Tableblock tableblock_sse2(u8 *p)
{
    Tableblock r = {};
    for (i32 i = 0; i < 4; i++) {
        V16 x      = load16(p + 16*i);
        V16 lower  = x | splat16(0x20);
        V16 digit  = (V16)((x > splat16('0'-1)) & (x < splat16('9'+1)));
        V16 hex    = (V16)((lower > splat16('a'-1)) & (lower < splat16('f'+1)));
        V16 suffix = (V16)((lower == splat16('h')) | (lower == splat16('l')) |
                           (lower == splat16('u')) | (lower == splat16('x')));
        V16 cr     = (V16)(x == splat16('\r'));
        V16 lf     = (V16)(x == splat16('\n'));
        V16 blank  = (V16)((x == splat16(' ')) | (x == splat16('\t'))) | cr | lf;
        V16 num    = (V16)(x == splat16('.')) | digit | hex | suffix;
        i32 at     = 16*i;
        r.num   |= (u64)bits16(num) << at;
        r.digit |= (u64)bits16(digit) << at;
        r.blank |= (u64)bits16(blank) << at;
        r.cr    |= (u64)bits16(cr) << at;
        r.lf    |= (u64)bits16(lf) << at;
        r.comma |= (u64)bits16((V16)(x == splat16(','))) << at;
    }
    return r;
}

AVX2 static Tableblock tableblock_avx2(u8 *p)
{
    Tableblock r = {};
    for (i32 i = 0; i < 2; i++) {
        V32 x;
        load32(x, p + 32*i);
        V32 lower  = x | (V32{} + (char)0x20);
        V32 digit  = (V32)((x > (V32{} + (char)('0'-1))) &
                           (x < (V32{} + (char)('9'+1))));
        V32 hex    = (V32)((lower > (V32{} + (char)('a'-1))) &
                           (lower < (V32{} + (char)('f'+1))));
        V32 suffix = (V32)((lower == (V32{} + 'h')) | (lower == (V32{} + 'l')) |
                           (lower == (V32{} + 'u')) | (lower == (V32{} + 'x')));
        V32 cr     = (V32)(x == (V32{} + '\r'));
        V32 lf     = (V32)(x == (V32{} + '\n'));
        V32 blank  = (V32)((x == (V32{} + ' ')) | (x == (V32{} + '\t'))) | cr | lf;
        V32 num    = (V32)(x == (V32{} + '.')) | digit | hex | suffix;
        i32 at     = 32*i;
        r.num   |= (u64)bits32(num) << at;
        r.digit |= (u64)bits32(digit) << at;
        r.blank |= (u64)bits32(blank) << at;
        r.cr    |= (u64)bits32(cr) << at;
        r.lf    |= (u64)bits32(lf) << at;
        r.comma |= (u64)bits32((V32)(x == (V32{} + ','))) << at;
    }
    return r;
}
#endif

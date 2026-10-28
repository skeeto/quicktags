// Scanning: each search at every level this processor has, against plain
// loops, over random bytes from small alphabets, at every length to 100
// and offsets across a block, so that matches fall on block edges, in
// the tails, and nowhere.

static u32 scanrand(u64 *rng)
{
    *rng = *rng*0x3243f6a8885a308d + 1;
    return (u32)(*rng >> 32);
}

// Describe a failing input: the search, its level, offset, and bytes.
static Str scanfail(Arena *a, Str search, i32 simd, u8 *s, iz len)
{
    Buf b(a, 256);
    print(&b, search);
    print(&b, " level ");
    print(&b, (i64)simd);
    print(&b, " len ");
    print(&b, (i64)len);
    print(&b, ":");
    for (iz i = 0; i < len; i++) {
        putbyte(&b, ' ');
        printhex(&b, s[i], 2);
    }
    return finish(&b);
}

static void test_scan(Test *t)
{
    Arena a    = t->perm;
    u64   rng  = 1;
    i32   best = simdlevel();
    u8    buf[256];

    // The alphabets favor the bytes searched for, which then cluster
    Str alphabets[] = {
        "ab", "a*/b", "\n", "x\nyz", "*/", "\"\\\n\rq", "\x80\xff\n*",
    };

    for (i32 simd = SIMD_NONE; simd <= best; simd++) {
        Str  fail  = {};
        iz   tries = 0;
        for (i32 trial = 0; trial<20000 && !fail.data; trial++) {
            Str alpha = alphabets[scanrand(&rng) % countof(alphabets)];
            for (iz i = 0; i < countof(buf); i++) {
                buf[i] = scanrand(&rng)%8 ? alpha[scanrand(&rng)%alpha.len]
                                         : (u8)scanrand(&rng);
            }
            iz  off = scanrand(&rng) % 40;
            iz  len = scanrand(&rng) % 101;
            u8 *s   = buf + off;
            u8 *end = s + len;
            u8  x   = alpha[0];
            u8  y   = alpha[alpha.len-1];
            tries++;

            u8 *want = s;
            for (; want<end && *want!=x; want++) {}
            if (findbyte(simd, s, end, x) != want) {
                fail = scanfail(&a, "findbyte", simd, s, len);
                break;
            }

            want = s;
            for (; want<end && *want!=x && *want!=y; want++) {}
            if (findbyte2(simd, s, end, x, y) != want) {
                fail = scanfail(&a, "findbyte2", simd, s, len);
                break;
            }

            want = s;
            for (; want<end && *want!='"' && *want!='\\' &&
                   *want!='\n' && *want!='\r'; want++) {}
            if (findbyte4(simd, s, end, '"', '\\', '\n', '\r') != want) {
                fail = scanfail(&a, "findbyte4", simd, s, len);
                break;
            }

            want = end;
            for (iz i = 0; i+1 < len; i++) {
                if (s[i]==x && s[i+1]==y) {
                    want = s + i;
                    break;
                }
            }
            if (findpair(simd, s, end, x, y) != want) {
                fail = scanfail(&a, "findpair", simd, s, len);
                break;
            }

            iz  count = 0;
            u8 *after = 0;
            for (iz i = 0; i < len; i++) {
                if (s[i] == '\n') {
                    count++;
                    after = s + i + 1;
                }
            }
            Lines l = countlines(simd, s, end);
            if (l.count!=count || l.after!=after) {
                fail = scanfail(&a, "countlines", simd, s, len);
                break;
            }
        }
        Buf name(&a, 32);
        print(&name, "scan level ");
        print(&name, (i64)simd);
        expect(t, finish(&name), fail.data ? fail : Str("ok"), "ok");
        expect(t, "scan tries", num(&a, tries), "20000");
    }

    // Differences at each place, past the vector width too
    Str fail = {};
    for (iz len = 0; len<100 && !fail.data; len++) {
        for (iz at = 0; at <= len; at++) {
            u8 x[100];
            u8 y[100];
            for (iz i = 0; i < len; i++) {
                x[i] = y[i] = (u8)scanrand(&rng);
            }
            if (at < len) {
                y[at] ^= (u8)(1 + scanrand(&rng)%255);
            }
            if (firstdiff(x, y, len) != at) {
                fail = scanfail(&a, "firstdiff", SIMD_SSE2, x, len);
                break;
            }
        }
    }
    expect(t, "firstdiff", fail.data ? fail : Str("ok"), "ok");

    // Data tables, the vector forms against the scalar: numbers, blanks,
    // and commas, over several blocks, with the bytes that end a run
    // sometimes put in, ending anywhere
    Str numbers[] = {
        "0x1f", "123", "1.5e3", "077uLL", "0xFFFFFFFFu", "0", "9.", ".5",
        "1'000", "12ab", "0xg", "1h", "x1",
    };
    Str blanks[]  = {"", " ", "\t", "\n", "\r\n", "  ", "\n\t "};
    Str breaks[]  = {"g", "_", "'", "\r", ",", " ", ".", "x", "", "(",
                     "\x80", ",,", "\r\r\n", "1 2"};
    iz  tries = 0;
    for (i32 trial = 0; trial<20000 && !fail.data; trial++) {
        Buf b(&a, 1024);
        i32 n = (i32)(scanrand(&rng) % 60);
        for (i32 i = 0; i < n; i++) {
            print(&b, blanks[scanrand(&rng) % countof(blanks)]);
            u32 r = scanrand(&rng);
            print(&b, r%8 ? numbers[r%7] : numbers[r%countof(numbers)]);
            u32 q = scanrand(&rng);
            print(&b, q%64 ? blanks[q%countof(blanks)] : Str("\r"));  // lone CR
            print(&b, scanrand(&rng)%64 ? Str(",") : breaks[r%countof(breaks)]);
        }
        Str table = finish(&b);
        iz  len   = table.len ? scanrand(&rng)%(u32)(table.len+1) : 0;
        u8 *s     = table.data;
        u8 *want  = numbersrun(SIMD_NONE, s, s+len);
        for (i32 simd = SIMD_SSE2; simd <= best; simd++) {
            if (numbersrun(simd, s, s+len) != want) {
                fail = scanfail(&a, "numbersrun", simd, s, len);
            }
        }
        tries += want != 0;
    }
    expect(t, "numbersrun", fail.data ? fail : Str("ok"), "ok");
    expect(t, "numbersrun runs", num(&a, tries > 10000), "1");
}

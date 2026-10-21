// Patterns: names matched as Universal Ctags matches them, with gnulib's
// fnmatch() and no flags (DISC-8)
// This is free and unencumbered software released into the public domain.

enum : i32 { CLASSMAX = 256 };  // longest class name, as gnulib is built

static u8 uppercase(u8 c)
{
    return c>='a' && c<='z' ? (u8)(c - 32) : c;
}

// A name's byte as patterns see it. On Windows, Universal Ctags turns
// '\' into '/' and uppercases both the name and the pattern first.
static u8 namebyte(u8 c, b32 windows)
{
    return !windows ? c : c=='\\' ? (u8)'/' : uppercase(c);
}

// A pattern's byte, uppercase where names fold, and 0 past its end,
// where gnulib finds the terminator.
static u8 patbyte(Str pat, iz i, b32 fold)
{
    u8 c = i<pat.len ? pat[i] : 0;
    return fold ? uppercase(c) : c;
}

// Whether a byte is in a class of the C locale, which holds no bytes
// above 0x7f: -1 when there is no class of that name.
static i32 classmatch(Str name, u8 c)
{
    b32 lo = c>='a' && c<='z';
    b32 up = c>='A' && c<='Z';
    b32 dg = c>='0' && c<='9';
    b32 gr = c>' ' && c<0x7f;
    struct { Str name; b32 in; } classes[] = {
        {"alnum",  lo || up || dg},
        {"alpha",  lo || up},
        {"blank",  c==' ' || c=='\t'},
        {"cntrl",  c<' ' || c==0x7f},
        {"digit",  dg},
        {"graph",  gr},
        {"lower",  lo},
        {"print",  gr || c==' '},
        {"punct",  gr && !lo && !up && !dg},
        {"space",  c==' ' || (c>='\t' && c<='\r')},
        {"upper",  up},
        {"xdigit", dg || ((c|0x20)>='a' && (c|0x20)<='f')},
    };
    for (iz i = 0; i < countof(classes); i++) {
        if (name == classes[i].name) {
            return classes[i].in;
        }
    }
    return -1;
}

// Skip the members of a bracket expression after the one that matched,
// at pat[i], through its ']': the index past it, or -1 if the pattern
// fails there. Only here are "[=X=]" and "[.X.]" told apart.
static iz skipmembers(Str pat, iz i, b32 fold)
{
    for (;;) {
        u8 c    = patbyte(pat, i++, fold);
        u8 next = patbyte(pat, i, fold);
        if (c == ']') {
            return i;
        } else if (!c) {
            return -1;
        } else if (c == '\\') {
            if (!next) return -1;
            i++;
        } else if (c=='[' && next==':') {
            for (iz j = i, n = 1;; n++) {
                u8 d = patbyte(pat, ++j, fold);
                if (n == CLASSMAX) {
                    return -1;
                } else if (d==':' && patbyte(pat, j+1, fold)==']') {
                    i = j + 2;
                    break;
                } else if (d<'a' || d>='z') {
                    break;  // the ':' is a member
                }
            }
        } else if (c=='[' && next=='=') {
            if (!patbyte(pat, i+1, fold) || patbyte(pat, i+2, fold)!='=' ||
                patbyte(pat, i+3, fold)!=']') {
                return -1;
            }
            i += 4;
        } else if (c=='[' && next=='.') {
            iz j = i + 1;
            for (; !(patbyte(pat, j, fold)=='.' && patbyte(pat, j+1, fold)==']'); j++) {
                if (!patbyte(pat, j, fold)) return -1;
            }
            i = j + 2;
        }
    }
}

// Match the bracket expression whose '[' ends just before pat[*p]
// against a byte, advancing *p past it. When it never closes and the
// byte is not among its members, the '[' was an ordinary character.
static b32 globbracket(Str pat, iz *p, u8 c, b32 fold)
{
    iz  beg = *p;
    iz  i   = beg;
    u8  ch  = patbyte(pat, i, fold);
    b32 neg = ch=='!' || ch=='^';
    i  += neg;
    ch  = patbyte(pat, i++, fold);

    b32 hit = 0;
    for (;;) {
        b32 member = 1;  // ch is a character, perhaps a range's start
        if (ch == '\\') {
            if (!patbyte(pat, i, fold)) return 0;
            ch = patbyte(pat, i++, fold);
        } else if (ch=='[' && patbyte(pat, i, fold)==':') {
            // A class, if lowercase letters (but z) and ":]" follow
            iz end = i + 1;
            for (;; end++) {
                u8 d = patbyte(pat, end, fold);
                if (end-i-1 == CLASSMAX) {
                    return 0;
                } else if (d==':' && patbyte(pat, end+1, fold)==']') {
                    break;
                } else if (d<'a' || d>='z') {
                    end = -1;  // a '[' member, then ':'
                    break;
                }
            }
            if (end >= 0) {
                i32 in = classmatch(span(pat.data+i+1, pat.data+end), c);
                if (in < 0) return 0;
                i = end + 2;
                if (in) {
                    hit = 1;
                    break;
                }
                ch     = patbyte(pat, i++, fold);
                member = 0;
            }
        } else if (!ch) {
            *p = beg;
            return c == '[';
        }

        if (member) {
            u8  next  = patbyte(pat, i, fold);
            u8  after = patbyte(pat, i+1, fold);
            b32 range = next=='-' && after && after!=']';
            if (!range && ch==c) {
                hit = 1;
                break;
            }
            u8 lo = ch;
            ch = patbyte(pat, i++, fold);
            if (ch=='-' && patbyte(pat, i, fold)!=']') {
                u8 hi = patbyte(pat, i++, fold);
                if (hi == '\\') {
                    hi = patbyte(pat, i++, fold);
                }
                if (!hi) return 0;
                if (lo<=c && c<=hi) {
                    hit = 1;
                    break;
                }
                ch = patbyte(pat, i++, fold);
            }
        }
        if (ch == ']') {
            break;
        }
    }

    if (hit) {
        i = skipmembers(pat, i, fold);
        if (i < 0) return 0;
    }
    *p = i;
    return hit != neg;
}

// Match the element at pat[*p], which is not '*', against a byte of a
// name, advancing *p past it.
static b32 globelem(Str pat, iz *p, u8 c, b32 fold)
{
    u8 e = patbyte(pat, (*p)++, fold);
    switch (e) {
    case '?':
        return 1;
    case '[':
        return globbracket(pat, p, c, fold);
    case '\\':
        if (*p == pat.len) {
            return 0;  // a trailing '\' matches nothing
        }
        e = patbyte(pat, (*p)++, fold);
    }
    return e == c;
}

// Match a whole name against a pattern as fnmatch() without flags does,
// so '*' and '?' also match '/' and a leading '.'. Every element but
// '*' matches one byte, so a mismatch need only retry the latest '*'.
static b32 globmatch(Str pat, Str s, b32 windows)
{
    iz p    = 0;
    iz i    = 0;
    iz star = -1;  // just past the latest '*'
    iz mark = 0;   // where the name resumes after it
    while (i < s.len) {
        iz next = p;
        if (p<pat.len && pat[p]=='*') {
            star = ++p;
            mark = i;
        } else if (p<pat.len &&
                   globelem(pat, &next, namebyte(s[i], windows), windows)) {
            p = next;
            i++;
        } else if (star >= 0) {
            p = star;
            i = ++mark;
        } else {
            return 0;
        }
    }
    for (; p<pat.len && pat[p]=='*'; p++) {}
    return p == pat.len;
}


// Lists of patterns

enum : i32 { GLOB_EXACT, GLOB_STAR, GLOB_INFIX, GLOB_ANY };

// A pattern, classified so that most need no general matching: a name
// alone, with one '*' (a suffix like "*.o"), or between two that begin
// and end it (a directory like "*/build/*").
struct Glob {
    Str pat;
    iz  star;  // GLOB_STAR: where its '*' is
    i32 kind;  // GLOB_*
};

// Classify a pattern, which ends at a NUL as a C string would.
static Glob newglob(Str pat)
{
    Glob r = {};
    r.star = -1;
    iz  stars   = 0;
    b32 special = 0;
    iz  len     = 0;
    for (; len<pat.len && pat[len]; len++) {
        u8 c = pat[len];
        if (c == '*') {
            r.star = len;
            stars++;
        }
        special |= c=='?' || c=='[' || c=='\\';
    }
    r.pat = takehead(pat, len);
    if (special || stars>2) {
        r.kind = GLOB_ANY;
    } else if (stars == 2) {
        b32 infix = len>2 && r.pat[0]=='*' && r.pat[len-1]=='*';
        r.kind = infix ? GLOB_INFIX : GLOB_ANY;
    } else {
        r.kind = stars ? GLOB_STAR : GLOB_EXACT;
    }
    return r;
}

// Order patterns by the cost of matching them, cheapest first.
static b32 globless(Glob *a, Glob *b)
{
    return a->kind < b->kind;
}

// Compare a name to a fixed part of a pattern of the same length.
static b32 samename(Str pat, Str s, b32 windows)
{
    if (!windows) {
        return pat == s;
    }
    for (iz i = 0; i < s.len; i++) {
        if (uppercase(pat[i]) != namebyte(s[i], 1)) {
            return 0;
        }
    }
    return 1;
}

// Whether a name holds a fixed part of a pattern anywhere.
static b32 holdsname(Str s, Str pat, b32 windows)
{
    u8 first = windows ? uppercase(pat[0]) : pat[0];
    for (iz i = 0; i+pat.len <= s.len; i++) {
        if (namebyte(s[i], windows)==first &&
            samename(pat, span(s.data+i, s.data+i+pat.len), windows)) {
            return 1;
        }
    }
    return 0;
}

static b32 globsmatch(Slice<Glob> globs, Str s, b32 windows)
{
    for (iz i = 0; i < globs.len; i++) {
        Glob *g = &globs[i];
        switch (g->kind) {
        case GLOB_EXACT:
            if (g->pat.len==s.len && samename(g->pat, s, windows)) {
                return 1;
            }
            break;
        case GLOB_STAR: {
            Str head = takehead(g->pat, g->star);
            Str tail = cuthead(g->pat, g->star+1);
            if (s.len>=head.len+tail.len &&
                samename(tail, taketail(s, tail.len), windows) &&
                samename(head, takehead(s, head.len), windows)) {
                return 1;
            }
            break;
        }
        case GLOB_INFIX:
            if (holdsname(s, cuttail(cuthead(g->pat, 1), 1), windows)) {
                return 1;
            }
            break;
        case GLOB_ANY:
            if (globmatch(g->pat, s, windows)) {
                return 1;
            }
        }
    }
    return 0;
}

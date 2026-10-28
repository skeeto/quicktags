// C and C++ preprocessing: Universal Ctags' character filter
//
// cppget() returns characters as the parser should see them: comments
// as a space, string and character literals as single symbols, line
// splices removed, trigraphs and digraphs mapped, directives consumed
// (tagging macros), and ignored conditional branches skipped. Which
// branches are ignored depends on the parser's progress through
// statements (resolve) and blocks (depth), exactly as in Universal Ctags.
//
// Departures from it (docs/spec.md): "/+" is not a comment (X1), strings
// end at a newline (X2), a quote is a digit separator only within a
// number (X3), NUL is data (X4), bytes 0x80 and above are identifier
// characters (X5), #elifdef and #elifndef are recognized (X7),
// conditionals nest deeper and stay in step (X8), and an untranslated
// "??" keeps its order (X9).
//
// This is free and unencumbered software released into the public domain.

enum : i32 {
    C_EOF    = -1,
    C_STRING = 0x100,  // a string literal, contents in Cpp.lit
    C_CHAR   = 0x101,  // a character literal
};

static b32 cppspace(i32 c)
{
    return c==' ' || (c>='\t' && c<='\r');
}

static constexpr b32 cppident(i32 c)
{
    return (c>='0' && c<='9') || (c>='a' && c<='z') || (c>='A' && c<='Z') ||
           c=='_' || c=='$' || (c>=0x80 && c<=0xff);
}

static b32 cppident1(i32 c)
{
    return (c>='a' && c<='z') || (c>='A' && c<='Z') ||
           c=='_' || c=='~' || c=='$' || (c>=0x80 && c<=0xff);
}

static b32 cppalpha(i32 c)
{
    return (c>='a' && c<='z') || (c>='A' && c<='Z');
}

static b32 cppxdigit(i32 c)
{
    return (c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F');
}

// Bytes of a file as Universal Ctags reads them: a CR before a LF is
// dropped, and the line number advances when the byte after a LF is
// read, so a LF belongs to the line it ends.
struct Reader {
    u8 *beg;
    u8 *end;
    u8 *p;
    i32 line;     // of the last byte read
    iz  linepos;  // offset of that line's first byte
    b32 newline;  // the next byte starts a line
    i32 simd;     // scanning level, SIMD_*
};

static Reader newreader(Str text, i32 simd)
{
    Reader r = {};
    r.beg     = text.data;
    r.end     = text.data + text.len;
    r.p       = r.beg;
    r.newline = 1;
    r.simd    = simd;
    return r;
}

// Begin a line if the last byte read ended one, as reading the next does.
static void startline(Reader *r)
{
    if (r->newline) {
        r->line++;
        r->linepos = r->p - r->beg;
        r->newline = 0;
    }
}

static i32 readbyte(Reader *r)
{
    if (r->p == r->end) {
        return C_EOF;
    }
    startline(r);
    u8 c = *r->p++;
    if (c=='\r' && r->p<r->end && *r->p=='\n') {
        c = *r->p++;
    }
    r->newline = c == '\n';
    return c;
}

// Read up to (not including) to, as readbyte would, for the fast paths.
// The last byte read must not be a CR before a LF.
static void advance(Reader *r, u8 *to)
{
    if (r->p == to) {
        return;
    }
    startline(r);
    Lines l = countlines(r->simd, r->p, to-1);  // a last LF is pending
    r->line   += (i32)l.count;
    r->linepos = l.after ? l.after-r->beg : r->linepos;
    r->newline = to[-1] == '\n';
    r->p       = to;
}

enum : i32 {
    D_NONE,     // ignore to the end of the line
    D_DEFINE,   // after "#define"
    D_HASH,     // after '#': read the directive's name
    D_IF,       // after "#if", "#ifdef", or "#ifndef"
    D_ELIF,
    D_PRAGMA,
    D_UNDEF,
    D_INCLUDE,
};

enum : i32 { IF_IF, IF_IFDEF, IF_IFNDEF };

// One level of conditional, from #if to #endif.
struct Cond {
    b32 ignoreall;  // within an ignored branch of the parent
    b32 single;     // follow only one branch
    b32 chosen;     // a branch was followed
    b32 ignoring;   // the current branch is ignored
    i32 depth;      // the parser's block depth at the #if
};

enum : i32 {
    CONDMAX = 64,
    LITMAX  = 1024,  // string contents kept
    CHARMAX = 10,    // character contents kept
};

// A replacement pushed back (ungetat): the characters above base, which
// report the position of the identifier replaced. Once they are read,
// those below report the position they did before, and have the size
// they did, kept here.
struct Mark {
    iz  base;
    i32 line;
    iz  pos;
    iz  size;
};

struct Cpp {
    Reader in;
    File  *f;
    Arena *a;
    u8    *back;      // characters pushed back, the last read next
    iz     nback;
    iz     capback;
    b32    backlive;  // pushed-back characters may remain
    i32    backline;  // the position reported while they do
    iz     backpos;
    iz     backsize;  // as Universal Ctags would allocate them (cppgrow)
    Mark  *marks;     // replacements in the pushback, innermost last
    iz     nmarks;
    iz     capmarks;
    i32    drains;    // times the pushback ran dry
    Word   lit;       // contents of the last literal
    Word   name;      // the identifier a directive is reading
    b32    resolve;   // a statement is in progress
    b32    brace;     // the second pass's rules
    i32    depth;     // the parser's block depth
    i32    state;
    i32    ifsub;
    b32    accept;    // a directive may start here
    i32    level;
    i32    over;      // levels past CONDMAX
    b32    innum;     // within a number
    b32    prevident; // the last character was part of an identifier
    b32    fast;      // read ahead without pushing back, and in bulk
    Cond   cond[CONDMAX];
};

static void cppinit(Cpp *p, File *f, Arena *a, b32 brace)
{
    *p = {};
    p->in     = newreader(f->src->text, f->simd);
    p->f      = f;
    p->a      = a;
    p->brace  = brace;
    p->accept = 1;
    p->fast   = !f->nofast;
}

// The position of the current character, or of the earliest pushed-back
// one if any may remain, as tokens and some tags take it.
static i32 cppline(Cpp *p)
{
    return p->backlive ? p->backline : p->in.line;
}

static iz cpppos(Cpp *p)
{
    return p->backlive ? p->backpos : p->in.linepos;
}

// Nothing is pushed back any more: read from the file again. That is
// counted unless the file is at its end, where replacements bringing
// their own macros back would otherwise never stop (X12).
static void cppdrained(Cpp *p)
{
    p->backlive = 0;
    p->backsize = 0;
    p->nmarks   = 0;
    p->drains  += p->in.p < p->in.end;
}

static i32 rawget(Cpp *p)
{
    if (p->backlive) {
        for (;;) {
            iz base = p->nmarks ? p->marks[p->nmarks-1].base : 0;
            if (p->nback > base) {
                return p->back[--p->nback];
            } else if (!p->nmarks) {
                break;
            }
            // A replacement was read: what is below it reports its own
            // position again
            Mark *m = p->marks + --p->nmarks;
            p->backline = m->line;
            p->backpos  = m->pos;
            p->backsize = m->size;
        }
        cppdrained(p);
    }
    return readbyte(&p->in);
}

// Whether the fast paths may read the file directly: they are on, and
// nothing is pushed back. Instead of reading a character to look at it,
// then pushing it back, they peek at the file's bytes. That leaves the
// pushback's bookkeeping (drains, backsize) behind where it would have
// been, but those only bound -I replacements between reads of the file,
// and reading continues just as it would, at the same positions.
static b32 direct(Cpp *p)
{
    return p->fast && !p->backlive;
}

// The byte n past the last one read from the file, or C_EOF.
static i32 peekbyte(Cpp *p, iz n)
{
    return n<p->in.end-p->in.p ? (i32)p->in.p[n] : (i32)C_EOF;
}

// Characters waiting in the innermost replacement, or in all of the
// pushback outside replacements.
static iz cppbacklen(Cpp *p)
{
    return p->nback - (p->nmarks ? p->marks[p->nmarks-1].base : 0);
}

// Universal Ctags pushes characters back into a stack of buffers, one
// for each replacement being read and one below them, and stops making
// replacements while the buffer being read is large (LEX-14). Track its
// size as it would grow for n characters pushed back as a string, or
// one at a time.
static void cppgrow(Cpp *p, iz n, b32 string)
{
    iz have = cppbacklen(p);
    iz size = p->backsize;
    if (string) {
        size = size<have+n ? 8+n+have : size;
    } else {
        size = !size ? 8 : have==size ? size+8 : size;
    }
    p->backsize = size;
}

static void pushback(Cpp *p, u8 c)
{
    if (p->nback == p->capback) {
        iz  cap  = p->capback ? p->capback*2 : 64;
        u8 *back = allocbytes(p->a, cap);
        copybytes(back, p->back, p->nback);
        p->back    = back;
        p->capback = cap;
    }
    p->back[p->nback++] = c;
}

// Push back a string, as one, its first character to be read first.
static void pushstr(Cpp *p, Str s)
{
    cppgrow(p, s.len, 1);
    for (iz i = s.len-1; i >= 0; i--) {
        pushback(p, s[i]);
    }
}

static void unget(Cpp *p, i32 c)
{
    if (!p->backlive) {
        p->backlive = 1;
        p->backline = p->in.line;
        p->backpos  = p->in.linepos;
    }
    if (c==C_STRING || c==C_CHAR) {
        u8 q = c==C_STRING ? '"' : '\'';
        cppgrow(p, 1, 0);
        pushback(p, q);
        pushstr(p, {p->lit.data, p->lit.len});
        cppgrow(p, 1, 0);
        pushback(p, q);
        p->lit.len = 0;
    } else if (c != C_EOF) {
        cppgrow(p, 1, 0);
        pushback(p, (u8)c);
    }
}

static void ungetstr(Cpp *p, Str s)
{
    if (!p->backlive) {
        p->backlive = 1;
        p->backline = p->in.line;
        p->backpos  = p->in.linepos;
    }
    pushstr(p, s);
}

// Push back the replacement of an identifier at line and pos, which its
// characters report until they are read.
static void ungetat(Cpp *p, Str s, i32 line, iz pos)
{
    if (p->nmarks == p->capmarks) {
        iz    cap   = p->capmarks ? p->capmarks*2 : 16;
        Mark *marks = alloc<Mark>(p->a, cap);
        for (iz i = 0; i < p->nmarks; i++) {
            marks[i] = p->marks[i];
        }
        p->marks    = marks;
        p->capmarks = cap;
    }
    Mark *m = p->marks + p->nmarks++;
    m->base = p->nback;
    m->line = p->backline;
    m->pos  = p->backpos;
    m->size = p->backsize;
    p->backlive = 1;
    p->backline = line;
    p->backpos  = pos;
    p->backsize = 0;
    pushstr(p, s);
}

// The nth byte before the last one read from the file, or 0 if it is
// before the line's start.
static i32 prevbyte(Cpp *p, i32 n)
{
    iz at = p->in.p - p->in.beg - 1 - n;
    return at>=p->in.linepos ? p->in.beg[at] : 0;
}

static b32 litput(Cpp *p, i32 c, iz max)
{
    if (p->lit.len >= max) {
        return 0;
    }
    if (p->lit.len == p->lit.cap) {
        iz  cap  = p->lit.cap ? p->lit.cap*2 : 64;
        u8 *data = allocbytes(p->a, cap);
        copybytes(data, p->lit.data, p->lit.len);
        p->lit.data = data;
        p->lit.cap  = cap;
    }
    p->lit.data[p->lit.len++] = (u8)c;
    return 1;
}

// Append bytes that are no escape, as litput would one at a time.
static void litcat(Cpp *p, u8 *s, iz len, iz max)
{
    iz room = max - p->lit.len;
    len = len<room ? len : room;
    if (len <= 0) {
        return;
    }
    if (p->lit.cap-p->lit.len < len) {
        iz cap = p->lit.cap ? p->lit.cap : 64;
        for (; cap-p->lit.len < len; cap *= 2) {}
        u8 *data = allocbytes(p->a, cap);
        copybytes(data, p->lit.data, p->lit.len);
        p->lit.data = data;
        p->lit.cap  = cap;
    }
    copybytes(p->lit.data+p->lit.len, s, len);
    p->lit.len += len;
}


// Literals and comments

static i32 skipstring(Cpp *p)
{
    p->lit.len = 0;
    for (;;) {
        if (direct(p)) {
            // A run of plain bytes at once, up to what ends or escapes
            u8 *s   = p->in.p;
            u8 *run = findbyte4(p->in.simd, s, p->in.end, '"', '\\', '\n', '\r');
            if (run > s) {
                startline(&p->in);
                litcat(p, s, run-s, LITMAX);
                p->in.p = run;
            }
        }
        i32 c = rawget(p);
        if (c == C_EOF) {
            break;
        } else if (c == '\\') {
            i32 c0 = rawget(p);
            if (c0 == '\n') {
                continue;
            } else if (c0 == C_EOF) {
                break;
            }
            if (litput(p, c, LITMAX)) {
                if (litput(p, c0, LITMAX)) {
                    continue;
                }
                p->lit.len--;
            }
        } else if (c == '"') {
            break;
        } else if (c == '\n') {
            unget(p, c);  // unterminated
            break;
        } else {
            litput(p, c, LITMAX);
        }
    }
    return C_STRING;
}

static b32 rawdelim(i32 c)
{
    return c!=' ' && c!='\f' && c!='\n' && c!='\r' && c!='\t' && c!='\v' &&
           c!='(' && c!=')' && c!='\\';
}

// After R", a raw string R"delim(...)delim". Its contents are not kept,
// so the token text shows the last string's contents, as it does in
// Universal Ctags.
static i32 skiprawstring(Cpp *p)
{
    i32 c = rawget(p);
    if (c!='(' && !rawdelim(c)) {
        unget(p, c);
        return skipstring(p);
    }

    i32 delim[16];
    i32 len     = 0;
    b32 collect = 1;
    do {
        if (collect) {
            if (rawdelim(c) && len<countof(delim)) {
                delim[len++] = c;
            } else {
                collect = 0;
            }
        } else if (c == ')') {
            i32 i = 0;
            while ((c = rawget(p))!=C_EOF && i<len && delim[i]==c) {
                i++;
            }
            if (i==len && c=='"') {
                break;
            }
            unget(p, c);
        }
    } while ((c = rawget(p)) != C_EOF);
    return C_STRING;
}

static i32 skipchar(Cpp *p)
{
    p->lit.len = 0;
    for (i32 c; (c = rawget(p)) != C_EOF;) {
        if (c == '\\') {
            i32 c0 = rawget(p);
            if (c0 == '\n') {
                continue;
            } else if (c0 == C_EOF) {
                break;
            }
            if (litput(p, c, CHARMAX)) {
                if (litput(p, c0, CHARMAX)) {
                    continue;
                }
                p->lit.len--;
            }
        } else if (c == '\'') {
            break;
        } else if (c == '\n') {
            unget(p, c);
            break;
        } else {
            litput(p, c, CHARMAX);
        }
    }
    return C_CHAR;
}

enum : i32 { COMMENT_NONE, COMMENT_C, COMMENT_CXX };

static i32 iscomment(Cpp *p)
{
    if (direct(p)) {
        i32 next = peekbyte(p, 0);
        if (next!='*' && next!='/') {
            return COMMENT_NONE;
        }
        p->in.p++;
        return next=='*' ? COMMENT_C : COMMENT_CXX;
    }
    i32 next = rawget(p);
    if (next == '*') {
        return COMMENT_C;
    } else if (next == '/') {
        return COMMENT_CXX;
    }
    unget(p, next);
    return COMMENT_NONE;
}

static i32 skipccomment(Cpp *p)  // ' ' for the comment, or EOF
{
    if (direct(p)) {
        u8 *end = p->in.end;
        u8 *at  = findpair(p->in.simd, p->in.p, end, '*', '/');
        advance(&p->in, at<end ? at+2 : end);
        return at<end ? (i32)' ' : (i32)C_EOF;
    }
    i32 c = rawget(p);
    while (c != C_EOF) {
        if (c != '*') {
            c = rawget(p);
        } else {
            i32 next = rawget(p);
            if (next != '/') {
                c = next;
            } else {
                c = ' ';
                break;
            }
        }
    }
    return c;
}

// Skip to the newline that ends a // comment, and return it, also
// pushed back to be read again, or EOF. A backslash escapes the next
// character, so a newline ends the comment after an even number of them.
// Read directly, the newline is instead left to be read.
static i32 skipcxxcomment(Cpp *p)
{
    if (direct(p)) {
        u8 *beg = p->in.p;
        u8 *end = p->in.end;
        for (u8 *s = beg;;) {
            u8 *lf = findbyte(p->in.simd, s, end, '\n');
            if (lf == end) {
                advance(&p->in, end);
                return C_EOF;
            }
            u8 *at = lf[-1]=='\r' ? lf-1 : lf;  // CRLF is one character
            iz  n  = 0;
            for (; at-n>beg && at[-n-1]=='\\'; n++) {}
            if (!(n & 1)) {
                advance(&p->in, at);
                return '\n';
            }
            s = lf + 1;
        }
    }
    i32 c;
    while ((c = rawget(p)) != C_EOF) {
        if (c == '\\') {
            rawget(p);
        } else if (c == '\n') {
            unget(p, c);
            break;
        }
    }
    return c;
}


// Directives

static b32 cppignoring(Cpp *p)
{
    return p->cond[p->level].ignoring;
}

static void readname(Cpp *p, i32 c, Word *w)
{
    w->len = 0;
    do {
        if (w->len == w->cap) {
            iz  cap  = w->cap ? w->cap*2 : 32;
            u8 *data = allocbytes(p->a, cap);
            copybytes(data, w->data, w->len);
            w->data = data;
            w->cap  = cap;
        }
        w->data[w->len++] = (u8)c;
        c = rawget(p);
    } while (c!=C_EOF && cppident(c));
    unget(p, c);
}

// Append a byte to a word, growing it in the arena.
static void cppput(Cpp *p, Word *w, i32 c)
{
    if (w->len == w->cap) {
        iz  cap  = w->cap ? w->cap*2 : 16;
        u8 *data = allocbytes(p->a, cap);
        copybytes(data, w->data, w->len);
        w->data = data;
        w->cap  = cap;
    }
    w->data[w->len++] = (u8)c;
}

// Tag a macro, unless file-scoped kinds are... (always on here)
static void cppmacro(Cpp *p, Str name, i32 line, iz linepos, Str sig)
{
    File *f = p->f;
    if (!kindon(f->kinds, f->src->lang, K_MACRO)) {
        return;
    }
    Tag t = {};
    t.name      = name;
    t.kind      = K_MACRO;
    t.line      = line;
    t.linepos   = linepos;
    t.filescope = !f->src->header;
    t.truncate  = 1;
    t.signature = sig;
    tagemit(f, &t);
}

static void cppparam(Cpp *p, Str name, Str macro)
{
    File *f = p->f;
    if (!kindon(f->kinds, f->src->lang, K_MACROPARAM)) {
        return;
    }
    Tag t = {};
    t.name      = name;
    t.kind      = K_MACROPARAM;
    t.line      = cppline(p);
    t.linepos   = cpppos(p);
    t.scopekind = "macro";
    t.scope     = macro;
    tagemit(f, &t);
}

// Tag a #define's macro and parameters. Not inlined into cppget, which
// runs for every character: there its registers and stack would cost
// every call, a few percent of the whole run.
[[gnu::noinline]] static void directivedefine(Cpp *p, i32 c)
{
    p->state = D_NONE;
    if (!cppident1(c)) {
        return;
    }
    readname(p, c, &p->name);
    if (cppignoring(p)) {
        return;
    }

    i32 line    = cppline(p);
    iz  linepos = cpppos(p);
    Str name    = clone(p->a, {p->name.data, p->name.len});
    i32 n       = rawget(p);
    if (n != '(') {
        // An object-like macro is where the file was read to
        unget(p, n);
        cppmacro(p, name, p->in.line, p->in.linepos, {});
        return;
    }

    // Parameters are read raw, without removing comments, into a list
    // of them in full, which is the signature (FLD-10)
    Word list  = {};
    iz   param = 1;  // where the parameter being read begins
    cppput(p, &list, '(');
    do {
        n = rawget(p);
        if ((n>='0' && n<='9') || cppalpha(n) || n=='_' || n=='$' || n=='.') {
            cppput(p, &list, n);
            continue;
        }
        if (list.len > param) {
            Str s = {list.data+param, list.len-param};
            if (s.len>3 && endswith(s, "...")) {
                cppparam(p, cuttail(s, 3), name);  // GNU args...
            } else if (s[0] != '.') {
                cppparam(p, s, name);
            }
            cppput(p, &list, ',');
            param = list.len;
        }
        if (n == '\\') {
            rawget(p);  // throw away the next character
        }
    } while (n!=')' && n!=C_EOF);

    Str sig = {};
    if (n==')' && wantfield(p->f, FIELD_SIGNATURE)) {
        if (list.len > 1) {
            list.len--;  // the comma after the last parameter
        }
        cppput(p, &list, ')');
        sig = {list.data, list.len};
    }
    cppmacro(p, name, line, linepos, sig);
}

static void directivepragma(Cpp *p, i32 c)
{
    p->state = D_NONE;
    if (!cppident1(c)) {
        return;
    }
    readname(p, c, &p->name);
    if (Str{p->name.data, p->name.len} == "weak") {
        do {
            c = rawget(p);
        } while (c == ' ');
        if (cppident1(c)) {
            readname(p, c, &p->name);
            Str name = clone(p->a, {p->name.data, p->name.len});
            cppmacro(p, name, p->in.line, p->in.linepos, {});
        }
    }
}

// Is the condition __ASSEMBLER__ or __ASSEMBLY__? Its first character
// is already read; the rest is pushed back to be read again.
static b32 assemblerblock(Cpp *p, i32 c)
{
    if (c != '_') {
        return 0;
    }
    Word w = {};
    readname(p, c, &w);
    Str  s = {w.data, w.len};
    b32  r = s=="__ASSEMBLER__" || s=="__ASSEMBLY__";
    for (iz i = s.len; i > 1; i--) {
        unget(p, s[i-1]);
    }
    return r;
}

static b32 pushcond(Cpp *p, b32 first)
{
    b32 ignoreall = cppignoring(p);
    if (p->level >= CONDMAX-1) {
        p->over++;
        return ignoreall;
    }
    Cond *c = &p->cond[++p->level];
    c->ignoreall = ignoreall;
    c->single    = p->resolve;
    c->chosen    = first;
    c->ignoring  = ignoreall || (!first && !p->brace);
    c->depth     = p->depth;
    return c->ignoring;
}

static b32 popcond(Cpp *p)
{
    if (p->over) {
        p->over--;
    } else if (p->level > 0) {
        p->level--;
    }
    return cppignoring(p);
}

// At #else or #elif: follow only one branch if a statement or block was
// left incomplete, and ignore this branch if one was followed.
static b32 ignorebranch(Cpp *p)
{
    Cond *c = &p->cond[p->level];
    if ((p->resolve || c->depth!=p->depth) && !p->brace) {
        c->single = 1;
    }
    return c->ignoreall || (c->chosen && c->single);
}

static b32 directiveif(Cpp *p, i32 c)
{
    // Only "0" as the first character makes a branch false
    b32 first = c != '0';
    if (first && assemblerblock(p, c) && p->ifsub!=IF_IFNDEF) {
        first = 0;
    }
    p->state = D_NONE;
    return pushcond(p, first);
}

static void directiveinclude(Cpp *p, i32 c)
{
    // Read past the file name: header tags are reference tags, not written
    if (c=='<' || c=='"') {
        i32 end = c=='<' ? '>' : '"';
        while ((c = rawget(p))!=C_EOF && c!=end && c!='\n') {}
    }
    p->state = D_NONE;
}

// After '#': at most nine letters name the directive.
static b32 directivehash(Cpp *p, i32 c)
{
    u8 buf[9];
    iz len = 0;
    for (; len < countof(buf); len++) {
        if (len) {
            c = rawget(p);
            if (c==C_EOF || !cppalpha(c)) {
                unget(p, c);
                break;
            }
        }
        buf[len] = (u8)c;
    }
    Str d = {buf, len};

    b32 ignore = 0;
    if (d == "define") {
        p->state = D_DEFINE;
    } else if (d == "include") {
        p->state = D_INCLUDE;
    } else if (d == "undef") {
        p->state = D_UNDEF;
    } else if (startswith(d, "if")) {
        p->state = D_IF;
        p->ifsub = d.len>2 && d[2]=='d' ? IF_IFDEF :
                   d.len>2 && d[2]=='n' ? IF_IFNDEF : IF_IF;
    } else if (d=="elif" || d=="else" || d=="elifdef" || d=="elifndef") {
        b32 iselse = d == "else";
        if (p->over) {
            ignore = cppignoring(p);
        } else {
            ignore = ignorebranch(p);
            p->cond[p->level].ignoring = ignore;
            if (!ignore && iselse && !p->brace) {
                Cond *cond = &p->cond[p->level];
                cond->chosen = cond->single || p->resolve;
            }
        }
        p->state = iselse ? D_NONE : D_ELIF;
    } else if (d == "endif") {
        ignore = popcond(p);
        p->state = D_NONE;
    } else if (d == "pragma") {
        p->state = D_PRAGMA;
    } else {
        p->state = D_NONE;
    }
    return ignore;
}

static b32 directive(Cpp *p, i32 c)
{
    b32 ignore = cppignoring(p);
    switch (p->state) {
    case D_NONE:    ignore = cppignoring(p);    break;
    case D_DEFINE:  directivedefine(p, c);      break;
    case D_HASH:    ignore = directivehash(p, c); break;
    case D_IF:      ignore = directiveif(p, c); break;
    case D_ELIF:    p->state = D_NONE;          break;
    case D_PRAGMA:  directivepragma(p, c);      break;
    case D_UNDEF:   p->state = D_NONE;          break;
    case D_INCLUDE: directiveinclude(p, c);     break;
    }
    return ignore;
}


// The filter

// The character a trigraph's third character stands for, or 0.
static i32 trigraph(i32 c)
{
    switch (c) {
    case '(':  return '[';
    case ')':  return ']';
    case '<':  return '{';
    case '>':  return '}';
    case '/':  return '\\';
    case '!':  return '|';
    case '\'': return '^';
    case '-':  return '~';
    case '=':  return '#';
    }
    return 0;
}

// Track numbers so that a quote is a digit separator only within one.
static void cppnumber(Cpp *p, i32 c)
{
    b32 ident = cppident(c);
    if (c>='0' && c<='9' && !p->prevident) {
        p->innum = 1;
    } else if (!p->innum || !(ident || c=='.')) {
        p->innum = 0;
    }
    p->prevident = ident;
}

// How the parser's next character is read when nothing is pushed back,
// by its first byte: most need no more than the reader's and the number
// tracking's updates, made inline by cppnext (BK_*).
enum : u8 {
    BK_SLOW,   // cppget reads it
    BK_PLAIN,  // in neither an identifier nor a number
    BK_IDENT,  // these four continue identifiers
    BK_HEX,    // a-f A-F
    BK_DIGIT,
    BK_R,      // unless a quote follows, which may begin a raw string
    BK_DOT,    // continues a number
    BK_BLANK,  // space and tab, which leave accept alone
    BK_LF,
    BK_CR,     // a LF if one follows, else plain
};

struct Bytekinds {
    u8 kind[256];
};

static constexpr Bytekinds bytekinds()
{
    Bytekinds r = {};
    for (i32 c = 0; c < 256; c++) {
        u8 k = BK_PLAIN;
        if (c>='0' && c<='9') {
            k = BK_DIGIT;
        } else if ((c>='a' && c<='f') || (c>='A' && c<='F')) {
            k = BK_HEX;
        } else if (c == 'R') {
            k = BK_R;
        } else if (cppident(c)) {
            k = BK_IDENT;
        }
        switch (c) {
        case ' ': case '\t':                    k = BK_BLANK; break;
        case '\n':                              k = BK_LF;    break;
        case '\r':                              k = BK_CR;    break;
        case '.':                               k = BK_DOT;   break;
        case '"': case '#': case '%': case '\'':
        case '/': case ':': case '<': case '?':
        case '\\':                              k = BK_SLOW;  break;
        }
        r.kind[c] = k;
    }
    return r;
}

static constexpr Bytekinds bytekind = bytekinds();

// The next character for the parser.
static i32 cppget(Cpp *p)
{
    b32 indirective = 0;
    b32 ignore      = 0;
    i32 c           = 0;
    do {
    again:
        c = rawget(p);
    process:
        if (c<0 || c>0xff || !cppident(c)) {
            if (c != '.') {
                p->innum = p->prevident = 0;
            }
        }
        switch (c) {
        case C_EOF:
            ignore      = 0;
            indirective = 0;
            break;

        case '\t':
        case ' ':
            break;

        case '\n':
            if (indirective && !ignore) {
                indirective = 0;
            }
            p->accept = 1;
            break;

        case '"':
            if (p->state == D_INCLUDE) {
                goto enter;
            }
            p->accept = 0;
            c = skipstring(p);
            break;

        case '#':
            if (p->accept) {
                indirective = 1;
                p->state    = D_HASH;
                p->accept   = 0;
            }
            break;

        case '\'':
            p->accept = 0;
            c = skipchar(p);
            break;

        case '/':
            switch (iscomment(p)) {
            case COMMENT_C:
                c = skipccomment(p);
                break;
            case COMMENT_CXX:
                c = skipcxxcomment(p);
                break;
            default:
                p->accept = 0;
            }
            break;

        case '\\': {
            if (direct(p)) {
                i32 n0 = peekbyte(p, 0);
                iz  n  = n0=='\n' ? 1 : n0=='\r' && peekbyte(p, 1)=='\n' ? 2 : 0;
                if (n) {
                    p->in.p      += n;
                    p->in.newline = 1;
                    goto again;
                }
                break;
            }
            i32 next = rawget(p);
            if (next == '\n') {
                goto again;
            }
            unget(p, next);
        } break;

        case '?': {
            if (direct(p)) {
                i32 t = peekbyte(p, 0)=='?' ? trigraph(peekbyte(p, 1)) : 0;
                if (t) {
                    p->in.p += 2;
                    c = t;
                    if (c=='\\' || c=='#') {
                        goto process;
                    }
                }
                break;
            }
            i32 next = rawget(p);
            if (next != '?') {
                unget(p, next);
                break;
            }
            next = rawget(p);
            switch (next) {
            case '(':  c = '[';  break;
            case ')':  c = ']';  break;
            case '<':  c = '{';  break;
            case '>':  c = '}';  break;
            case '/':  c = '\\'; goto process;
            case '!':  c = '|';  break;
            case '\'': c = '^';  break;
            case '-':  c = '~';  break;
            case '=':  c = '#';  goto process;
            default:
                unget(p, next);
                unget(p, '?');
            }
        } break;

        // Digraphs: <: :> <% %> %:
        case '<': {
            if (direct(p)) {
                // <:: not followed by : or > is a '<' and a "::"
                i32 n0 = peekbyte(p, 0);
                i32 n1 = peekbyte(p, 1);
                i32 n2 = peekbyte(p, 2);
                if (n0==':' && (n1!=':' || n2==':' || n2=='>')) {
                    p->in.p++;
                    c = '[';
                } else if (n0 == '%') {
                    p->in.p++;
                    c = '{';
                }
                goto enter;
            }
            i32 n0 = rawget(p);
            if (n0 == ':') {
                i32 n1 = rawget(p);
                if (n1 == ':') {
                    // <:: not followed by : or > is a '<' and a "::"
                    i32 n2 = rawget(p);
                    if (n2!=':' && n2!='>') {
                        unget(p, n2);
                        unget(p, n1);
                        unget(p, n0);
                    } else {
                        unget(p, n2);
                        unget(p, n1);
                        c = '[';
                    }
                } else {
                    unget(p, n1);
                    c = '[';
                }
            } else if (n0 == '%') {
                c = '{';
            } else {
                unget(p, n0);
            }
        } goto enter;

        case ':': {
            if (direct(p)) {
                if (peekbyte(p, 0) == '>') {
                    p->in.p++;
                    c = ']';
                }
                goto enter;
            }
            i32 next = rawget(p);
            if (next == '>') {
                c = ']';
            } else {
                unget(p, next);
            }
        } goto enter;

        case '%': {
            if (direct(p)) {
                i32 next = peekbyte(p, 0);
                if (next=='>' || next==':') {
                    p->in.p++;
                    c = next=='>' ? '}' : '#';
                    if (c == '#') {
                        goto process;
                    }
                }
                goto enter;
            }
            i32 next = rawget(p);
            if (next == '>') {
                c = '}';
            } else if (next == ':') {
                c = '#';
                goto process;
            } else {
                unget(p, next);
            }
        } goto enter;

        default:
            cppnumber(p, c);
            if (c == 'R') {
                // A raw string, unless R ends an identifier other than
                // the prefixes L, u, U, and u8
                i32 prev  = prevbyte(p, 1);
                i32 prev2 = prevbyte(p, 2);
                i32 prev3 = prevbyte(p, 3);
                if (!cppident(prev) ||
                        (!cppident(prev2) && (prev=='L' || prev=='u' || prev=='U')) ||
                        (!cppident(prev3) && prev2=='u' && prev=='8')) {
                    if (direct(p) && peekbyte(p, 0)!='"') {
                        goto enter;
                    }
                    i32 next = rawget(p);
                    if (next == '"') {
                        p->accept = 0;
                        p->innum  = p->prevident = 0;
                        c = skiprawstring(p);
                        break;
                    }
                    unget(p, next);
                }
            } else if (cppxdigit(c) && p->innum) {
                // A quote after a digit within a number separates digits
                if (direct(p)) {
                    if (peekbyte(p, 0) == '\'') {
                        p->in.p++;
                    }
                } else {
                    i32 next = rawget(p);
                    if (next != '\'') {
                        unget(p, next);
                    }
                }
            } else if (cppxdigit(c) && p->nmarks) {
                // Universal Ctags looks past every hex digit, which ends
                // a replacement read to its end (LEX-14)
                unget(p, rawget(p));
            }
        enter:
            p->accept = 0;
            if (indirective) {
                ignore = directive(p, c);
            }
            break;
        }
    } while (indirective || ignore);
    return c;
}

// The next character for the parser: by cppget, or where it would only
// return the next byte with the same updates, directly. The fast paths
// rest on this: a call to cppget never begins inside a directive or an
// ignored branch, whose characters it consumes before returning.
static i32 cppnext(Cpp *p)
{
    Reader *r = &p->in;
    if (!direct(p) || r->p==r->end) {
        return cppget(p);
    }
    i32 c = *r->p;
    iz  n = 1;
    switch (bytekind.kind[c]) {
    case BK_SLOW:
        return cppget(p);
    case BK_CR:
        if (r->end-r->p<2 || r->p[1]!='\n') {
            p->innum = p->prevident = p->accept = 0;
            break;
        }
        c = '\n';
        n = 2;
        [[fallthrough]];
    case BK_LF:
        p->accept = 1;
        [[fallthrough]];
    case BK_BLANK:
        p->innum = p->prevident = 0;
        break;
    case BK_R:
        if (r->end-r->p>1 && r->p[1]=='"') {
            return cppget(p);
        }
        [[fallthrough]];
    case BK_IDENT:
        p->prevident = 1;
        p->accept    = 0;
        break;
    case BK_DIGIT:
        p->innum |= !p->prevident;
        [[fallthrough]];
    case BK_HEX:
        p->prevident = 1;
        p->accept    = 0;
        if (p->innum && r->end-r->p>1 && r->p[1]=='\'') {
            n = 2;  // a digit separator, dropped
        }
        break;
    case BK_DOT:
        p->prevident = 0;
        p->accept    = 0;
        break;
    case BK_PLAIN:
        p->innum = p->prevident = p->accept = 0;
        break;
    }
    startline(r);
    r->p      += n;
    r->newline = c == '\n';
    return c;
}

// After an identifier's character, read directly: the rest of the run
// that is plainly the same identifier, up to an R before a quote, which
// may begin a raw string, else nothing.
static Str identrun(Cpp *p)
{
    Reader *r = &p->in;
    if (!direct(p) || !p->prevident || p->innum) {
        return {};
    }
    u8 *s = r->p;
    for (; s < r->end; s++) {
        u8 k = bytekind.kind[*s];
        if (k<BK_IDENT || k>BK_R || (k==BK_R && r->end-s>1 && s[1]=='"')) {
            break;
        }
    }
    Str run = span(r->p, s);
    if (run.len) {
        startline(r);
        r->p = s;
    }
    return run;
}

// After a blank, read directly: skip the rest of a run of spaces and
// tabs, which change nothing a blank did not.
static void blankrun(Cpp *p)
{
    Reader *r = &p->in;
    if (!direct(p)) {
        return;
    }
    u8 *s = r->p;
    for (; s<r->end && (*s==' ' || *s=='\t'); s++) {}
    if (s > r->p) {
        startline(r);
        r->p = s;
    }
}

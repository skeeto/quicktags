// Output: sorting, merging, and writing the tags file
// This is free and unencumbered software released into the public domain.

// Lines are ordered by bytes (SORT-1), or with fold, by bytes with
// ASCII letters uppercased, then by bytes (SORT-3). Each order is its
// own instance, so the default pays nothing for the other.

static u8 foldbyte(u8 c)
{
    return c>='a' && c<='z' ? (u8)(c - 32) : c;
}

template<b32 fold>
static u64 linekey(u8 *p, iz len)
{
    u64 k = 0;
    for (iz i = 0; i < 8; i++) {
        u8 c = i<len ? p[i] : 0;
        k = k<<8 | (fold ? foldbyte(c) : c);
    }
    return k;
}

// Byte-wise, unsigned, shorter first: strcmp() order, since lines hold
// no NUL bytes. That is the order Vim's binary search expects, and the
// order Universal Ctags writes. With fold, struppercmp() order, and
// lines equal by it in strcmp() order.
template<b32 fold>
static b32 lineless(Line *a, Line *b)
{
    if (a->key != b->key) {
        return a->key < b->key;
    }
    iz len = a->len<b->len ? a->len : b->len;
    if (!fold && len>8) {
        i32 d = __builtin_memcmp(a->data+8, b->data+8, (uz)(len-8));
        if (d) return d < 0;
    } else if (fold) {
        for (iz i = 8; i < len; i++) {
            u8 x = foldbyte(a->data[i]);
            u8 y = foldbyte(b->data[i]);
            if (x != y) return x < y;
        }
    }
    if (fold && a->len==b->len) {
        i32 d = __builtin_memcmp(a->data, b->data, (uz)len);
        return d < 0;
    }
    return a->len < b->len;
}

static b32 lineequal(Line *a, Line *b)
{
    return a->key==b->key && a->len==b->len &&
           !__builtin_memcmp(a->data, b->data, (uz)a->len);
}

// Index and sort a worker's lines, in its own thread.
template<b32 fold>
static void sortlines_(Worker *w)
{
    Str text = finish(&w->tags);
    w->lines = alloc<Line>(&w->perm, w->nlines);
    iz n = 0;
    for (iz i = 0; i < text.len;) {
        iz end = i;
        for (; text[end] != '\n'; end++) {}
        Line *l = w->lines + n++;
        l->data = text.data + i;
        l->len  = end - i;
        l->key  = linekey<fold>(l->data, l->len);
        i = end + 1;
    }
    assert(n == w->nlines);
    sort(w->lines, n, lineless<fold>, w->scratch);
}

static void sortlines(Worker *w)
{
    if (w->ctx->opt->sort == SORT_FOLD) {
        sortlines_<1>(w);
    } else {
        sortlines_<0>(w);
    }
}


// Buffered output with a sticky error

struct Out {
    Os  *os;
    u8  *buf;
    iz   len;
    iz   cap;
    i32  fd;
    b32  err;
};

static Out *newout(Os *os, Arena *a, i32 fd)
{
    Out *o = alloc<Out>(a);
    o->os  = os;
    o->cap = (iz)1<<20;
    o->buf = allocbytes(a, o->cap);
    o->fd  = fd;
    return o;
}

static void flush(Out *o)
{
    if (!o->err && o->len) {
        o->err = !os_write(o->os, o->fd, {o->buf, o->len});
    }
    o->len = 0;
}

static void print(Out *o, Str s)
{
    if (s.len > o->cap-o->len) {
        flush(o);
        if (s.len > o->cap) {
            o->err |= !o->err && !os_write(o->os, o->fd, s);
            return;
        }
    }
    copybytes(o->buf+o->len, s.data, s.len);
    o->len += s.len;
}


// Merging

// Lines in order: a span of an index, or a cursor over text, which
// reads it a line at a time into cur, where beg then points.
struct Stream {
    Line     *beg;
    Line     *end;
    Line      cur;
    u8       *next;  // a cursor's text yet to read, up to stop, ending in LF
    u8       *stop;
    Map<b32> *drop;  // files whose lines a cursor skips (D9)
};

// Whether a line is one of a file tagged again, whose file field is in
// the set, and not a pseudo-tag (D9).
static b32 dropped(Map<b32> *drop, Str line)
{
    if (!drop || startswith(line, "!_")) {
        return 0;
    }
    Cut name = cut(line, '\t');
    return name.ok && upsert(&drop, cut(name.tail, '\t').head, 0);
}

// Move an empty cursor to its next line that is not blank or dropped,
// if any.
template<b32 fold>
static void refill(Stream *s)
{
    while (s->next < s->stop) {
        u8 *beg = s->next;
        u8 *end = beg;
        for (; *end != '\n'; end++) {}
        s->next = end + 1;
        if (end>beg && !dropped(s->drop, span(beg, end))) {
            s->cur.data = beg;
            s->cur.len  = end - beg;
            s->cur.key  = linekey<fold>(beg, end-beg);
            s->beg      = &s->cur;
            s->end      = &s->cur + 1;
            return;
        }
    }
}

template<b32 fold>
static b32 streamless(Stream *s, i32 a, i32 b)
{
    return lineless<fold>(s[a].beg, s[b].beg);
}

template<b32 fold>
static void siftdown(i32 *heap, i32 len, i32 i, Stream *s)
{
    for (;;) {
        i32 least = i;
        i32 l     = 2*i + 1;
        i32 r     = l + 1;
        if (l<len && streamless<fold>(s, heap[l], heap[least])) least = l;
        if (r<len && streamless<fold>(s, heap[r], heap[least])) least = r;
        if (least == i) {
            return;
        }
        i32 t = heap[i];
        heap[i]     = heap[least];
        heap[least] = t;
        i = least;
    }
}

// Write the union of sorted streams in order, each distinct line once.
template<b32 fold>
static iz mergelines(Out *o, Stream *s, i32 n, Arena scratch)
{
    i32 *heap = alloc<i32>(&scratch, n);
    i32  len  = 0;
    for (i32 i = 0; i < n; i++) {
        if (s[i].beg == s[i].end) {
            refill<fold>(s+i);
        }
        if (s[i].beg < s[i].end) {
            heap[len++] = i;
        }
    }
    for (i32 i = len/2 - 1; i >= 0; i--) {
        siftdown<fold>(heap, len, i, s);
    }

    iz   count = 0;
    Line prev  = {};  // a copy, as a cursor reuses its line
    while (len) {
        Stream *top = s + heap[0];
        Line   *l   = top->beg++;
        if (!count || !lineequal(&prev, l)) {
            print(o, {l->data, l->len+1});  // with its newline
            prev = *l;
            count++;
        }
        if (top->beg == top->end) {
            refill<fold>(top);
            if (top->beg == top->end) {
                heap[0] = heap[--len];
            }
        }
        siftdown<fold>(heap, len, 0, s);
    }
    return count;
}

static b32 chunkless(Chunk **a, Chunk **b)
{
    return compare((*a)->key, (*b)->key) < 0;
}

// Write each file's lines as its parser made them, the files in the
// walk's order (SORT-4), and return how many.
static iz writechunks(Out *o, Worker *ws, i32 n, Arena scratch)
{
    iz total = 0;
    for (i32 i = 0; i < n; i++) {
        total += ws[i].nchunks;
    }
    Chunk **all = alloc<Chunk *>(&scratch, total);
    iz      len = 0;
    for (i32 i = 0; i < n; i++) {
        for (Chunk *k = ws[i].chunks; k; k = k->next) {
            all[len++] = k;
        }
    }
    sort(all, len, chunkless, scratch);

    iz count = 0;
    for (iz i = 0; i < len; i++) {
        print(o, all[i]->text);
        count += all[i]->ntags;
    }
    return count;
}


// Appending (OPT-20)

// The tags file to append to, read whole.
struct Old {
    Str   text;    // CRs made LFs, then a sentinel LF; null data: none
    Line *lines;   // if out of order: its lines but blank ones, sorted
    iz    nlines;
};

// In one pass over the old lines, rewrite the sorted flag among the
// leading pseudo-tags, and check the order unless unsorted; then if
// they are out of order, index and sort them.
template<b32 fold>
static void scanold(Old *old, i32 mode, Arena *mem)
{
    Str  flag   = "!_TAG_FILE_SORTED\t";
    u8  *p      = old->text.data;
    u8  *stop   = p + old->text.len;
    b32  head   = 1;  // among the leading lines starting with '!'
    b32  sorted = 1;
    iz   count  = 0;
    Line prev   = {};
    for (u8 *beg = p; beg < stop;) {
        u8 *end = beg;
        for (; *end != '\n'; end++) {}
        Str line = span(beg, end);
        beg = end + 1;
        if (!line.len) {
            continue;
        }
        head &= line[0] == '!';
        if (head && line.len>flag.len && startswith(line, flag)) {
            u8 *c = &line[flag.len];
            *c = *c=='0' || *c=='1' ? (u8)('0' + mode) : *c;
        }
        if (mode != SORT_NO) {
            Line l = {linekey<fold>(line.data, line.len), line.data, line.len};
            sorted &= !count || !lineless<fold>(&l, &prev);
            prev = l;
        }
        count++;
    }
    if (sorted) {
        return;  // read as a cursor
    }

    Stream s = {};
    s.next = p;
    s.stop = stop;
    old->lines  = alloc<Line>(mem, count);
    old->nlines = count;
    for (iz i = 0; i < count; i++) {
        refill<fold>(&s);
        old->lines[i] = s.cur;
    }
    sort(old->lines, count, lineless<fold>, *mem);
}

// Read the tags file to append to, if it exists and is not empty, into
// the arena, then prepare its lines for merging. False on failure.
static b32 loadold(Old *old, Os *os, Options *opt, Arena *mem, Buf *log)
{
    *old = {};
    Stat s = os_stat(os, *mem, opt->output);
    if (s.type!=STAT_FILE || !s.size) {
        return 1;  // a new file
    }

    iz   max = arenasize(*mem) - 1;  // room for the sentinel
    Read r   = os_read(os, mem, opt->output, max);
    if (r.err) {
        print(log, "ctags: cannot open tag file \"");
        print(log, opt->output);
        print(log, "\" : ");
        print(log, errstr(r.err));
        putbyte(log, '\n');
        return 0;
    } else if (r.size > max) {
        os_oom();
    } else if (!r.data.len) {
        return 1;
    }

    u8 *sentinel = allocbytes(mem, 1);
    assert(sentinel == r.data.data+r.data.len);
    *sentinel = '\n';
    old->text = {r.data.data, r.data.len+1};
    for (iz i = 0; i < r.data.len; i++) {
        u8 *c = &old->text[i];
        *c = *c=='\r' ? '\n' : *c;
    }

    if (opt->sort == SORT_FOLD) {
        scanold<1>(old, opt->sort, mem);
    } else {
        scanold<0>(old, opt->sort, mem);
    }
    return 1;
}


// The tags file

// The working directory as PROC_CWD has it (PT-4), or null to skip it.
static Str proccwd(Os *os, Options *opt, Arena *a, Buf *log)
{
    Str cwd = os_cwd(os, a);
    if (!cwd.data) {
        return {};
    }
    b32 windows = opt->windows;
    Buf b(a, cwd.len+2);
    for (iz i = 0; i < cwd.len; i++) {
        u8 c = cwd[i];
        putbyte(&b, windows && c=='\\' ? '/' : c);
    }
    u8 last = cwd.len ? cwd[cwd.len-1] : 0;
    if (last!='/' && !(windows && (last=='\\' || last==':'))) {
        putbyte(&b, '/');
    }
    cwd = finish(&b);

    if (opt->writer != WRITER_ECTAGS) {
        Buf e(a, cwd.len+8);
        printescaped(&e, cwd, 0);
        return finish(&e);
    }
    iz tab     = find(cwd, "\t");
    iz newline = find(cwd, "\n");
    if (tab>=0 || newline>=0) {
        print(log, "ctags: Warning: skip priting !_TAG_PROC_CWD pseudo tag; "
                   "the input field of the pseudo tag includes a ");
        print(log, tab>=0 ? Str("tab") : Str("newline"));
        print(log, " character: ");
        print(log, cwd);
        putbyte(log, '\n');
        return {};
    }
    return cwd;
}

// The pseudo-tags at the head of a new tags file (PT-4), in order.
static Str pseudotags(Os *os, Options *opt, Arena *a, Buf *log)
{
    static Str digits[]  = {"0", "1", "2"};
    static Str formats[] = {
        "unknown format", "original ctags format",
        "extended format; --format=1 will not append ;\" to lines",
    };
    static Str excmds[] = {"mixed", "number", "pattern", "combineV2"};

    b32 ectags = opt->writer == WRITER_ECTAGS;
    b32 extras = opt->format>1 && fieldon(opt->fields, FIELD_EXTRAS);
    Buf limit(a, 16);
    print(&limit, (i64)opt->patmax);
    Str cwd = proccwd(os, opt, a, log);

    struct { Str name, value, desc; } tags[] = {
        {"FILE_FORMAT",          digits[opt->format], formats[opt->format]},
        {"FILE_SORTED",          digits[opt->sort],
                                 "0=unsorted, 1=sorted, 2=foldcase"},
        {"OUTPUT_EXCMD",         excmds[opt->excmd],
                                 "number, pattern, mixed, or combineV2"},
        {"OUTPUT_FILESEP",       ectags && opt->windows ? Str("backslash")
                                                        : Str("slash"),
                                 "slash or backslash"},
        {"OUTPUT_MODE",          ectags ? Str("e-ctags") : Str("u-ctags"),
                                 "u-ctags or e-ctags"},
        {"PATTERN_LENGTH_LIMIT", finish(&limit), "0 for no limit"},
        {"PROC_CWD",             cwd, ""},
        {"PROGRAM_NAME",         "ctags", ""},
        {"PROGRAM_VERSION",      version, ""},
    };
    Buf b(a, 1024);
    for (iz i = 0; i < countof(tags); i++) {
        if (!tags[i].value.data) {
            continue;
        }
        print(&b, "!_TAG_");
        print(&b, tags[i].name);
        putbyte(&b, '\t');
        print(&b, tags[i].value);
        print(&b, "\t/");
        print(&b, tags[i].desc);
        putbyte(&b, '/');
        print(&b, extras ? Str(";\"\textras:pseudo") : Str(""));
        putbyte(&b, '\n');
    }
    return finish(&b);
}

// Universal Ctags' test for an existing tags file it may overwrite (CLI-3):
// its first line, up to a NUL, starts an etags section, or is a name not
// starting with '#', a file not ending in ';', each followed by exactly
// one tab, and an address, up to a CR, that is a pattern or digits up to
// a ';'.
static b32 istagsline(Str s)
{
    if (s.len>=2 && s[0]=='\f' && (s[1]=='\n' || s[1]=='\r')) {
        return 1;  // etags
    }
    Str line = cut(cut(s, '\n').head, 0).head;
    Cut name = cut(line, '\t');
    Cut file = cut(name.tail, '\t');
    Str addr = cut(file.tail, '\r').head;
    if (!name.ok || !file.ok || !name.head.len || name.head[0]=='#' ||
        !file.head.len || endswith(file.head, ";") || !addr.len ||
        addr[0]=='\t') {
        return 0;
    } else if (addr[0]=='/' || addr[0]=='?') {
        return 1;
    }
    Str num = cut(addr, ';').head;
    for (iz i = 0; i < num.len; i++) {
        if (!digit(num[i])) {
            return 0;
        }
    }
    return num.len > 0;
}

// Refuse to replace a directory or a file that isn't a tags file.
static b32 checkoutput(Os *os, Str path, Arena scratch, Buf *log)
{
    Stat s = os_stat(os, scratch, path);
    if (s.type == STAT_DIR) {
        print(log, "ctags: \"");
        print(log, path);
        print(log, "\" already exists as a directory; "
                   "I cannot write tag entries there.\n");
        return 0;
    }
    if (s.type==STAT_FILE && s.size) {
        Read r = os_read(os, &scratch, path, 4096);
        if (!r.err && !istagsline(r.data)) {
            print(log, "ctags: \"");
            print(log, path);
            print(log, "\" doesn't look like a tag file; "
                       "I refuse to overwrite it.\n");
            return 0;
        }
    }
    return 1;
}

// Write the workers' lines, and those of a tags file appended to, in
// the order the options ask, and return how many.
static iz writelines(Out *o, Options *opt, Worker *ws, i32 n, Old *old,
                     Arena scratch)
{
    Stream *s  = alloc<Stream>(&scratch, n+1);
    i32     ns = n;
    if (old->text.data) {
        // The old lines but those of files tagged again (D9)
        Map<b32> *drop = 0;
        for (i32 i = 0; i < n; i++) {
            for (Chunk *k = ws[i].chunks; k; k = k->next) {
                *upsert(&drop, k->outpath, &scratch) = 1;
            }
        }
        Stream *t = s + ns++;
        if (old->lines) {
            iz len = 0;
            for (iz i = 0; i < old->nlines; i++) {
                Line *l = old->lines + i;
                if (!dropped(drop, {l->data, l->len})) {
                    old->lines[len++] = *l;
                }
            }
            t->beg = old->lines;
            t->end = old->lines + len;
        } else {
            t->next = old->text.data;
            t->stop = old->text.data + old->text.len;
            t->drop = drop;
        }
    }

    if (opt->sort == SORT_NO) {
        iz count = 0;
        if (ns > n) {
            // The old lines first, in their order
            Stream *t = s + n;
            for (refill<0>(t); t->beg < t->end; t->beg++, refill<0>(t)) {
                print(o, {t->cur.data, t->cur.len+1});
                count++;
            }
        }
        return count + writechunks(o, ws, n, scratch);
    }

    for (i32 i = 0; i < n; i++) {
        s[i].beg = ws[i].lines;
        s[i].end = ws[i].lines + ws[i].nlines;
    }
    if (opt->sort == SORT_FOLD) {
        return mergelines<1>(o, s, ns, scratch);
    }
    return mergelines<0>(o, s, ns, scratch);
}

// Write the merged lines to standard output, or to a new file that
// then replaces the tags file, so that Vim never reads a partial one.
static b32 writetags(Os *os, Options *opt, Worker *ws, i32 n, Old *old,
                     iz *count, Arena scratch, Buf *log)
{
    if (opt->output == "-") {
        Out *o = newout(os, &scratch, 1);
        *count = writelines(o, opt, ws, n, old, scratch);
        flush(o);
        if (o->err) {
            print(log, "ctags: error writing output\n");
        }
        return !o->err;
    }

    Str tmp = {};
    i32 err = 0;
    for (i32 i = 0; i < 100; i++) {
        Buf b(&scratch, opt->output.len+8);
        print(&b, opt->output);
        print(&b, ".tmp");
        print(&b, (i64)i);
        tmp = finish(&b);
        err = os_create(os, scratch, tmp);
        if (err != OSERR_EXIST) {
            break;
        }
    }
    if (err) {
        print(log, "ctags: cannot open tag file \"");
        print(log, tmp);
        print(log, "\" : ");
        print(log, errstr(err));
        putbyte(log, '\n');
        return 0;
    }

    Out *o = newout(os, &scratch, 3);
    if (opt->extras&EXTRA_PSEUDO && !old->text.data) {
        print(o, pseudotags(os, opt, &scratch, log));
    }
    *count = writelines(o, opt, ws, n, old, scratch);
    flush(o);
    b32 ok = !o->err;
    ok &= os_close(os, 3);
    if (!ok) {
        os_delete(os, scratch, tmp);
        print(log, "ctags: error writing \"");
        print(log, tmp);
        print(log, "\"\n");
        return 0;
    }

    err = os_rename(os, scratch, tmp, opt->output);
    if (err) {
        os_delete(os, scratch, tmp);
        print(log, "ctags: cannot replace \"");
        print(log, opt->output);
        print(log, "\" : ");
        print(log, errstr(err));
        putbyte(log, '\n');
        return 0;
    }
    return 1;
}

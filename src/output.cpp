// Output: sorting, merging, and writing the tags file
// This is free and unencumbered software released into the public domain.

static u64 linekey(u8 *p, iz len)
{
    u64 k = 0;
    for (iz i = 0; i < 8; i++) {
        k = k<<8 | (i<len ? p[i] : 0);
    }
    return k;
}

// Byte-wise, unsigned, shorter first: strcmp() order, since lines hold
// no NUL bytes. That is the order Vim's binary search expects, and the
// order Universal Ctags writes.
static b32 lineless(Line *a, Line *b)
{
    if (a->key != b->key) {
        return a->key < b->key;
    }
    iz len = a->len<b->len ? a->len : b->len;
    if (len > 8) {
        i32 d = __builtin_memcmp(a->data+8, b->data+8, (uz)(len-8));
        if (d) return d < 0;
    }
    return a->len < b->len;
}

static b32 lineequal(Line *a, Line *b)
{
    return a->key==b->key && a->len==b->len &&
           !__builtin_memcmp(a->data, b->data, (uz)a->len);
}

// Index and sort a worker's lines, in its own thread.
static void sortlines(Worker *w)
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
        l->key  = linekey(l->data, l->len);
        i = end + 1;
    }
    assert(n == w->nlines);
    sort(w->lines, n, lineless, w->scratch);
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

struct Stream {
    Line *beg;
    Line *end;
};

static b32 streamless(Stream *s, i32 a, i32 b)
{
    return lineless(s[a].beg, s[b].beg);
}

static void siftdown(i32 *heap, i32 len, i32 i, Stream *s)
{
    for (;;) {
        i32 least = i;
        i32 l     = 2*i + 1;
        i32 r     = l + 1;
        if (l<len && streamless(s, heap[l], heap[least])) least = l;
        if (r<len && streamless(s, heap[r], heap[least])) least = r;
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
static iz mergelines(Out *o, Stream *s, i32 n, Arena scratch)
{
    i32 *heap = alloc<i32>(&scratch, n);
    i32  len  = 0;
    for (i32 i = 0; i < n; i++) {
        if (s[i].beg < s[i].end) {
            heap[len++] = i;
        }
    }
    for (i32 i = len/2 - 1; i >= 0; i--) {
        siftdown(heap, len, i, s);
    }

    iz    count = 0;
    Line *prev  = 0;
    while (len) {
        Stream *top = s + heap[0];
        Line   *l   = top->beg++;
        if (!prev || !lineequal(prev, l)) {
            print(o, {l->data, l->len+1});  // with its newline
            prev = l;
            count++;
        }
        if (top->beg == top->end) {
            heap[0] = heap[--len];
        }
        siftdown(heap, len, 0, s);
    }
    return count;
}


// The tags file

static Str pseudotags =
"!_TAG_FILE_FORMAT\t2\t/extended format; --format=1 will not append ;\" to lines/\n"
"!_TAG_FILE_SORTED\t1\t/0=unsorted, 1=sorted, 2=foldcase/\n"
"!_TAG_PROGRAM_NAME\tctags\t//\n";

// Universal Ctags' test for an existing tags file it may overwrite: its
// first line has a name, a file, and an address, separated by tabs.
static b32 istagsline(Str s)
{
    if (s.len && s[0]=='\f') {
        return 1;  // etags
    }
    Cut line = cut(s, '\n');
    Str l    = line.head;
    if (endswith(l, "\r")) {
        l = cuttail(l, 1);
    }
    Cut name = cut(l, '\t');
    Cut file = cut(name.tail, '\t');
    return name.ok && file.ok && name.head.len && !startswith(name.head, "#") &&
           file.head.len && !endswith(file.head, ";") && file.tail.len;
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

static Stream *streams(Worker *ws, i32 n, Arena *a)
{
    Stream *s = alloc<Stream>(a, n);
    for (i32 i = 0; i < n; i++) {
        s[i].beg = ws[i].lines;
        s[i].end = ws[i].lines + ws[i].nlines;
    }
    return s;
}

// Write the merged lines to standard output, or to a new file that
// then replaces the tags file, so that Vim never reads a partial one.
static b32 writetags(Os *os, Options *opt, Worker *ws, i32 n, iz *count,
                     Arena scratch, Buf *log)
{
    Stream *s = streams(ws, n, &scratch);

    if (opt->output == "-") {
        Out *o = newout(os, &scratch, 1);
        *count = mergelines(o, s, n, scratch);
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
    print(o, pseudotags);
    print(o, "!_TAG_PROGRAM_VERSION\t");
    print(o, version);
    print(o, "\t//\n");
    *count = mergelines(o, s, n, scratch);
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

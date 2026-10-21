// Discovery: walking the inputs in parallel and tagging each file
// This is free and unencumbered software released into the public domain.

enum : i32 {
    ITEM_DIR,
    ITEM_FILE,
    ITEM_ARG,   // an argument, to examine
    ITEM_NONE,  // an argument with nothing to do
};

enum : iz { FILEMAX = (iz)64<<20 };  // larger files are skipped

// A directory to list or a file to tag, pushed on the work stack once.
struct Item {
    Item *next;
    Str   path;    // as constructed: the input to anonymous names
    Str   key;     // its place in the walk's order, if output is ordered
    i64   mtime;   // a file's, as listed or examined
    i32   type;
    i32   lang;
    b32   header;
    i32   err;     // an argument that cannot be examined: OSERR_*
};

// A lock-free stack of items (Treiber). While workers run, each item is
// pushed at most once and never reused, so a popped item cannot reappear
// at the top, and the stack has no ABA problem. (Arguments examined by
// workers are pushed again for the next run.) Work is done when pending,
// items pushed but not yet finished, reaches zero.
struct Queue {
    alignas(64) Item *top;
    alignas(64) iz    pending;
};

struct Ctx {
    Os      *os;
    Options *opt;
    Str      cwd;      // with --tag-relative: the current directory
    Str      base;     // and the directory paths are relative to
    b32      ordered;  // files' tags are written in the walk's order
    Queue    queue;
};

// One file's tags, for output in the walk's order: a span of its
// worker's tag lines, as offsets until the worker finishes them.
struct Chunk {
    Chunk *next;
    Str    key;      // the item's
    Str    path;     // as constructed
    Str    outpath;  // as the file field has it
    Str    text;     // its lines, set when the worker is done
    iz     beg;
    iz     len;
    iz     ntags;
};

struct alignas(64) Worker {
    Ctx   *ctx;
    Arena  perm;     // items, paths, warnings, the line index
    Arena  text;     // tag lines, in one growing buffer
    Arena  scratch;  // contents and parse state of the current file
    Buf    tags;
    Buf    log;
    Line  *lines;
    Chunk *chunks;   // if ordered, one per file, last first
    iz     nchunks;
    iz     nlines;
    iz     nfiles;
    i64    nbytes;
    i64    busy;     // microseconds, for --_stats
    Str    fatal;    // the least path that cannot be made absolute
    i32    id;
};

static void sortlines(Worker *);
static void parsefile(File *, Arena scratch);

static void pushitems(Queue *q, Item *first, Item *last)
{
    Item *top = __atomic_load_n(&q->top, __ATOMIC_RELAXED);
    do {
        last->next = top;
    } while (!__atomic_compare_exchange_n(
        &q->top, &top, first, 1, __ATOMIC_RELEASE, __ATOMIC_RELAXED
    ));
}

static Item *popitem(Queue *q)
{
    Item *top = __atomic_load_n(&q->top, __ATOMIC_ACQUIRE);
    while (top && !__atomic_compare_exchange_n(
        &q->top, &top, top->next, 1, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE
    )) {}
    return top;
}


// Paths

static b32 isseparator(u8 c, b32 windows)
{
    return c=='/' || (windows && c=='\\');
}

static Str basename(Str path, b32 windows)
{
    iz i = path.len;
    for (; i>0 && !isseparator(path[i-1], windows); i--) {}
    return cuthead(path, i);
}

// Join a listed name onto its directory as Universal Ctags does: names
// under "." stand alone, and no separator is added after one already
// there (or after a drive letter's colon).
static Str joinpath(Arena *a, Str dir, Str name, b32 windows)
{
    if (dir == ".") {
        return clone(a, name);
    }
    Str r = clone(a, dir);
    u8  c = dir.len ? dir[dir.len-1] : 0;
    if (!isseparator(c, windows) && !(windows && c==':')) {
        r = concat(a, r, "/");
    }
    return concat(a, r, name);
}

// Whether patterns match a path's base name, or else the whole path as
// constructed (DISC-3).
static b32 pathmatch(Slice<Glob> globs, Str path, Str base, b32 windows)
{
    return globsmatch(globs, base, windows) ||
           (base.len!=path.len && globsmatch(globs, path, windows));
}

// Whether an entry is excluded, checked before it is examined: patterns
// match it and no exception does, so an excluded directory is pruned
// unless an exception matches it (D11).
static b32 excluded(Options *opt, Str path, Str base)
{
    return pathmatch(opt->excludes, path, base, opt->windows) &&
           !pathmatch(opt->exceptions, path, base, opt->windows);
}

struct Lang {
    i32 lang;
    b32 header;
};

// A name's extension: after its last '.', or empty without one.
static Str extension(Str name)
{
    iz dot = name.len;
    for (; dot>0 && name[dot-1]!='.'; dot--) {}
    return dot ? cuthead(name, dot) : Str("");
}

// Whether a list holds a string, ignoring case.
static b32 inlist(Slice<Str> list, Str s)
{
    for (iz i = 0; i < list.len; i++) {
        if (equalsfold(list[i], s)) {
            return 1;
        }
    }
    return 0;
}

// Whether a name sorts before another, ignoring case, as strcasecmp()
// orders them.
static b32 lessfold(Str a, Str b)
{
    for (iz i = 0; i<a.len && i<b.len; i++) {
        u8 x = lowercase(a[i]);
        u8 y = lowercase(b[i]);
        if (x != y) {
            return x < y;
        }
    }
    return a.len < b.len;
}

// Whether a map takes a name: in the first pass by a pattern, in the
// second by its extension, ignoring case.
static b32 maptakes(Slice<Str> globs, Slice<Str> exts, Str name, Str ext,
                    i32 pass, b32 windows)
{
    if (pass) {
        return inlist(exts, ext);
    }
    for (iz i = 0; i < globs.len; i++) {
        if (globmatch(globs[i], name, windows)) {
            return 1;
        }
    }
    return 0;
}

// The languages that take a name, gathered: how many, the first by
// name, and whether any lacks the selector C and C++ share (LANG-2).
struct Candidates {
    iz  count;
    Str name;
    i32 lang;    // LANG_*, LANG_FOREIGN for one from --langdef
    b32 others;  // neither C nor C++
};

static void nominate(Candidates *c, i32 lang, Str name)
{
    if (!c->count || lessfold(name, c->name)) {
        c->name = name;
        c->lang = lang;
    }
    c->count++;
    c->others |= lang!=LANG_C && lang!=LANG_CXX;
}

// A language chosen for a name, or, of several that took it with none
// chosen, a fallback (LANG-2).
struct Pick {
    i32 lang;
    i32 fallback;
};

// Which of the enabled languages, ours and those from --langdef, take a
// base name: those with a pattern that matches it, else those with its
// extension, ignoring case, but for C++'s own "C", which takes only
// ".C", and first (D1). Of several, C and C++ choose the first by name,
// as their shared selector does, and with any other among them none is
// chosen: the first is a fallback.
static Pick matchlang(Options *opt, Str name)
{
    Pick       r   = {};
    LangMap   *m   = &opt->map;
    Candidates c   = {};
    Str        ext = extension(name);
    for (i32 pass = 0; pass<2 && !c.count; pass++) {
        if (pass && ext=="C" && m->upperc && opt->langs>>LANG_CXX&1) {
            r.lang = LANG_CXX;
            return r;
        }
        for (i32 lang = LANG_C; lang <= LANG_FORTRAN; lang++) {
            if (opt->langs>>lang&1 && maptakes(m->globs[lang], m->exts[lang],
                                               name, ext, pass, opt->windows)) {
                nominate(&c, lang, langnames[lang]);
            }
        }
        for (iz i = 0; i < m->langdefs.len; i++) {
            Langdef *d = &m->langdefs[i];
            if (d->on && maptakes(d->globs, d->exts, name, ext, pass,
                                  opt->windows)) {
                nominate(&c, LANG_FOREIGN, d->name);
            }
        }
    }
    if (c.count>1 && c.others) {
        r.fallback = c.lang;
    } else {
        r.lang = c.lang;
    }
    return r;
}

// Choose a file's language by its base name, then without a final
// ".in", unless one is forced, and say whether it is a header by the
// name without ".in", whatever the language (LANG-2, LANG-5, D2). One
// this program does not parse is no language here.
static Lang detectlang(Options *opt, Str path)
{
    Lang r    = {};
    Str  base = basename(path, opt->windows);
    b32  in   = base.len>=3 && equalsfold(taketail(base, 3), ".in");
    Str  bare = in ? cuttail(base, 3) : base;
    i32  lang = opt->force;
    if (!lang) {
        Pick p = matchlang(opt, base);
        if (!p.lang && in) {
            Pick t = matchlang(opt, bare);
            p.lang     = t.lang;
            p.fallback = p.fallback ? p.fallback : t.fallback;
        }
        lang = p.lang ? p.lang : p.fallback;
    }
    r.lang   = lang!=LANG_FOREIGN ? lang : LANG_NONE;
    r.header = r.lang && inlist(opt->headers, extension(bare));
    return r;
}

// The file field: the path with '\' as '/', escaped.
static Str outpath(Arena *a, Str path, b32 windows)
{
    Buf b(a, path.len+8);
    for (iz i = 0; i < path.len; i++) {
        u8 c = path[i];
        if (windows && c=='\\') {
            putbyte(&b, '/');
        } else {
            printescaped(&b, {&path[i], 1}, 0);
        }
    }
    return finish(&b);
}


// Paths for --tag-relative (OPT-18)

// Whether a byte separates names as Universal Ctags makes paths absolute
// and relative: on Windows '\', and ':' too.
static b32 relsep(u8 c, b32 windows)
{
    return c=='/' || (windows && (c=='\\' || c==':'));
}

// Whether a path is absolute: a separator first, or on Windows a letter
// and ':', then a separator or nothing.
static b32 isabsolute(Str path, b32 windows)
{
    if (path.len && relsep(path[0], windows)) {
        return 1;
    }
    return windows && path.len>=2 && letter(path[0]) && path[1]==':' &&
           (path.len==2 || relsep(path[2], windows));
}

// Whether a path names a drive but does not start from its root, "C:a.c",
// which Universal Ctags will not make absolute.
static b32 driverelative(Str path, b32 windows)
{
    return windows && path.len>2 && letter(path[0]) && path[1]==':' &&
           !relsep(path[2], windows);
}

static void driveerror(Buf *log, Str path)
{
    print(log, "ctags: ");
    print(log, path);
    print(log, ": relative file names with drive letters not supported\n");
}

// The length of a path's root on Windows: a drive, "C:", or a UNC
// path's "\\host\share" (D16). None elsewhere.
static iz rootlen(Str path, b32 windows)
{
    if (!windows || path.len<2) {
        return 0;
    } else if (path[1] == ':') {
        return 2;
    } else if (!isseparator(path[0], 1) || !isseparator(path[1], 1)) {
        return 0;
    }
    iz i = 2;
    for (; i<path.len && !isseparator(path[i], 1); i++) {}  // host
    if (i < path.len) {
        for (i++; i<path.len && !isseparator(path[i], 1); i++) {}  // share
    }
    return i;
}

// A path made absolute from the current directory, on Windows from its
// root, drive or share, if the path has none (D16): each "." name
// dropped with the separator before it, each ".." with the separator and
// name before that, back to a separator of any kind, and past doubled
// separators (D14), but never into the root (D15), the separators but
// ':' made '/', and the drive letter uppercased.
static Str abspath(Arena *a, Str cwd, Str path, b32 windows)
{
    Str full = path;
    if (!isabsolute(path, windows)) {
        b32 sep = cwd.len && relsep(cwd[cwd.len-1], windows);
        full = concat(a, concat(a, cwd, sep ? Str("") : Str("/")), path);
    } else if (windows && !rootlen(path, windows)) {
        full = concat(a, takehead(cwd, rootlen(cwd, windows)), path);
    }

    u8 *r    = allocbytes(a, full.len+1);
    iz  root = rootlen(full, windows);
    for (iz j = 0; j < root; j++) {
        r[j] = full[j]=='\\' ? '/' : full[j];
    }
    iz i = root;
    for (; i<full.len && !relsep(full[i], windows); i++) {}
    copybytes(r+root, full.data+root, i-root);
    iz len = i;
    while (i < full.len) {
        u8 sep = full[i++];
        iz beg = i;
        for (; i<full.len && !relsep(full[i], windows); i++) {}
        Str name = span(full.data+beg, full.data+i);
        if (name == ".") {
            continue;
        } else if (name == "..") {
            for (; len>root && r[len-1]=='/'; len--) {}  // empty names
            iz cut = len;
            for (; cut>root && !relsep(r[cut-1], windows); cut--) {}
            len = cut>root ? cut-1 : len;
            continue;
        }
        r[len++] = sep==':' ? ':' : '/';
        copybytes(r+len, name.data, name.len);
        len += name.len;
    }
    if (!len) {
        r[len++] = '/';
    }
    if (windows && len>=2 && r[1]==':' && r[0]>='a' && r[0]<='z') {
        r[0] = (u8)(r[0] - 32);
    }
    a->beg = (byte *)(r + len);
    return {r, len};
}

// A path relative to a directory, both absolute, from the first byte
// where they differ, comparing ASCII letters regardless of case on
// Windows: a "../" for each separator the directory has past the last
// separator they share, but doubled ones, then the path past it, which
// without a "../" does not begin with a separator (D14). With none
// shared, or another root, drive or share (D16), the path itself.
static Str relpath(Arena *a, Str path, Str dir, b32 windows)
{
    iz root = rootlen(path, windows);
    if (root != rootlen(dir, windows) ||
        !equalsfold(takehead(path, root), takehead(dir, root))) {
        return path;
    }

    iz same = 0;
    for (; same<path.len && same<dir.len; same++) {
        u8 x = path[same];
        u8 y = dir[same];
        if (windows ? lowercase(x)!=lowercase(y) : x!=y) {
            break;
        }
    }
    iz shared = same;  // the last separator in common
    do {
        if (!shared) {
            return path;
        }
        shared--;
    } while (!relsep(path[shared], windows));

    Buf b(a, path.len);
    for (iz i = shared+1; i < dir.len; i++) {
        if (relsep(dir[i], windows) && !relsep(dir[i-1], windows)) {
            print(&b, "../");
        }
    }
    iz rest = shared + 1;
    if (!b.len && path[shared]!=':') {
        for (; rest<path.len && relsep(path[rest], windows); rest++) {}
    }
    print(&b, cuthead(path, rest));
    return finish(&b);
}

// The tags file's directory, its name up to its last separator, or for
// one without or for standard output, the current directory, "".
static Str outdir(Str output, b32 windows)
{
    iz len = output=="-" ? 0 : output.len;
    for (; len>0 && !relsep(output[len-1], windows); len--) {}
    return takehead(output, len);
}

// The path for the file field, before escaping: as constructed, or by
// --tag-relative, relative to the base or absolute.
static Str tagpath(Arena *a, Ctx *c, Str path)
{
    i32 mode    = c->opt->tagrelative;
    b32 windows = c->opt->windows;
    if (mode==TREL_NO || (mode==TREL_YES && isabsolute(path, windows))) {
        return path;
    }
    Str abs = abspath(a, c->cwd, path, windows);
    return mode==TREL_NEVER ? abs : relpath(a, abs, c->base, windows);
}

// A file tag's name: the path as constructed after its last separator,
// on Windows ':' too, as Universal Ctags names it (EX-7).
static Str filetagname(Str path, b32 windows)
{
    iz i = path.len;
    for (; i>0 && !relsep(path[i-1], windows); i--) {}
    return cuthead(path, i);
}

static Str errstr(i32 err)
{
    switch (err) {
    case OSERR_NOENT: return "No such file or directory";
    case OSERR_ACCES: return "Permission denied";
    case OSERR_EXIST: return "File exists";
    case OSERR_ISDIR: return "Is a directory";
    case OSERR_INVAL: return "Invalid argument";
    }
    return "Input/output error";
}

// Report a problem with an input, with the system's reason if any.
static void warn(Buf *log, Str msg, Str path, i32 err)
{
    print(log, "ctags: Warning: ");
    print(log, msg);
    print(log, " \"");
    print(log, path);
    putbyte(log, '"');
    if (err) {
        print(log, " : ");
        print(log, errstr(err));
    }
    putbyte(log, '\n');
}


// Workers

// The order key of a directory's entry: its directory's key, then its
// index in the listing, big-endian, so that keys compare as the walk
// visits them, depth-first in listing order (DISC-2, SORT-4).
static Str childkey(Arena *a, Str parent, u32 index)
{
    Str r = {allocbytes(a, parent.len+4), parent.len+4};
    copybytes(r.data, parent.data, parent.len);
    for (i32 i = 0; i < 4; i++) {
        r[parent.len+i] = (u8)(index >> (24 - 8*i));
    }
    return r;
}

static Item *newitem(Arena *a, Str path, i32 type, Lang lang)
{
    Item *it = alloc<Item>(a);
    it->path   = path;
    it->type   = type;
    it->lang   = lang.lang;
    it->header = lang.header;
    return it;
}

// List a directory onto the work stack: its subdirectories on top, so
// that the walk keeps ahead of the parsing.
static void visitdir(Worker *w, Item *it)
{
    Ctx    *c       = w->ctx;
    b32     windows = c->opt->windows;
    Arena   tmp     = w->scratch;
    Listing l       = os_list(c->os, &tmp, it->path);
    if (l.err) {
        warn(&w->log, "cannot recurse into directory", it->path, l.err);
        return;
    }

    Item *dirs  = 0;
    Item *files = 0;
    Item *last  = 0;  // of files, else of dirs
    Item *dlast = 0;
    iz    count = 0;
    u32   index = 0;
    for (Dirent *d = l.head; d; d = d->next, index++) {
        byte *mark = w->perm.beg;
        Str   path = joinpath(&w->perm, it->path, d->name, windows);
        if ((d->islink && c->opt->nolinks) ||
            excluded(c->opt, path, d->name)) {
            w->perm.beg = mark;
            continue;
        }
        if (d->isdir) {
            Item *n = newitem(&w->perm, path, ITEM_DIR, {});
            n->key  = c->ordered ? childkey(&w->perm, it->key, index) : Str{};
            n->next = dirs;
            dirs    = n;
            if (!dlast) dlast = n;
        } else {
            Lang lang = detectlang(c->opt, path);
            if (!lang.lang) {
                w->perm.beg = mark;
                continue;
            }
            Item *n = newitem(&w->perm, path, ITEM_FILE, lang);
            n->key   = c->ordered ? childkey(&w->perm, it->key, index) : Str{};
            n->mtime = d->mtime;
            n->next  = files;
            files    = n;
            if (!last) last = n;
        }
        count++;
    }
    w->scratch.end = tmp.end>w->scratch.end ? tmp.end : w->scratch.end;
    if (!count) {
        return;
    }

    Item *first = dirs ? dirs : files;
    if (dirs) {
        dlast->next = files;
        if (!files) last = dlast;
    }
    __atomic_fetch_add(&c->queue.pending, count, __ATOMIC_RELAXED);
    pushitems(&c->queue, first, last);
}

static void visitfile(Worker *w, Item *it)
{
    Ctx  *c   = w->ctx;
    Arena tmp = w->scratch;
    Read  r   = os_read(c->os, &tmp, it->path, FILEMAX);
    if (r.err) {
        warn(&w->log, "cannot open input file", it->path, r.err);
        return;
    }
    if (c->opt->tagrelative!=TREL_NO &&
        driverelative(it->path, c->opt->windows)) {
        // Universal Ctags stops here, as the main thread will
        if (!w->fatal.data || compare(it->path, w->fatal)<0) {
            w->fatal = it->path;
        }
        return;
    }
    if (r.size > FILEMAX) {
        warn(&w->log, "skipping input file larger than 64 MiB:", it->path, 0);
        return;
    }

    // Zeros past the end allow word-at-a-time scanning
    u8 *pad = allocbytes(&tmp, 8);
    assert(pad == r.data.data+r.data.len);
    __builtin_memset(pad, 0, 8);

    b32    raw = c->opt->writer == WRITER_ECTAGS;  // paths as given, FLD-6
    Str    tp  = tagpath(&tmp, c, it->path);
    Source src = {};
    src.path     = it->path;
    src.outpath  = raw ? tp : outpath(&tmp, tp, c->opt->windows);
    src.text     = r.data;
    src.mtime    = it->mtime;
    src.anonhash = anonhash(it->path);
    src.lang     = it->lang;
    src.header   = it->header;
    if (startswith(src.text, "\xef\xbb\xbf")) {
        src.text = cuthead(src.text, 3);
    }

    File f = {};
    f.src    = &src;
    f.kinds  = &c->opt->kinds;
    f.fmt    = c->opt->fmt;
    f.extras = c->opt->extras;
    f.macros = c->opt->macros.map ? &c->opt->macros : 0;
    f.out    = &w->tags;
    f.mark   = w->tags.len;
    f.nofast = c->opt->nofast;
    parsefile(&f, tmp);
    if (c->opt->extras&EXTRA_INPUTFILE && c->opt->filekind) {
        filetag(&f, filetagname(it->path, c->opt->windows));
    }

    if (c->ordered) {
        // The output path lives in scratch, so copy it out
        Chunk *k = alloc<Chunk>(&w->perm);
        k->key     = it->key;
        k->path    = it->path;
        k->outpath = clone(&w->perm, src.outpath);
        k->beg     = f.mark;
        k->len     = w->tags.len - f.mark;
        k->ntags   = f.ntags;
        k->next    = w->chunks;
        w->chunks  = k;
        w->nchunks++;
    }

    w->nlines += f.ntags;
    w->nfiles++;
    w->nbytes += r.data.len;
    w->scratch.end = tmp.end>w->scratch.end ? tmp.end : w->scratch.end;
}

// Examine an argument, making it a directory to list, a file to tag, or
// nothing to do. One that is missing records why, for a warning given
// in the arguments' order.
static void examine(Ctx *c, Item *it, Arena scratch)
{
    Stat s = os_stat(c->os, scratch, it->path);
    it->type = ITEM_NONE;
    if (s.islink && c->opt->nolinks) {
        return;  // even one that leads nowhere (DISC-7)
    } else if (s.type == STAT_NONE) {
        it->err = s.err;
    } else if (s.type == STAT_DIR) {
        it->type = ITEM_DIR;
    } else if (s.type==STAT_FILE && it->lang) {
        it->type  = ITEM_FILE;
        it->mtime = s.mtime;
    }
}

// Examine arguments from the work stack until none are left.
static void examineall(Worker *w)
{
    Queue *q = &w->ctx->queue;
    for (Item *it = popitem(q); it; it = popitem(q)) {
        examine(w->ctx, it, w->scratch);
    }
}

enum : i32 { STEP_DONE, STEP_DID, STEP_WAIT };

// Pop and process one item: STEP_WAIT when the stack is empty but other
// workers still hold items, STEP_DONE when all work is done.
static i32 workstep(Worker *w)
{
    Queue *q  = &w->ctx->queue;
    Item  *it = popitem(q);
    if (!it) {
        b32 done = !__atomic_load_n(&q->pending, __ATOMIC_ACQUIRE);
        return done ? STEP_DONE : STEP_WAIT;
    }
    if (it->type == ITEM_DIR) {
        visitdir(w, it);
    } else {
        visitfile(w, it);
    }
    __atomic_fetch_sub(&q->pending, 1, __ATOMIC_RELEASE);
    return STEP_DID;
}

// Finish a worker's output once all work is done.
static void workend(Worker *w)
{
    if (!w->ctx->ordered) {
        sortlines(w);
        return;
    }
    Str text = finish(&w->tags);
    for (Chunk *k = w->chunks; k; k = k->next) {
        k->text = {text.data+k->beg, k->len};
    }
}

static void work(Worker *w)
{
    Ctx *c     = w->ctx;
    i64  start = c->opt->stats ? os_clock(c->os) : 0;
    for (i32 attempt = 0;;) {
        i32 r = workstep(w);
        if (r == STEP_DONE) {
            break;
        } else if (r == STEP_WAIT) {
            os_backoff(c->os, attempt++);
        } else {
            attempt = 0;
        }
    }
    if (c->opt->stats) {
        w->busy = os_clock(c->os) - start;
    }
    workend(w);
}

// Discovery: walking the inputs in parallel and tagging each file
// This is free and unencumbered software released into the public domain.

enum : i32 { ITEM_DIR, ITEM_FILE };

enum : iz { FILEMAX = (iz)64<<20 };  // larger files are skipped

// A directory to list or a file to tag, pushed on the work stack once.
struct Item {
    Item *next;
    Str   path;    // as constructed: the input to anonymous names
    i32   type;
    i32   lang;
    b32   header;
};

// A lock-free stack of items (Treiber). Each item is pushed exactly
// once and never reused, so a popped item cannot reappear at the top,
// and the stack has no ABA problem. Work is done when pending, items
// pushed but not yet finished, reaches zero.
struct Queue {
    alignas(64) Item *top;
    alignas(64) iz    pending;
};

struct Ctx {
    Os      *os;
    Options *opt;
    Queue    queue;
};

struct alignas(64) Worker {
    Ctx   *ctx;
    Arena  perm;     // items, paths, warnings, the line index
    Arena  text;     // tag lines, in one growing buffer
    Arena  scratch;  // contents and parse state of the current file
    Buf    tags;
    Buf    log;
    Line  *lines;
    iz     nlines;
    iz     nfiles;
    i64    nbytes;
    i64    busy;     // microseconds, for --_stats
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

static b32 globchar(u8 p, u8 c, b32 fold)
{
    return p=='?' || p==c || (fold && lowercase(p)==lowercase(c));
}

// Match a glob of literal bytes, '?', and '*', which also matches '/',
// like fnmatch() without flags. Optionally ignores ASCII case.
static b32 globmatch(Str pat, Str s, b32 fold)
{
    iz p    = 0;
    iz i    = 0;
    iz star = -1;
    iz mark = 0;
    while (i < s.len) {
        if (p<pat.len && pat[p]=='*') {
            star = p++;
            mark = i;
        } else if (p<pat.len && globchar(pat[p], s[i], fold)) {
            p++;
            i++;
        } else if (star >= 0) {
            p = star + 1;
            i = ++mark;
        } else {
            return 0;
        }
    }
    for (; p<pat.len && pat[p]=='*'; p++) {}
    return p == pat.len;
}

// Universal Ctags' built-in exclusions, tried against the base name
// and then the whole path, ignoring case on Windows.
static b32 excluded(Str path, b32 windows)
{
    static Str patterns[] = {
        "*.a", "*.class", "*.dll", "*.exe", "*.gcda", "*.gcno", "*.lib",
        "*.o", "*.obj", "*.pyc", "*.pyo", "*.so", "*~", ".*.swp",
        ".DS_Store", ".arch-ids", ".arch-inventory", ".bzr", ".bzrignore",
        ".cvsignore", ".deps", ".dvi", ".git", ".gitattributes",
        ".gitignore", ".hg", ".hgignore", ".svn", "BitKeeper", "CVS",
        "EIFGEN", "PENDING", "RCS", "RESYNC", "SCCS", "_darcs",
        "autom4te.cache", "{arch}",
    };
    Str base = basename(path, windows);
    for (iz i = 0; i < countof(patterns); i++) {
        if (globmatch(patterns[i], base, windows) ||
            globmatch(patterns[i], path, windows)) {
            return 1;
        }
    }
    return 0;
}

struct Lang {
    i32 lang;
    b32 header;
};

static b32 anyfold(Str s, Str *set, iz len)
{
    for (iz i = 0; i < len; i++) {
        if (equalsfold(s, set[i])) {
            return 1;
        }
    }
    return 0;
}

// Choose a language by the file name's extension, ignoring case, after
// stripping a ".in" suffix. Unlike Universal Ctags on Windows, ".C" is
// C++, and ".inl" and ".hp" count as headers.
static Lang detectlang(Str path, b32 windows)
{
    static Str c[]   = {"c"};
    static Str cxx[] = {
        "c++", "cc", "cp", "cpp", "cxx", "h", "h++", "hh", "hp", "hpp",
        "hxx", "inl",
    };
    static Str ftn[] = {
        "f", "for", "ftn", "f77", "f90", "f95", "f03", "f08", "f15",
    };
    static Str hdr[] = {
        "h", "hh", "hpp", "hxx", "h++", "inc", "def", "inl", "hp",
    };

    Lang r    = {};
    Str  base = basename(path, windows);
    for (i32 tries = 0; tries < 2; tries++) {
        iz dot = base.len - 1;
        for (; dot>=0 && base[dot]!='.'; dot--) {}
        if (dot < 0) {
            break;
        }
        Str ext = cuthead(base, dot+1);
        if (!tries && equalsfold(ext, "in")) {
            base = takehead(base, dot);
            continue;
        }
        if (ext == "C") {
            r.lang = LANG_CXX;
        } else if (anyfold(ext, c, countof(c))) {
            r.lang = LANG_C;
        } else if (anyfold(ext, cxx, countof(cxx))) {
            r.lang = LANG_CXX;
        } else if (anyfold(ext, ftn, countof(ftn))) {
            r.lang = LANG_FORTRAN;
        }
        r.header = r.lang && anyfold(ext, hdr, countof(hdr));
        break;
    }
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

static Str errstr(i32 err)
{
    switch (err) {
    case OSERR_NOENT: return "No such file or directory";
    case OSERR_ACCES: return "Permission denied";
    case OSERR_EXIST: return "File exists";
    case OSERR_ISDIR: return "Is a directory";
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
    for (Dirent *d = l.head; d; d = d->next) {
        byte *mark = w->perm.beg;
        Str   path = joinpath(&w->perm, it->path, d->name, windows);
        if (excluded(path, windows)) {
            w->perm.beg = mark;
            continue;
        }
        if (d->isdir) {
            Item *n = newitem(&w->perm, path, ITEM_DIR, {});
            n->next = dirs;
            dirs    = n;
            if (!dlast) dlast = n;
        } else {
            Lang lang = detectlang(path, windows);
            if (!lang.lang) {
                w->perm.beg = mark;
                continue;
            }
            Item *n = newitem(&w->perm, path, ITEM_FILE, lang);
            n->next = files;
            files   = n;
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
    if (r.size > FILEMAX) {
        warn(&w->log, "skipping input file larger than 64 MiB:", it->path, 0);
        return;
    }

    // Zeros past the end allow word-at-a-time scanning
    u8 *pad = allocbytes(&tmp, 8);
    assert(pad == r.data.data+r.data.len);
    __builtin_memset(pad, 0, 8);

    Source src = {};
    src.path     = it->path;
    src.outpath  = outpath(&tmp, it->path, c->opt->windows);
    src.text     = r.data;
    src.anonhash = anonhash(it->path);
    src.lang     = it->lang;
    src.header   = it->header;
    if (startswith(src.text, "\xef\xbb\xbf")) {
        src.text = cuthead(src.text, 3);
    }

    File f = {};
    f.src    = &src;
    f.kinds  = &c->opt->kinds;
    f.extras = c->opt->extras;
    f.out    = &w->tags;
    f.mark   = w->tags.len;
    f.nofast = c->opt->nofast;
    parsefile(&f, tmp);

    w->nlines += f.ntags;
    w->nfiles++;
    w->nbytes += r.data.len;
    w->scratch.end = tmp.end>w->scratch.end ? tmp.end : w->scratch.end;
}

static void work(Worker *w)
{
    Ctx  *c     = w->ctx;
    Queue *q    = &c->queue;
    i64   start = c->opt->stats ? os_clock(c->os) : 0;
    for (i32 attempt = 0;;) {
        Item *it = popitem(q);
        if (!it) {
            if (!__atomic_load_n(&q->pending, __ATOMIC_ACQUIRE)) {
                break;
            }
            os_backoff(c->os, attempt++);
            continue;
        }
        attempt = 0;
        if (it->type == ITEM_DIR) {
            visitdir(w, it);
        } else {
            visitfile(w, it);
        }
        __atomic_fetch_sub(&q->pending, 1, __ATOMIC_RELEASE);
    }
    if (c->opt->stats) {
        w->busy = os_clock(c->os) - start;
    }
    sortlines(w);
}

// A platform for tests: an in-memory file system, captured output, and
// threads that run one after another, or with a seed, take turns at
// random
// This is free and unencumbered software released into the public domain.

struct VNode {
    VNode  *next;   // sibling, in creation order, the listing order
    VNode  *child;
    VNode **tail;   // where the next child goes
    Str     name;
    Str     data;
    b32     isdir;
    b32     islink;  // a link to a file, which data stands for
    i32     err;     // reading it fails so: OSERR_*
};

struct Os {
    Arena *a;        // for the file system and captured output
    VNode  root;     // the current directory
    Str    input;    // standard input
    Buf    out;      // fd 1
    Buf    err;      // fd 2
    Buf    file;     // fd 3, until closed
    Str    created;  // fd 3's path
    u64    seed;     // nonzero: workers take turns at random
    i32    ncpu;
    i32    nworked;  // workers that tagged a file, in the last run
    b32    reverse;  // run workers last to first
    b32    windows;
};

static Os *newos(Arena *a)
{
    Os *os = alloc<Os>(a);
    os->a          = a;
    os->root.isdir = 1;
    os->root.tail  = &os->root.child;
    os->ncpu       = 1;
    os->out        = Buf(a, 1<<12);
    os->err        = Buf(a, 1<<12);
    return os;
}

static b32 vfssep(Os *os, u8 c)
{
    return c=='/' || (os->windows && c=='\\');
}

// Find a node, or with an arena, make it and any missing directories.
static VNode *vfsfind(Os *os, Str path, Arena *a)
{
    VNode *n = &os->root;
    for (iz i = 0; i < path.len;) {
        iz end = i;
        for (; end<path.len && !vfssep(os, path[end]); end++) {}
        Str part = span(path.data+i, path.data+end);
        i = end + 1;
        if (!part.len || part==".") {
            continue;
        }
        if (!n->isdir) {
            return 0;
        }
        VNode *c = n->child;
        for (; c && c->name!=part; c = c->next) {}
        if (!c) {
            if (!a) {
                return 0;
            }
            c = alloc<VNode>(a);
            c->name  = clone(a, part);
            c->isdir = end < path.len;
            c->tail  = &c->child;
            *n->tail = c;
            n->tail  = &c->next;
        }
        n = c;
    }
    return n;
}

// Add a file to the file system, with its directories.
static void vfsfile(Os *os, Str path, Str data)
{
    VNode *n = vfsfind(os, path, os->a);
    n->data = clone(os->a, data);
}

// Add a link to a file, standing for the file with its contents.
static void vfslink(Os *os, Str path, Str data)
{
    vfsfile(os, path, data);
    vfsfind(os, path, 0)->islink = 1;
}

// Add a file that can be examined but not read.
static void vfsunreadable(Os *os, Str path)
{
    vfsfile(os, path, "");
    vfsfind(os, path, 0)->err = OSERR_ACCES;
}

static void vfsdir(Os *os, Str path)
{
    VNode *n = vfsfind(os, path, os->a);
    n->isdir = 1;
}

static void vfsunlink(Os *os, Str path)
{
    VNode *parent = &os->root;
    iz     cut    = path.len;
    for (; cut>0 && !vfssep(os, path[cut-1]); cut--) {}
    if (cut) {
        parent = vfsfind(os, takehead(path, cut-1), 0);
    }
    if (!parent) {
        return;
    }
    Str name = cuthead(path, cut);
    for (VNode **p = &parent->child; *p; p = &(*p)->next) {
        if ((*p)->name == name) {
            if (parent->tail == &(*p)->next) {
                parent->tail = p;
            }
            *p = (*p)->next;
            return;
        }
    }
}

static Stat os_stat(Os *os, Arena, Str path)
{
    Stat   r = {};
    VNode *n = vfsfind(os, path, 0);
    if (!n) {
        r.err = OSERR_NOENT;
    } else if (n->isdir) {
        r.type = STAT_DIR;
    } else {
        r.type   = STAT_FILE;
        r.size   = n->data.len;
        r.islink = n->islink;
    }
    return r;
}

static Listing os_list(Os *os, Arena *a, Str path)
{
    Listing r = {};
    VNode  *n = vfsfind(os, path, 0);
    if (!n || !n->isdir) {
        r.err = OSERR_NOENT;
        return r;
    }
    Dirent **tail = &r.head;
    for (VNode *c = n->child; c; c = c->next) {
        Dirent *d = alloc<Dirent>(a);
        d->name   = clone(a, c->name);
        d->size   = c->data.len;
        d->isdir  = c->isdir;
        d->islink = c->islink;
        *tail = d;
        tail  = &d->next;
    }
    return r;
}

static Read os_read(Os *os, Arena *a, Str path, iz max)
{
    Read   r = {};
    VNode *n = vfsfind(os, path, 0);
    if (!n) {
        r.err = OSERR_NOENT;
        return r;
    } else if (n->isdir) {
        r.err = OSERR_ISDIR;
        return r;
    } else if (n->err) {
        r.err = n->err;
        return r;
    }
    r.size = n->data.len;
    iz len = r.size<max ? (iz)r.size : max;
    r.data.data = allocbytes(a, len);
    r.data.len  = len;
    copybytes(r.data.data, n->data.data, len);
    return r;
}

static Str os_stdin(Os *os, Arena *a)
{
    return clone(a, os->input);
}

static i32 os_create(Os *os, Arena, Str path)
{
    if (vfsfind(os, path, 0)) {
        return OSERR_EXIST;
    }
    vfsfile(os, path, "");
    os->created = clone(os->a, path);
    os->file    = Buf(os->a, 1<<12);
    return 0;
}

static b32 os_write(Os *os, i32 fd, Str s)
{
    Buf *b = fd==1 ? &os->out : fd==2 ? &os->err : &os->file;
    print(b, s);
    return 1;
}

static b32 os_close(Os *os, i32 fd)
{
    assert(fd == 3);
    vfsfind(os, os->created, 0)->data = finish(&os->file);
    return 1;
}

static i32 os_rename(Os *os, Arena, Str src, Str dst)
{
    VNode *s = vfsfind(os, src, 0);
    if (!s) {
        return OSERR_NOENT;
    }
    Str data = s->data;
    vfsunlink(os, src);
    vfsfind(os, dst, os->a)->data = data;
    return 0;
}

static void os_delete(Os *os, Arena, Str path)
{
    vfsunlink(os, path);
}

static b32 os_windows(Os *os)
{
    return os->windows;
}

static i32 os_ncpu(Os *os)
{
    return os->ncpu;
}

// Run the workers one after another. With a seed, the work loop instead
// goes a step at a time, each step by a live worker chosen at random,
// which interleaves the items as threads might.
static void os_parallel(Os *os, Worker *ws, i32 n, void (*fn)(Worker *))
{
    if (os->seed && fn==work) {
        Worker *live[32];
        i32     nlive = n;
        u64     rng   = os->seed;
        assert(n <= countof(live));
        for (i32 i = 0; i < n; i++) {
            live[i] = ws + i;
        }
        while (nlive) {
            rng = rng*0x3243f6a8885a308d + 1;
            i32 i = (i32)((rng>>32) * (u64)nlive >> 32);
            i32 r = workstep(live[i]);
            assert(r != STEP_WAIT);  // one at a time, none holds an item
            if (r == STEP_DONE) {
                live[i] = live[--nlive];
            }
        }
        for (i32 i = 0; i < n; i++) {
            workend(ws + i);
        }
    } else {
        for (i32 i = 0; i < n; i++) {
            fn(ws + (os->reverse ? n-1-i : i));
        }
    }

    os->nworked = 0;
    for (i32 i = 0; i < n; i++) {
        os->nworked += ws[i].nfiles > 0;
    }
}

static void os_backoff(Os *, i32)
{
    assert(0);  // run one at a time, a worker never waits
}

static i64 os_clock(Os *)
{
    return 0;
}

[[noreturn]] static void os_oom()
{
    __builtin_trap();
}

static b32 os_commit(byte *, iz)
{
    return 1;
}

// ctags: fast tags for C, C++, and Fortran, compatible with Universal Ctags
//
// Unity build: a platform layer (main_win32.cpp, main_posix.cpp,
// main_test.cpp, main_fuzz.cpp, at the repository's root) includes this
// file and implements the os_* API. Build from the root:
//
// w64dk: $ c++ -std=c++20 -O2 -nostartfiles -static-libgcc -Wl,--stack,0x800000 -s -o ctags.exe main_win32.cpp
// unix:  $ c++ -std=c++20 -O2 -pthread -o ctags main_posix.cpp
// tests: $ c++ -std=c++20 -g3 -fsanitize=undefined -fsanitize-trap -o ctags_test main_test.cpp
//
// Output matches Universal Ctags 6.2.1 (with --fields=+n) where it is
// correct, and departs from it where it is not. docs/spec.md lists the
// rules and the departures.
//
// This is free and unencumbered software released into the public domain.
#include "base.cpp"

static Str version = "0.1";


// Platform API

struct Os;
struct Worker;

struct Dirent {
    Dirent *next;   // in the order the system listed them
    Str     name;
    i64     size;
    b32     isdir;
};

enum : i32 { STAT_NONE, STAT_FILE, STAT_DIR, STAT_OTHER };

enum : i32 {
    OSERR_NOENT = 1,
    OSERR_ACCES,
    OSERR_EXIST,
    OSERR_ISDIR,
    OSERR_IO,
};

struct Stat    { i32 type; i64 size; i32 err; };
struct Listing { Dirent *head; i32 err; };      // !err && !head: empty
struct Read    { Str data; i64 size; i32 err; };

// Classify a path, following links.
static Stat    os_stat(Os *, Arena, Str path);
// List a directory in system order, without "." and "..", devices, or
// links and junctions to directories. Links to files are listed.
static Listing os_list(Os *, Arena *, Str path);
// Read at most max bytes of a regular file into the arena, not zeroed.
// The size is the whole file's: size > max means the data was cut.
static Read    os_read(Os *, Arena *, Str path, iz max);
// Create a new file for writing as fd 3, or fail with OSERR_EXIST.
static i32     os_create(Os *, Arena, Str path);  // 0 or OSERR_*
// Write all of s to fd 1, fd 2 (console-aware), or the created fd 3.
static b32     os_write(Os *, i32 fd, Str s);
// Close fd 3, returning 0 if any write was lost.
static b32     os_close(Os *, i32 fd);
// Atomically replace dst with src, retrying while dst is held open.
static i32     os_rename(Os *, Arena, Str src, Str dst);  // 0 or OSERR_*
// Delete a file, ignoring failure.
static void    os_delete(Os *, Arena, Str path);
// 1 if '\' and ':' separate paths and names match regardless of case.
static b32     os_windows(Os *);
// Logical processors, at least 1.
static i32     os_ncpu(Os *);
// Run fn(ws+i) for i in [0, n): ws+0 on the caller, the rest on new
// threads, or on the caller if a thread cannot start. Returns when all
// have returned.
static void    os_parallel(Os *, Worker *ws, i32 n, void (*fn)(Worker *));
// Wait for other threads: pause, then yield, then sleep.
static void    os_backoff(Os *, i32 attempt);
// Monotonic microseconds, only for measuring intervals.
static i64     os_clock(Os *);
// Also, without context since base.cpp calls them: os_oom(), os_commit().


// Helpers

static b32 equalsfold(Str a, Str b)
{
    if (a.len != b.len) {
        return 0;
    }
    for (iz i = 0; i < a.len; i++) {
        if (lowercase(a[i]) != lowercase(b[i])) {
            return 0;
        }
    }
    return 1;
}

static b32 strless(Str *a, Str *b)
{
    return compare(*a, *b) < 0;
}

// Split len bytes off the front of an arena as an arena of its own.
static Arena carve(Arena *a, iz len)
{
    Arena r = {};
    r.beg = a->beg;
    if (a->lim) {
        assert(len <= a->lim - a->beg);
        a->beg += len;
        r.lim = a->beg;
        r.end = a->end<a->beg ? a->end : a->beg;
        r.end = r.end>r.beg ? r.end : r.beg;
        a->end = a->end>a->beg ? a->end : a->beg;
    } else {
        assert(len <= a->end - a->beg);
        a->beg += len;
        r.end = a->beg;
    }
    return r;
}


#include "kinds.cpp"
#include "options.cpp"
#include "tags.cpp"


// Parsing

#include "cxx.cpp"
#include "fortran.cpp"

// Tag one file. Each parser makes a second pass with different rules
// when the first fails, discarding the first pass's tags but keeping
// its count of anonymous names.
static void parsefile(File *f, Arena scratch)
{
    b32 (*parse)(File *, b32, Arena) = 0;
    switch (f->src->lang) {
    case LANG_C:
    case LANG_CXX:     parse = cxxtags; break;
    case LANG_FORTRAN: parse = ftntags; break;
    }
    if (parse && !parse(f, 0, scratch)) {
        f->out->len = f->mark;
        f->ntags    = 0;
        parse(f, 1, scratch);
    }
}


#include "walk.cpp"
#include "output.cpp"


// Main

static iz arenasize(Arena a)
{
    return (a.lim ? a.lim : a.end) - a.beg;
}

static i32 ctags_main(Os *os, Arena mem, Str *args, i32 nargs)
{
    // Global memory for the main thread, the rest split between workers
    iz    gcap = arenasize(mem)/4;
    gcap = gcap<(iz)64<<20 ? gcap : (iz)64<<20;
    Arena g    = carve(&mem, gcap);
    Buf   log(&g, 1<<12);

    Options opt = {};
    opt.windows = os_windows(os);
    switch (parseargs(&opt, args, nargs, &g, &log)) {
    case ARGS_EXIT:
        return os_write(os, 1, finish(&log)) ? 0 : 1;
    case ARGS_FATAL:
        os_write(os, 2, finish(&log));
        return 1;
    }
    if (opt.output!="-" && !checkoutput(os, opt.output, g, &log)) {
        os_write(os, 2, finish(&log));
        return 1;
    }

    Ctx ctx = {};
    ctx.os  = os;
    ctx.opt = &opt;

    // The arguments are the roots of the work
    iz    nroots = 0;
    b32   anydir = 0;
    Item *first  = 0;
    Item *last   = 0;
    for (iz i = 0; i < opt.nfiles; i++) {
        Str path = opt.files[i];
        if (excluded(path, opt.windows)) {
            continue;
        }
        Stat  s  = os_stat(os, g, path);
        Item *it = 0;
        if (s.type == STAT_NONE) {
            warn(&log, "cannot open input file", path, s.err);
        } else if (s.type==STAT_DIR && opt.recurse) {
            it = newitem(&g, path, ITEM_DIR, {});
            anydir = 1;
        } else if (s.type == STAT_FILE) {
            Lang lang = detectlang(path, opt.windows);
            if (lang.lang) {
                it = newitem(&g, path, ITEM_FILE, lang);
            }
        }
        if (it) {
            if (last) {
                last->next = it;
            } else {
                first = it;
            }
            last = it;
            nroots++;
        }
    }
    if (first) {
        ctx.queue.pending = nroots;
        pushitems(&ctx.queue, first, last);
    }

    // Workers, as many as asked or as there are processors, but no more
    // than there are files if there are no directories to walk, and
    // only as many as can each have a generous share of memory
    i32 n = opt.jobs ? opt.jobs : os_ncpu(os);
    n = n<1 ? 1 : n>32 ? 32 : n;
    if (!anydir) {
        n = nroots<n ? (i32)(nroots>1 ? nroots : 1) : n;
    }
    iz slotmin = !mem.lim ? (iz)1<<20 : sizeof(void *)==8 ? (iz)1<<30 : (iz)128<<20;
    iz slot    = arenasize(mem) / n;
    for (; n>1 && slot<slotmin; n--, slot = arenasize(mem)/n) {}

    Worker *ws = alloc<Worker>(&g, n);
    for (i32 i = 0; i < n; i++) {
        Worker *w = ws + i;
        Arena   s = carve(&mem, slot);
        w->ctx     = &ctx;
        w->id      = i;
        w->text    = carve(&s, slot/2);
        w->perm    = carve(&s, slot/4);
        w->scratch = s;
        w->tags    = Buf(&w->text, 1<<16);
        w->log     = Buf(&w->perm, 1<<8);
    }

    i64 t0 = opt.stats ? os_clock(os) : 0;
    os_parallel(os, ws, n, work);
    i64 t1 = opt.stats ? os_clock(os) : 0;

    // Warnings in a stable order, whatever the scheduling
    {
        Arena      tmp   = g;
        Slice<Str> warns = {};
        for (i32 i = 0; i < n; i++) {
            for (Str s = finish(&ws[i].log); s.len;) {
                Cut c = cut(s, '\n');
                warns = push(&tmp, warns, c.head);
                s = c.tail;
            }
        }
        sort(warns.data, warns.len, strless, tmp);
        for (iz i = 0; i < warns.len; i++) {
            print(&log, warns[i]);
            putbyte(&log, '\n');
        }
    }

    iz  ntags = 0;
    b32 ok    = writetags(os, &opt, ws, n, &ntags, g, &log);
    i64 t2    = opt.stats ? os_clock(os) : 0;

    if (opt.stats) {
        iz  nfiles = 0;
        i64 nbytes = 0;
        i64 busymin = 0x7fffffffffffffff;
        i64 busymax = 0;
        for (i32 i = 0; i < n; i++) {
            nfiles += ws[i].nfiles;
            nbytes += ws[i].nbytes;
            busymin = ws[i].busy<busymin ? ws[i].busy : busymin;
            busymax = ws[i].busy>busymax ? ws[i].busy : busymax;
        }
        print(&log, "ctags: ");
        print(&log, (i64)n);
        print(&log, " jobs, ");
        print(&log, (i64)nfiles);
        print(&log, " files, ");
        print(&log, nbytes);
        print(&log, " bytes, ");
        print(&log, (i64)ntags);
        print(&log, " tags; tag ");
        print(&log, (t1 - t0)/1000);
        print(&log, " ms (busy ");
        print(&log, busymin/1000);
        print(&log, "-");
        print(&log, busymax/1000);
        print(&log, " ms), write ");
        print(&log, (t2 - t1)/1000);
        print(&log, " ms\n");
    }

    if (log.len) {
        os_write(os, 2, finish(&log));
    }
    return ok ? 0 : 1;
}

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

static Str version = "0.1.0";


// Platform API

struct Os;
struct Worker;

struct Dirent {
    Dirent *next;    // in the order the system listed them
    Str     name;
    i64     size;
    i64     mtime;   // last modified, seconds since 1970 UTC
    b32     isdir;
    b32     islink;  // a symbolic link (to a file)
};

enum : i32 { STAT_NONE, STAT_FILE, STAT_DIR, STAT_OTHER };

enum : i32 {
    OSERR_NOENT = 1,
    OSERR_ACCES,
    OSERR_EXIST,
    OSERR_ISDIR,
    OSERR_IO,
    OSERR_INVAL,
};

struct Stat    { i32 type; i64 size; i64 mtime; i32 err; b32 islink; };
struct Listing { Dirent *head; i32 err; };      // !err && !head: empty
struct Read    { Str data; i64 size; i32 err; };

// Classify a path, following links, and note whether it is a link
// itself, which a dangling link also is. A file's size and mtime are
// those Dirent would list.
static Stat    os_stat(Os *, Arena, Str path);
// List a directory in system order, without "." and "..", devices, or
// links and junctions to directories. Links to files are listed.
static Listing os_list(Os *, Arena *, Str path);
// Read at most max bytes of a regular file into the arena, not zeroed.
// The size is the whole file's: size > max means the data was cut. A
// device or pipe is read to its end.
static Read    os_read(Os *, Arena *, Str path, iz max);
// Read standard input to its end into the arena: empty if it is closed
// or unreadable, and cut short by an error.
static Str     os_stdin(Os *, Arena *);
// The current directory, absolute, as the system writes it, or null
// data if it cannot be found.
static Str     os_cwd(Os *, Arena *);
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

// The bytes an arena may yet hand out, committed or not.
static iz arenasize(Arena a)
{
    return (a.lim ? a.lim : a.end) - a.beg;
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
#include "glob.cpp"
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

static i32 ctags_main(Os *os, Arena mem, Str *args, i32 nargs)
{
    // Global memory for the main thread, the rest split between workers.
    // A reservation commits only what is used, so on 64-bit it may be
    // generous, as long lists of files need.
    iz    gmax = mem.lim ? (iz)1<<(sizeof(void *)==8 ? 32 : 26) : (iz)64<<20;
    iz    gcap = arenasize(mem)/4;
    gcap = gcap<gmax ? gcap : gmax;
    Arena g    = carve(&mem, gcap);
    Buf   log(&g, 1<<12);

    Options opt = {};
    opt.windows = os_windows(os);
    switch (parseargs(&opt, os, args, nargs, &g, &log)) {
    case ARGS_EXIT:
        if (log.len) {
            os_write(os, 2, finish(&log));
        }
        return os_write(os, 1, opt.print) ? 0 : 1;
    case ARGS_FATAL:
        os_write(os, 2, finish(&log));
        return 1;
    }
    if (opt.output!="-" && !checkoutput(os, opt.output, g, &log)) {
        os_write(os, 2, finish(&log));
        return 1;
    }

    // A tags file appended to may be larger than g can hold, so it is
    // read from the memory the workers would otherwise share
    Old old = {};
    if (opt.append && !loadold(&old, os, &opt, &mem, &log)) {
        os_write(os, 2, finish(&log));
        return 1;
    }

    // Universal Ctags opens a TAGS file before tagging, so that the walk
    // finds it, of no language, and gives it a section (OPT-21)
    b32 etags = opt.writer == WRITER_ETAGS;
    if (etags && opt.output!="-" &&
        os_stat(os, g, opt.output).type==STAT_NONE &&
        !os_create(os, g, opt.output)) {
        os_close(os, 3);
    }

    Ctx ctx = {};
    ctx.os      = os;
    ctx.opt     = &opt;
    ctx.ordered = opt.sort==SORT_NO || etags;
    ctx.chunked = ctx.ordered || old.text.data;
    if (opt.tagrelative != TREL_NO) {
        // Paths are made relative to the tags file's directory (OPT-18)
        ctx.cwd = os_cwd(os, &g);
        Str dir = outdir(opt.output, opt.windows);
        if (!ctx.cwd.data) {
            print(&log, "ctags: failed in getcwd()\n");
            os_write(os, 2, finish(&log));
            return 1;
        } else if (driverelative(dir, opt.windows)) {
            driveerror(&log, dir);
            os_write(os, 2, finish(&log));
            return 1;
        }
        ctx.base = abspath(&g, ctx.cwd, dir, opt.windows);
    }

    // The arguments are the roots of the work, or with none, "." itself,
    // which is not checked for exclusion (DISC-1). Their order keys are
    // their indexes, the -L list's entries following the arguments.
    b32   dot     = !opt.files.len && !opt.listfile.data;
    iz    ninputs = dot ? 1 : opt.files.len;
    Item *roots   = alloc<Item>(&g, ninputs);
    iz    nroots  = 0;
    for (iz i = 0; i < ninputs; i++) {
        Str path = dot ? Str(".") : opt.files[i];
        Str base = basename(path, opt.windows);
        if (dot || !excluded(&opt, path, base)) {
            Lang  lang = detectlang(&opt, path);
            Item *it   = roots + nroots++;
            it->path   = path;
            it->type   = ITEM_ARG;
            it->lang   = lang.lang;
            it->header = lang.header;
            it->key    = ctx.ordered ? childkey(&g, {}, (u32)i) : Str{};
        }
    }

    // Workers, as many as asked or as there are processors, but no more
    // than there are files if there are no directories to walk, and
    // only as many as can each have a generous share of memory. Roots
    // are examined first: here if they are few, else by the workers,
    // with scratch from the memory they are yet to share.
    i32 n = opt.jobs ? opt.jobs : os_ncpu(os);
    n = n<1 ? 1 : n>32 ? 32 : n;
    if (nroots > n) {
        Arena   tmp  = g;
        Arena   rest = mem;
        Worker *ws   = alloc<Worker>(&tmp, n);
        for (i32 i = 0; i < n; i++) {
            ws[i].ctx     = &ctx;
            ws[i].scratch = carve(&rest, arenasize(mem)/n);
        }
        for (iz i = 1; i < nroots; i++) {
            roots[i-1].next = roots + i;
        }
        pushitems(ctx.queue.stacks+STACK_FILES, roots, roots+nroots-1);
        os_parallel(os, ws, n, examineall);
    } else {
        for (iz i = 0; i < nroots; i++) {
            examine(&ctx, roots+i, g);
        }
    }
    Batch b      = {};
    iz    count  = 0;
    b32   anydir = 0;
    for (iz i = 0; i < nroots; i++) {
        Item *it = roots + i;
        if (it->type != ITEM_NONE) {
            batchadd(&b, it);
            count++;
            anydir |= it->type == ITEM_DIR;
        }
    }
    if (!anydir) {
        n = count<n ? (i32)(count>1 ? count : 1) : n;
    }
    ctx.queue.pending = count;
    pushbatch(&ctx.queue, &b);
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

    // Arguments that do not exist, in their order
    for (iz i = 0; i < nroots; i++) {
        if (roots[i].err) {
            warn(&log, "cannot open input file", roots[i].path, roots[i].err);
        }
    }

    // Warnings in a stable order, whatever the scheduling. The log grows
    // to hold them first, as it would otherwise grow over the list.
    {
        iz total = 0;
        for (i32 i = 0; i < n; i++) {
            total += ws[i].log.len + 1;  // a newline, if the last lacks one
        }
        reserve(&log, total);
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

    // A file that could not be made absolute would have stopped
    // Universal Ctags: the least of them stops the run (OPT-18)
    Str nodrive = {};
    for (i32 i = 0; i < n; i++) {
        Str f = ws[i].fatal;
        if (f.data && (!nodrive.data || compare(f, nodrive)<0)) {
            nodrive = f;
        }
    }
    if (nodrive.data) {
        driveerror(&log, nodrive);
        os_write(os, 2, finish(&log));
        return 1;
    }

    // Room for a message about the tags file, which names a temporary
    // file in the scratch arena that the log would otherwise grow over
    reserve(&log, opt.output.len + 128);
    iz  ntags = 0;
    b32 ok    = writetags(os, &opt, ws, n, &old, &ntags, g, &log);
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

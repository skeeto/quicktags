// POSIX platform layer (Linux, macOS, BSD)
// $ c++ -std=c++20 -O2 -pthread -o ctags main_posix.cpp
// This is free and unencumbered software released into the public domain.
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "src/ctags.cpp"

struct Os {
    i32 out;  // the file created as fd 3
};

static char *cstr(Arena *a, Str s)
{
    char *r = alloc<char>(a, s.len+1);
    copybytes(r, s.data, s.len);
    return r;
}

static i32 oserr(i32 err)
{
    switch (err) {
    case ENOENT:
    case ENOTDIR:
    case ENAMETOOLONG: return OSERR_NOENT;
    case EACCES:
    case EPERM:        return OSERR_ACCES;
    case EEXIST:       return OSERR_EXIST;
    case EISDIR:       return OSERR_ISDIR;
    }
    return OSERR_IO;
}

static Stat os_stat(Os *, Arena scratch, Str path)
{
    Stat        r  = {};
    struct stat st = {};
    if (stat(cstr(&scratch, path), &st)) {
        r.err = oserr(errno);
    } else if (S_ISDIR(st.st_mode)) {
        r.type = STAT_DIR;
    } else if (S_ISREG(st.st_mode)) {
        r.type = STAT_FILE;
        r.size = (i64)st.st_size;
    } else {
        r.type = STAT_OTHER;
    }
    return r;
}

static Listing os_list(Os *, Arena *a, Str path)
{
    Listing r   = {};
    Arena   tmp = *a;
    DIR    *dir = opendir(cstr(&tmp, path));
    if (!dir) {
        r.err = oserr(errno);
        return r;
    }

    Dirent **tail = &r.head;
    for (struct dirent *e; (errno = 0, e = readdir(dir));) {
        Str name = {(u8 *)e->d_name, 0};
        for (; name.data[name.len]; name.len++) {}
        if (name=="." || name=="..") {
            continue;
        }

        // Links to directories might make cycles, so are skipped
        Arena       t    = *a;  // past the entries allocated so far
        Str         full = concat(&t, concat(&t, path, "/"), name);
        char       *cful = cstr(&t, full);
        struct stat st   = {};
        if (lstat(cful, &st)) {
            continue;
        }
        if (S_ISLNK(st.st_mode)) {
            if (stat(cful, &st) || S_ISDIR(st.st_mode)) {
                continue;
            }
        }
        if (!S_ISDIR(st.st_mode) && !S_ISREG(st.st_mode)) {
            continue;
        }
        Dirent *d = alloc<Dirent>(a);
        d->name  = clone(a, name);
        d->size  = (i64)st.st_size;
        d->isdir = S_ISDIR(st.st_mode);
        *tail = d;
        tail  = &d->next;
    }
    if (errno) {
        r.head = 0;
        r.err  = oserr(errno);
    }
    closedir(dir);
    return r;
}

static Read os_read(Os *, Arena *a, Str path, iz max)
{
    Read  r   = {};
    Arena tmp = *a;
    i32   fd  = open(cstr(&tmp, path), O_RDONLY);
    if (fd < 0) {
        r.err = oserr(errno);
        return r;
    }
    struct stat st = {};
    if (fstat(fd, &st) || !S_ISREG(st.st_mode)) {
        close(fd);
        r.err = S_ISDIR(st.st_mode) ? OSERR_ISDIR : OSERR_IO;
        return r;
    }

    r.size = (i64)st.st_size;
    iz len = r.size<max ? (iz)r.size : max;
    r.data.data = allocbytes(a, len);
    while (r.data.len < len) {
        iz n = read(fd, r.data.data+r.data.len, (uz)(len - r.data.len));
        if (n < 0) {
            if (errno == EINTR) continue;
            r.err = oserr(errno);
            break;
        } else if (!n) {
            break;  // shrank while reading
        }
        r.data.len += n;
    }
    close(fd);
    return r;
}

static b32 writeall(i32 fd, Str s)
{
    for (iz off = 0; off < s.len;) {
        iz n = write(fd, s.data+off, (uz)(s.len-off));
        if (n < 0) {
            if (errno == EINTR) continue;
            return 0;
        }
        off += n;
    }
    return 1;
}

static i32 os_create(Os *os, Arena scratch, Str path)
{
    i32 fd = open(cstr(&scratch, path), O_WRONLY|O_CREAT|O_EXCL, 0666);
    if (fd < 0) {
        return oserr(errno);
    }
    os->out = fd;
    return 0;
}

static b32 os_write(Os *os, i32 fd, Str s)
{
    return writeall(fd==3 ? os->out : fd, s);
}

static b32 os_close(Os *os, i32 fd)
{
    assert(fd == 3);
    return !close(os->out);
}

static i32 os_rename(Os *, Arena scratch, Str src, Str dst)
{
    return rename(cstr(&scratch, src), cstr(&scratch, dst)) ? oserr(errno) : 0;
}

static void os_delete(Os *, Arena scratch, Str path)
{
    unlink(cstr(&scratch, path));
}

static b32 os_windows(Os *)
{
    return 0;
}

static i32 os_ncpu(Os *)
{
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n>1 ? (i32)(n<1024 ? n : 1024) : 1;
}

struct Thread {
    void  (*fn)(Worker *);
    Worker *w;
};

static void *threadentry(void *arg)
{
    Thread *t = (Thread *)arg;
    t->fn(t->w);
    return 0;
}

static void os_parallel(Os *, Worker *ws, i32 n, void (*fn)(Worker *))
{
    Thread    threads[32] = {};
    pthread_t handles[32] = {};
    b32       started[32] = {};
    assert(n>=1 && n<=countof(threads));

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, (uz)8<<20);
    for (i32 i = 1; i < n; i++) {
        threads[i].fn = fn;
        threads[i].w  = ws + i;
        started[i] = !pthread_create(handles+i, &attr, threadentry, threads+i);
    }
    pthread_attr_destroy(&attr);

    fn(ws);
    for (i32 i = 1; i < n; i++) {
        if (started[i]) {
            pthread_join(handles[i], 0);
        } else {
            fn(ws + i);
        }
    }
}

static void os_backoff(Os *, i32 attempt)
{
    if (attempt < 64) {
        #if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
        #endif
    } else if (attempt < 1024) {
        sched_yield();
    } else {
        struct timespec ts = {0, 1000000};
        nanosleep(&ts, 0);
    }
}

static i64 os_clock(Os *)
{
    struct timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (i64)ts.tv_sec*1000000 + ts.tv_nsec/1000;
}

[[noreturn]] static void os_oom()
{
    writeall(2, "ctags: out of memory\n");
    _exit(2);
}

static b32 os_commit(byte *, iz)
{
    return 1;  // the kernel commits pages as they are touched
}

int main(int argc, char **argv)
{
    // As much address space as the system will give, up to 64 GiB,
    // halving on refusal. Pages are only committed as used.
    iz    cap = (iz)1 << (sizeof(void *)==8 ? 36 : 30);
    byte *mem = 0;
    for (; cap >= (iz)1<<24; cap /= 2) {
        void *p = mmap(0, (uz)cap, PROT_READ|PROT_WRITE,
                       MAP_PRIVATE|MAP_ANON|MAP_NORESERVE, -1, 0);
        if (p != MAP_FAILED) {
            mem = (byte *)p;
            break;
        }
    }
    if (!mem) {
        os_oom();
    }
    Arena a = {mem, mem+cap, mem+cap};

    Str *args = alloc<Str>(&a, argc);
    for (i32 i = 0; i < argc; i++) {
        args[i].data = (u8 *)argv[i];
        for (; args[i].data[args[i].len]; args[i].len++) {}
    }

    Os os = {};
    return ctags_main(&os, a, args, argc);
}

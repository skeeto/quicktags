// Win32 platform layer: no CRT, no headers, and kernel32.dll the only
// import, all of it present in Windows XP
// $ c++ -std=c++20 -O2 -nostartfiles -static-libgcc -Wl,--stack,0x800000 -s -o ctags.exe main_win32.cpp
//
// Paths are UTF-8 (WTF-8) with either separator. Each is made absolute
// and extended-length (\\?\) before use, so MAX_PATH never applies.
//
// This is free and unencumbered software released into the public domain.
#include "src/ctags.cpp"
#include "src/wide.cpp"

enum : i32 {
    CREATE_NEW                    = 1,
    ERROR_ACCESS_DENIED           = 5,
    ERROR_ALREADY_EXISTS          = 183,
    ERROR_BROKEN_PIPE             = 109,
    ERROR_DIRECTORY               = 267,
    ERROR_FILE_EXISTS             = 80,
    ERROR_FILE_NOT_FOUND          = 2,
    ERROR_INVALID_NAME            = 123,
    ERROR_INVALID_PARAMETER       = 87,
    ERROR_NO_MORE_FILES           = 18,
    ERROR_PATH_NOT_FOUND          = 3,
    ERROR_SHARING_VIOLATION       = 32,
    FILE_ATTRIBUTE_DEVICE         = 0x40,
    FILE_ATTRIBUTE_DIRECTORY      = 0x10,
    FILE_ATTRIBUTE_NORMAL         = 0x80,
    FILE_ATTRIBUTE_REPARSE_POINT  = 0x400,
    FILE_SHARE_ALL                = 7,
    FILE_TYPE_CHAR                = 2,
    FILE_TYPE_DISK                = 1,
    FILE_TYPE_PIPE                = 3,
    FindExInfoBasic               = 1,
    FindExInfoStandard            = 0,
    FindExSearchNameMatch         = 0,
    GENERIC_READ                  = (i32)0x80000000,
    GENERIC_WRITE                 = 0x40000000,
    GetFileExInfoStandard         = 0,
    IO_REPARSE_TAG_NAME_SURROGATE = 0x20000000,  // symlinks, junctions
    MEM_COMMIT                    = 0x1000,
    MEM_RESERVE                   = 0x2000,
    MOVEFILE_REPLACE_EXISTING     = 1,
    OPEN_EXISTING                 = 3,
    PAGE_READWRITE                = 4,
    STACK_SIZE_PARAM_IS_A_RESERVATION = 0x10000,
    STD_ERROR_HANDLE              = -12,
    STD_INPUT_HANDLE              = -10,
};

enum : uz {
    INVALID_HANDLE_VALUE = (uz)-1,
};

struct Filetime {
    u32 lo;
    u32 hi;
};

struct FileAttributes {  // WIN32_FILE_ATTRIBUTE_DATA
    u32      attr;
    Filetime ctime;
    Filetime atime;
    Filetime mtime;
    u32      sizehi;
    u32      sizelo;
};
static_assert(sizeof(FileAttributes) == 36);

struct FindData {  // WIN32_FIND_DATAW
    u32      attr;
    Filetime ctime;
    Filetime atime;
    Filetime mtime;
    u32      sizehi;
    u32      sizelo;
    u32      reparsetag;
    u32      reserved;
    c16      name[260];
    c16      altname[14];
};
static_assert(sizeof(FindData) == 592);

struct SystemInfo {  // SYSTEM_INFO
    u32 arch;        // wProcessorArchitecture, wReserved
    u32 pagesize;
    uz  minaddr;
    uz  maxaddr;
    uz  activemask;
    i32 nproc;
    u32 proctype;
    u32 granularity;
    u32 level;       // wProcessorLevel, wProcessorRevision
};
static_assert(sizeof(SystemInfo) == (sizeof(uz)==8 ? 48 : 36));

#define W32(r, p) extern "C" __declspec(dllimport) r __stdcall p noexcept
W32(b32,    CloseHandle(uz));
W32(uz,     CreateFileW(c16 *, i32, i32, uz, i32, i32, uz));
W32(uz,     CreateThread(uz, iz, i32 (__stdcall *)(void *), void *, i32, i32 *));
W32(b32,    DeleteFileW(c16 *));
W32(void,   ExitProcess[[noreturn]](i32));
W32(b32,    FindClose(uz));
W32(uz,     FindFirstFileExW(c16 *, i32, FindData *, i32, uz, i32));
W32(b32,    FindNextFileW(uz, FindData *));
W32(c16 *,  GetCommandLineW());
W32(b32,    GetConsoleMode(uz, i32 *));
W32(i32,    GetCurrentDirectoryW(i32, c16 *));
W32(b32,    GetFileAttributesExW(c16 *, i32, FileAttributes *));
W32(b32,    GetFileSizeEx(uz, i64 *));
W32(i32,    GetFileType(uz));
W32(i32,    GetFullPathNameW(c16 *, i32, c16 *, c16 **));
W32(i32,    GetLastError());
W32(uz,     GetStdHandle(i32));
W32(void,   GetSystemInfo(SystemInfo *));
W32(b32,    MoveFileExW(c16 *, c16 *, i32));
W32(b32,    QueryPerformanceCounter(i64 *));
W32(b32,    QueryPerformanceFrequency(i64 *));
W32(b32,    ReadFile(uz, u8 *, i32, i32 *, uz));
W32(void,   Sleep(i32));
W32(b32,    SwitchToThread());
W32(byte *, VirtualAlloc(byte *, iz, i32, i32));
W32(i32,    WaitForSingleObject(uz, i32));
W32(b32,    WriteConsoleW(uz, c16 *, i32, i32 *, uz));
W32(b32,    WriteFile(uz, u8 *, i32, i32 *, uz));

// GCC emits calls to these even without a CRT. Defining them keeps
// msvcrt.dll out of the imports.
#if defined(__x86_64__) || defined(__i386__)
extern "C" void *memcpy(void *dst, void const *src, uz len)
{
    void *r = dst;
    __asm volatile ("rep movsb" : "+D"(dst), "+S"(src), "+c"(len) : : "memory");
    return r;
}

// Backward when dst starts inside src, which copying forward would
// overwrite before reading.
extern "C" void *memmove(void *dst, void const *src, uz len)
{
    void *r = dst;
    if ((uz)dst-(uz)src >= len) {
        __asm volatile ("rep movsb" : "+D"(dst), "+S"(src), "+c"(len) : : "memory");
    } else {
        dst = (u8 *)dst + len - 1;
        src = (u8 *)src + len - 1;
        __asm volatile (
            "std; rep movsb; cld"
            : "+D"(dst), "+S"(src), "+c"(len) : : "memory"
        );
    }
    return r;
}

extern "C" void *memset(void *dst, i32 c, uz len)
{
    void *r = dst;
    __asm volatile ("rep stosb" : "+D"(dst), "+c"(len) : "a"(c) : "memory");
    return r;
}

// By SSE2 where built for it, since sorting tags compares many lines
// that share long prefixes.
extern "C" void *memchr(void const *s, i32 c, uz len)
{
    u8 *beg = (u8 *)s;
    u8 *end = beg + len;
    u8 *r   = findbyte(SIMD_SSE2, beg, end, (u8)c);
    return r<end ? r : 0;
}

extern "C" i32 memcmp(void const *a, void const *b, uz len)
{
    u8 *x = (u8 *)a;
    u8 *y = (u8 *)b;
    iz  i = firstdiff(x, y, (iz)len);
    return i<(iz)len ? x[i]-y[i] : 0;
}
#endif

struct Os {
    uz  out;      // the file created as fd 3
    i32 oldfind;  // listings use the arguments before Windows 7
};

static i32 chunk(iz len)  // bound a single ReadFile/WriteFile
{
    i32 max = 1<<21;
    return len<max ? (i32)len : max;
}

static i32 oserr(i32 err)
{
    switch (err) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_NAME:
    case ERROR_DIRECTORY:
        return OSERR_NOENT;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
        return OSERR_ACCES;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS:
        return OSERR_EXIST;
    }
    return OSERR_IO;
}

// Absolute, extended-length, null-terminated form of a path. Falls back
// to the plain conversion if the system rejects the path, leaving the
// error for the caller's system call to report.
static Str16 widepath(Arena *a, Str path)
{
    Str16 rel = towide(a, path);
    c16  *buf = alloc<c16>(a, 0);
    iz    max = (a->end - (byte *)buf)/(iz)sizeof(c16) - 8;
    if (max < 0x8000+8 && a->lim) {
        grow(a, (0x8000+16)*(iz)sizeof(c16));
        max = (a->end - (byte *)buf)/(iz)sizeof(c16) - 8;
    }
    i32   cap = max<0x8000 ? (i32)max : 0x8000;  // longest possible path
    i32   len = cap>0 ? GetFullPathNameW(rel.data, cap, buf+6, 0) : 0;
    if (!len || len>=cap) {
        return rel;
    }
    Str16 r = extendpath(buf, len);
    a->beg = (byte *)(r.data + r.len + 1);  // keep the terminator
    return r;
}

static b32 writeall(uz h, Str s)
{
    for (iz off = 0; off < s.len;) {
        i32 len = chunk(s.len - off);
        if (!WriteFile(h, s.data+off, len, &len, 0) || !len) {
            return 0;
        }
        off += len;
    }
    return 1;
}

// The first entry of a listing, without short names where the system
// supports it (Windows 7). Earlier versions reject the argument, and from
// then on get theirs. Not FIND_FIRST_EX_LARGE_FETCH: its 64 KiB buffer
// comes from the process heap, and threads listing at once queue on the
// heap's lock, which doubled the time to walk llvm-project.
static uz findfirst(Os *os, c16 *pattern, FindData *fd)
{
    uz h = INVALID_HANDLE_VALUE;
    if (!__atomic_load_n(&os->oldfind, __ATOMIC_RELAXED)) {
        #if CTAGS_FORCE_XP
        h = INVALID_HANDLE_VALUE;
        b32 refused = 1;
        #else
        h = FindFirstFileExW(
            pattern, FindExInfoBasic, fd, FindExSearchNameMatch, 0, 0
        );
        b32 refused = h==INVALID_HANDLE_VALUE &&
                      GetLastError()==ERROR_INVALID_PARAMETER;
        #endif
        if (!refused) {
            return h;
        }
        __atomic_store_n(&os->oldfind, 1, __ATOMIC_RELAXED);
    }
    return FindFirstFileExW(
        pattern, FindExInfoStandard, fd, FindExSearchNameMatch, 0, 0
    );
}

// Whether a found entry is a symbolic link or a junction.
static b32 linked(FindData *fd)
{
    return fd->attr&FILE_ATTRIBUTE_REPARSE_POINT &&
           fd->reparsetag&IO_REPARSE_TAG_NAME_SURROGATE;
}

// A file time, in 100 ns ticks since 1601 UTC, as seconds since 1970,
// rounded down. On i686 the division is libgcc's, linked statically.
static i64 unixtime(Filetime t)
{
    u64 ticks = (u64)t.hi<<32 | t.lo;
    u64 epoch = 116444736000000000;
    u64 tick  = 10000000;
    return ticks>=epoch ? (i64)((ticks - epoch)/tick)
                        : -(i64)((epoch - ticks + tick - 1)/tick);
}

static Stat os_stat(Os *os, Arena scratch, Str path)
{
    Stat           r = {};
    FileAttributes a = {};
    c16           *w = widepath(&scratch, path).data;
    if (!GetFileAttributesExW(w, GetFileExInfoStandard, &a)) {
        r.err = oserr(GetLastError());
        return r;
    }
    if (a.attr & FILE_ATTRIBUTE_REPARSE_POINT) {
        // Only a search finds the kind of reparse point
        FindData fd = {};
        uz       h  = findfirst(os, w, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            r.islink = linked(&fd);
            FindClose(h);
        }
    }
    if (a.attr & FILE_ATTRIBUTE_DIRECTORY) {
        r.type = STAT_DIR;
    } else if (a.attr & FILE_ATTRIBUTE_DEVICE) {
        r.type = STAT_OTHER;
    } else {
        r.type  = STAT_FILE;
        r.size  = (i64)((u64)a.sizehi<<32 | a.sizelo);
        r.mtime = unixtime(a.mtime);
    }
    return r;
}

static Listing os_list(Os *os, Arena *a, Str path)
{
    Listing r = {};
    if (!path.len) {
        r.err = OSERR_NOENT;
        return r;
    }

    Arena tmp = *a;
    Str16 dir = widepath(&tmp, path);
    c16  *pat = alloc<c16>(&tmp, dir.len+3);
    iz    len = dir.len;
    copybytes(pat, dir.data, len*(iz)sizeof(c16));
    if (!len || (pat[len-1]!='\\' && pat[len-1]!='/')) {
        pat[len++] = '\\';
    }
    pat[len] = '*';

    // The listing goes where the pattern was, which is dead once the
    // search begins
    FindData fd = {};  // too big for the arena's churn, fine on the stack
    uz h = findfirst(os, pat, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        i32 err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND) {
            r.err = oserr(err);
        }
        return r;  // empty
    }

    Dirent **tail = &r.head;
    do {
        c16 *name = fd.name;
        if (name[0]=='.' && (!name[1] || (name[1]=='.' && !name[2]))) {
            continue;
        }
        if (fd.attr & FILE_ATTRIBUTE_DEVICE) {
            continue;
        }
        b32 isdir = !!(fd.attr & FILE_ATTRIBUTE_DIRECTORY);
        if (isdir && linked(&fd)) {
            continue;  // a link or junction, which might make a cycle
        }
        iz namelen = 0;
        for (; namelen<countof(fd.name) && name[namelen]; namelen++) {}
        Dirent *d = alloc<Dirent>(a);
        d->name   = fromwide(a, name, namelen);
        d->size   = (i64)((u64)fd.sizehi<<32 | fd.sizelo);
        d->mtime  = unixtime(fd.mtime);
        d->isdir  = isdir;
        d->islink = linked(&fd);
        *tail = d;
        tail  = &d->next;
    } while (FindNextFileW(h, &fd));
    i32 err = GetLastError();
    FindClose(h);
    if (err != ERROR_NO_MORE_FILES) {
        r.head = 0;
        r.err  = oserr(err);  // not a partial listing
    }
    return r;
}

// The reason the C runtime gives for a file it cannot open, which
// Universal Ctags reports for option and -I files.
static i32 openerr(i32 err)
{
    switch (err) {
    case 2: case 3: case 15: case 18: case 53: case 67: case 161: case 206:
        return OSERR_NOENT;  // not found, bad drive, bad path or name
    case 5: case 16: case 65: case 82: case 83: case 108: case 132:
    case 158: case 167:
        return OSERR_ACCES;  // denied, locked, or a directory
    case 80: case 183:
        return OSERR_EXIST;
    }
    return err>=19 && err<=36 ? OSERR_ACCES : OSERR_INVAL;
}

// Read a device or pipe to its end, keeping at most max bytes, as the
// arena grows in place.
static Read readstream(uz h, Arena *a, iz max)
{
    Read r = {};
    r.data.data = (u8 *)a->beg;
    while (r.data.len <= max) {
        iz  want = max + 1 - r.data.len;
        i32 n    = want<(1<<16) ? (i32)want : 1<<16;
        u8 *buf  = allocbytes(a, n);
        if (!ReadFile(h, buf, n, &n, 0)) {
            r.err = GetLastError()==ERROR_BROKEN_PIPE ? 0 : OSERR_IO;
            n = 0;
        }
        a->beg = (byte *)(buf + n);
        r.data.len += n;
        if (!n) {
            break;
        }
    }
    r.size = r.data.len;
    r.data.len = r.data.len<max ? r.data.len : max;
    a->beg = (byte *)(r.data.data + r.data.len);
    return r;
}

static Read os_read(Os *, Arena *a, Str path, iz max)
{
    Read  r   = {};
    Arena tmp = *a;
    uz h = CreateFileW(
        widepath(&tmp, path).data, GENERIC_READ, FILE_SHARE_ALL, 0,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    );
    if (h == INVALID_HANDLE_VALUE) {
        // Also as the C runtime would open it, for the same reasons
        h = CreateFileW(
            towide(&tmp, path).data, GENERIC_READ, FILE_SHARE_ALL, 0,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
        );
        if (h == INVALID_HANDLE_VALUE) {
            r.err = openerr(GetLastError());
            return r;
        }
    }
    i32 type = GetFileType(h);
    if (type==FILE_TYPE_CHAR || type==FILE_TYPE_PIPE) {
        r = readstream(h, a, max);
        CloseHandle(h);
        return r;
    } else if (type!=FILE_TYPE_DISK || !GetFileSizeEx(h, &r.size)) {
        CloseHandle(h);
        r.err = OSERR_IO;
        return r;
    }

    iz len = r.size<max ? (iz)r.size : max;
    r.data.data = allocbytes(a, len);
    while (r.data.len < len) {
        i32 n = chunk(len - r.data.len);
        if (!ReadFile(h, r.data.data+r.data.len, n, &n, 0)) {
            r.err = oserr(GetLastError());
            break;
        } else if (!n) {
            break;  // shrank while reading
        }
        r.data.len += n;
    }
    CloseHandle(h);
    return r;
}

// Read in chunks that extend one another at the arena's front, until
// end of file, the end of a pipe (ERROR_BROKEN_PIPE), or an error.
static Str os_stdin(Os *, Arena *a)
{
    Str r = {};
    uz  h = GetStdHandle(STD_INPUT_HANDLE);
    if (!h || h==INVALID_HANDLE_VALUE) {
        return r;  // no standard input at all
    }
    r.data = (u8 *)a->beg;
    for (;;) {
        i32 cap = 1<<16;
        i32 n   = 0;
        u8 *buf = allocbytes(a, cap);
        b32 ok  = ReadFile(h, buf, cap, &n, 0);
        a->beg -= cap - n;
        r.len  += n;
        if (!ok || !n) {
            return r;
        }
    }
}

// The current directory as the system holds it, asking first for its
// size. Not GetFullPathNameW("."), which at a drive's root asks for too
// small a buffer, then given that, writes the drive alone, "C:".
static Str os_cwd(Os *, Arena *a)
{
    i32 cap = GetCurrentDirectoryW(0, 0);  // with the terminator
    if (cap <= 0) {
        return {};
    }
    c16 *buf = alloc<c16>(a, cap);
    i32  len = GetCurrentDirectoryW(cap, buf);
    return len>0 && len<cap ? fromwide(a, buf, len) : Str{};
}

static i32 os_create(Os *os, Arena scratch, Str path)
{
    uz h = CreateFileW(
        widepath(&scratch, path).data, GENERIC_WRITE, 0, 0, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL, 0
    );
    if (h == INVALID_HANDLE_VALUE) {
        return oserr(GetLastError());
    }
    os->out = h;
    return 0;
}

static b32 os_write(Os *os, i32 fd, Str s)
{
    if (fd == 3) {
        return writeall(os->out, s);
    }

    uz  h    = GetStdHandle(-10 - fd);
    i32 mode = 0;
    if (!GetConsoleMode(h, &mode)) {
        return writeall(h, s);
    }

    // Consoles display UTF-16 correctly regardless of code page.
    c16 buf[512];
    i32 len = 0;
    for (iz i = 0; i < s.len;) {
        len += (i32)utf16encode(buf+len, utf8decode(s, &i));
        if (len>countof(buf)-2 || i==s.len) {
            i32 dummy;
            if (!WriteConsoleW(h, buf, len, &dummy, 0)) {
                return 0;
            }
            len = 0;
        }
    }
    return 1;
}

static b32 os_close(Os *os, i32 fd)
{
    assert(fd == 3);
    return CloseHandle(os->out);
}

// Retry for about a second while something holds the destination, as
// Vim does for a moment as it reads a tags file, or a virus scanner.
static i32 os_rename(Os *, Arena scratch, Str src, Str dst)
{
    c16 *wsrc = widepath(&scratch, src).data;
    c16 *wdst = widepath(&scratch, dst).data;
    for (i32 wait = 1;; wait *= 2) {
        if (MoveFileExW(wsrc, wdst, MOVEFILE_REPLACE_EXISTING)) {
            return 0;
        }
        i32 err = oserr(GetLastError());
        if (err!=OSERR_ACCES || wait>512) {
            return err;
        }
        Sleep(wait);
    }
}

static void os_delete(Os *, Arena scratch, Str path)
{
    DeleteFileW(widepath(&scratch, path).data);
}

static b32 os_windows(Os *)
{
    return 1;
}

static i32 os_ncpu(Os *)
{
    SystemInfo info = {};
    GetSystemInfo(&info);
    return info.nproc>1 ? info.nproc : 1;
}

struct Thread {
    void  (*fn)(Worker *);
    Worker *w;
};

__attribute((force_align_arg_pointer))
static i32 __stdcall threadentry(void *arg)
{
    Thread *t = (Thread *)arg;
    t->fn(t->w);
    return 0;
}

static void os_parallel(Os *, Worker *ws, i32 n, void (*fn)(Worker *))
{
    Thread threads[32] = {};
    uz     handles[32] = {};
    assert(n>=1 && n<=countof(threads));
    for (i32 i = 1; i < n; i++) {
        threads[i].fn = fn;
        threads[i].w  = ws + i;
        handles[i] = CreateThread(
            0, (iz)8<<20, threadentry, threads+i,
            STACK_SIZE_PARAM_IS_A_RESERVATION, 0
        );
    }
    fn(ws);
    for (i32 i = 1; i < n; i++) {
        if (handles[i]) {
            WaitForSingleObject(handles[i], -1);
            CloseHandle(handles[i]);
        } else {
            fn(ws + i);  // couldn't start a thread, so do its share here
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
        SwitchToThread();
    } else {
        Sleep(1);
    }
}

static i64 os_clock(Os *)
{
    i64 freq  = 0;
    i64 count = 0;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return count/freq*1000000 + count%freq*1000000/freq;
}

[[noreturn]] static void os_oom()
{
    writeall(GetStdHandle(STD_ERROR_HANDLE), "ctags: out of memory\n");
    ExitProcess(2);
}

static b32 os_commit(byte *p, iz len)
{
    return !len || VirtualAlloc(p, len, MEM_COMMIT, PAGE_READWRITE);
}

// As much address space as the system will reserve, up to 64 GiB (1 GiB
// in a 32-bit process), halving on refusal. It is committed as used.
static Arena reserve()
{
    iz cap = (iz)1 << (sizeof(void *)==8 ? 36 : 30);
    for (; cap >= (iz)1<<24; cap /= 2) {
        byte *p = VirtualAlloc(0, cap, MEM_RESERVE, PAGE_READWRITE);
        if (p) {
            Arena r = {};
            r.beg = r.end = p;
            r.lim = p + cap;
            return r;
        }
    }
    os_oom();
}

extern "C" __attribute((force_align_arg_pointer))
void mainCRTStartup()
{
    Arena a = reserve();

    c16 *cmd = GetCommandLineW();
    iz   len = 0;
    for (; cmd[len]; len++) {}
    Slice<Str> args = splitcmdline(&a, fromwide(&a, cmd, len));

    Os os = {};
    ExitProcess(ctags_main(&os, a, args.data, (i32)args.len));
}

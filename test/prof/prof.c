// prof: a sampling profiler for Windows programs, for performance work
// $ cc -O2 -o prof.exe test/prof/prof.c -lwinmm -lshell32
//
// Usage: prof OUT INTERVAL_US -- COMMAND [ARGS...]
//
// Runs COMMAND as a debuggee, and every INTERVAL_US microseconds (busy
// waiting below 1000, else sleeping) suspends each of its threads to read
// the instruction pointer. For a thread in a system DLL, it also records
// the first word on its stack that points into the program's code, which
// is usually the program's call into the system. OUT gets "base ADDR" for
// the program's image, "dll ADDR PATH" for each DLL, then one line per
// sample: "MICROSECONDS THREAD RIP CALLER". prof.py summarizes it.
//
// Environment:
//   PROF_AFFINITY=MASK  pin the debuggee to these processors (hex)
//   PROF_SELF=MASK      pin the sampler itself (hex)
//   _NO_DEBUG_HEAP=1    keep the debug heap, which a debuggee gets from
//                       Windows by default, out of the profile
//
// A debuggee's threads stop at each debug event, but a ctags run raises
// few beyond thread creation, and sampling at 2 kHz costs it about 10%.
//
// This is free and unencumbered software released into the public domain.
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    DWORD  tid;
    HANDLE h;
    int    live;
} Thread;

typedef struct {
    unsigned           t;
    unsigned short     thread;
    unsigned long long rip;
    unsigned long long caller;
} Sample;

static Thread             threads[256];
static int                nthreads;
static HANDLE             proc;
static unsigned long long imagebeg, imageend;  // the program's image
static unsigned long long codebeg, codeend;    // and its code
static CONTEXT            ctx __attribute((aligned(16)));

// Find the program's image and code ranges from its headers.
static void readimage(unsigned long long base)
{
    IMAGE_DOS_HEADER  dos = {0};
    IMAGE_NT_HEADERS  nt  = {0};
    SIZE_T            got = 0;
    imagebeg = codebeg = base;
    imageend = codeend = base + 0x100000;
    if (!ReadProcessMemory(proc, (void *)base, &dos, sizeof(dos), &got) ||
        !ReadProcessMemory(proc, (void *)(base + dos.e_lfanew), &nt,
                           sizeof(nt), &got)) {
        return;
    }
    imageend = base + nt.OptionalHeader.SizeOfImage;
    codebeg  = base + nt.OptionalHeader.BaseOfCode;
    codeend  = codebeg + nt.OptionalHeader.SizeOfCode;
}

// For a sample outside the program, the first word on the stack that
// points into its code: its call into the system, most likely.
static unsigned long long findcaller(unsigned long long rsp)
{
    static unsigned long long stack[1024];
    SIZE_T got = 0;
    ReadProcessMemory(proc, (void *)rsp, stack, sizeof(stack), &got);
    for (SIZE_T i = 0; i < got/8; i++) {
        if (stack[i]>=codebeg && stack[i]<codeend) {
            return stack[i];
        }
    }
    return 0;
}

static unsigned long long hexenv(char *name, unsigned long long dflt)
{
    char *v = getenv(name);
    return v ? strtoull(v, 0, 16) : dflt;
}

int main(void)
{
    int       argc = 0;
    wchar_t  *cmd  = GetCommandLineW();
    wchar_t **argv = CommandLineToArgvW(cmd, &argc);
    wchar_t  *sep  = wcsstr(cmd, L" -- ");
    if (argc<5 || !sep) {
        fprintf(stderr, "usage: prof OUT INTERVAL_US -- COMMAND [ARGS...]\n");
        return 1;
    }
    int interval = _wtoi(argv[2]);

    FILE *out = _wfopen(argv[1], L"w");
    if (!out) {
        fprintf(stderr, "prof: cannot write %ls\n", argv[1]);
        return 1;
    }

    STARTUPINFOW        si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    if (!CreateProcessW(0, sep+4, 0, 0, TRUE, DEBUG_ONLY_THIS_PROCESS, 0, 0,
                        &si, &pi)) {
        fprintf(stderr, "prof: cannot run the command (%lu)\n",
                GetLastError());
        return 1;
    }
    unsigned long long aff  = hexenv("PROF_AFFINITY", 0);
    unsigned long long self = hexenv("PROF_SELF", 0);
    if (aff) {
        SetProcessAffinityMask(pi.hProcess, (DWORD_PTR)aff);
    }
    if (self) {
        SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)self);
    }
    timeBeginPeriod(1);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    size_t  cap = (size_t)1 << 22;
    size_t  n   = 0;
    Sample *s   = malloc(cap * sizeof(*s));
    LARGE_INTEGER freq, t0, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    for (int done = 0; !done;) {
        DEBUG_EVENT ev;
        while (!done && WaitForDebugEvent(&ev, 0)) {
            DWORD cont = DBG_CONTINUE;
            switch (ev.dwDebugEventCode) {
            case CREATE_PROCESS_DEBUG_EVENT:
                proc = ev.u.CreateProcessInfo.hProcess;
                readimage((unsigned long long)ev.u.CreateProcessInfo.lpBaseOfImage);
                fprintf(out, "base %llx\n", imagebeg);
                threads[nthreads++] = (Thread){
                    ev.dwThreadId, ev.u.CreateProcessInfo.hThread, 1
                };
                if (ev.u.CreateProcessInfo.hFile) {
                    CloseHandle(ev.u.CreateProcessInfo.hFile);
                }
                break;
            case CREATE_THREAD_DEBUG_EVENT:
                if (nthreads < 256) {
                    threads[nthreads++] = (Thread){
                        ev.dwThreadId, ev.u.CreateThread.hThread, 1
                    };
                }
                break;
            case EXIT_THREAD_DEBUG_EVENT:
                for (int i = 0; i < nthreads; i++) {
                    if (threads[i].tid == ev.dwThreadId) {
                        threads[i].live = 0;
                    }
                }
                break;
            case LOAD_DLL_DEBUG_EVENT: {
                wchar_t path[1024] = L"?";
                if (ev.u.LoadDll.hFile) {
                    GetFinalPathNameByHandleW(ev.u.LoadDll.hFile, path, 1024, 0);
                    CloseHandle(ev.u.LoadDll.hFile);
                }
                fprintf(out, "dll %llx %ls\n",
                        (unsigned long long)ev.u.LoadDll.lpBaseOfDll, path);
            } break;
            case EXCEPTION_DEBUG_EVENT: {
                // The loader's breakpoints are expected; anything else is
                // the program's to handle
                DWORD code = ev.u.Exception.ExceptionRecord.ExceptionCode;
                if (code!=EXCEPTION_BREAKPOINT && code!=0x4000001f) {
                    cont = DBG_EXCEPTION_NOT_HANDLED;
                }
            } break;
            case EXIT_PROCESS_DEBUG_EVENT:
                done = 1;
                break;
            }
            ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, cont);
        }
        if (done) {
            break;
        }

        QueryPerformanceCounter(&now);
        unsigned t = (unsigned)((now.QuadPart - t0.QuadPart)*1000000 / freq.QuadPart);
        for (int i = 0; i < nthreads; i++) {
            if (!threads[i].live ||
                SuspendThread(threads[i].h)==(DWORD)-1) {
                continue;
            }
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (GetThreadContext(threads[i].h, &ctx) && n<cap) {
                unsigned long long caller = 0;
                if (ctx.Rip<imagebeg || ctx.Rip>=imageend) {
                    caller = findcaller(ctx.Rsp);
                }
                s[n++] = (Sample){t, (unsigned short)i, ctx.Rip, caller};
            }
            ResumeThread(threads[i].h);
        }

        if (interval >= 1000) {
            Sleep(interval/1000);
        } else {
            LARGE_INTEGER a, b;
            QueryPerformanceCounter(&a);
            do {
                QueryPerformanceCounter(&b);
            } while ((b.QuadPart - a.QuadPart)*1000000/freq.QuadPart < interval);
        }
    }

    for (size_t i = 0; i < n; i++) {
        fprintf(out, "%u %u %llx %llx\n",
                s[i].t, s[i].thread, s[i].rip, s[i].caller);
    }
    fclose(out);
    fprintf(stderr, "prof: %zu samples%s, %d threads\n",
            n, n==cap ? " (full)" : "", nthreads);
    return 0;
}

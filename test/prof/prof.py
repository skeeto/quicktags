# prof.py: summarize the samples prof.exe records
#
# usage: python test/prof/prof.py SAMPLES EXE [options]
#   --top N        rows per table (default 40)
#   --from MS      only samples at or after MS milliseconds
#   --to MS        only samples before MS milliseconds
#   --thread N     only thread N, where 0 is the main thread
#   --report LIST  of funcs, lines, calls, timeline (default all)
#
# EXE is the program profiled, built with symbols: as the one-command
# build at the top of src/ctags.cpp, but with -g and without -s. For a
# single thread on a P-core, from llvm-project's root:
#   $ c++ -std=c++20 -O2 -g -nostartfiles -static-libgcc \
#         -Wl,--stack,0x800000 -o ctags-g.exe ../ctags/main_win32.cpp
#   $ _NO_DEBUG_HEAP=1 PROF_AFFINITY=4 PROF_SELF=100 \
#         prof.exe s.txt 500 -- ctags-g.exe --jobs=1 -R -f x.tags
#   $ python ../ctags/test/prof/prof.py s.txt ctags-g.exe
# addr2line resolves the addresses, inlined frames included, and objdump
# reads the system DLLs' exports.
#
# Reports:
#   funcs     samples by function: the innermost inlined frame, then the
#             function the code physically belongs to
#   lines     samples by source line, of the innermost frame
#   calls     samples in system DLLs, by the export at or before the
#             address (a system call's stub, in ntdll.dll), with the
#             program's caller found on the stack
#   timeline  samples per 50 ms, all threads: in the program, in other
#             system calls, waiting on a lock, idle (sleeping or yielding
#             in a backoff), and joining threads
#
# This is free and unencumbered software released into the public domain.
import argparse, bisect, collections, os, re, subprocess, struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

def prefbase(exe):
    # The image base the program was linked for, from its PE header
    with open(exe, "rb") as f:
        data = f.read(4096)
    pe  = struct.unpack_from("<I", data, 0x3c)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] == 0x20b:
        return struct.unpack_from("<Q", data, opt + 24)[0]
    return struct.unpack_from("<I", data, opt + 28)[0]

def exports(path):
    # A DLL's exports as sorted (rva, name)
    out = subprocess.run(["objdump", "-p", path],
                         capture_output=True, text=True).stdout
    rva = {}
    for m in re.finditer(r"\[\s*\d+\] \+base\[\s*(\d+)\] ([0-9a-f]{8}) Export RVA", out):
        rva[int(m.group(1))] = int(m.group(2), 16)
    names = []
    for m in re.finditer(r"\[\s*\d+\] \+base\[\s*(\d+)\]\s+[0-9a-f]{4} (\S+)", out):
        if int(m.group(1)) in rva:
            names.append((rva[int(m.group(1))], m.group(2)))
    return sorted(names)

def relative(loc):
    loc  = loc.split(" (discriminator")[0].replace("\\", "/")
    root = ROOT.replace("\\", "/") + "/"
    if loc.lower().startswith(root.lower()):
        loc = loc[len(root):]
    return loc

def symbolize(exe, addrs):
    # Each address's inlined frames, innermost first, as (function, line)
    addrs = sorted(addrs)
    if not addrs:
        return {}
    inp = "".join("%x\n" % a for a in addrs)
    out = subprocess.run(["addr2line", "-f", "-i", "-C", "-a", "-e", exe],
                         input=inp, capture_output=True, text=True).stdout
    frames = {}
    cur = None
    lines = out.splitlines()
    i = 0
    while i < len(lines):
        if lines[i].startswith("0x"):
            cur = int(lines[i], 16)
            frames[cur] = []
            i += 1
        else:
            fn = lines[i].split("(")[0]
            loc = relative(lines[i+1]) if i+1 < len(lines) else "?"
            frames[cur].append((fn, loc))
            i += 2
    return frames

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("samples")
    ap.add_argument("exe")
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--from", dest="lo", type=float, default=0)
    ap.add_argument("--to", dest="hi", type=float, default=1e12)
    ap.add_argument("--thread", type=int, default=-1)
    ap.add_argument("--report", default="funcs,lines,calls,timeline")
    a = ap.parse_args()
    reports = a.report.split(",")

    base, dlls, samples = None, [], []
    for line in open(a.samples):
        f = line.split()
        if f[0] == "base":
            base = int(f[1], 16)
        elif f[0] == "dll":
            dlls.append((int(f[1], 16), f[2].replace("\\\\?\\", "")))
        else:
            t, ti = int(f[0]), int(f[1])
            if a.lo*1000 <= t < a.hi*1000 and (a.thread < 0 or ti == a.thread):
                caller = int(f[3], 16) if len(f) > 3 else 0
                samples.append((t, int(f[2], 16), caller))
    dlls.sort()
    total = len(samples) or 1
    pref = prefbase(a.exe)
    end  = base + 0x10000000

    # Addresses outside the program by DLL export
    tables = {}
    def sysname(rip):
        i = bisect.bisect_right(dlls, (rip, "￿")) - 1
        if i < 0:
            return "?"
        dbase, path = dlls[i]
        name = os.path.basename(path)
        if path not in tables:
            tables[path] = exports(path)
        t = tables[path]
        j = bisect.bisect_right(t, (rip - dbase, "￿")) - 1
        return name + "!" + (t[j][1] if j >= 0 else "?")

    inside = {rip for _, rip, _ in samples if base <= rip < end}
    callers = {c for _, rip, c in samples if c and not base <= rip < end}
    frames = symbolize(a.exe, {r - base + pref for r in inside} |
                              {c - 1 - base + pref for c in callers})
    def framesof(addr):
        return frames.get(addr - base + pref) or [("?", "?")]

    def show(title, ctr):
        print("== %s (%d samples)" % (title, len(samples)))
        acc = 0
        for name, n in ctr.most_common(a.top):
            acc += n
            print("%6.2f%% %6.2f%%  %s" % (100*n/total, 100*acc/total, name))
        print()

    if "funcs" in reports or "lines" in reports:
        inner = collections.Counter()
        outer = collections.Counter()
        lines = collections.Counter()
        for _, rip, _ in samples:
            if base <= rip < end:
                fs = framesof(rip)
                inner[fs[0][0]] += 1
                outer[fs[-1][0]] += 1
                lines["%s %s" % (fs[0][1], fs[0][0])] += 1
            else:
                name = "[" + sysname(rip).split("!")[0] + "]"
                inner[name] += 1
                outer[name] += 1
                lines[name] += 1
        if "funcs" in reports:
            show("by function, innermost inlined frame", inner)
            show("by function, physical", outer)
        if "lines" in reports:
            show("by line", lines)

    if "calls" in reports:
        calls = collections.Counter()
        for _, rip, caller in samples:
            if not base <= rip < end:
                chain = framesof(caller - 1)[:3] if caller else [("?", "?")]
                where = " < ".join("%s@%s" % fl for fl in chain)
                calls["%-40s %s" % (sysname(rip), where)] += 1
        show("system calls, with the program's caller", calls)

    if "timeline" in reports:
        kinds = {
            "ZwDelayExecution": "idle", "NtDelayExecution": "idle",
            "ZwYieldExecution": "idle", "NtYieldExecution": "idle",
            "ZwWaitForAlertByThreadId": "lock",
            "RtlSleepConditionVariableCS": "lock",
            "RtlSleepConditionVariableSRW": "lock",
            "ZwWaitForSingleObject": "join",
            "ZwWaitForMultipleObjects": "join",
        }
        buckets = collections.defaultdict(collections.Counter)
        for t, rip, _ in samples:
            if base <= rip < end:
                kind = "user"
            else:
                kind = kinds.get(sysname(rip).split("!")[-1], "sys")
            buckets[t // 50000][kind] += 1
        cols = ["user", "sys", "lock", "idle", "join"]
        print("== timeline, samples per 50 ms")
        print("   ms " + "".join("%6s" % c for c in cols))
        for b in sorted(buckets):
            print("%5d " % (b*50) + "".join("%6d" % buckets[b][c] for c in cols))
        print()

main()

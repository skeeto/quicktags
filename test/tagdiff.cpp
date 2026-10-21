// tagdiff: compare a tags file against Universal Ctags' for the same input
//
//   $ tagdiff [options] REF.tags OURS.tags
//
// Pseudo-tags and CRs are ignored. Lines only in REF are "missing" from
// ours, lines only in OURS are "extra". A ledger explains deliberate
// differences: each entry starts "@ SCOPE: reason" (SCOPE a glob, as
// fnmatch without flags), followed by its lines, "- LINE" for a missing
// line, "+ LINE" for an extra one, with literal tabs. The comparison
// passes when every difference is in a ledger entry whose scope matches,
// and every such entry's lines all occurred.
//
// Options:
//   --ledger FILE      explain differences with this ledger
//   --scope NAME       this comparison's name, matched against ledgers
//   --only KINDS       compare only these kinds, e.g. "C:d,C++:d,Fortran:*"
//   --show N           print up to N differences (default 40)
//
// This is free and unencumbered software released into the public domain.
#include <stdio.h>
#include <stdlib.h>
#include "../base.cpp"

[[noreturn]] static void os_oom()
{
    fputs("tagdiff: out of memory\n", stderr);
    exit(2);
}

static b32 os_commit(byte *, iz)
{
    return 1;
}

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

static Str cstr(char *s)
{
    Str r = {(u8 *)s, 0};
    for (; s[r.len]; r.len++) {}
    return r;
}

static char *tocstr(Arena *a, Str s)
{
    char *r = alloc<char>(a, s.len+1);
    copybytes(r, s.data, s.len);
    return r;
}

static Str readfile(Arena *a, Str path)
{
    FILE *f = fopen(tocstr(a, path), "rb");
    if (!f) {
        return {};
    }
    Buf b(a, 1<<16);
    for (;;) {
        reserve(&b, 1<<16);
        uz n = fread(b.data+b.len, 1, (uz)(b.cap-b.len), f);
        b.len += (iz)n;
        if (!n) {
            break;
        }
    }
    fclose(f);
    Str r = finish(&b);
    if (!r.data) {
        r.data = (u8 *)"";
    }
    return r;
}

static b32 globmatch(Str pat, Str s)
{
    iz p    = 0;
    iz i    = 0;
    iz star = -1;
    iz mark = 0;
    while (i < s.len) {
        if (p<pat.len && pat[p]=='*') {
            star = p++;
            mark = i;
        } else if (p<pat.len && (pat[p]=='?' || pat[p]==s[i])) {
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

static b32 strless(Str *a, Str *b)
{
    return compare(*a, *b) < 0;
}

// The language of a tag line, by its file field's extension.
static Str langof(Str line)
{
    Cut name = cut(line, '\t');
    Cut file = cut(name.tail, '\t');
    Str path = file.head;
    iz  dot  = path.len - 1;
    for (; dot>=0 && path[dot]!='.' && path[dot]!='/'; dot--) {}
    if (dot<0 || path[dot]!='.') {
        return "";
    }
    Str ext = cuthead(path, dot+1);
    Str c[]   = {"c"};
    Str cxx[] = {"c++", "cc", "cp", "cpp", "cxx", "h", "h++", "hh", "hp",
                 "hpp", "hxx", "inl"};
    Str ftn[] = {"f", "for", "ftn", "f77", "f90", "f95", "f03", "f08",
                 "f15"};
    if (ext == "C") return "C++";
    for (iz i = 0; i < countof(c); i++) {
        if (equalsfold(ext, c[i])) return "C";
    }
    for (iz i = 0; i < countof(cxx); i++) {
        if (equalsfold(ext, cxx[i])) return "C++";
    }
    for (iz i = 0; i < countof(ftn); i++) {
        if (equalsfold(ext, ftn[i])) return "Fortran";
    }
    return "";
}

// The kind letter of a tag line: the first field after ;"
static u8 kindof(Str line)
{
    iz at = -1;
    for (iz i = 0; i+2 < line.len; i++) {
        if (line[i]==';' && line[i+1]=='"' && line[i+2]=='\t') {
            at = i + 3;  // the last such, after any pattern
        }
    }
    return at>=0 && at<line.len ? line[at] : 0;
}

// Does "C:d,C++:d,Fortran:*" select this line?
static b32 selected(Str only, Str line)
{
    if (!only.data) {
        return 1;
    }
    Str lang = langof(line);
    u8  kind = kindof(line);
    for (Cut c = cut(only, ','); ; c = cut(c.tail, ',')) {
        Cut lk = cut(c.head, ':');
        if (lk.head == lang) {
            for (iz i = 0; i < lk.tail.len; i++) {
                if (lk.tail[i]=='*' || lk.tail[i]==kind) {
                    return 1;
                }
            }
        }
        if (!c.ok) {
            return 0;
        }
    }
}

// Tag lines, sorted, without pseudo-tags, CRs, or duplicates.
static Slice<Str> taglines(Arena *a, Str text, Str only)
{
    Slice<Str> r = {};
    for (Cut c = cut(text, '\n'); c.head.len || c.ok; c = cut(c.tail, '\n')) {
        Str l = c.head;
        if (endswith(l, "\r")) {
            l = cuttail(l, 1);
        }
        if (l.len && !startswith(l, "!_TAG_") && selected(only, l)) {
            r = push(a, r, l);
        }
        if (!c.ok) {
            break;
        }
    }
    sort(r.data, r.len, strless, *a);
    iz n = 0;
    for (iz i = 0; i < r.len; i++) {
        if (!n || !(r[n-1] == r[i])) {
            r.data[n++] = r.data[i];
        }
    }
    r.len = n;
    return r;
}

struct Entry {
    Entry *next;
    Str    head;    // the "@ ..." line
    Str    scope;
    Str    lines;   // "- " and "+ " lines, each ending in a newline
    iz     used;
    iz     count;
};

static Entry *readledger(Arena *a, Str text, Str scope)
{
    Entry  *head = 0;
    Entry **tail = &head;
    Entry  *cur  = 0;
    for (Cut c = cut(text, '\n'); ; c = cut(c.tail, '\n')) {
        Str l = c.head;
        if (endswith(l, "\r")) {
            l = cuttail(l, 1);
        }
        if (startswith(l, "@ ")) {
            Cut s = cut(cuthead(l, 2), ':');
            cur = 0;
            if (globmatch(trim(s.head), scope)) {
                cur = alloc<Entry>(a);
                cur->head  = l;
                cur->scope = trim(s.head);
                *tail = cur;
                tail  = &cur->next;
            }
        } else if (cur && (startswith(l, "- ") || startswith(l, "+ "))) {
            cur->lines = concat(a, cur->lines, l);
            cur->lines = concat(a, cur->lines, "\n");
            cur->count++;
        }
        if (!c.ok) {
            break;
        }
    }
    return head;
}

// Find a ledger line for a difference, marking it used.
static b32 explained(Entry *ledger, u8 sign, Str line)
{
    for (Entry *e = ledger; e; e = e->next) {
        for (Cut c = cut(e->lines, '\n'); c.ok; c = cut(c.tail, '\n')) {
            if (c.head.len>=2 && c.head[0]==sign && cuthead(c.head, 2)==line) {
                e->used++;
                return 1;
            }
        }
    }
    return 0;
}

static void printstr(FILE *f, Str s)
{
    fwrite(s.data, 1, (uz)s.len, f);
}

static i32 usage()
{
    fputs("usage: tagdiff [--ledger FILE] [--scope NAME] [--only KINDS] "
          "[--show N] REF OURS\n", stderr);
    return 2;
}

int main(int argc, char **argv)
{
    iz    cap = (iz)1<<32;
    if (sizeof(void *) < 8) cap = (iz)1<<29;
    byte *mem = (byte *)malloc((uz)cap);
    if (!mem) {
        os_oom();
    }
    Arena a = {mem, mem+cap};

    Str ledgerpath = {};
    Str scope      = "";
    Str only       = {};
    i32 show       = 40;
    Str files[2]   = {};
    i32 nfiles     = 0;
    for (i32 i = 1; i < argc; i++) {
        Str arg = cstr(argv[i]);
        Str v   = i+1<argc ? cstr(argv[i+1]) : Str{};
        if (arg=="--ledger" && v.data) {
            ledgerpath = v; i++;
        } else if (arg=="--scope" && v.data) {
            scope = v; i++;
        } else if (arg=="--only" && v.data) {
            only = v; i++;
        } else if (arg=="--show" && v.data) {
            show = atoi(argv[++i]);
        } else if (nfiles<2 && !startswith(arg, "--")) {
            files[nfiles++] = arg;
        } else {
            return usage();
        }
    }
    if (nfiles != 2) {
        return usage();
    }

    Str reftext  = readfile(&a, files[0]);
    Str ourstext = readfile(&a, files[1]);
    if (!reftext.data || !ourstext.data) {
        fprintf(stderr, "tagdiff: cannot read %s\n",
                reftext.data ? argv[argc-1] : "the reference");
        return 2;
    }
    Slice<Str> ref  = taglines(&a, reftext, only);
    Slice<Str> ours = taglines(&a, ourstext, only);

    Entry *ledger = 0;
    if (ledgerpath.data) {
        Str text = readfile(&a, ledgerpath);
        if (!text.data) {
            fprintf(stderr, "tagdiff: cannot read ledger\n");
            return 2;
        }
        ledger = readledger(&a, text, scope);
    }

    // Merge-walk the sorted lines
    iz missing = 0, extra = 0, unexplained = 0, shown = 0;
    iz i = 0, j = 0;
    while (i<ref.len || j<ours.len) {
        u8  sign = 0;
        Str line = {};
        if (j==ours.len || (i<ref.len && compare(ref[i], ours[j])<0)) {
            sign = '-';
            line = ref[i++];
            missing++;
        } else if (i==ref.len || compare(ours[j], ref[i])<0) {
            sign = '+';
            line = ours[j++];
            extra++;
        } else {
            i++;
            j++;
            continue;
        }
        if (!explained(ledger, sign, line)) {
            unexplained++;
            if (shown++ < show) {
                fputc(sign, stdout);
                fputc(' ', stdout);
                printstr(stdout, line);
                fputc('\n', stdout);
            }
        }
    }

    iz stale = 0;
    for (Entry *e = ledger; e; e = e->next) {
        if (e->used != e->count) {
            stale++;
            printf("stale ledger entry (%td of %td lines used): ",
                   e->used, e->count);
            printstr(stdout, e->head);
            fputc('\n', stdout);
        }
    }

    printf("%td ref, %td ours, %td missing, %td extra, %td unexplained, "
           "%td stale\n", ref.len, ours.len, missing, extra, unexplained,
           stale);
    return unexplained || stale ? 1 : 0;
}

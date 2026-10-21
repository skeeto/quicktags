// tagdiff: compare a tags file against Universal Ctags' for the same input
//
//   $ tagdiff [options] REF.tags OURS.tags
//
// Pseudo-tags and CRs are ignored. Lines only in REF are "missing" from
// ours, lines only in OURS are "extra". A ledger explains deliberate
// differences: each entry starts "@ SCOPE: reason" (SCOPE a glob, as
// fnmatch without flags), followed by its lines, "- LINE" for a missing
// line, "+ LINE" for an extra one, with literal tabs, or "= N PATH" for
// exactly N differences, either kind, in the file PATH. The comparison
// passes when every difference is in a ledger entry whose scope matches,
// and every such entry's lines all occurred. For a systematic difference,
// an entry may instead name a rewrite of the reference's lines, "! NAME",
// applied before comparing:
//   anon-namespace-file   functions, variables, and members within an
//                         anonymous namespace are file: outside headers
//   typeref-trailing-space   a typeref does not end in spaces
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

// "= N PATH": up to N differences in the file
struct FileRule {
    FileRule *next;
    Str       path;
    iz        count;
    iz        used;
};

struct Entry {
    Entry    *next;
    Str       head;    // the "@ ..." line
    Str       scope;
    Str       lines;   // "- " and "+ " lines, each ending in a newline
    FileRule *files;
    u32       rewrites;  // REWRITE_*
    iz        used;
    iz        count;
};

enum : u32 {
    REWRITE_ANONFILE  = 1u<<0,
    REWRITE_TYPESPACE = 1u<<1,
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
        } else if (cur && startswith(l, "= ")) {
            Cut       n = cut(cuthead(l, 2), ' ');
            FileRule *r = alloc<FileRule>(a);
            for (iz i = 0; i < n.head.len; i++) {
                r->count = r->count*10 + n.head[i] - '0';
            }
            r->path    = n.tail;
            r->next    = cur->files;
            cur->files = r;
            cur->count += r->count;
        } else if (cur && startswith(l, "! ")) {
            Str name = trim(cuthead(l, 2));
            if (name == "anon-namespace-file") {
                cur->rewrites |= REWRITE_ANONFILE;
            } else if (name == "typeref-trailing-space") {
                cur->rewrites |= REWRITE_TYPESPACE;
            } else {
                fprintf(stderr, "tagdiff: unknown rewrite: %.*s\n",
                        (int)name.len, (char *)name.data);
                exit(2);
            }
        }
        if (!c.ok) {
            break;
        }
    }
    return head;
}

// Find a ledger line for a difference, marking it used: an exact line,
// or else the first file rule for its file with room left.
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
    Str file = cut(cut(line, '\t').tail, '\t').head;
    for (Entry *e = ledger; e; e = e->next) {
        for (FileRule *r = e->files; r; r = r->next) {
            if (r->path==file && r->used<r->count) {
                r->used++;
                e->used++;
                return 1;
            }
        }
    }
    return 0;
}

static b32 ishex(u8 c)
{
    return (c>='0' && c<='9') || (c>='a' && c<='f');
}

// Is the path a header, as ctags decides it (X.in by X)?
static b32 isheaderpath(Str path)
{
    if (path.len>3 && equalsfold(cuthead(path, path.len-3), ".in")) {
        path = cuttail(path, 3);
    }
    iz dot = path.len - 1;
    for (; dot>=0 && path[dot]!='.' && path[dot]!='/'; dot--) {}
    if (dot<0 || path[dot]!='.') {
        return 0;
    }
    Str ext   = cuthead(path, dot+1);
    Str hdr[] = {"h", "hh", "hpp", "hxx", "h++", "inc", "def", "inl", "hp"};
    for (iz i = 0; i < countof(hdr); i++) {
        if (equalsfold(ext, hdr[i])) {
            return 1;
        }
    }
    return 0;
}

// Does a scope name have an anonymous namespace among its parts? Its
// generated name ends in the namespace kind's index, 0x11.
static b32 anonnamespace(Str scope)
{
    while (scope.len) {
        iz n = 0;
        for (; n<scope.len && scope[n]!=':'; n++) {}
        Str part = {scope.data, n};
        for (; n<scope.len && scope[n]==':'; n++) {}
        scope = cuthead(scope, n);

        if (part.len>=18 && startswith(part, "__anon") && endswith(part, "11")) {
            b32 hex = 1;
            for (iz i = 6; i < part.len; i++) {
                hex = hex && ishex(part[i]);
            }
            if (hex) {
                return 1;
            }
        }
    }
    return 0;
}

// Trim spaces ending the typeref field.
static Str trimtyperef(Arena *a, Str line, b32 *changed)
{
    iz at = -1;
    for (iz i = 0; i+8 < line.len; i++) {
        if (line[i]=='\t' && startswith(cuthead(line, i+1), "typeref:")) {
            at = i + 1;
        }
    }
    if (at < 0) {
        return line;
    }
    iz end = at;
    for (; end<line.len && line[end]!='\t'; end++) {}
    iz trim = end;
    for (; trim>at && line[trim-1]==' '; trim--) {}
    if (trim == end) {
        return line;
    }
    *changed = 1;
    Str head = {line.data, trim};
    return concat(a, head, cuthead(line, end));
}

// Apply rewrites to a reference line, or return it unchanged.
static Str rewrite(Arena *a, u32 rewrites, Str line, b32 *changed)
{
    *changed = 0;
    if (rewrites & REWRITE_TYPESPACE) {
        line = trimtyperef(a, line, changed);
    }
    if (!(rewrites & REWRITE_ANONFILE)) {
        return line;
    }

    Str file = cut(cut(line, '\t').tail, '\t').head;
    u8  kind = kindof(line);
    if (isheaderpath(file) || (kind!='f' && kind!='v' && kind!='m')) {
        return line;
    }

    // The fields after the pattern: kind, line, scope, typeref, file
    iz at = -1;
    for (iz i = 0; i+2 < line.len; i++) {
        if (line[i]==';' && line[i+1]=='"' && line[i+2]=='\t') {
            at = i + 3;
        }
    }
    b32 anon = 0;
    for (Cut c = cut(cuthead(line, at), '\t'); ; c = cut(c.tail, '\t')) {
        if (c.head == "file:") {
            return line;
        }
        Cut kv = cut(c.head, ':');
        if (kv.ok && kv.head!="line" && kv.head!="typeref" && kv.head!="name") {
            anon = anon || anonnamespace(kv.tail);
        }
        if (!c.ok) {
            break;
        }
    }
    if (!anon) {
        return line;
    }
    *changed = 1;
    return concat(a, line, "\tfile:");
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
    iz    cap = (iz)1 << (sizeof(void *)<8 ? 29 : 32);
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

    // Rewrite the reference by the rules of matching entries, which
    // never go stale since a whole scope may lack what they rewrite
    u32 rewrites = 0;
    for (Entry *e = ledger; e; e = e->next) {
        rewrites |= e->rewrites;
    }
    if (rewrites) {
        for (iz i = 0; i < ref.len; i++) {
            b32 changed = 0;
            ref[i] = rewrite(&a, rewrites, ref[i], &changed);
        }
        sort(ref.data, ref.len, strless, a);
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
        if (e->used!=e->count && !e->rewrites) {
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

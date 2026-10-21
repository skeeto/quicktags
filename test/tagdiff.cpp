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
//                         anonymous namespace are file-scoped outside
//                         headers: file:, and fileScope in extras:
//   typeref-trailing-space   a typeref does not end in spaces
//
// Options:
//   --ledger FILE      explain differences with this ledger
//   --scope NAME       this comparison's name, matched against ledgers
//   --only KINDS       compare only these kinds, e.g. "C:d,C++:d,Fortran:*"
//   --show N           print up to N differences (default 40)
//   --ordered          compare in order, with duplicates
//   --base RB OB       compare only the tags on which RB and OB agree
//   --etags            REF and OURS are Emacs TAGS files
//
// Lines are compared as sorted sets, unless --ordered: then duplicates
// count, and the lines in both files must also come in the same order,
// within each file, and by file (by its first line). A file's first
// divergence in order is one difference in it, for a "= N PATH" rule.
//
// With --base, RB and OB are Universal Ctags' and our output for the same
// inputs without the options under test, compared on their own. A tag's
// key is its name, file, and line: its line: field, else the number its
// address starts with, else its address. A key is clean when RB and OB
// hold the same lines for it (or none), and only lines with clean keys
// are compared.
//
// With --etags, each section "\f\n<path>,<size>\n" becomes a line
// "\t<path>\t0\t", and each of its entries, "<text>\x7f<name>\x01<line>,
// <offset>\n", a line "<name>\t<path>\t<line>\t<text>,<offset>". A size
// must count its section's bytes. Base files may be in either format.
//
// This is free and unencumbered software released into the public domain.
#include <stdio.h>
#include <stdlib.h>
#include "../src/base.cpp"

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

static void printstr(FILE *f, Str s)
{
    fwrite(s.data, 1, (uz)s.len, f);
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


// Tag lines

// The file field of a tag line.
static Str fileof(Str line)
{
    return cut(cut(line, '\t').tail, '\t').head;
}

// The language of a tag line, by its file field's extension.
static Str langof(Str line)
{
    Str path = fileof(line);
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

// Where a tag line's fields begin, after its ;" and tab, or -1.
static iz fieldsat(Str line)
{
    iz at = -1;
    for (iz i = 0; i+2 < line.len; i++) {
        if (line[i]==';' && line[i+1]=='"' && line[i+2]=='\t') {
            at = i + 3;  // the last such, after any pattern
        }
    }
    return at;
}

// A field's value, or a null string if the line lacks the field.
static Str field(Str line, Str key)
{
    iz at = fieldsat(line);
    if (at < 0) {
        return {};
    }
    for (Cut c = cut(cuthead(line, at), '\t'); ; c = cut(c.tail, '\t')) {
        Cut kv = cut(c.head, ':');
        if (kv.ok && kv.head==key) {
            return kv.tail;
        }
        if (!c.ok) {
            return {};
        }
    }
}

struct KindName {
    u8  letter;
    Str name;
};

// The letter of a long kind name, in the line's language.
static u8 kindletter(Str lang, Str name)
{
    static KindName cxx[] = {  // C's kinds, and C++'s own
        {'D', "macroparam"}, {'L', "label"},     {'d', "macro"},
        {'e', "enumerator"}, {'f', "function"},  {'g', "enum"},
        {'h', "header"},     {'l', "local"},     {'m', "member"},
        {'p', "prototype"},  {'s', "struct"},    {'t', "typedef"},
        {'u', "union"},      {'v', "variable"},  {'x', "externvar"},
        {'z', "parameter"},  {'A', "alias"},     {'M', "module"},
        {'N', "name"},       {'P', "partition"}, {'U', "using"},
        {'Z', "tparam"},     {'c', "class"},     {'n', "namespace"},
        {'F', "file"},
    };
    static KindName ftn[] = {
        {'E', "enum"},       {'L', "local"},     {'M', "method"},
        {'N', "enumerator"}, {'P', "prototype"}, {'S', "submodule"},
        {'b', "blockData"},  {'c', "common"},    {'e', "entry"},
        {'f', "function"},   {'i', "interface"}, {'k', "component"},
        {'l', "label"},      {'m', "module"},    {'n', "namelist"},
        {'p', "program"},    {'s', "subroutine"},{'t', "type"},
        {'v', "variable"},   {'F', "file"},
    };
    b32       fortran = lang == "Fortran";
    KindName *kinds   = fortran ? ftn : cxx;
    iz        len     = fortran ? countof(ftn) : countof(cxx);
    for (iz i = 0; i < len; i++) {
        if (kinds[i].name == name) {
            return kinds[i].letter;
        }
    }
    return 0;
}

// The kind letter of a tag line, from its kind field: a bare letter or
// long name (fields k and K), or either after "kind:" (field z).
static u8 kindof(Str line)
{
    iz at = fieldsat(line);
    if (at < 0) {
        return 0;
    }
    for (Cut c = cut(cuthead(line, at), '\t'); ; c = cut(c.tail, '\t')) {
        Cut kv   = cut(c.head, ':');
        Str kind = !kv.ok ? c.head : kv.head=="kind" ? kv.tail : Str{};
        if (kind.len == 1) {
            return kind[0];
        } else if (kind.len) {
            return kindletter(langof(line), kind);
        } else if (!c.ok) {
            return 0;
        }
    }
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

// Tag lines in file order, without pseudo-tags or CRs.
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
    }
    return r;
}

struct Lines {
    Slice<Str> lines;
    iz         bad;    // malformed sections and entries
};

// An Emacs TAGS file's sections and entries as lines (see above).
static Lines etagslines(Arena *a, Str text, Str only)
{
    Lines r    = {};
    Str   path = {};
    iz    size = -1;  // the open section's, and the bytes it has
    iz    have = 0;
    for (Cut c = cut(text, '\n'); c.head.len || c.ok; c = cut(c.tail, '\n')) {
        Str l = c.head;
        if (l == "\f") {
            r.bad += size>=0 && size!=have;
            c = cut(c.tail, '\n');
            iz comma = c.head.len - 1;
            for (; comma>=0 && c.head[comma]!=','; comma--) {}
            Str digits = cuthead(c.head, comma+1);
            b32 ok     = comma>0 && digits.len;
            path = takehead(c.head, comma>0 ? comma : 0);
            size = 0;
            have = 0;
            for (iz i = 0; ok && i < digits.len; i++) {
                ok   = digit(digits[i]) && size<100000000;
                size = size*10 + (digits[i] - '0');
            }
            if (!ok) {
                r.bad++;
                size = -1;
            }
            Buf b(a, path.len+4);
            putbyte(&b, '\t');
            print(&b, path);
            print(&b, "\t0\t");
            if (selected(only, finish(&b))) {
                r.lines = push(a, r.lines, finish(&b));
            }
            continue;
        }

        have += l.len + c.ok;
        iz del = l.len - 1;
        for (; del>=0 && l[del]!=0x7f; del--) {}
        if (!path.data || del<0) {
            r.bad++;
            continue;
        }
        Cut name = cut(cuthead(l, del+1), 0x01);
        Str tag  = name.ok ? name.head : Str{};
        Cut pos  = cut(name.ok ? name.tail : name.head, ',');
        Str src  = takehead(l, del);
        iz  len  = tag.len + path.len + pos.head.len + src.len + pos.tail.len;
        Buf b(a, len+4);
        print(&b, tag);
        putbyte(&b, '\t');
        print(&b, path);
        putbyte(&b, '\t');
        print(&b, pos.head);
        putbyte(&b, '\t');
        print(&b, src);
        putbyte(&b, ',');
        print(&b, pos.tail);
        if (selected(only, finish(&b))) {
            r.lines = push(a, r.lines, finish(&b));
        }
    }
    r.bad += size>=0 && size!=have;
    return r;
}

enum : u32 {
    FIELD_FILE   = 1u<<0,
    FIELD_EXTRAS = 1u<<1,
};

// The fields a tags file enables, of those the rewrites add, by its
// pseudo-tags. Without them, the defaults: file but not extras.
static u32 fieldsof(Str text)
{
    Str key = "!_TAG_FIELD_DESCRIPTION\t";
    b32 any = 0;
    u32 r   = 0;
    Cut c   = cut(text, '\n');
    for (; startswith(c.head, "!_"); c = cut(c.tail, '\n')) {
        if (startswith(c.head, key)) {
            Str name = cut(cuthead(c.head, key.len), '\t').head;
            any = 1;
            if (name == "file") {
                r |= FIELD_FILE;
            } else if (name == "extras") {
                r |= FIELD_EXTRAS;
            }
        }
    }
    return any ? r : FIELD_FILE;
}

// Sort lines and drop duplicates.
static Slice<Str> sortunique(Slice<Str> s, Arena scratch)
{
    sort(s.data, s.len, strless, scratch);
    iz n = 0;
    for (iz i = 0; i < s.len; i++) {
        if (!n || !(s[n-1] == s[i])) {
            s.data[n++] = s.data[i];
        }
    }
    s.len = n;
    return s;
}


// Ledger

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

// Find a file rule with room left for a difference in the file, and
// mark it used.
static b32 explainedfile(Entry *ledger, Str file)
{
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
    return explainedfile(ledger, fileof(line));
}


// Rewrites

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

// Universal Ctags writes the kind, line, language, scope, and typeref
// fields, then file:, then these in this order, then the parser's own.
enum : i32 { LATE_EXTRAS = 6, LATE_PARSER = 10 };

static i32 lateness(Str key)  // 0: before file:
{
    static Str late[] = {
        "inherits", "access", "implementation", "signature", "roles",
        "extras", "end", "epoch", "nth",
    };
    static Str parser[] = {
        "alias", "captures", "macrodef", "name", "properties", "section",
        "specialization", "template",
    };
    for (i32 i = 0; i < countof(late); i++) {
        if (key == late[i]) return i + 1;
    }
    for (i32 i = 0; i < countof(parser); i++) {
        if (key == parser[i]) return LATE_PARSER;
    }
    return 0;
}

// Make a function, variable, or member in an anonymous namespace, not in
// a header, file-scoped: insert file: in its place, and add fileScope to
// the extras, first in their order.
static Str anonfile(Arena *a, u32 fields, Str line)
{
    u8 kind = kindof(line);
    iz at   = fieldsat(line);
    if (at<0 || isheaderpath(fileof(line))) {
        return line;
    } else if (kind!='f' && kind!='v' && kind!='m') {
        return line;
    }

    b32 anon     = 0;
    iz  fileat   = line.len;  // the tab before the first late field
    iz  extrasat = line.len;  // the tab before the first after extras:
    iz  valueat  = -1;        // the extras field's value
    for (iz i = at; i <= line.len;) {
        iz end = i;
        for (; end<line.len && line[end]!='\t'; end++) {}
        Cut kv   = cut(span(line.data+i, line.data+end), ':');
        i32 late = kv.ok ? lateness(kv.head) : 0;
        if (kv.ok && kv.head=="file") {
            return line;
        } else if (kv.ok && kv.head=="extras") {
            for (Cut x = cut(kv.tail, ','); ; x = cut(x.tail, ',')) {
                if (x.head == "fileScope") {
                    return line;
                } else if (!x.ok) {
                    break;
                }
            }
            valueat = i + 7;
        }
        fileat   = late && fileat==line.len ? i-1 : fileat;
        extrasat = late>LATE_EXTRAS && extrasat==line.len ? i-1 : extrasat;
        if (kv.ok && !late && kv.head!="kind" && kv.head!="line" &&
                kv.head!="language" && kv.head!="typeref") {
            anon = anon || anonnamespace(kv.tail);  // a scope field
        }
        i = end + 1;
    }
    if (!anon) {
        return line;
    }

    Str insfile   = fields&FIELD_FILE ? Str("\tfile:") : Str("");
    Str insextras = "";
    if (valueat >= 0) {
        extrasat  = valueat;
        insextras = "fileScope,";
    } else if (fields & FIELD_EXTRAS) {
        insextras = "\textras:fileScope";
    }
    Buf b(a, line.len + insfile.len + insextras.len);
    print(&b, takehead(line, fileat));
    print(&b, insfile);
    print(&b, span(line.data+fileat, line.data+extrasat));
    print(&b, insextras);
    print(&b, cuthead(line, extrasat));
    return finish(&b);
}

// Trim spaces ending the typeref field.
static Str trimtyperef(Arena *a, Str line)
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
    Str head = {line.data, trim};
    return concat(a, head, cuthead(line, end));
}

// Apply rewrites to a reference line, or return it unchanged.
static Str rewrite(Arena *a, u32 rewrites, u32 fields, Str line)
{
    if (rewrites & REWRITE_TYPESPACE) {
        line = trimtyperef(a, line);
    }
    if (rewrites & REWRITE_ANONFILE) {
        line = anonfile(a, fields, line);
    }
    return line;
}


// Inputs

enum : i32 { FORMAT_TAGS, FORMAT_ETAGS, FORMAT_ANY };

struct Input {
    Slice<Str> lines;
    u32        fields;  // FIELD_*
    iz         bad;     // malformed TAGS sections and entries
};

// Read a file's lines, rewritten if it is a reference, and unless they
// are ordered, sorted without duplicates.
static Input readinput(Arena *a, Str path, i32 format, Str only,
                       u32 rewrites, b32 ordered)
{
    Input r    = {};
    Str   text = readfile(a, path);
    if (!text.data) {
        fprintf(stderr, "tagdiff: cannot read %.*s\n",
                (int)path.len, (char *)path.data);
        exit(2);
    }

    if (format == FORMAT_ANY) {
        format = startswith(text, "\f") ? FORMAT_ETAGS : FORMAT_TAGS;
    }
    if (format == FORMAT_TAGS) {
        r.lines  = taglines(a, text, only);
        r.fields = fieldsof(text);
    } else if (text.len && !startswith(text, "\f\n")) {
        r.bad = 1;
    } else {
        Lines l = etagslines(a, text, only);
        r.lines = l.lines;
        r.bad   = l.bad;
    }
    if (r.bad) {
        printf("malformed TAGS file (%td bad sections or entries): ", r.bad);
        printstr(stdout, path);
        fputc('\n', stdout);
    }

    if (!ordered) {
        r.lines = sortunique(r.lines, *a);
    }
    if (rewrites) {
        for (iz i = 0; i < r.lines.len; i++) {
            r.lines[i] = rewrite(a, rewrites, r.fields, r.lines[i]);
        }
        if (!ordered) {
            sort(r.lines.data, r.lines.len, strless, *a);
        }
    }
    return r;
}


// Keys

// A tag's key: its name, file, and line.
static Str keyof(Arena *a, Str line)
{
    Cut name = cut(line, '\t');
    Cut file = cut(name.tail, '\t');
    if (!file.ok) {
        return line;
    }
    Str head = takehead(line, name.head.len + file.head.len + 2);
    Str num  = field(line, "line");
    if (!num.data) {
        Str addr = file.tail;
        iz  at   = fieldsat(line);
        if (at-3 >= head.len) {
            addr = takehead(addr, at-3-head.len);
        }
        iz n = 0;
        for (; n<addr.len && digit(addr[n]); n++) {}
        num = n ? takehead(addr, n) : addr;
    }
    if (num.data == head.data+head.len) {
        return takehead(line, head.len+num.len);
    }
    return concat(a, head, num);
}

struct Keyed {
    Str key;
    Str line;
};

static b32 keyedless(Keyed *a, Keyed *b)
{
    i32 c = compare(a->key, b->key);
    return c<0 || (!c && compare(a->line, b->line)<0);
}

static Keyed *bykey(Arena *a, Slice<Str> lines)
{
    Keyed *r = alloc<Keyed>(a, lines.len);
    for (iz i = 0; i < lines.len; i++) {
        r[i].key  = keyof(a, lines[i]);
        r[i].line = lines[i];
    }
    sort(r, lines.len, keyedless, *a);
    return r;
}

enum : i32 { KEY_CLEAN, KEY_DIRTY, KEY_COMPARED, KEY_SKIPPED };

struct Keys {
    Map<i32> *map;       // KEY_*, absent for clean keys not yet seen
    iz        compared;  // clean keys seen
    iz        skipped;   // dirty keys seen
};

// Mark the keys whose lines differ between the base files.
static Keys dirtykeys(Arena *a, Slice<Str> rb, Slice<Str> ob)
{
    Keys   k = {};
    Keyed *r = bykey(a, rb);
    Keyed *o = bykey(a, ob);
    iz     i = 0;
    iz     j = 0;
    while (i<rb.len || j<ob.len) {
        b32 takeref = j==ob.len || (i<rb.len && compare(r[i].key, o[j].key)<=0);
        Str key     = takeref ? r[i].key : o[j].key;
        iz  iend    = i;
        iz  jend    = j;
        for (; iend<rb.len && r[iend].key==key; iend++) {}
        for (; jend<ob.len && o[jend].key==key; jend++) {}
        b32 same = iend-i == jend-j;
        for (iz n = 0; same && n < iend-i; n++) {
            same = r[i+n].line == o[j+n].line;
        }
        if (!same) {
            *upsert(&k.map, key, a) = KEY_DIRTY;
        }
        i = iend;
        j = jend;
    }
    return k;
}

// Keep the lines whose keys are clean, counting the keys seen.
static Slice<Str> keepclean(Arena *a, Keys *k, Slice<Str> lines)
{
    iz n = 0;
    for (iz i = 0; i < lines.len; i++) {
        i32 *state = upsert(&k->map, keyof(a, lines[i]), a);
        if (*state == KEY_CLEAN) {
            *state = KEY_COMPARED;
            k->compared++;
        } else if (*state == KEY_DIRTY) {
            *state = KEY_SKIPPED;
            k->skipped++;
        }
        if (*state == KEY_COMPARED) {
            lines.data[n++] = lines[i];
        }
    }
    lines.len = n;
    return lines;
}


// Comparison

struct Tally {
    Entry *ledger;
    iz     show;
    iz     shown;
    iz     missing;
    iz     extra;
    iz     reordered;
    iz     unexplained;
};

// Count a difference, unless the ledger explains it.
static void differ(Tally *t, u8 sign, Str line)
{
    if (sign == '-') {
        t->missing++;
    } else {
        t->extra++;
    }
    if (!explained(t->ledger, sign, line)) {
        t->unexplained++;
        if (t->shown++ < t->show) {
            fputc(sign, stdout);
            fputc(' ', stdout);
            printstr(stdout, line);
            fputc('\n', stdout);
        }
    }
}

// Count a file's divergence in order, unless a file rule explains it.
static void misorder(Tally *t, Str file, Str what, Str ref, Str ours)
{
    t->reordered++;
    if (!explainedfile(t->ledger, file)) {
        t->unexplained++;
        if (t->shown++ < t->show) {
            fputs("~ ", stdout);
            printstr(stdout, file);
            fputs(": ", stdout);
            printstr(stdout, what);
            fputs("\n  - ", stdout);
            printstr(stdout, ref);
            fputs("\n  + ", stdout);
            printstr(stdout, ours);
            fputc('\n', stdout);
        }
    }
}

struct Seq {
    Str line;
    iz  at;    // its place in file order
};

static b32 seqless(Seq *a, Seq *b)
{
    return compare(a->line, b->line) < 0;
}

static Seq *bylines(Arena *a, Slice<Str> lines, b32 sorted)
{
    Seq *r = alloc<Seq>(a, lines.len);
    for (iz i = 0; i < lines.len; i++) {
        r[i].line = lines[i];
        r[i].at   = i;
    }
    if (!sorted) {
        sort(r, lines.len, seqless, *a);
    }
    return r;
}

// The lines marked in a list of flags, in order.
static Slice<Str> marked(Arena *a, Slice<Str> lines, b32 *marks)
{
    Slice<Str> r = {};
    r.data = alloc<Str>(a, lines.len);
    for (iz i = 0; i < lines.len; i++) {
        if (marks[i]) {
            r.data[r.len++] = lines[i];
        }
    }
    r.cap = lines.len;
    return r;
}

// Files in the order of their first lines.
static Slice<Str> fileorder(Arena *a, Slice<Str> lines)
{
    Map<b32>  *seen = 0;
    Slice<Str> r    = {};
    r.data = alloc<Str>(a, lines.len);
    r.cap  = lines.len;
    for (iz i = 0; i < lines.len; i++) {
        Str  file = fileof(lines[i]);
        b32 *dup  = upsert(&seen, file, a);
        if (!*dup) {
            *dup = 1;
            r.data[r.len++] = file;
        }
    }
    return r;
}

struct Placed {
    Str file;
    Str line;
};

static b32 placedless(Placed *a, Placed *b)
{
    return compare(a->file, b->file) < 0;
}

// Lines grouped by file, each file's in order.
static Placed *byfile(Arena *a, Slice<Str> lines)
{
    Placed *r = alloc<Placed>(a, lines.len);
    for (iz i = 0; i < lines.len; i++) {
        r[i].file = fileof(lines[i]);
        r[i].line = lines[i];
    }
    sort(r, lines.len, placedless, *a);
    return r;
}

// Compare the order of the same lines in each: by file, then by line
// within each file, counting each file's first divergence.
static void compareorder(Tally *t, Slice<Str> ref, Slice<Str> ours,
                         Arena scratch)
{
    assert(ref.len == ours.len);
    Slice<Str> rf = fileorder(&scratch, ref);
    Slice<Str> of = fileorder(&scratch, ours);
    assert(rf.len == of.len);
    for (iz i = 0; i < rf.len; i++) {
        if (!(rf[i] == of[i])) {
            misorder(t, rf[i], "files differ in order", rf[i], of[i]);
            break;
        }
    }

    Placed *r = byfile(&scratch, ref);
    Placed *o = byfile(&scratch, ours);
    for (iz i = 0; i < ref.len;) {
        Str file = r[i].file;
        iz  end  = i;
        for (; end<ref.len && r[end].file==file; end++) {}
        for (iz n = i; n < end; n++) {
            assert(o[n].file == file);
            if (!(r[n].line == o[n].line)) {
                Str what = "lines differ in order";
                misorder(t, file, what, r[n].line, o[n].line);
                break;
            }
        }
        i = end;
    }
}

// Compare the lines as multisets, then if ordered, the order of those
// in both.
static void comparelines(Tally *t, Slice<Str> ref, Slice<Str> ours,
                         b32 ordered, Arena scratch)
{
    Seq *r     = bylines(&scratch, ref, !ordered);
    Seq *o     = bylines(&scratch, ours, !ordered);
    b32 *rboth = alloc<b32>(&scratch, ref.len);
    b32 *oboth = alloc<b32>(&scratch, ours.len);
    iz   i     = 0;
    iz   j     = 0;
    while (i<ref.len || j<ours.len) {
        if (j==ours.len || (i<ref.len && compare(r[i].line, o[j].line)<0)) {
            differ(t, '-', r[i++].line);
        } else if (i==ref.len || compare(o[j].line, r[i].line)<0) {
            differ(t, '+', o[j++].line);
        } else {
            rboth[r[i++].at] = 1;
            oboth[o[j++].at] = 1;
        }
    }
    if (ordered) {
        Slice<Str> rb = marked(&scratch, ref, rboth);
        Slice<Str> ob = marked(&scratch, ours, oboth);
        compareorder(t, rb, ob, scratch);
    }
}

static i32 usage()
{
    fputs("usage: tagdiff [--ledger FILE] [--scope NAME] [--only KINDS] "
          "[--show N]\n               [--ordered] [--base RB OB] [--etags] "
          "REF OURS\n", stderr);
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
    b32 ordered    = 0;
    b32 etags      = 0;
    Str bases[2]   = {};
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
        } else if (arg == "--ordered") {
            ordered = 1;
        } else if (arg == "--etags") {
            etags = 1;
        } else if (arg=="--base" && i+2<argc) {
            bases[0] = v;
            bases[1] = cstr(argv[i+2]);
            i += 2;
        } else if (nfiles<2 && !startswith(arg, "--")) {
            files[nfiles++] = arg;
        } else {
            return usage();
        }
    }
    if (nfiles != 2) {
        return usage();
    }

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

    i32   format = etags ? FORMAT_ETAGS : FORMAT_TAGS;
    Input ref    = readinput(&a, files[0], format, only, rewrites, ordered);
    Input ours   = readinput(&a, files[1], format, only, 0, ordered);
    iz    bad    = ref.bad + ours.bad;

    // Only the tags on which the base runs agree
    Keys keys = {};
    if (bases[0].data) {
        Input rb = readinput(&a, bases[0], FORMAT_ANY, only, rewrites, ordered);
        Input ob = readinput(&a, bases[1], FORMAT_ANY, only, 0, ordered);
        bad       += rb.bad + ob.bad;
        keys       = dirtykeys(&a, rb.lines, ob.lines);
        ref.lines  = keepclean(&a, &keys, ref.lines);
        ours.lines = keepclean(&a, &keys, ours.lines);
    }

    Tally t = {};
    t.ledger = ledger;
    t.show   = show;
    comparelines(&t, ref.lines, ours.lines, ordered, a);

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

    if (bases[0].data) {
        printf("%td keys compared, %td skipped; ", keys.compared, keys.skipped);
    }
    printf("%td ref, %td ours, %td missing, %td extra, ",
           ref.lines.len, ours.lines.len, t.missing, t.extra);
    if (ordered) {
        printf("%td reordered, ", t.reordered);
    }
    printf("%td unexplained, %td stale\n", t.unexplained, stale);
    return t.unexplained || stale || bad ? 1 : 0;
}

// Tags: formatting finished tags into tags-file lines
// This is free and unencumbered software released into the public domain.

// An input file, read whole into memory.
struct Source {
    Str  path;      // as constructed, the input to anonymous names
    Str  outpath;   // the file field: '\' as '/', escaped
    Str  text;      // contents after any UTF-8 BOM
    iz   bom;       // its length, before text, for etags offsets
    i64  mtime;     // for the file tag's epoch: field
    u32  anonhash;
    i32  lang;
    b32  header;
};

// How tag lines are written, when the options change it from the
// default: line: and the default fields, mixed addresses, patterns of at
// most PATTERNLIMIT bytes.
struct Format {
    u64 fields;    // FIELD_* bits
    iz  patmax;    // the pattern length limit, a huge one for none
    i32 excmd;     // EXCMD_*
    b32 nofields;  // --format=0 or 1, or etags: no ;" and no fields
    b32 ectags;    // --output-format=e-ctags
    b32 etags;     // Emacs TAGS entries instead of lines
};

// One parse of one file. A failed first pass discards its lines but
// keeps the anonymous counter, so a second pass numbers on from it, and
// may leave the second pass other state, as Universal Ctags does.
struct File {
    Source *src;
    Kinds  *kinds;
    Format *fmt;    // null for the default format
    u32     extras;
    Macros *macros;  // -I, null for none
    Buf    *out;
    iz      mark;   // where this file's lines begin in out
    iz      ntags;
    i32     anon;
    i32     carry;  // left by a failed pass for the next, 0 for nothing
    b32     nofast;
};

struct Tag {
    Str name;
    Str scopekind;  // long kind name of the enclosing scope, or null
    Str scope;
    Str typekind;   // "typename", "struct", ..., or null for no typeref
    Str type;
    i32 kind;       // index into the language's kind table
    i32 line;
    iz  linepos;    // offset of the line's first byte in the text
    b32 filescope;
    b32 truncate;   // cut the pattern just after the name
    b32 lineaddr;   // address by line number instead of a pattern
    b32 anon;       // named for an unnamed entity
    b32 qualified;  // a copy named by its scope (--extras=+q)
    Str aliased;    // C++ namespace aliases: the "name:" field
    Str inherits;   // base classes, a parent type or module
    Str access;     // "public", "private", "protected", or null
    Str impl;       // "abstract", "deferred", "non_overridable", or null
    Str signature;  // parameters
};

// A finished tags-file line, keyed for sorting by its first bytes.
struct Line {
    u64 key;   // first eight bytes, big endian, zero padded
    u8 *data;
    iz  len;   // without the newline
};

enum : i32 { PATTERNLIMIT = 96 };  // --pattern-length-limit's default

// djb2 over the path's bytes, as Universal Ctags names anonymous tags.
static u32 anonhash(Str path)
{
    u32 h = 5381;
    for (iz i = 0; i < path.len; i++) {
        h = h*33 + path[i];
    }
    return h;
}

// Name the next anonymous entity in a file: __anon, the path hash, a
// per-file counter, and the kind's index, in hex.
static Str anonname(File *f, Arena *a, i32 kind)
{
    Buf b(a, 32);
    print(&b, "__anon");
    printhex(&b, f->src->anonhash, 8);
    u32 n = (u32)++f->anon;
    i32 digits = 2;
    for (u32 t = n>>8; t; t >>= 4) digits++;
    printhex(&b, n, digits);
    printhex(&b, (u32)kind, 2);
    return finish(&b);
}

// Escape a name or field value like Universal Ctags: backslash and
// control bytes, and for tag names a leading space or '!', which would
// otherwise sort among the pseudo-tags.
static void printescaped(Buf *b, Str s, b32 isname)
{
    for (iz i = 0; i < s.len; i++) {
        u8 c = s[i];
        if (c == '\\') {
            print(b, "\\\\");
        } else if (c<0x20 || c==0x7f) {
            switch (c) {
            case '\a': print(b, "\\a"); break;
            case '\b': print(b, "\\b"); break;
            case '\t': print(b, "\\t"); break;
            case '\n': print(b, "\\n"); break;
            case '\v': print(b, "\\v"); break;
            case '\f': print(b, "\\f"); break;
            case '\r': print(b, "\\r"); break;
            default:
                print(b, "\\x");
                putbyte(b, "0123456789ABCDEF"[c>>4]);
                putbyte(b, "0123456789ABCDEF"[c&15]);
            }
        } else if (isname && !i && (c==' ' || c=='!')) {
            print(b, c==' ' ? Str("\\x20") : Str("\\x21"));
        } else {
            putbyte(b, c);
        }
    }
}

// Offset of the last occurrence of needle in s, or -1.
static iz findlast(Str s, Str needle)
{
    if (!needle.len || needle.len>s.len) {
        return -1;
    }
    for (iz i = s.len - needle.len; i >= 0; i--) {
        if (s[i]==needle[0] && takehead(cuthead(s, i), needle.len)==needle) {
            return i;
        }
    }
    return -1;
}

// The physical line starting at pos, without its line ending. Only LF
// ends a line, and a CR just before it belongs to the ending.
struct Physline {
    Str text;
    b32 newline;  // ended by LF, not by the end of the file
};

static Physline lineat(Str text, iz pos)
{
    Physline r = {};
    iz   end = pos;
    for (; end<text.len && text[end]!='\n'; end++) {}
    r.newline = end < text.len;
    r.text    = span(text.data+pos, text.data+end);
    if (r.newline && r.text.len && r.text[r.text.len-1]=='\r') {
        r.text.len--;
    }
    return r;
}

// A search pattern for the tag's line, as Vim reads it: nomagic, with
// only '\', '/', and a final '$' escaped. Like Universal Ctags it holds
// at most max bytes after escaping, plus enough to finish a UTF-8
// sequence, and ends with the "$" anchor only when it reaches the end of
// the line. Unlike it, a lone CR or a NUL ends the pattern without the
// anchor, since Vim would not find the line, and a '$' where the pattern
// is cut is escaped, so it cannot be read as an anchor.
static void printpattern(Buf *b, Str text, Tag *t, iz max)
{
    Physline l = lineat(text, t->linepos);
    if (t->truncate) {
        // Through the last occurrence of the name, plus one byte
        iz at = findlast(l.text, t->name);
        if (at >= 0) {
            iz keep = at + t->name.len + 1;
            if (keep <= l.text.len) {
                l.text    = takehead(l.text, keep);
                l.newline = 0;
            }
        }
    }

    print(b, "/^");
    iz  len   = 0;
    i32 extra = 0;  // continuation bytes past the limit
    b32 cut   = 0;
    for (iz i = 0; i < l.text.len; i++) {
        u8 c = l.text[i];
        if (c=='\r' || !c) {
            cut = 1;
            break;
        }
        if (len>=max && ((c&0xc0)!=0x80 || ++extra>3)) {
            cut = 1;
            break;
        }
        b32 last = 0;  // the last byte written
        if (c == '$') {
            iz  n    = i + 1;
            b32 more = n < l.text.len;
            u8  next = more ? l.text[n] : 0;
            last = !more || next=='\r' || !next ||
                   (len+1>=max && (next&0xc0)!=0x80);
        }
        if (c=='\\' || c=='/' || last) {
            putbyte(b, '\\');
            len++;
        }
        putbyte(b, c);
        len++;
    }
    if (l.newline && !cut) {
        putbyte(b, '$');
    }
    putbyte(b, '/');
}

// The format the options ask for, or null for the default, which
// tagemit writes itself.
static Format *newformat(Options *opt, Arena *a)
{
    u64 mask    = ((u64)1<<NCOMMONFIELDS) - 1;
    mask |= (u64)1 << FIELD_CXXNAME;
    b32 fields  = ((opt->fields ^ defaultfields()) & mask) != 0;
    b32 address = opt->excmd!=EXCMD_MIXED || opt->patmax!=PATTERNLIMIT;
    if (!fields && !address && opt->format==2 && opt->writer==WRITER_UCTAGS) {
        return 0;
    }

    iz      huge = (iz)(~(uz)0 >> 1);
    Format *f    = alloc<Format>(a);
    f->fields   = opt->fields;
    f->patmax   = !opt->patmax || (u64)opt->patmax>(u64)huge ? huge
                                                             : (iz)opt->patmax;
    f->excmd    = opt->excmd;
    f->etags    = opt->writer == WRITER_ETAGS;
    f->nofields = opt->format<2 || f->etags;
    f->ectags   = opt->writer == WRITER_ECTAGS;
    return f;
}

// Will the file's lines show this field? Parsers build values that cost
// something, such as signatures, only then.
static b32 wantfield(File *f, i32 field)
{
    Format *fmt = f->fmt;
    return fmt && !fmt->nofields && fieldon(fmt->fields, field);
}

// Would a reader end a field or the line inside it: at a tab, a
// newline, or a NUL, which ends a C string (X4)?
static b32 hasbreak(Str s)
{
    for (iz i = 0; i < s.len; i++) {
        if (s[i]=='\t' || s[i]=='\n' || !s[i]) {
            return 1;
        }
    }
    return 0;
}

// Begin a field, after ;" if it is the first.
static void beginfield(Buf *b, b32 *any, Str key)
{
    print(b, *any ? Str("\t") : Str(";\"\t"));
    *any = 1;
    if (key.data) {
        print(b, key);
        putbyte(b, ':');
    }
}

// Append a tag as the format asks (OUT-6, FLD-5): the address by excmd,
// then the enabled fields in Universal Ctags' order (FLD-4), the first
// after ;", which unlike Universal Ctags includes the parser's (D10).
// E-ctags writes names, paths, scopes, and signatures as they are, so it
// drops tags it could not write that way (FLD-6), also for a NUL (X4).
static void tagline(File *f, Tag *t)
{
    Source *src = f->src;
    Format *fmt = f->fmt;
    Buf    *b   = f->out;
    u64     on  = fmt->fields;
    b32     raw = fmt->ectags;
    b32     ext = !fmt->nofields;  // the fields are written
    if (raw && (hasbreak(t->name) || hasbreak(src->path) ||
                (ext && fieldon(on, FIELD_SCOPE) && hasbreak(t->scope)) ||
                (ext && fieldon(on, FIELD_SIGNATURE) &&
                 hasbreak(t->signature)) ||
                (fieldon(on, FIELD_CXXNAME) && hasbreak(t->aliased)))) {
        return;
    }

    if (raw) {
        print(b, t->name);
    } else {
        printescaped(b, t->name, 1);
    }
    putbyte(b, '\t');
    print(b, src->outpath);
    putbyte(b, '\t');
    i32 excmd = fmt->excmd;
    if (excmd==EXCMD_NUMBER || (t->lineaddr && excmd!=EXCMD_PATTERN)) {
        print(b, (i64)t->line);
    } else {
        if (excmd == EXCMD_COMBINE) {
            print(b, (i64)t->line);
            putbyte(b, ';');
        }
        printpattern(b, src->text, t, fmt->patmax);
    }

    if (ext) {
        b32 any  = 0;
        b32 kind = fieldon(on, FIELD_KINDLONG) || fieldon(on, FIELD_KIND);
        if (kind) {
            Str key = fieldon(on, FIELD_KINDKEY) ? Str("kind") : Str{};
            beginfield(b, &any, key);
            if (fieldon(on, FIELD_KINDLONG)) {
                print(b, kindname(src->lang, t->kind));
            } else {
                putbyte(b, kindletter(src->lang, t->kind));
            }
        }
        if (fieldon(on, FIELD_LINE)) {
            beginfield(b, &any, "line");
            print(b, (i64)t->line);
        }
        if (fieldon(on, FIELD_LANGUAGE)) {
            beginfield(b, &any, "language");
            print(b, langnames[src->lang]);
        }
        if (fieldon(on, FIELD_SCOPE) && t->scopekind.data) {
            Str key = fieldon(on, FIELD_SCOPEKEY) ? Str("scope") : Str{};
            beginfield(b, &any, key);
            print(b, t->scopekind);
            putbyte(b, ':');
            if (raw) {
                print(b, t->scope);
            } else {
                printescaped(b, t->scope, 0);
            }
        }
        if (fieldon(on, FIELD_TYPEREF) && t->typekind.data) {
            beginfield(b, &any, "typeref");
            print(b, t->typekind);
            putbyte(b, ':');
            printescaped(b, t->type, 0);
        }
        if (fieldon(on, FIELD_FILE) && t->filescope) {
            beginfield(b, &any, "file");
        }
        if (fieldon(on, FIELD_INHERITS) && t->inherits.data) {
            beginfield(b, &any, "inherits");
            printescaped(b, t->inherits, 0);
        }
        if (fieldon(on, FIELD_ACCESS) && t->access.data) {
            beginfield(b, &any, "access");
            print(b, t->access);
        }
        if (fieldon(on, FIELD_IMPL) && t->impl.data) {
            beginfield(b, &any, "implementation");
            print(b, t->impl);
        }
        if (fieldon(on, FIELD_SIGNATURE) && t->signature.data) {
            beginfield(b, &any, "signature");
            if (raw) {
                print(b, t->signature);
            } else {
                printescaped(b, t->signature, 0);
            }
        }
        if (fieldon(on, FIELD_ROLES)) {
            beginfield(b, &any, "roles");
            print(b, "def");  // only reference tags have others
        }
        if (fieldon(on, FIELD_EXTRAS) && (t->filescope || t->qualified ||
                                          t->anon)) {
            Str extras[] = {
                t->filescope ? Str("fileScope") : Str{},
                t->qualified ? Str("qualified") : Str{},
                t->anon      ? Str("anonymous") : Str{},
            };
            beginfield(b, &any, "extras");
            Str sep = "";
            for (iz i = 0; i < countof(extras); i++) {
                if (extras[i].data) {
                    print(b, sep);
                    print(b, extras[i]);
                    sep = ",";
                }
            }
        }

        // The parser's fields, after ;" even when alone (D10)
        if (fieldon(on, FIELD_CXXNAME) && t->aliased.data) {
            beginfield(b, &any, "name");
            printescaped(b, t->aliased, 0);
        }
    }
    putbyte(b, '\n');
    f->ntags++;
}

// Append a tag as an Emacs TAGS entry (OPT-21): its line as read raw,
// with a CR before the LF dropped, cut after the name as a pattern is,
// or kept with its LF when the name is not in it, then cut to the length
// limit; DEL, the name, SOH, the line number, and where the line starts
// in the file, the BOM counted.
static void etagsline(File *f, Tag *t)
{
    Source *src  = f->src;
    Buf    *b    = f->out;
    Str     text = src->text;
    if (t->linepos >= text.len) {
        return;  // no line to show
    }

    Cut l      = cut(cuthead(text, t->linepos), '\n');
    Str line   = l.head;
    b32 keeplf = 0;
    if (l.ok && line.len && line[line.len-1]=='\r') {
        line.len--;
    }
    if (t->truncate) {
        iz at = findlast(line, t->name);
        if (at < 0) {
            keeplf = l.ok;
        } else {
            iz end = at + t->name.len;
            line = takehead(line, end + (end<line.len));
        }
    }
    iz max = f->fmt->patmax;
    if (line.len+keeplf > max) {
        iz end = max;  // and up to 3 UTF-8 continuation bytes
        for (; end<line.len && end-max<3 && (line[end]&0xc0)==0x80; end++) {}
        line   = takehead(line, end);
        keeplf = 0;
    }

    print(b, line);
    print(b, keeplf ? Str("\n\x7f") : Str("\x7f"));
    print(b, t->name);
    putbyte(b, 0x01);
    print(b, (i64)t->line);
    putbyte(b, ',');
    print(b, (i64)(src->bom + t->linepos));
    putbyte(b, '\n');
    f->ntags++;
}

// Append the file's own tag, of the file kind, which the inputFile extra
// adds after its other tags (EX-7): the name given, the address line 1
// whatever the excmd, and of the fields only the kind, line, language,
// roles, extras, and epoch. E-ctags writes the name and path as they
// are, so drops the tag if either would break the line (FLD-6). Etags
// has only the name and line, at offset 0 (OPT-21).
static void filetag(File *f, Str name)
{
    Source *src = f->src;
    Format *fmt = f->fmt;
    Buf    *b   = f->out;
    u64     on  = fmt ? fmt->fields : defaultfields();
    b32     raw = fmt && fmt->ectags;
    if (fmt && fmt->etags) {
        putbyte(b, 0x7f);
        print(b, name);
        print(b, "\x01" "1,0\n");
        f->ntags++;
        return;
    } else if (raw && (hasbreak(name) || hasbreak(src->path))) {
        return;
    }

    if (raw) {
        print(b, name);
    } else {
        printescaped(b, name, 1);
    }
    putbyte(b, '\t');
    print(b, src->outpath);
    print(b, "\t1");

    if (!fmt || !fmt->nofields) {
        b32 any = 0;
        if (fieldon(on, FIELD_KINDLONG) || fieldon(on, FIELD_KIND)) {
            Str key = fieldon(on, FIELD_KINDKEY) ? Str("kind") : Str{};
            beginfield(b, &any, key);
            print(b, fieldon(on, FIELD_KINDLONG) ? Str("file") : Str("F"));
        }
        if (fieldon(on, FIELD_LINE)) {
            beginfield(b, &any, "line");
            print(b, "1");
        }
        if (fieldon(on, FIELD_LANGUAGE)) {
            beginfield(b, &any, "language");
            print(b, langnames[src->lang]);
        }
        if (fieldon(on, FIELD_ROLES)) {
            beginfield(b, &any, "roles");
            print(b, "def");
        }
        if (fieldon(on, FIELD_EXTRAS)) {
            beginfield(b, &any, "extras");
            print(b, "inputFile");
        }
        if (fieldon(on, FIELD_EPOCH)) {
            beginfield(b, &any, "epoch");
            print(b, src->mtime);
        }
    }
    putbyte(b, '\n');
    f->ntags++;
}

// Append a finished tag to its file's output as one tags-file line:
// name, file, address, then the kind, line, scope, typeref, and file:
// fields in the order Universal Ctags writes them with --fields=+n, or
// as other options ask (tagline, etagsline). File-scoped and anonymous
// tags are dropped without their extras.
static void tagemit(File *f, Tag *t)
{
    Source *src = f->src;
    Buf    *b   = f->out;
    assert(kindon(f->kinds, src->lang, t->kind));
    if ((t->filescope && !(f->extras & EXTRA_FILESCOPE)) ||
        (t->anon      && !(f->extras & EXTRA_ANONYMOUS))) {
        return;
    }
    if (f->fmt) {
        if (f->fmt->etags) {
            etagsline(f, t);
        } else {
            tagline(f, t);
        }
        return;
    }

    printescaped(b, t->name, 1);
    putbyte(b, '\t');
    print(b, src->outpath);
    putbyte(b, '\t');
    if (t->lineaddr) {
        print(b, (i64)t->line);
    } else {
        printpattern(b, src->text, t, PATTERNLIMIT);
    }
    print(b, ";\"\t");
    putbyte(b, kindletter(src->lang, t->kind));
    print(b, "\tline:");
    print(b, (i64)t->line);
    if (t->scopekind.data) {
        putbyte(b, '\t');
        print(b, t->scopekind);
        putbyte(b, ':');
        printescaped(b, t->scope, 0);
    }
    if (t->typekind.data) {
        print(b, "\ttyperef:");
        print(b, t->typekind);
        putbyte(b, ':');
        printescaped(b, t->type, 0);
    }
    if (t->filescope) {
        print(b, "\tfile:");
    }
    if (t->aliased.data) {
        print(b, "\tname:");
        printescaped(b, t->aliased, 0);
    }
    putbyte(b, '\n');
    f->ntags++;
}

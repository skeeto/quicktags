// Emacs tags checks, shared by the fuzzers (main_test.cpp, main_fuzz.cpp)

// A number of decimal digits, or -1.
static iz etagsnum(Str s)
{
    iz n = 0;
    for (iz i = 0; i < s.len; i++) {
        if (!digit(s[i]) || n>99999999) {
            return -1;
        }
        n = n*10 + s[i] - '0';
    }
    return s.len ? n : -1;
}

// The distinct names and line numbers of tags, sorted, from etags
// entries (names escaped as tag lines escape them) or from the line:
// fields of tag lines.
static Slice<Str> etagsdistinct(Arena *a, Slice<Str> keys)
{
    sort(keys.data, keys.len, strless, *a);
    Slice<Str> r = {};
    for (iz i = 0; i < keys.len; i++) {
        if (!i || !(keys[i] == keys[i-1])) {
            r = push(a, r, keys[i]);
        }
    }
    return r;
}

// Is the Emacs tags output for one file well formed (OPT-21)? One
// section, for the path, whose size counts its entries' bytes; in each
// entry, the offset where a line of the source starts, the number of
// that line, and a text that begins that line, with an LF at its end
// only where the line ends. Unless deflt is null, the default output for
// the file, its tags by name and line must be those. The problem, or
// null data.
static Str etagslint(Arena a, Str out, Str path, Str src, Str deflt)
{
    Str head = concat(&a, concat(&a, "\f\n", path), ",");
    if (!startswith(out, head)) {
        return "no section for the file";
    }
    Cut size = cut(cuthead(out, head.len), '\n');
    if (!size.ok || etagsnum(size.head)!=size.tail.len) {
        return "the section's size is not its bytes";
    }

    iz         bom  = startswith(src, "\xef\xbb\xbf") ? 3 : 0;
    Slice<Str> tags = {};
    for (Str rest = size.tail; rest.len;) {
        // The text runs to the last DEL of the entry's line, or with its
        // LF, to the end of a line not ending as an entry does, just
        // before a DEL
        Cut l     = cut(rest, '\n');
        Str text  = {};
        iz  soh   = l.head.len - 1;
        for (; soh>=0 && l.head[soh]!=0x01; soh--) {}
        Cut tail  = cut(cuthead(l.head, soh+1), ',');
        b32 whole = soh>=0 && tail.ok && etagsnum(tail.head)>=0 &&
                    etagsnum(tail.tail)>=0;
        b32 haslf = !whole && l.ok && startswith(l.tail, "\x7f");
        if (haslf) {
            text = l.head;
            l    = cut(l.tail, '\n');
        } else {
            iz del = l.head.len - 1;
            for (; del>=0 && l.head[del]!=0x7f; del--) {}
            if (del < 0) {
                return "an entry without a DEL";
            }
            text   = takehead(l.head, del);
            l.head = cuthead(l.head, del);
        }
        rest = l.tail;
        if (!l.ok) {
            return "an entry without an LF";
        }

        Cut name = cut(cuthead(l.head, 1), 0x01);
        Cut pos  = cut(name.tail, ',');
        iz  line = etagsnum(pos.head);
        iz  off  = etagsnum(pos.tail);
        if (!name.ok || !pos.ok || line<1 || off<0 || off>src.len) {
            return "an entry without a line and offset";
        } else if (off!=bom && (off<bom || src[off-1]!='\n')) {
            return "an offset not at the start of a line";
        }
        iz n = 1;
        for (iz i = 0; i < off; i++) {
            n += src[i] == '\n';
        }
        if (n != line) {
            return "a line number not the offset's";
        }
        Str at = cuthead(src, off);
        if (!startswith(at, text)) {
            return "a text not the start of its line";
        } else if (haslf && !startswith(cuthead(at, text.len), "\n") &&
                   !startswith(cuthead(at, text.len), "\r\n")) {
            return "a text with an LF not its line's";
        }

        Buf b(&a, name.head.len+32);
        printescaped(&b, name.head, 1);
        putbyte(&b, '\t');
        print(&b, (i64)line);
        tags = push(&a, tags, finish(&b));
    }
    if (!deflt.data) {
        return {};
    }

    Slice<Str> want = {};
    for (Cut c = cut(deflt, '\n'); c.ok; c = cut(c.tail, '\n')) {
        Str name = cut(c.head, '\t').head;
        iz  at   = find(c.head, "\tline:");
        if (at < 0) {
            return "a default tag without a line";
        }
        Str line = cut(cuthead(c.head, at+6), '\t').head;
        want = push(&a, want, concat(&a, concat(&a, name, "\t"), line));
    }
    Slice<Str> x = etagsdistinct(&a, tags);
    Slice<Str> y = etagsdistinct(&a, want);
    if (x.len != y.len) {
        return "not the default tags";
    }
    for (iz i = 0; i < x.len; i++) {
        if (!(x[i] == y[i])) {
            return "not the default tags";
        }
    }
    return {};
}

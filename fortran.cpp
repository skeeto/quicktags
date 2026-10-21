// Fortran: tags from fixed-form and free-form source
// This is free and unencumbered software released into the public domain.
//
// A recursive descent over a token stream, statement by statement, with
// Universal Ctags' heuristics (docs/spec.md, FTN-*). Tags are written as
// they are found, in the scope of the innermost named enclosing unit, and
// labels as the characters are read, so the order of reads matters.


// Characters

enum : i32 { FTN_EOF = -1 };

// Fixed-form lines, by their first six columns
enum : i32 {
    FL_INVALID,    // not fixed form
    FL_COMMENT,
    FL_CONTINUED,
    FL_EOF,
    FL_INITIAL,    // begins a statement
    FL_SHORT,      // ends before column 6
};

// Tokens
enum : i32 {
    FT_UNDEF, FT_EOF, FT_EOS, FT_IDENT, FT_KEYWORD, FT_NUMBER, FT_STRING,
    FT_OP, FT_COMMA, FT_COLON, FT_COLONS, FT_PERCENT, FT_LPAREN, FT_RPAREN,
    FT_LSQUARE, FT_RSQUARE,
};

// Keywords, in the order of ftnwords
enum : i32 {
    FW_ABSTRACT, FW_ALLOCATABLE, FW_ASSIGNMENT, FW_ASSOCIATE, FW_AUTOMATIC,
    FW_BIND, FW_BLOCK, FW_BYTE, FW_CEXTERNAL, FW_CGLOBAL, FW_CHARACTER,
    FW_CLASS, FW_CODIMENSION, FW_COMMON, FW_COMPLEX, FW_CONTAINS, FW_DATA,
    FW_DEFERRED, FW_DIMENSION, FW_DLL_EXPORT, FW_DLL_IMPORT, FW_DO,
    FW_DOUBLE, FW_ELEMENTAL, FW_END, FW_ENTRY, FW_ENUM, FW_ENUMERATOR,
    FW_EQUIVALENCE, FW_EXTENDS, FW_EXTERNAL, FW_FINAL, FW_FORALL,
    FW_FORMAT, FW_FUNCTION, FW_GENERIC, FW_IF, FW_IMPLICIT, FW_IMPORT,
    FW_INCLUDE, FW_INLINE, FW_INTEGER, FW_INTENT, FW_INTERFACE,
    FW_INTRINSIC, FW_KIND, FW_LEN, FW_LOGICAL, FW_MAP, FW_MODULE,
    FW_NAMELIST, FW_NON_OVERRIDABLE, FW_NOPASS, FW_OPERATOR, FW_OPTIONAL,
    FW_PARAMETER, FW_PASCAL, FW_PASS, FW_PEXTERNAL, FW_PGLOBAL, FW_POINTER,
    FW_PRECISION, FW_PRIVATE, FW_PROCEDURE, FW_PROGRAM, FW_PROTECTED,
    FW_PUBLIC, FW_PURE, FW_REAL, FW_RECORD, FW_RECURSIVE, FW_SAVE,
    FW_SELECT, FW_SEQUENCE, FW_STATIC, FW_STDCALL, FW_STRUCTURE,
    FW_SUBMODULE, FW_SUBROUTINE, FW_TARGET, FW_THEN, FW_TYPE, FW_UNION,
    FW_USE, FW_VALUE, FW_VIRTUAL, FW_VOLATILE, FW_WHERE, FW_WHILE,
    NFWORDS
};

static Str ftnwords[NFWORDS] = {
    "abstract", "allocatable", "assignment", "associate", "automatic",
    "bind", "block", "byte", "cexternal", "cglobal", "character",
    "class", "codimension", "common", "complex", "contains", "data",
    "deferred", "dimension", "dll_export", "dll_import", "do",
    "double", "elemental", "end", "entry", "enum", "enumerator",
    "equivalence", "extends", "external", "final", "forall",
    "format", "function", "generic", "if", "implicit", "import",
    "include", "inline", "integer", "intent", "interface",
    "intrinsic", "kind", "len", "logical", "map", "module",
    "namelist", "non_overridable", "nopass", "operator", "optional",
    "parameter", "pascal", "pass", "pexternal", "pglobal", "pointer",
    "precision", "private", "procedure", "program", "protected",
    "public", "pure", "real", "record", "recursive", "save",
    "select", "sequence", "static", "stdcall", "structure",
    "submodule", "subroutine", "target", "then", "type", "union",
    "use", "value", "virtual", "volatile", "where", "while",
};

struct FtnTok {
    Buf text;
    i32 type;
    i32 word;     // keyword, or -1
    i32 tag;      // kind it was tagged with, or -1
    i32 line;
    iz  linepos;
    b32 hassub;   // a secondary token follows: see ftnsub
    i32 subtype;
    i32 subword;
};

// An entity about to be tagged, or an enclosing unit
struct FtnName {
    Str text;
    i32 type;
    i32 tag;      // kind, or -1: an enclosing unit without a tag
    i32 line;
    iz  linepos;
};

struct Ftn {
    File   *f;
    Arena  *a;
    Str     src;
    iz      pos;       // next byte of src
    i32     line;      // of the last byte read
    iz      linepos;   // first byte of that line
    b32     bol;       // the next byte begins a line
    i32     raw[8];    // bytes put back, read before src
    i32     nraw;
    i32     back;      // a character put back by the lexer
    b32     hasback;
    b32     freeform;
    i32     column;    // fixed: columns read on this line, 0 at a margin
    b32     freefound; // fixed: this is not fixed form
    b32     newline;   // free: the last character was a line end
    FtnTok  tok;
    FtnTok  aux;       // reading secondary tokens
    Slice<FtnName> scopes;
};

static b32 ftndigit(i32 c)
{
    return c>='0' && c<='9';
}

static b32 ftnalpha(i32 c)
{
    return (c>='a' && c<='z') || (c>='A' && c<='Z');
}

static b32 ftnidchar(i32 c)
{
    return ftnalpha(c) || ftndigit(c) || c=='_' || c=='$';
}

static b32 ftnspace(i32 c)
{
    return c==' ' || (c>='\t' && c<='\r');
}

// The next byte of the input, with CR-LF as LF. Like Universal Ctags,
// it counts a line when its first byte is read.
static i32 ftnraw(Ftn *p)
{
    if (p->nraw) {
        return p->raw[--p->nraw];
    }
    if (p->pos == p->src.len) {
        return FTN_EOF;
    }
    if (p->bol) {
        p->bol     = 0;
        p->line++;
        p->linepos = p->pos;
    }
    i32 c = p->src[p->pos++];
    if (c=='\r' && p->pos<p->src.len && p->src[p->pos]=='\n') {
        c = p->src[p->pos++];
    }
    p->bol = c == '\n';
    return c;
}

static void ftnunraw(Ftn *p, i32 c)
{
    assert(p->nraw < countof(p->raw));
    p->raw[p->nraw++] = c;
}

// Read through the end of the line: '\n', or FTN_EOF on the last line.
static i32 ftnskipline(Ftn *p)
{
    i32 c;
    do {
        c = ftnraw(p);
    } while (c!=FTN_EOF && c!='\n');
    return c;
}

static void ftntag(Ftn *, FtnName *, i32 kind);

// Fixed form: read the margin, columns 1-6, of a line, tag its label,
// and classify the line. Column 1 marks comments with any of *Cc!#$Dd,
// a tab ends the margin, and columns 1-5 hold a label or blanks.
static i32 ftnmargin(Ftn *p)
{
    u8  label[5];
    i32 nlabel = 0;
    i32 type   = -1;
    for (i32 col = 0; col<6 && type<0; col++) {
        i32 c = ftnraw(p);
        switch (c) {
        case '*': case 'C': case 'c': case '!':
        case '#': case '$': case 'D': case 'd':
            if (!col) {
                type = FL_COMMENT;
                continue;
            }
        }
        if (c == '\t') {
            type = FL_INITIAL;
        } else if (col == 5) {
            // Column 6: blank or zero begins a statement, anything else
            // continues one, but a label cannot head a continuation
            if (c==' ' || c=='0') {
                type = FL_INITIAL;
            } else {
                type = nlabel ? FL_INVALID : FL_CONTINUED;
            }
        } else if (c == ' ') {
            // blank
        } else if (c == FTN_EOF) {
            type = FL_EOF;
        } else if (c == '\n') {
            type = FL_SHORT;
        } else if (ftndigit(c)) {
            label[nlabel++] = (u8)c;
        } else {
            type = FL_INVALID;
        }
    }
    if (nlabel) {
        FtnName n = {{label, nlabel}, FT_NUMBER, -1, p->line, p->linepos};
        ftntag(p, &n, F_LABEL);
    }
    return type;
}

// Fixed form: the first character of the text after a margin. Blanks
// before it read as one space, except when it is in column 7.
static i32 ftnfixedtext(Ftn *p)
{
    i32 c;
    p->column = 5;
    do {
        c = ftnraw(p);
        p->column++;
    } while (c==' ' || c=='\t');
    if (c == '\n') {
        p->column = 0;  // only blanks: read the next margin
    } else if (p->column > 6) {
        ftnunraw(p, c);
        c = ' ';
    }
    return c;
}

// Fixed form: the next character of a statement. Comment and short
// lines are skipped, a continuation line joins the statement, and a
// line end reads as '\n' only before a line that begins a statement.
// There is no limit at column 72, and '!' outside a string comments out
// the rest of the line. Signs of free form ("&" ending a line, a margin
// that cannot be fixed form) set freefound, and the latter also reads
// as the end of the input, though reading may go on from there.
static i32 ftnfixed(Ftn *p, b32 instring)
{
    b32 eol = 0;
    i32 c   = 0;
    if (p->column) {
        c = ftnraw(p);
        p->column++;
        if (c == '\n') {
            eol = 1;
        } else if (c=='!' && !instring) {
            c   = ftnskipline(p);
            eol = 1;
        } else if (c == '&') {
            i32 next = ftnraw(p);
            if (next == '\n') {
                p->freefound = 1;  // and the line end is lost
            } else {
                ftnunraw(p, next);
            }
        }
        if (eol) {
            p->column = 0;
        }
    }
    while (!p->column) {
        switch (ftnmargin(p)) {
        case FL_INVALID:
            p->freefound = 1;
            return FTN_EOF;
        case FL_COMMENT:
            ftnskipline(p);
            break;
        case FL_SHORT:
            break;
        case FL_EOF:
            p->column = 6;
            c = eol ? (i32)'\n' : (i32)FTN_EOF;
            break;
        case FL_INITIAL:
            if (eol) {
                p->column = 6;
                c = '\n';
                break;
            }
            c = ftnfixedtext(p);
            break;
        case FL_CONTINUED:
            c = ftnfixedtext(p);
            break;
        }
    }
    return c;
}

// Free form: the next character of a statement. An "&" that ends a line
// (before blanks or a comment) joins the next line, skipping blank and
// comment lines and an "&" that begins it. A line that begins with '!',
// or '#' in column 1, is a comment.
static i32 ftnfree(Ftn *p)
{
    b32 join = 0;
    i32 c    = ftnraw(p);
    if (c == '&') {
        do {
            c = ftnraw(p);
        } while (ftnspace(c) && c!='\n');
        if (c == '\n') {
            p->newline = 1;
            join = 1;
        } else if (c == '!') {
            join = 1;
        } else {
            ftnunraw(p, c);
            c = '&';
        }
    } else if (p->newline && (c=='!' || c=='#')) {
        join = 1;
    }
    while (join) {
        for (; ftnspace(c); c = ftnraw(p)) {}
        if (c=='!' || (p->newline && c=='#')) {
            c = ftnskipline(p);
            if (c != FTN_EOF) {
                c = ftnraw(p);
            }
            p->newline = 1;
        } else if (c == '&') {
            c = ftnraw(p);
        } else {
            join = 0;
        }
    }
    p->newline = c == '\n';
    return c;
}

static i32 ftngetc(Ftn *p, b32 instring)
{
    if (p->hasback) {
        p->hasback = 0;
        return p->back;
    }
    return p->freeform ? ftnfree(p) : ftnfixed(p, instring);
}

static void ftnunget(Ftn *p, i32 c)
{
    p->hasback = 1;
    p->back    = c;
}


// Tokens

// Look up a keyword, ignoring case: an FW_*, or -1.
static i32 ftnkeyword(Str s)
{
    u8 buf[16];
    if (s.len > countof(buf)) {
        return -1;
    }
    for (iz i = 0; i < s.len; i++) {
        buf[i] = lowercase(s[i]);
    }
    Str key = {buf, s.len};
    i32 lo  = 0;
    i32 hi  = NFWORDS;
    while (lo < hi) {
        i32 mid = lo + (hi - lo)/2;
        i32 d   = compare(key, ftnwords[mid]);
        if (!d) {
            return mid;
        }
        if (d < 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
    }
    return -1;
}

static void ftnput(FtnTok *t, i32 c)
{
    putbyte(&t->text, (u8)c);
}

// Append identifier characters from c on.
static void ftnword(Ftn *p, FtnTok *t, i32 c)
{
    do {
        ftnput(t, c);
        c = ftngetc(p, 0);
    } while (ftnidchar(c));
    ftnunget(p, c);
}

// Digits starting at c, or at the next character if c is not one, and
// a kind suffix ("_" and letters), which is not kept.
static void ftndigits(Ftn *p, FtnTok *t, i32 c)
{
    if (!ftndigit(c)) {
        c = ftngetc(p, 0);
    }
    for (; ftndigit(c); c = ftngetc(p, 0)) {
        ftnput(t, c);
    }
    if (c == '_') {
        do {
            c = ftngetc(p, 0);
        } while (ftnalpha(c));
    }
    ftnunget(p, c);
}

// Free form: after a line end, a label of up to five digits.
static void ftnlabel(Ftn *p)
{
    i32 c;
    do {
        c = ftngetc(p, 0);
    } while (c==' ' || c=='\t');
    FtnName n = {};
    u8      label[5];
    for (; n.text.len<5 && ftndigit(c); c = ftngetc(p, 0)) {
        if (!n.text.len) {
            n.line    = p->line;
            n.linepos = p->linepos;
        }
        label[n.text.len++] = (u8)c;
    }
    if (n.text.len) {
        n.text.data = label;
        ftntag(p, &n, F_LABEL);
    }
    ftnunget(p, c);
}

static void ftnlex(Ftn *p, FtnTok *t)
{
    t->text.len = 0;
    t->type     = FT_UNDEF;
    t->word     = -1;
    t->tag      = -1;
    t->hassub   = 0;

    i32 c;
    do {
        c = ftngetc(p, 0);
        t->line    = p->line;
        t->linepos = p->linepos;
    } while (c==' ' || c=='\t');

    switch (c) {
    case FTN_EOF: t->type = FT_EOF;     break;
    case ',':     t->type = FT_COMMA;   break;
    case '(':     t->type = FT_LPAREN;  break;
    case ')':     t->type = FT_RPAREN;  break;
    case '[':     t->type = FT_LSQUARE; break;
    case ']':     t->type = FT_RSQUARE; break;
    case '%':     t->type = FT_PERCENT; break;
    case ';':     t->type = FT_EOS;     break;

    case '*': case '/': case '+': case '-': case '=': case '<': case '>':
        // An operator: these characters, any but '-' continuing it
        for (;;) {
            ftnput(t, c);
            c = ftngetc(p, 0);
            switch (c) {
            case '*': case '/': case '+': case '=': case '<': case '>':
                continue;
            }
            break;
        }
        ftnunget(p, c);
        t->type = FT_OP;
        break;

    case '!':
        if (p->freeform) {
            do {
                c = ftngetc(p, 0);
            } while (c!='\n' && c!=FTN_EOF);
        } else {
            ftnskipline(p);
            p->column = 0;
        }
        [[fallthrough]];
    case '\n':
        t->type = FT_EOS;
        if (p->freeform) {
            ftnlabel(p);
        }
        break;

    case '.':
        // A dotted operator such as ".eq.", or else undefined
        ftnword(p, t, c);
        c = ftngetc(p, 0);
        if (c == '.') {
            ftnput(t, c);
            t->type = FT_OP;
        } else {
            ftnunget(p, c);
        }
        break;

    case '"':
    case '\'': {
        // To the closing quote or the end of the line, and a line end
        // inside a string is a sign of free form
        i32 quote = c;
        for (;;) {
            c = ftngetc(p, 1);
            if (c==quote || c=='\n' || c==FTN_EOF) {
                break;
            }
            ftnput(t, c);
        }
        if (c=='\n' && !p->freeform) {
            p->freefound = 1;
        }
        t->type = FT_STRING;
    } break;

    case ':':
        c = ftngetc(p, 0);
        if (c == ':') {
            t->type = FT_COLONS;
        } else {
            ftnunget(p, c);
            t->type = FT_COLON;
        }
        break;

    default:
        if (ftnalpha(c) || c=='_') {
            // Unlike Universal Ctags, a name may start with '_' (F3)
            ftnword(p, t, c);
            Str text = finish(&t->text);
            t->word = ftnkeyword(text);
            if (t->word >= 0) {
                t->type = FT_KEYWORD;
                break;
            }
            t->type = FT_IDENT;
            // A joined END: "enddo" is END with a secondary DO
            if (startswith(text, "end")) {
                i32 word = ftnkeyword(cuthead(text, 3));
                if (word >= 0) {
                    t->word    = FW_END;
                    t->hassub  = 1;
                    t->subtype = FT_KEYWORD;
                    t->subword = word;
                }
            }
        } else if (ftndigit(c)) {
            // Digits, a fraction, and an exponent's letter and digits
            ftndigits(p, t, c);
            c = ftngetc(p, 0);
            if (c == '.') {
                ftnput(t, c);
                ftndigits(p, t, 0);
                c = ftngetc(p, 0);
            }
            if (c=='e' || c=='E') {
                ftnput(t, c);
                ftndigits(p, t, 0);
            } else {
                ftnunget(p, c);
            }
            t->type = FT_NUMBER;
        }
    }
}


// Tags and scopes

// Tag a name if its kind is enabled, in the scope of the innermost
// enclosing unit named by an identifier and tagged (enabled or not).
static void ftntag(Ftn *p, FtnName *n, i32 kind)
{
    n->tag = kind;
    if (!n->text.len || !kindon(p->f->kinds, LANG_FORTRAN, kind)) {
        return;
    }
    Tag t = {};
    t.name      = n->text;
    t.kind      = kind;
    t.line      = n->line;
    t.linepos   = n->linepos;
    t.filescope = kind==F_LABEL || kind==F_LOCAL;
    t.truncate  = kind != F_LABEL;
    t.lineaddr  = kind == F_COMMON;
    for (iz i = p->scopes.len-1; i >= 0; i--) {
        FtnName *s = &p->scopes[i];
        if (s->type==FT_IDENT && s->tag>=0) {
            t.scopekind = kindname(LANG_FORTRAN, s->tag);
            t.scope     = s->text;
            break;
        }
    }
    tagemit(p->f, &t);
}

// The current token as a name, its text valid until the next token.
static FtnName ftnview(Ftn *p)
{
    FtnName r = {};
    r.text    = finish(&p->tok.text);
    r.type    = p->tok.type;
    r.tag     = p->tok.tag;
    r.line    = p->tok.line;
    r.linepos = p->tok.linepos;
    return r;
}

// Tag the current token.
static void ftntagtok(Ftn *p, i32 kind)
{
    FtnName n = ftnview(p);
    ftntag(p, &n, kind);
    p->tok.tag = kind;
}

// The current token as a lasting name. It takes the token's secondary
// token with it.
static FtnName ftntake(Ftn *p)
{
    FtnName r = ftnview(p);
    r.text = clone(p->a, r.text);
    p->tok.hassub = 0;
    return r;
}

// A name for an anonymous entity, at the current token, which loses its
// secondary token. The counter advances whether or not it is tagged.
static FtnName ftnanon(Ftn *p, i32 kind)
{
    FtnName r = ftnview(p);
    r.text = anonname(p->f, p->a, kind);
    r.type = FT_IDENT;
    p->tok.hassub = 0;
    return r;
}

static void ftnpush(Ftn *p, FtnName n)
{
    p->scopes = push(p->a, p->scopes, n);
}

// Enter the unit headed by the current token.
static void ftnpushtok(Ftn *p)
{
    FtnName n = ftnview(p);
    if (n.tag >= 0) {
        n.text = clone(p->a, n.text);
    }
    ftnpush(p, n);
}

static void ftnpop(Ftn *p)
{
    assert(p->scopes.len > 0);
    p->scopes.len--;
}

static b32 ftninterfaced(Ftn *p)
{
    for (iz i = 0; i < p->scopes.len; i++) {
        if (p->scopes[i].tag == F_INTERFACE) {
            return 1;
        }
    }
    return 0;
}

// The kind of an entity declared in the innermost unit.
static i32 ftnvarkind(Ftn *p, b32 method)
{
    if (!p->scopes.len) {
        return F_VARIABLE;
    }
    switch (p->scopes[p->scopes.len-1].tag) {
    case F_TYPE:       return method ? F_METHOD : F_COMPONENT;
    case F_FUNCTION:
    case F_SUBROUTINE:
    case F_PROTOTYPE:  return F_LOCAL;
    case F_ENUM:       return F_ENUMERATOR;
    }
    return F_VARIABLE;
}


// Skipping

static void ftnnext(Ftn *p)
{
    ftnlex(p, &p->tok);
}

// Give the current token a secondary token, the next one, unless it
// has one already: an END looks at the word after it this way.
static void ftnsub(Ftn *p)
{
    if (!p->tok.hassub) {
        ftnlex(p, &p->aux);
        p->tok.hassub  = 1;
        p->tok.subtype = p->aux.type;
        p->tok.subword = p->aux.word;
    }
}

static b32 ftnis(Ftn *p, i32 type)
{
    return p->tok.type == type;
}

static b32 ftnisop(Ftn *p, Str op)
{
    return p->tok.type==FT_OP && finish(&p->tok.text)==op;
}

static b32 ftnatend(Ftn *p)
{
    FtnTok *t = &p->tok;
    return t->type==FT_EOS || t->type==FT_EOF || (t->hassub && t->subtype==FT_EOS);
}

// Skip to a token of this type, stopping at the end of the statement.
static void ftnskipto(Ftn *p, i32 type)
{
    while (!ftnis(p, type) && !ftnatend(p)) {
        ftnnext(p);
    }
}

static void ftnskippast(Ftn *p, i32 type)
{
    ftnskipto(p, type);
    if (!ftnis(p, FT_EOS)) {
        ftnnext(p);
    }
}

// Skip to the first token of the next statement.
static void ftnskipstmt(Ftn *p)
{
    do {
        ftnskipto(p, FT_EOS);
        ftnnext(p);
    } while (ftnis(p, FT_EOS));
}

// Skip a bracketed group, nested, to the token after its close, but
// not past the end of the statement. Not at an open bracket, it skips
// one token.
static void ftnskipgroup(Ftn *p, i32 open, i32 close)
{
    i32 depth = 0;
    do {
        if (ftnis(p, FT_EOS)) {
            break;
        }
        depth += ftnis(p, open) - ftnis(p, close);
        ftnnext(p);
    } while (depth>0 && !ftnis(p, FT_EOF));
}

static void ftnskipparens(Ftn *p)
{
    ftnskipgroup(p, FT_LPAREN, FT_RPAREN);
}

static void ftnskipsquares(Ftn *p)
{
    ftnskipgroup(p, FT_LSQUARE, FT_RSQUARE);
}


// Declarations

static b32 ftnistypespec(i32 word)
{
    switch (word) {
    case FW_BYTE:      case FW_CHARACTER: case FW_CLASS:    case FW_COMPLEX:
    case FW_DOUBLE:    case FW_ENUMERATOR: case FW_FINAL:   case FW_GENERIC:
    case FW_INTEGER:   case FW_LOGICAL:   case FW_PROCEDURE: case FW_REAL:
    case FW_RECORD:    case FW_TYPE:
        return 1;
    }
    return 0;
}

static b32 ftnisprefix(i32 word)
{
    switch (word) {
    case FW_ELEMENTAL: case FW_PURE: case FW_RECURSIVE: case FW_STDCALL:
        return 1;
    }
    return 0;
}

// Skip a kind selector: "(...)", "*N", or "*(...)".
static void ftnselector(Ftn *p)
{
    if (ftnis(p, FT_LPAREN)) {
        ftnskipparens(p);
    }
    if (ftnisop(p, "*")) {
        ftnnext(p);
        if (ftnis(p, FT_LPAREN)) {
            ftnskipparens(p);
        } else {
            ftnnext(p);
        }
    }
}

static void ftnderived(Ftn *);

// Skip a type specifier to the token after it. TYPE not followed by a
// parenthesized name begins a derived type definition.
static void ftntypespec(Ftn *p)
{
    switch (p->tok.word) {
    case FW_CHARACTER:
        ftnnext(p);
        if (ftnisop(p, "*")) {
            ftnnext(p);
        }
        if (ftnis(p, FT_LPAREN)) {
            ftnskipparens(p);
        } else if (ftnis(p, FT_NUMBER)) {
            ftnnext(p);
        }
        break;
    case FW_BYTE: case FW_CLASS: case FW_COMPLEX: case FW_INTEGER:
    case FW_LOGICAL: case FW_PROCEDURE: case FW_REAL:
        ftnnext(p);
        ftnselector(p);
        break;
    case FW_DOUBLE:
        ftnnext(p);
        if (p->tok.word==FW_COMPLEX || p->tok.word==FW_PRECISION) {
            ftnnext(p);
        } else {
            ftnskipto(p, FT_EOS);
        }
        break;
    case FW_RECORD:
        ftnnext(p);
        if (ftnisop(p, "/")) {
            ftnnext(p);  // structure name
            ftnnext(p);  // closing slash
            ftnnext(p);
        }
        break;
    case FW_TYPE:
        ftnnext(p);
        if (ftnis(p, FT_LPAREN)) {
            ftnskipparens(p);
        } else {
            ftnderived(p);
        }
        break;
    case FW_ENUMERATOR: case FW_FINAL: case FW_GENERIC:
        ftnnext(p);
        break;
    default:
        assert(0);
    }
}

// Skip a list of attributes, from the comma before the first, through
// the "::" after them. An unknown attribute ends the statement.
static void ftnattrs(Ftn *p)
{
    do {
        ftnnext(p);
        switch (p->tok.word) {
        case FW_ABSTRACT:  case FW_ALLOCATABLE: case FW_DEFERRED:
        case FW_EXTERNAL:  case FW_INTRINSIC:   case FW_KIND:
        case FW_LEN:       case FW_NON_OVERRIDABLE: case FW_NOPASS:
        case FW_OPTIONAL:  case FW_PARAMETER:   case FW_POINTER:
        case FW_PRIVATE:   case FW_PROTECTED:   case FW_PUBLIC:
        case FW_SAVE:      case FW_TARGET:
            ftnnext(p);
            break;
        case FW_BIND: case FW_DIMENSION: case FW_EXTENDS: case FW_INTENT:
            ftnnext(p);
            ftnskipparens(p);
            break;
        case FW_PASS:
            ftnnext(p);
            if (ftnis(p, FT_LPAREN)) {
                ftnskipparens(p);
            }
            break;
        case FW_CODIMENSION:
            ftnnext(p);
            ftnskipsquares(p);
            break;
        default:
            ftnskipto(p, FT_EOS);
        }
    } while (ftnis(p, FT_COMMA));
    if (!ftnis(p, FT_COLONS)) {
        ftnskipto(p, FT_EOS);
    }
}

// Tag one entity of a declaration and skip its array specs, length,
// and initializer, to the comma or the end of the statement.
static void ftnentity(Ftn *p, b32 method)
{
    ftntagtok(p, ftnvarkind(p, method));
    ftnnext(p);
    if (ftnis(p, FT_LPAREN)) {
        ftnskipparens(p);
    }
    if (ftnis(p, FT_LSQUARE)) {
        ftnskipsquares(p);
    }
    if (ftnisop(p, "*")) {
        ftnnext(p);
        if (ftnis(p, FT_LPAREN)) {
            ftnskipparens(p);
        } else {
            ftnnext(p);
        }
    }
    if (ftnisop(p, "/")) {
        // DEC structure field initializer, "/value/"
        ftnnext(p);
        ftnskippast(p, FT_OP);
    } else if (ftnisop(p, "=") || ftnisop(p, "=>")) {
        while (!ftnis(p, FT_COMMA) && !ftnis(p, FT_EOS) && !ftnis(p, FT_EOF)) {
            ftnnext(p);
            if (ftnis(p, FT_LPAREN)) {
                ftnskipparens(p);
            }
            if (ftnis(p, FT_LSQUARE)) {
                ftnskipsquares(p);
            }
        }
    }
}

// Tag a list of entities: identifiers, or keywords other than FUNCTION
// and SUBROUTINE. A leading '%' (DEC %FILL) skips the statement.
static void ftnentities(Ftn *p, b32 method)
{
    if (ftnis(p, FT_PERCENT)) {
        ftnskipstmt(p);
        return;
    }
    for (;;) {
        i32 word = p->tok.word;
        b32 name = ftnis(p, FT_IDENT) || (ftnis(p, FT_KEYWORD) &&
                   word!=FW_FUNCTION && word!=FW_SUBROUTINE);
        if (!name) {
            break;
        }
        ftnentity(p, method);
        if (ftnis(p, FT_COMMA)) {
            ftnnext(p);
        } else if (ftnis(p, FT_EOS)) {
            ftnskipstmt(p);
            break;
        }
    }
}

// A type declaration statement: type, attributes, and entities.
static void ftndecl(Ftn *p)
{
    ftntypespec(p);
    if (!ftnis(p, FT_EOS)) {
        if (ftnis(p, FT_COMMA)) {
            ftnattrs(p);
        }
        if (ftnis(p, FT_COLONS)) {
            ftnnext(p);
        }
        ftnentities(p, 0);
    }
    if (ftnis(p, FT_EOS)) {
        ftnskipstmt(p);
    }
}

// COMMON or NAMELIST: each "/name/" is tagged with the kind, and the
// members as locals.
static void ftnblocklist(Ftn *p, i32 kind)
{
    ftnnext(p);
    do {
        if (ftnisop(p, "/")) {
            ftnnext(p);
            if (ftnis(p, FT_IDENT)) {
                ftntagtok(p, kind);
                ftnnext(p);
            }
            ftnskippast(p, FT_OP);
        }
        if (ftnis(p, FT_IDENT)) {
            ftntagtok(p, F_LOCAL);
        }
        ftnnext(p);
        if (ftnis(p, FT_LPAREN)) {
            ftnskipparens(p);
        }
        if (ftnis(p, FT_COMMA)) {
            ftnnext(p);
        }
    } while (!ftnis(p, FT_EOS) && !ftnis(p, FT_EOF));
    ftnskipstmt(p);
}

static void ftnstructure(Ftn *);
static void ftnunion(Ftn *);

// A field of a DEC structure or map.
static void ftnfield(Ftn *p)
{
    i32 word = p->tok.word;
    if (ftnistypespec(word)) {
        ftndecl(p);
    } else if (word == FW_STRUCTURE) {
        ftnstructure(p);
    } else if (word == FW_UNION) {
        ftnunion(p);
    } else {
        ftnskipstmt(p);
    }
}

// The fields of a DEC map, through its END.
static void ftnmap(Ftn *p)
{
    ftnskipstmt(p);
    while (p->tok.word!=FW_END && !ftnis(p, FT_EOF)) {
        ftnfield(p);
    }
    ftnsub(p);
    ftnskipstmt(p);
}

// The maps of a DEC union, through its END.
static void ftnunion(Ftn *p)
{
    ftnskipstmt(p);
    while (p->tok.word == FW_MAP) {
        ftnmap(p);
    }
    ftnsub(p);
    ftnskipstmt(p);
}

// A DEC structure, "STRUCTURE [/name/] [fields]", a type named after it
// (or anonymous), with the field names that follow as components.
static void ftnstructure(Ftn *p)
{
    FtnName name  = {};
    b32     named = 0;
    ftnnext(p);
    if (ftnisop(p, "/")) {
        ftnnext(p);
        if (ftnis(p, FT_IDENT) || ftnis(p, FT_KEYWORD)) {
            name  = ftntake(p);
            name.type = FT_IDENT;
            named = 1;
        }
        ftnskippast(p, FT_OP);
    }
    if (!named) {
        name = ftnanon(p, F_COMPONENT);
    }
    ftntag(p, &name, F_TYPE);
    while (ftnis(p, FT_IDENT)) {
        ftntagtok(p, F_COMPONENT);
        ftnnext(p);
        if (ftnis(p, FT_COMMA)) {
            ftnnext(p);
        }
    }
    ftnskipstmt(p);
    ftnpush(p, name);
    while (p->tok.word!=FW_END && !ftnis(p, FT_EOF)) {
        ftnfield(p);
    }
    ftnsub(p);
    ftnskipstmt(p);
    ftnpop(p);
}

// Specification statements beyond type declarations, or 0.
static b32 ftnspecstmt(Ftn *p)
{
    switch (p->tok.word) {
    case FW_COMMON:
        ftnblocklist(p, F_COMMON);
        return 1;
    case FW_NAMELIST:
        ftnblocklist(p, F_NAMELIST);
        return 1;
    case FW_STRUCTURE:
        ftnstructure(p);
        return 1;
    case FW_ALLOCATABLE: case FW_DATA:      case FW_DIMENSION:
    case FW_EQUIVALENCE: case FW_EXTERNAL:  case FW_INTENT:
    case FW_INTRINSIC:   case FW_OPTIONAL:  case FW_POINTER:
    case FW_PRIVATE:     case FW_PROTECTED: case FW_PUBLIC:
    case FW_SAVE:        case FW_TARGET:
        ftnskipstmt(p);
        return 1;
    }
    return 0;
}

// The generic spec of a type-bound GENERIC: a name, or the operator of
// OPERATOR(op) or ASSIGNMENT(=), tagged as a method.
static void ftngeneric(Ftn *p)
{
    i32 word = p->tok.word;
    if (word==FW_ASSIGNMENT || word==FW_OPERATOR) {
        ftnnext(p);
        if (ftnis(p, FT_LPAREN)) {
            ftnnext(p);
        }
        if (ftnis(p, FT_OP)) {
            ftntagtok(p, F_METHOD);
        }
    } else {
        ftntagtok(p, F_METHOD);
    }
    ftnskipstmt(p);
}

// A statement in a derived type definition that begins with a type
// specifier. PROCEDURE, FINAL, and GENERIC declare methods.
static void ftncomponent(Ftn *p)
{
    i32 word   = p->tok.word;
    b32 method = word==FW_PROCEDURE || word==FW_FINAL || word==FW_GENERIC;
    ftntypespec(p);
    if (ftnis(p, FT_COMMA)) {
        ftnattrs(p);
    }
    if (ftnis(p, FT_COLONS)) {
        ftnnext(p);
    }
    if (word == FW_GENERIC) {
        ftngeneric(p);
    } else {
        ftnentities(p, method);
    }
}

// A derived type definition, after TYPE: "[, attrs] [::] name", then
// its components, through its END, which stays the current token.
static void ftnderived(Ftn *p)
{
    if (ftnis(p, FT_COMMA)) {
        ftnattrs(p);
    }
    if (ftnis(p, FT_COLONS)) {
        ftnnext(p);
    }
    if (ftnis(p, FT_IDENT) || ftnis(p, FT_KEYWORD)) {
        p->tok.type = FT_IDENT;
        ftntagtok(p, F_TYPE);
    }
    ftnpushtok(p);
    ftnskipstmt(p);
    if (p->tok.word==FW_PRIVATE || p->tok.word==FW_SEQUENCE) {
        ftnskipstmt(p);
    }
    while (p->tok.word!=FW_END && !ftnis(p, FT_EOF)) {
        if (ftnistypespec(p->tok.word)) {
            ftncomponent(p);
        } else {
            ftnskipstmt(p);
        }
    }
    ftnsub(p);
    ftnskipto(p, FT_EOS);
    ftnpop(p);
}

static void ftnsubprogram(Ftn *);

// An interface block: named by a generic name or an operator, or else
// anonymous. Its subprograms are prototypes.
static void ftninterface(Ftn *p)
{
    FtnName name  = {};
    b32     named = 0;
    ftnnext(p);
    i32 word = p->tok.word;
    if (word==FW_ASSIGNMENT || word==FW_OPERATOR) {
        ftnnext(p);
        if (ftnis(p, FT_LPAREN)) {
            ftnnext(p);
        }
        if (ftnis(p, FT_OP)) {
            name  = ftntake(p);  // not an identifier, so never a scope
            named = 1;
        }
    } else if (ftnis(p, FT_IDENT) || ftnis(p, FT_KEYWORD)) {
        name  = ftntake(p);
        name.type = FT_IDENT;
        named = 1;
    }
    if (!named) {
        name = ftnanon(p, F_INTERFACE);
    }
    ftntag(p, &name, F_INTERFACE);
    ftnpush(p, name);
    while (p->tok.word!=FW_END && !ftnis(p, FT_EOF)) {
        word = p->tok.word;
        if (word==FW_FUNCTION || word==FW_SUBROUTINE) {
            ftnsubprogram(p);
        } else if (ftnisprefix(word)) {
            ftnnext(p);
        } else if (ftnistypespec(word)) {
            ftntypespec(p);
        } else {
            ftnskipstmt(p);
        }
    }
    ftnsub(p);
    ftnskipstmt(p);
    ftnpop(p);
}

// An enum: "ENUM [, BIND(C)] [kind] [::] [name]", then its enumerators,
// through its END.
static void ftnenum(Ftn *p)
{
    ftnnext(p);
    if (ftnis(p, FT_COMMA)) {
        ftnnext(p);
        if (ftnis(p, FT_KEYWORD)) {
            ftnnext(p);
        }
        if (ftnis(p, FT_LPAREN)) {
            ftnskipparens(p);
        }
    }
    ftnselector(p);
    if (ftnis(p, FT_COLONS)) {
        ftnnext(p);
    }
    FtnName name = {};
    if (ftnis(p, FT_IDENT) || ftnis(p, FT_KEYWORD)) {
        name = ftntake(p);
        name.type = FT_IDENT;
    } else {
        name = ftnanon(p, F_ENUM);
    }
    ftntag(p, &name, F_ENUM);
    ftnskipstmt(p);
    ftnpush(p, name);
    while (p->tok.word!=FW_END && !ftnis(p, FT_EOF)) {
        if (ftnistypespec(p->tok.word)) {
            ftndecl(p);
        } else {
            ftnskipstmt(p);
        }
    }
    ftnsub(p);
    ftnskipstmt(p);
    ftnpop(p);
}

static void ftnentry(Ftn *p)
{
    ftnnext(p);
    if (ftnis(p, FT_IDENT)) {
        ftntagtok(p, F_ENTRY);
    }
    ftnskipstmt(p);
}

// A statement that starts with an identifier belongs to the
// specification part only as a statement function, "f(...) = expr".
// It is skipped either way.
static b32 ftnstmtfunc(Ftn *p)
{
    b32 r = 0;
    ftnnext(p);
    if (ftnis(p, FT_LPAREN)) {
        ftnskipparens(p);
        r = ftnisop(p, "=");
    }
    ftnskipstmt(p);
    return r;
}

// Declarations skipped without a tag.
static b32 ftnisignored(i32 word)
{
    switch (word) {
    case FW_CEXTERNAL:  case FW_CGLOBAL:   case FW_DLL_EXPORT:
    case FW_DLL_IMPORT: case FW_EXTERNAL:  case FW_FORMAT:
    case FW_INCLUDE:    case FW_INLINE:    case FW_PARAMETER:
    case FW_PASCAL:     case FW_PEXTERNAL: case FW_PGLOBAL:
    case FW_STATIC:     case FW_VALUE:     case FW_VIRTUAL:
    case FW_VOLATILE:
        return 1;
    }
    return 0;
}

// One declaration construct, or 0 at the end of the specification part.
static b32 ftndeclaration(Ftn *p)
{
    switch (p->tok.word) {
    case FW_ENTRY:
        ftnentry(p);
        return 1;
    case FW_INTERFACE:
        ftninterface(p);
        return 1;
    case FW_ENUM:
        ftnenum(p);
        return 1;
    case FW_STDCALL:
        ftnnext(p);
        return 1;
    case FW_ABSTRACT:
        ftnnext(p);
        if (p->tok.word == FW_INTERFACE) {
            ftninterface(p);
        } else {
            ftnskipstmt(p);
        }
        return 1;
    case FW_AUTOMATIC:
        ftnnext(p);
        if (ftnistypespec(p->tok.word)) {
            ftndecl(p);
        } else {
            ftnskipstmt(p);
        }
        return 1;
    }
    if (ftnisignored(p->tok.word)) {
        ftnskipstmt(p);
        return 1;
    }
    if (ftnistypespec(p->tok.word)) {
        ftndecl(p);
        return 1;
    }
    if (ftnis(p, FT_IDENT)) {
        return ftnstmtfunc(p);
    }
    return ftnspecstmt(p);
}

// IMPLICIT, INCLUDE, PARAMETER, FORMAT, and ENTRY, or 0.
static b32 ftnimplicit(Ftn *p)
{
    switch (p->tok.word) {
    case FW_ENTRY:
        ftnentry(p);
        return 1;
    case FW_FORMAT: case FW_IMPLICIT: case FW_INCLUDE: case FW_PARAMETER:
        ftnskipstmt(p);
        return 1;
    }
    return 0;
}

// The specification part: USE, IMPORT, the implicit part, then
// declarations. Returns 1 if it read any statement.
static b32 ftnspec(Ftn *p)
{
    b32 r = 0;
    for (; p->tok.word == FW_USE; r = 1) {
        ftnskipstmt(p);
    }
    for (; p->tok.word == FW_IMPORT; r = 1) {
        ftnskipstmt(p);
    }
    while (ftnimplicit(p)) {
        r = 1;
    }
    while (ftndeclaration(p)) {
        r = 1;
    }
    return r;
}


// Program units

// BLOCK DATA [name], through its END.
static void ftnblockdata(Ftn *p)
{
    ftnnext(p);
    if (p->tok.word == FW_DATA) {
        ftnnext(p);
        if (ftnis(p, FT_IDENT)) {
            ftntagtok(p, F_BLOCKDATA);
        }
    }
    ftnpushtok(p);
    ftnskipstmt(p);
    ftnspec(p);
    while (p->tok.word!=FW_END && !ftnis(p, FT_EOF)) {
        ftnskipstmt(p);
    }
    ftnsub(p);
    ftnskipstmt(p);
    ftnpop(p);
}

// After CONTAINS, token by token: subprograms, up to an END.
static void ftncontains(Ftn *p)
{
    if (p->tok.word == FW_CONTAINS) {
        ftnskipstmt(p);
    }
    do {
        i32 word = p->tok.word;
        if (word==FW_FUNCTION || word==FW_SUBROUTINE) {
            ftnsubprogram(p);
        } else if (word == FW_END) {
            break;
        } else if (ftnistypespec(word)) {
            ftntypespec(p);
        } else {
            ftnnext(p);
        }
    } while (!ftnis(p, FT_EOF));
}

// A submodule's parent, "(ancestor[:parent])": identifiers and colons,
// at least one. Returns 0 if malformed.
static b32 ftnparent(Ftn *p)
{
    if (!ftnis(p, FT_LPAREN)) {
        return 0;
    }
    for (b32 any = 0;; any = 1) {
        ftnnext(p);
        if (ftnis(p, FT_RPAREN)) {
            return any;
        } else if (!ftnis(p, FT_IDENT) && !ftnis(p, FT_COLON)) {
            return 0;
        }
    }
}

// MODULE name or SUBMODULE (parent) name, through the first END after
// its specification part and module subprograms.
static void ftnmodule(Ftn *p, b32 sub)
{
    if (sub) {
        ftnnext(p);
        if (!ftnparent(p)) {
            ftnskipstmt(p);
            return;
        }
    }
    ftnnext(p);
    if (ftnis(p, FT_IDENT) || ftnis(p, FT_KEYWORD)) {
        p->tok.type = FT_IDENT;
        ftntagtok(p, sub ? F_SUBMODULE : F_MODULE);
    }
    ftnpushtok(p);
    ftnskipstmt(p);
    ftnspec(p);
    if (p->tok.word == FW_CONTAINS) {
        ftncontains(p);
    }
    while (p->tok.word!=FW_END && !ftnis(p, FT_EOF)) {
        ftnskipstmt(p);
    }
    ftnsub(p);
    ftnskipstmt(p);
    ftnpop(p);
}

// An END that closes a construct inside an execution part.
static b32 ftnendsconstruct(Ftn *p)
{
    if (!p->tok.hassub) {
        return 0;
    }
    switch (p->tok.subword) {
    case FW_ASSOCIATE: case FW_BLOCK: case FW_DO: case FW_ENUM:
    case FW_FORALL: case FW_IF: case FW_SELECT: case FW_WHERE:
        return 1;
    }
    return 0;
}

// The execution part, statement by statement, tagging entries, up to
// CONTAINS, FUNCTION, SUBROUTINE, or the unit's END. Returns 1 if it
// read anything.
static b32 ftnexec(Ftn *p)
{
    b32 r = 0;
    while (!ftnis(p, FT_EOF)) {
        i32 word = p->tok.word;
        if (word==FW_CONTAINS || word==FW_FUNCTION || word==FW_SUBROUTINE) {
            break;
        } else if (word == FW_END) {
            ftnsub(p);
            if (!ftnendsconstruct(p)) {
                break;
            }
            ftnskipstmt(p);
        } else if (word == FW_ENTRY) {
            ftnentry(p);
        } else if (ftnisprefix(word)) {
            ftnnext(p);
        } else {
            ftnskipstmt(p);
        }
        r = 1;
    }
    return r;
}

// A main program, function, or subroutine, after its keyword, through
// its END. A subroutine's or prototype's argument list is read before
// it is tagged, a function's after.
static void ftnunit(Ftn *p, i32 kind)
{
    ftnnext(p);
    if (ftnis(p, FT_IDENT) || ftnis(p, FT_KEYWORD)) {
        FtnName name = ftntake(p);  // a keyword name stays a keyword
        if (kind==F_SUBROUTINE || kind==F_PROTOTYPE) {
            ftnnext(p);
            if (ftnis(p, FT_LPAREN)) {
                ftnskipparens(p);
            }
        }
        ftntag(p, &name, kind);
        ftnpush(p, name);
    } else {
        ftnpushtok(p);
    }
    ftnskipstmt(p);
    ftnspec(p);
    ftnexec(p);
    if (p->tok.word == FW_CONTAINS) {
        ftncontains(p);
    }
    ftnsub(p);
    ftnskipstmt(p);
    ftnpop(p);
}

// A function or subroutine, a prototype inside an interface.
static void ftnsubprogram(Ftn *p)
{
    i32 kind = p->tok.word==FW_SUBROUTINE ? F_SUBROUTINE : F_FUNCTION;
    ftnunit(p, ftninterfaced(p) ? F_PROTOTYPE : kind);
}

static void ftnunits(Ftn *p)
{
    ftnnext(p);
    do {
        if (ftnis(p, FT_EOS)) {
            ftnnext(p);
            continue;
        }
        switch (p->tok.word) {
        case FW_BLOCK:      ftnblockdata(p);       break;
        case FW_END:        ftnskipstmt(p);        break;
        case FW_FUNCTION:
        case FW_SUBROUTINE: ftnsubprogram(p);      break;
        case FW_SUBMODULE:  ftnmodule(p, 1);       break;
        case FW_MODULE:     ftnmodule(p, 0);       break;
        case FW_PROGRAM:    ftnunit(p, F_PROGRAM); break;
        default:
            if (ftnisprefix(p->tok.word)) {
                ftnnext(p);
            } else {
                // Statements outside any unit: an implied main program
                b32 spec = ftnspec(p);
                b32 exec = ftnexec(p);
                if (!spec && !exec) {
                    ftnnext(p);
                }
            }
        }
    } while (!ftnis(p, FT_EOF));
}

// Tag a Fortran file. The first pass reads fixed form, and fails on
// signs of free form, then the second pass (freeform=1) reads free form.
// Returns 0 if the pass failed. A character the lexer put back at the
// end of the first pass is read first by the second (f->carry holds it
// plus 2), as in Universal Ctags, which resets it only for the first.
static b32 ftntags(File *f, b32 freeform, Arena scratch)
{
    Ftn *p = alloc<Ftn>(&scratch);
    p->f        = f;
    p->a        = &scratch;
    p->src      = f->src->text;
    p->bol      = 1;
    p->freeform = freeform;
    p->newline  = 1;
    p->tok.text = Buf(&scratch, 64);
    p->aux.text = Buf(&scratch, 64);
    if (freeform && f->carry) {
        p->hasback = 1;
        p->back    = f->carry - 2;
    }
    ftnunits(p);
    f->carry = p->hasback ? p->back+2 : 0;
    return freeform || !p->freefound;
}

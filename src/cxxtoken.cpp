// C and C++ tokens: their chains, keywords, and type-name spacing
//
// Tokens and chains are recycled through free lists, since one statement
// can churn through millions of them (data tables of numbers), and token
// text grows in place. A chain's count is kept as Universal Ctags keeps
// it, including where some operations neglect it, since parsing
// decisions read it.
//
// This is free and unencumbered software released into the public domain.

enum : u32 {
    T_EOF       = 1u<<0,
    T_IDENT     = 1u<<1,
    T_KEYWORD   = 1u<<2,
    T_NUMBER    = 1u<<3,
    T_COLON     = 1u<<4,   // :
    T_COLONS    = 1u<<5,   // ::
    T_SEMICOLON = 1u<<6,
    T_COMMA     = 1u<<7,
    T_ASSIGN    = 1u<<8,   // =
    T_OPERATOR  = 1u<<9,   // != == += ++ / ...
    T_UNKNOWN   = 1u<<10,
    T_DOT       = 1u<<11,  // .
    T_ARROW     = 1u<<12,  // ->
    T_STRING    = 1u<<13,
    T_STAR      = 1u<<14,
    T_AND       = 1u<<15,  // &
    T_ANDS      = 1u<<16,  // &&
    T_CHAR      = 1u<<17,
    T_DOTS      = 1u<<18,  // ...

    // Openers shifted by 4 are their closers, by 8 their chains
    T_LBRACE    = 1u<<19,
    T_LPAREN    = 1u<<20,
    T_LBRACKET  = 1u<<21,
    T_LT        = 1u<<22,
    T_RBRACE    = 1u<<23,
    T_RPAREN    = 1u<<24,
    T_RBRACKET  = 1u<<25,
    T_GT        = 1u<<26,
    T_BRACES    = 1u<<27,  // {...}
    T_PARENS    = 1u<<28,  // (...)
    T_BRACKETS  = 1u<<29,  // [...]
    T_ANGLES    = 1u<<30,  // <...>
};


// Keywords

enum : i32 {
    KW__ATTRIBUTE__, KW__CONSTANT__, KW__DECLSPEC, KW__DEVICE__,
    KW__FASTCALL, KW__FORCEINLINE, KW__FORCEINLINE__, KW__GLOBAL__,
    KW__HOST__, KW__INLINE, KW__INLINE__, KW__MANAGED__, KW__NOINLINE__,
    KW__RESTRICT, KW__RESTRICT__, KW__SHARED__, KW__STDCALL,
    KW__THISCALL, KW__THREAD, KW__TYPEOF, KW__TYPEOF__, KW_ALIGNAS,
    KW_THREAD_LOCAL, KW_alignas, KW_alignof, KW_asm, KW_auto, KW_bool,
    KW_break, KW_case, KW_catch, KW_char, KW_char16_t, KW_char32_t,
    KW_class, KW_concept, KW_const, KW_consteval, KW_constexpr,
    KW_constinit, KW_const_cast, KW_continue, KW_decltype, KW_default,
    KW_delete, KW_do, KW_double, KW_dynamic_cast, KW_else, KW_enum,
    KW_explicit, KW_export, KW_extern, KW_false, KW_final, KW_float,
    KW_for, KW_friend, KW_goto, KW_if, KW_inline, KW_int, KW_long,
    KW_mutable, KW_namespace, KW_new, KW_noexcept, KW_nullptr,
    KW_operator, KW_private, KW_protected, KW_public, KW_register,
    KW_reinterpret_cast, KW_requires, KW_restrict, KW_return, KW_short,
    KW_signed, KW_sizeof, KW_static, KW_static_assert, KW_static_cast,
    KW_struct, KW_switch, KW_template, KW_this, KW_thread_local,
    KW_throw, KW_true, KW_try, KW_typedef, KW_typeid, KW_typename,
    KW_typeof, KW_union, KW_unsigned, KW_using, KW_virtual, KW_void,
    KW_volatile, KW_wchar_t, KW_while,
    NKEYWORDS,
};

enum : u32 {
    KL_C   = 1,       // a keyword in C
    KL_CXX = 2,       // a keyword in C++
    KL_ALL = KL_C|KL_CXX,
};

enum : u32 {
    KF_TYPEPART = 1u<<0,  // usually part of a type name: int, const, ...
    KF_TYPEMARK = 1u<<1,  // struct, class, union, enum, typename
    KF_NOTTYPE  = 1u<<2,  // near a type but not part of it: static, ...
    KF_CONSTANT = 1u<<3,  // true, false, nullptr
    KF_VARDECL  = 1u<<5,  // may appear in a variable declaration
    KF_DECLTYPE = 1u<<6,  // decltype and the typeofs
    KF_DROP     = 1u<<7,  // dropped by the tokenizer: __attribute__ ...
};

struct Keyword {
    Str name;
    u32 langs;
    u32 flags;
};

static constexpr Keyword keywords[NKEYWORDS] = {
    {"__attribute__",    KL_ALL, KF_VARDECL|KF_DROP},
    {"__constant__",     0,      KF_VARDECL|KF_NOTTYPE},  // CUDA
    {"__declspec",       KL_ALL, KF_VARDECL|KF_NOTTYPE|KF_DROP},
    {"__device__",       0,      KF_VARDECL|KF_NOTTYPE},  // CUDA
    {"__fastcall",       KL_CXX, 0},
    {"__forceinline",    KL_ALL, KF_NOTTYPE},
    {"__forceinline__",  0,      KF_NOTTYPE},             // CUDA
    {"__global__",       0,      KF_VARDECL|KF_NOTTYPE},  // CUDA
    {"__host__",         0,      KF_VARDECL|KF_NOTTYPE},  // CUDA
    {"__inline",         KL_ALL, KF_VARDECL|KF_NOTTYPE},
    {"__inline__",       KL_ALL, KF_VARDECL|KF_NOTTYPE},
    {"__managed__",      0,      KF_VARDECL|KF_NOTTYPE},  // CUDA
    {"__noinline__",     0,      KF_NOTTYPE},             // CUDA
    {"__restrict",       KL_ALL, KF_VARDECL|KF_NOTTYPE},
    {"__restrict__",     KL_ALL, KF_VARDECL|KF_NOTTYPE},
    {"__shared__",       0,      KF_VARDECL|KF_NOTTYPE},  // CUDA
    {"__stdcall",        KL_ALL, 0},
    {"__thiscall",       KL_CXX, 0},
    {"__thread",         KL_ALL, KF_VARDECL|KF_NOTTYPE},
    {"__typeof",         KL_ALL, KF_DECLTYPE|KF_VARDECL|KF_TYPEPART},
    {"__typeof__",       KL_ALL, KF_DECLTYPE|KF_VARDECL|KF_TYPEPART},
    {"_Alignas",         KL_C,   KF_VARDECL|KF_DROP},
    {"_Thread_local",    KL_C,   KF_VARDECL|KF_NOTTYPE},
    {"alignas",          KL_ALL, KF_VARDECL|KF_DROP},
    {"alignof",          KL_CXX, 0},
    {"asm",              KL_ALL, 0},
    {"auto",             KL_CXX, KF_VARDECL|KF_TYPEPART},
    {"bool",             KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"break",            KL_ALL, 0},
    {"case",             KL_ALL, 0},
    {"catch",            KL_CXX, 0},
    {"char",             KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"char16_t",         KL_CXX, KF_VARDECL|KF_TYPEPART},
    {"char32_t",         KL_CXX, KF_VARDECL|KF_TYPEPART},
    {"class",            KL_CXX, KF_VARDECL|KF_TYPEPART|KF_TYPEMARK},
    {"concept",          KL_CXX, 0},
    {"const",            KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"consteval",        KL_CXX, KF_NOTTYPE},
    {"constexpr",        KL_CXX, KF_VARDECL|KF_NOTTYPE},
    {"constinit",        KL_CXX, KF_VARDECL|KF_NOTTYPE},
    {"const_cast",       KL_CXX, 0},
    {"continue",         KL_ALL, 0},
    {"decltype",         KL_CXX, KF_DECLTYPE|KF_VARDECL|KF_TYPEPART},
    {"default",          KL_ALL, 0},
    {"delete",           KL_CXX, 0},
    {"do",               KL_ALL, 0},
    {"double",           KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"dynamic_cast",     KL_CXX, 0},
    {"else",             KL_ALL, 0},
    {"enum",             KL_ALL, KF_VARDECL|KF_TYPEPART|KF_TYPEMARK},
    {"explicit",         KL_CXX, 0},
    {"export",           KL_CXX, KF_VARDECL|KF_NOTTYPE},
    {"extern",           KL_ALL, 0},
    {"false",            KL_ALL, KF_CONSTANT},
    {"final",            KL_CXX, 0},
    {"float",            KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"for",              KL_ALL, 0},
    {"friend",           KL_CXX, KF_NOTTYPE},
    {"goto",             KL_ALL, 0},
    {"if",               KL_ALL, 0},
    {"inline",           KL_ALL, KF_VARDECL|KF_NOTTYPE},
    {"int",              KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"long",             KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"mutable",          KL_CXX, KF_VARDECL},
    {"namespace",        KL_CXX, 0},
    {"new",              KL_CXX, 0},
    {"noexcept",         KL_CXX, 0},
    {"nullptr",          KL_CXX, KF_CONSTANT},
    {"operator",         KL_CXX, 0},
    {"private",          KL_CXX, 0},
    {"protected",        KL_CXX, 0},
    {"public",           KL_CXX, 0},
    {"register",         KL_ALL, KF_VARDECL},
    {"reinterpret_cast", KL_CXX, 0},
    {"requires",         KL_CXX, 0},
    {"restrict",         KL_ALL, KF_VARDECL|KF_NOTTYPE},
    {"return",           KL_ALL, 0},
    {"short",            KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"signed",           KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"sizeof",           KL_ALL, 0},
    {"static",           KL_ALL, KF_VARDECL|KF_NOTTYPE},
    {"static_assert",    KL_CXX, 0},
    {"static_cast",      KL_CXX, 0},
    {"struct",           KL_ALL, KF_VARDECL|KF_TYPEPART|KF_TYPEMARK},
    {"switch",           KL_ALL, 0},
    {"template",         KL_CXX, 0},
    {"this",             KL_CXX, 0},
    {"thread_local",     KL_CXX, KF_VARDECL|KF_NOTTYPE},
    {"throw",            KL_CXX, 0},
    {"true",             KL_ALL, KF_CONSTANT},
    {"try",              KL_CXX, 0},
    {"typedef",          KL_ALL, 0},
    {"typeid",           KL_CXX, 0},
    {"typename",         KL_CXX, KF_VARDECL|KF_TYPEPART|KF_TYPEMARK},
    {"typeof",           KL_ALL, KF_DECLTYPE|KF_VARDECL|KF_TYPEPART},
    {"union",            KL_ALL, KF_VARDECL|KF_TYPEPART|KF_TYPEMARK},
    {"unsigned",         KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"using",            KL_CXX, 0},
    {"virtual",          KL_CXX, KF_NOTTYPE},
    {"void",             KL_ALL, KF_VARDECL|KF_TYPEPART},
    {"volatile",         KL_ALL, KF_VARDECL},
    {"wchar_t",          KL_CXX, KF_VARDECL|KF_TYPEPART},
    {"while",            KL_ALL, 0},
};

// Keywords by a hash of their first two bytes, last byte, and length,
// probed linearly: each slot holds a keyword's index plus one, or 0. At
// most a fifth full, a word not among them usually finds an empty slot,
// or a keyword of another length, in one probe.
enum : i32 { KWSLOTS = 512 };

static constexpr u32 kwhash(u8 first, u8 second, u8 last, iz len)
{
    u32 key = first | (u32)second<<8 | (u32)last<<16 | (u32)len<<24;
    return key*0x9e3779b1u >> 23;  // the high 9 bits: a slot
}

struct Kwtable {
    u8 slots[KWSLOTS];
    iz maxlen;
};

static constexpr Kwtable kwtables()
{
    Kwtable r = {};
    for (i32 kw = 0; kw < NKEYWORDS; kw++) {
        char const *name = keywords[kw].name.cdata;
        iz          len  = keywords[kw].name.len;
        u32         i    = kwhash((u8)name[0], (u8)name[1], (u8)name[len-1], len);
        for (; r.slots[i]; i = (i + 1) % KWSLOTS) {}
        r.slots[i] = (u8)(kw + 1);
        r.maxlen   = len>r.maxlen ? len : r.maxlen;
    }
    return r;
}

static constexpr Kwtable kwtable = kwtables();

// The keyword spelled s in the language (KL_C or KL_CXX), or -1.
static i32 kwlookup(Str s, u32 lang)
{
    if (s.len<2 || s.len>kwtable.maxlen) {
        return -1;
    }
    for (u32 i = kwhash(s[0], s[1], s[s.len-1], s.len);; i = (i + 1) % KWSLOTS) {
        i32 kw = kwtable.slots[i] - 1;
        if (kw < 0) {
            return -1;
        }
        Str name = keywords[kw].name;
        if (name == s) {
            return keywords[kw].langs&lang ? kw : -1;
        }
    }
}

static b32 kwflag(i32 kw, u32 flag)
{
    return !!(keywords[kw].flags & flag);
}


// Tokens and chains

struct Word {
    u8 *data;
    iz  len;
    iz  cap;
};

struct Chain;

struct Token {
    Token *next;
    Token *prev;
    Chain *chain;          // a chain token's contents
    Word   word;
    u32    type;
    i32    kw;
    i32    line;
    iz     linepos;        // offset of its line's first byte
    b32    space;          // followed by white space
    b32    anon;           // a made-up name for an unnamed entity
    b32    scopeexported;  // scope bookkeeping, see the scope stack
    u8     scopetype;
    u8     scopeaccess;
    u8     scopeanon;      // an anonymous namespace
};

struct Chain {
    Token *head;
    Token *tail;
    iz     count;
    Chain *nextfree;
};

// Where tokens and chains come from, and return to.
struct Pool {
    Arena *a;
    Token *tokens;
    Chain *chains;
};

static Str wordstr(Token *t)
{
    return {t->word.data, t->word.len};
}

static void wordput(Pool *p, Word *w, u8 c)
{
    if (w->len == w->cap) {
        iz  cap  = w->cap ? w->cap*2 : 16;
        u8 *data = allocbytes(p->a, cap);
        copybytes(data, w->data, w->len);
        w->data = data;
        w->cap  = cap;
    }
    w->data[w->len++] = c;
}

static void wordcat(Pool *p, Word *w, Str s)
{
    if (w->cap-w->len < s.len) {
        iz cap = w->cap ? w->cap : 16;
        for (; cap-w->len < s.len; cap *= 2) {}
        u8 *data = allocbytes(p->a, cap);
        copybytes(data, w->data, w->len);
        w->data = data;
        w->cap  = cap;
    }
    copybytes(w->data+w->len, s.data, s.len);
    w->len += s.len;
}

static Token *newtoken(Pool *p)
{
    Token *t = p->tokens;
    if (t) {
        p->tokens = t->next;
        Word w = t->word;
        *t = {};
        t->word     = w;
        t->word.len = 0;
    } else {
        t = alloc<Token>(p->a);
    }
    return t;
}

static Chain *newchain(Pool *p)
{
    Chain *c = p->chains;
    if (c) {
        p->chains = c->nextfree;
        *c = {};
    } else {
        c = alloc<Chain>(p->a);
    }
    return c;
}

static void freechain(Pool *, Chain *);

static void freetoken(Pool *p, Token *t)
{
    if (!t) {
        return;
    }
    if (t->chain) {
        freechain(p, t->chain);
        t->chain = 0;
    }
    t->next   = p->tokens;
    p->tokens = t;
}

// Destroy a chain and every token in it.
static void freechain(Pool *p, Chain *c)
{
    if (!c) {
        return;
    }
    for (Token *t = c->head; t;) {
        Token *next = t->next;
        freetoken(p, t);
        t = next;
    }
    c->nextfree = p->chains;
    p->chains   = c;
}

static Token *copytoken(Pool *p, Token *t)
{
    Token *r = newtoken(p);
    r->line    = t->line;
    r->linepos = t->linepos;
    r->type    = t->type;
    r->kw      = t->kw;
    r->space   = t->space;
    wordcat(p, &r->word, wordstr(t));
    return r;
}

static Token *keywordtoken(Pool *p, i32 line, iz linepos, i32 kw)
{
    Token *t = newtoken(p);
    t->line    = line;
    t->linepos = linepos;
    t->type    = T_KEYWORD;
    t->kw      = kw;
    t->space   = 1;
    wordcat(p, &t->word, keywords[kw].name);
    return t;
}

static b32 iskeyword(Token *t, i32 kw)
{
    return t->type==T_KEYWORD && t->kw==kw;
}

static Token *chaintakefirst(Chain *c)
{
    if (!c || !c->head) {
        return 0;
    }
    Token *t = c->head;
    if (t == c->tail) {
        c->head  = 0;
        c->tail  = 0;
        c->count = 0;
        return t;
    }
    c->count--;
    t->next->prev = 0;
    c->head = t->next;
    return t;
}

static Token *chaintakelast(Chain *c)
{
    if (!c || !c->tail) {
        return 0;
    }
    Token *t = c->tail;
    if (t == c->head) {
        c->head  = 0;
        c->tail  = 0;
        c->count = 0;
        return t;
    }
    c->count--;
    t->prev->next = 0;
    c->tail = t->prev;
    return t;
}

static void chaintake(Chain *c, Token *t)
{
    if (!c || !c->head) {
        return;
    }
    if (t == c->head) {
        chaintakefirst(c);
        return;
    }
    if (t == c->tail) {
        chaintakelast(c);
        return;
    }
    t->next->prev = t->prev;
    t->prev->next = t->next;
    c->count--;
}

// Take a token from wherever it is within the chain or its subchains.
static b32 chaintakenested(Chain *c, Token *t)
{
    if (!c) {
        return 0;
    }
    for (Token *x = c->head; x; x = x->next) {
        if (x == t) {
            chaintake(c, x);
            return 1;
        }
        if (x->type&(T_PARENS|T_ANGLES|T_BRACKETS|T_BRACES) &&
                chaintakenested(x->chain, t)) {
            return 1;
        }
    }
    return 0;
}

static void destroylast(Pool *p, Chain *c)
{
    freetoken(p, chaintakelast(c));
}

static void destroyfirst(Pool *p, Chain *c)
{
    freetoken(p, chaintakefirst(c));
}

static void chainclear(Pool *p, Chain *c)
{
    if (!c || c->count<1) {
        return;
    }
    for (Token *t; (t = chaintakefirst(c));) {
        freetoken(p, t);
    }
}

static void chainappend(Chain *c, Token *t)
{
    c->count++;
    t->next = 0;
    if (!c->tail) {
        t->prev = 0;
        c->head = c->tail = t;
        return;
    }
    t->prev = c->tail;
    c->tail->next = t;
    c->tail = t;
}

static void chainprepend(Chain *c, Token *t)
{
    c->count++;
    t->prev = 0;
    if (!c->head) {
        t->next = 0;
        c->head = c->tail = t;
        return;
    }
    t->next = c->head;
    c->head->prev = t;
    c->head = t;
}

// Insert after a token, or first. Like Universal Ctags, an insertion in
// the middle does not count the token.
static void chaininsertafter(Chain *c, Token *before, Token *t)
{
    if (!before) {
        chainprepend(c, t);
        return;
    }
    if (!before->next) {
        chainappend(c, t);
        return;
    }
    t->next = before->next;
    t->prev = before;
    before->next  = t;
    t->next->prev = t;
}

static void joinchain(Pool *, Chain *, Word *, Str sep, b32 nospace);

static void tokenappend(Pool *p, Word *w, Token *t)
{
    if (t->type & (T_PARENS|T_BRACKETS|T_BRACES|T_ANGLES)) {
        joinchain(p, t->chain, w, {}, 0);
    } else {
        wordcat(p, w, wordstr(t));
    }
}

// Append the text of tokens from..to, with a space after each token
// followed by one (unless nospace), and sep between tokens.
static void joinrange(Pool *p, Token *from, Token *to, Word *w, Str sep, b32 nospace)
{
    if (!from) {
        return;
    }
    Token *t = from;
    tokenappend(p, w, t);
    if (!nospace && t->space) {
        wordput(p, w, ' ');
    }
    while (t && t!=to) {
        t = t->next;
        if (!t) {
            return;
        }
        wordcat(p, w, sep);
        tokenappend(p, w, t);
        if (!nospace && t->space) {
            wordput(p, w, ' ');
        }
    }
}

static void joinchain(Pool *p, Chain *c, Word *w, Str sep, b32 nospace)
{
    if (!c || !c->count) {
        return;
    }
    for (Token *t = c->head; t; t = t->next) {
        if (t != c->head) {
            wordcat(p, w, sep);
        }
        tokenappend(p, w, t);
        if (!nospace && t->space) {
            wordput(p, w, ' ');
        }
    }
}

// A new token whose text joins tokens from..to.
static Token *extractrange(Pool *p, Token *from, Token *to, b32 nospace)
{
    if (!from) {
        return 0;
    }
    Token *r = newtoken(p);
    r->line    = from->line;
    r->linepos = from->linepos;
    r->type    = from->type;
    for (Token *t = from;; t = t->next) {
        if (!t) {
            return r;
        }
        tokenappend(p, &r->word, t);
        if (!nospace && t->space) {
            wordput(p, &r->word, ' ');
        }
        r->space = t->space;
        if (t == to) {
            return r;
        }
    }
}

// In a type name, whether a token other than a bracket chain is followed
// by a space before the next.
static b32 typespace(Token *t, Token *next)
{
    u32 spaced = T_PARENS|T_IDENT|T_KEYWORD|T_STAR|T_AND|T_ANDS;
    if (t->type & (T_PARENS|T_BRACKETS)) {
        return t->space;  // decided by what precedes it
    } else if (t->type == T_KEYWORD) {
        return !kwflag(t->kw, KF_DECLTYPE) && next->type&spaced;
    } else if (t->type & (T_IDENT|T_GT|T_AND|T_ANDS)) {
        return !!(next->type & spaced);
    } else if (t->type == T_STAR) {
        return !(next->type & (T_STAR|T_COMMA|T_RPAREN));
    }
    return 0;
}

// Like extractrange, but skipping keywords that are not part of type
// names (static, inline, ...). Null if there is nothing else.
static Token *extracttypename(Pool *p, Token *from, Token *to)
{
    if (!from) {
        return 0;
    }
    Token *t = from;
    while (t->type==T_KEYWORD && kwflag(t->kw, KF_NOTTYPE)) {
        if (t == to) {
            return 0;
        }
        t = t->next;
        if (!t) {
            return 0;
        }
    }

    Token *r = newtoken(p);
    r->line    = t->line;
    r->linepos = t->linepos;
    r->type    = t->type;

    // Departure P8: space each token against the next one written, not
    // a skipped keyword, so "int * restrict" is "int *", not "int * "
    Token *last    = 0;
    b32    skipped = 0;
    for (;; t = t->next) {
        if (t->type==T_KEYWORD && kwflag(t->kw, KF_NOTTYPE)) {
            skipped = 1;
        } else {
            if (last && (skipped ? typespace(last, t) : last->space)) {
                wordput(p, &r->word, ' ');
            }
            tokenappend(p, &r->word, t);
            r->space = t->space;
            last     = t;
            skipped  = 0;
        }
        if (t==to || !t->next) {
            return r;
        }
    }
}

static Token *condenseintotoken(Pool *p, Chain *c, b32 nospace)
{
    if (!c || !c->head) {
        return 0;
    }
    Token *r = newtoken(p);
    r->type    = T_UNKNOWN;
    r->line    = c->head->line;
    r->linepos = c->head->linepos;
    for (Token *t = c->head; t; t = t->next) {
        tokenappend(p, &r->word, t);
        if (!nospace && t->space) {
            wordput(p, &r->word, ' ');
        }
        r->space = t->space;
    }
    return r;
}

// Replace a chain's tokens with one token of their joined text.
static void condense(Pool *p, Chain *c, b32 nospace)
{
    Token *t = condenseintotoken(p, c, nospace);
    if (t) {
        chainclear(p, c);
        chainappend(c, t);
    }
}

static Token *chainat(Chain *c, iz i)
{
    if (!c || i<0 || i>=c->count) {
        return 0;
    }
    Token *t = c->head;
    for (; t && i; i--) {
        t = t->next;
    }
    return t;
}

// From a '<', the matching '>', counting nested angle brackets.
static Token *skipangles(Token *t)
{
    if (!t) {
        return 0;
    }
    i32 level = 1;
    for (t = t->next; t; t = t->next) {
        if (t->type == T_LT) {
            level++;
        } else if (t->type == T_GT) {
            if (level == 1) {
                return t;
            }
            level--;
        }
    }
    return 0;
}

// From a '>', back to the matching '<'.
static Token *skipanglesback(Token *t)
{
    if (!t) {
        return 0;
    }
    i32 level = 1;
    for (t = t->prev; t; t = t->prev) {
        if (t->type == T_GT) {
            level++;
        } else if (t->type == T_LT) {
            if (level == 1) {
                return t;
            }
            level--;
        }
    }
    return 0;
}

static Token *firstof(Chain *c, u32 types)
{
    Token *t = c ? c->head : 0;
    for (; t && !(t->type&types); t = t->next) {}
    return t;
}

static Token *firstnotof(Chain *c, u32 types)
{
    Token *t = c ? c->head : 0;
    for (; t && t->type&types; t = t->next) {}
    return t;
}

static Token *lastof(Chain *c, u32 types)
{
    Token *t = c ? c->tail : 0;
    for (; t && !(t->type&types); t = t->prev) {}
    return t;
}

static Token *nextof(Token *t, u32 types)
{
    if (!t) {
        return 0;
    }
    for (t = t->next; t && !(t->type&types); t = t->next) {}
    return t;
}

static Token *nextnotof(Token *t, u32 types)
{
    if (!t) {
        return 0;
    }
    for (t = t->next; t && t->type&types; t = t->next) {}
    return t;
}

static Token *prevof(Token *t, u32 types)
{
    if (!t) {
        return 0;
    }
    for (t = t->prev; t && !(t->type&types); t = t->prev) {}
    return t;
}

static Token *prevnotof(Token *t, u32 types)
{
    if (!t) {
        return 0;
    }
    for (t = t->prev; t && t->type&types; t = t->prev) {}
    return t;
}

// The last token of the types, looking inside parenthesis chains too,
// which also gives the chain holding it.
static Token *lastnestedof(Chain *c, u32 types, Chain **parent)
{
    if (!c) {
        return 0;
    }
    for (Token *t = c->tail; t; t = t->prev) {
        if (t->type & types) {
            if (parent) *parent = c;
            return t;
        }
        if (t->type == T_PARENS) {
            Token *r = lastnestedof(t->chain, types, parent);
            if (r) return r;
        }
    }
    return 0;
}

static Token *firstnestedof(Chain *c, u32 types, Chain **parent)
{
    if (!c) {
        return 0;
    }
    for (Token *t = c->head; t; t = t->next) {
        if (t->type & types) {
            if (parent) *parent = c;
            return t;
        }
        if (t->type == T_PARENS) {
            Token *r = firstnestedof(t->chain, types, parent);
            if (r) return r;
        }
    }
    return 0;
}

static Token *firstkeyword(Chain *c, i32 kw)
{
    if (!c || c->count<1) {
        return 0;
    }
    Token *t = c->head;
    for (; t && !iskeyword(t, kw); t = t->next) {}
    return t;
}

// Take and destroy tokens from..to, inclusive.
static void destroyrange(Pool *p, Chain *c, Token *from, Token *to)
{
    if (!from || !to) {
        return;
    }
    for (;;) {
        Token *next = from->next;
        chaintake(c, from);
        b32 last = from == to;
        freetoken(p, from);
        if (last) {
            return;
        }
        from = next;
    }
}

// Remove the tokens between a closer and its opener, leaving the
// chain's count as it was.
static void reducebackward(Pool *p, Token *start)
{
    u32    opener = start->type >> 4;
    Token *t      = start->prev;
    while (t && !(t->type&opener)) {
        Token *x = t;
        t = t->prev;
        t->next = x->next;
        x->next->prev = t;
        freetoken(p, x);
    }
}

static void normalizerange(Token *from, Token *to);

static void normalizechain(Chain *c)
{
    if (c && c->count>=1) {
        normalizerange(c->head, c->tail);
    }
}

// Space a type name the canonical way: "unsigned int *", "const T &",
// "int (*)(type &,type *)", "decltype(a) const".
static void normalizerange(Token *from, Token *to)
{
    if (!from || !to) {
        return;
    }
    for (Token *t = from;; t = t->next) {
        if (t->type & (T_PARENS|T_BRACKETS)) {
            b32 space = t->prev && t->prev->type==T_KEYWORD &&
                        kwflag(t->prev->kw, KF_DECLTYPE);
            normalizechain(t->chain);
            t->space = space;
        } else {
            t->space = t->next && typespace(t, t->next);
        }
        if (t == to) {
            break;
        }
    }
    to->space = 0;
}

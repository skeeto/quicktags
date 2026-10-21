// C and C++ parsing: tokens, statements, blocks, scopes, and tags
//
// This follows Universal Ctags' C/C++ parser decision for decision, so
// that its tags, and the order in which it numbers anonymous entities,
// come out the same. Statements accumulate in a chain of tokens, with
// bracketed parts condensed into subchains, until something notable
// (a keyword, a terminator, a block) decides how to analyze them.
//
// This is free and unencumbered software released into the public domain.

enum : u8 {
    S_FUNCTION, S_NAMESPACE, S_CLASS, S_ENUM, S_UNION, S_STRUCT,
    S_VARIABLE, S_PROTOTYPE, S_TYPEDEF, S_MODULE,
};

enum : u8 { A_UNKNOWN, A_PUBLIC, A_PRIVATE, A_PROTECTED };

// Keywords seen in the current statement
enum : u32 {
    SEEN_TYPEDEF     = 1u<<0,
    SEEN_INLINE      = 1u<<1,
    SEEN_EXTERN      = 1u<<2,
    SEEN_STATIC      = 1u<<3,
    SEEN_EXPLICIT    = 1u<<4,
    SEEN_OPERATOR    = 1u<<5,
    SEEN_VIRTUAL     = 1u<<6,
    SEEN_RETURN      = 1u<<7,
    SEEN_MUTABLE     = 1u<<8,
    SEEN_CONST       = 1u<<9,
    SEEN_VOLATILE    = 1u<<10,
    SEEN_DEPRECATED  = 1u<<11,
    SEEN_FRIEND      = 1u<<12,
    SEEN_CONSTEXPR   = 1u<<13,
    SEEN_CONSTEVAL   = 1u<<14,
    SEEN_CONSTINIT   = 1u<<15,
    SEEN_THREADLOCAL = 1u<<16,
    SEEN_EXPORT      = 1u<<17,
};

enum : i32 {
    CHAINMAX = 16384,  // tokens in one statement
    NESTMAX  = 1024,   // nested blocks and brackets
    PARAMMAX = 24,     // parameters described
    USEMAX   = 8,      // replacements of one -I macro between file reads
    BACKMAX  = 65536,  // pushed-back characters that stop replacements
};

// Replacements of a macro -I names since reading last went to the file.
struct Use {
    i32 count;
    i32 drains;  // Cpp.drains when counted
};

// Parameters (or template parameters) found in a list.
struct Params {
    i32    count;
    Chain *chain;
    Token *typestart[PARAMMAX];
    Token *typeend[PARAMMAX];
    Token *ids[PARAMMAX];
    u32    anon;  // which ids were made up
};

enum : u32 {
    SIG_PURE      = 1u<<0,
    SIG_DEFAULT   = 1u<<1,
    SIG_OVERRIDE  = 1u<<2,
    SIG_FINAL     = 1u<<3,
    SIG_DELETE    = 1u<<4,
    SIG_VOLATILE  = 1u<<5,
    SIG_SPEC      = 1u<<6,  // template specialization
    SIG_SCOPESPEC = 1u<<7,  // of the scope
    SIG_TRYBLOCK  = 1u<<8,  // a function-try-block
};

// A function signature found in a statement.
struct Sig {
    Token *paren;           // the parameter list
    Chain *parencontainer;  // holding it
    Token *idstart;
    Token *idend;
    Chain *idchain;         // holding the name
    Token *sigconst;
    Token *scopestart;
    Token *typestart;
    Token *typeend;
    b32    typewrapsid;     // the type contains name and parameters
    Token *trailingcomma;
    Token *specstart;
    Token *specend;
    u32    flags;
};

struct Cxx {
    Cpp    cpp;
    Pool   pool;
    File  *f;
    u32    lang;       // KL_C or KL_CXX
    Chain *chain;      // the statement
    Chain *tmpl;       // a template<...> prefix
    Chain *spec;       // a template specialization
    Params tparams;
    Token *tok;        // the chain's last token
    Token *unget;
    i32    ch;         // the character after the last token
    u32    seen;       // SEEN_*
    b32    confirmed;  // surely C++
    i32    nesting;
    b32    inattr;     // within [[...]]
    b32    pppon;      // public, protected, private are keywords
    b32    finalon;    // final is a keyword
    Chain *scope;
    Word   scopename;
    b32    scopedirty;
    Word   qualname;   // for --extras=+q
    Token *module;     // the current C++20 module
    b32    dryrun;     // tags are not written
    b32    skipnums;   // numbers and commas may be skipped (skipnumbers)
    Use   *uses;       // of -I replacements, by Macro.index, or null
    Tag    tag;
};

static b32 iscxx(Cxx *c)
{
    return c->lang == KL_CXX;
}

static b32 isc(Cxx *c)
{
    return c->lang == KL_C;
}

static b32 isheader(Cxx *c)
{
    return c->f->src->header;
}

static b32 cxxkind(Cxx *c, i32 kind)
{
    return kindon(c->f->kinds, c->f->src->lang, kind);
}

// Universal Ctags turns keywords on and off as it learns more.
static b32 kwdisabled(Cxx *c, i32 kw)
{
    switch (kw) {
    case KW_public:
    case KW_protected:
    case KW_private:   return !c->pppon;
    case KW_final:     return !c->finalon;
    }
    return 0;
}

// Parsing, defined below and in the other modules
static b32    cxxnext(Cxx *);
static void   cxxunget(Cxx *);
static b32    condensesubchain(Cxx *, u32 markers, b32 accepteof, b32 reduce);
static b32    condenseupto(Cxx *, u32 types, u32 markers, b32 reduce);
static b32    upto(Cxx *, u32 types, b32 reduce);
static b32    parseblock(Cxx *, b32 closer);
static b32    parseblockfull(Cxx *, b32 closer, b32 exported);
static b32    openingbracket(Cxx *);
static b32    parseenum(Cxx *);
static b32    parseclass(Cxx *, i32 kw, i32 kind, u8 scope);
static b32    parsegenerictypedef(Cxx *);
static void   extracttypedef(Cxx *, Chain *, b32 terminator, b32 tmpl);
static b32    parsenamespace(Cxx *);
static b32    parseusing(Cxx *);
static b32    parsetemplateprefix(Cxx *);
static Chain *templatetochain(Cxx *, b32 capture);
static void   emittemplateparams(Cxx *);
static Token *islambda(Cxx *);
static b32    handlelambda(Cxx *, Token *paren);
static b32    parsemodule(Cxx *);
static b32    parseimport(Cxx *);
static i32    knrfunction(Cxx *);
static b32    looksparams(Cxx *, Chain *, Params *);
static b32    lookforsig(Cxx *, Chain *, Sig *, Params *);
static i32    emitfunction(Cxx *, Sig *, i32 kind, b32 pushscopes);
static void   emitparams(Cxx *, Params *);
static i32    sigbeforebracket(Cxx *, Sig *);
static Token *macrorun(Cxx *, Chain *, Sig *);
static b32    macrotrial(Cxx *, Chain *, Token *end, b32 vars);
static b32    extractvars(Cxx *, Chain *, b32 knr);
static Token *firstnestedid(Cxx *, Chain *, Chain **parent);


// Statements

static void newstatementfull(Cxx *c, b32 exported)
{
    chainclear(&c->pool, c->chain);
    if (c->tmpl) {
        freechain(&c->pool, c->tmpl);
        c->tmpl = 0;
        c->tparams.count = 0;
    }
    c->seen = 0;
    if (exported) {
        c->seen = SEEN_EXPORT;
    }
    c->cpp.resolve = 0;
}

static void newstatement(Cxx *c)
{
    newstatementfull(c, 0);
}


// Scopes: a chain of name tokens, innermost last

static b32 scopeisglobal(Cxx *c)
{
    return c->scope->count < 1;
}

static b32 scopeisexported(Cxx *c)
{
    return c->scope->tail && c->scope->tail->scopeexported;
}

static u8 scopetype(Cxx *c)
{
    if (c->scope->count < 1) {
        return S_NAMESPACE;
    }
    return c->scope->tail->scopetype;
}

// Departure P10: within an anonymous namespace, names are file-scoped.
static b32 inanon(Cxx *c)
{
    for (Token *t = c->scope->head; t; t = t->next) {
        if (t->scopeanon) {
            return 1;
        }
    }
    return 0;
}

static i32 scopevarkind(Cxx *c)
{
    switch (scopetype(c)) {
    case S_CLASS:
    case S_UNION:
    case S_STRUCT:   return K_MEMBER;
    case S_FUNCTION: return K_LOCAL;
    }
    return K_VARIABLE;
}

// The long name of the kind of the innermost scope, for scope fields.
static Str scopekindname(Cxx *c)
{
    switch (c->scope->tail->scopetype) {
    case S_NAMESPACE: return "namespace";
    case S_CLASS:     return "class";
    case S_ENUM:      return "enum";
    case S_FUNCTION:  return "function";
    case S_PROTOTYPE: return "prototype";
    case S_STRUCT:    return "struct";
    case S_UNION:     return "union";
    case S_VARIABLE:  return "variable";
    case S_TYPEDEF:   return "typedef";
    case S_MODULE:    return "module";
    }
    return "function";
}

static Str scopefullname(Cxx *c)
{
    if (c->scopedirty) {
        c->scopename.len = 0;
        joinchain(&c->pool, c->scope, &c->scopename, "::", 1);
        c->scopedirty = 0;
    }
    return {c->scopename.data, c->scopename.len};
}

static void scopepushtop(Cxx *c, Token *t)
{
    if (c->scope->tail && c->scope->tail->scopeexported) {
        t->scopeexported = 1;
    }
    chainappend(c->scope, t);
    c->scopedirty = 1;
}

static Token *scopetaketop(Cxx *c)
{
    Token *t = chaintakelast(c->scope);
    c->scopedirty = 1;
    return t;
}

static void scopepush(Cxx *c, Token *t, u8 type, u8 access)
{
    t->scopetype   = type;
    t->scopeaccess = access;
    scopepushtop(c, t);
}

static void scopepushexported(Cxx *c, Token *t, u8 type, u8 access, b32 exported)
{
    scopepush(c, t, type, access);
    t->scopeexported = exported;
}

static void scopepop(Cxx *c)
{
    freetoken(&c->pool, scopetaketop(c));
}

static void scopesetaccess(Cxx *c, u8 access)
{
    if (c->scope->count >= 1) {
        c->scope->tail->scopeaccess = access;
    }
}


// Tags

// Begin a tag named by the token, in the current scope, or null if its
// kind is not enabled. Fill in the rest, then commit.
static Tag *cxxtag(Cxx *c, i32 kind, Token *t)
{
    if (!cxxkind(c, kind)) {
        return 0;
    }
    Tag *tag = &c->tag;
    *tag = {};
    tag->name    = wordstr(t);
    tag->anon    = t->anon;
    tag->kind    = kind;
    tag->line    = t->line;
    tag->linepos = t->linepos;
    if (!scopeisglobal(c)) {
        tag->scopekind = scopekindname(c);
        tag->scope     = scopefullname(c);
    }
    return tag;
}

// Write a finished tag. With the qualified extra, a tag in a scope is
// written again named by its scope, as in ns::Class::name, unless the
// scope is a function or prototype. An enumerator is qualified by its
// enum's scope, so one in a global enum gets no second tag.
static void cxxcommit(Cxx *c, Tag *tag)
{
    if (!tag->name.len || c->dryrun) {
        return;
    }
    tagemit(c->f, tag);
    if (!(c->f->extras & EXTRA_QUALIFIED) || !tag->scope.data) {
        return;
    }

    u8    type = S_NAMESPACE;
    Word *w    = &c->qualname;
    if (!scopeisglobal(c)) {
        type = c->scope->tail->scopetype;
    }
    w->len = 0;
    switch (type) {
    case S_FUNCTION:
    case S_PROTOTYPE:
        return;
    case S_ENUM:
        if (c->scope->count < 2) {
            return;
        }
        joinrange(&c->pool, c->scope->head, c->scope->tail->prev, w, "::", 1);
        break;
    default:
        wordcat(&c->pool, w, tag->scope);
    }
    wordcat(&c->pool, w, type==S_MODULE ? Str(":") : "::");
    wordcat(&c->pool, w, tag->name);

    Tag q = *tag;
    q.name = {w->data, w->len};
    tagemit(c->f, &q);
}

// Is the range plausible as a type? Rejects long runs, too many
// identifiers in a row (macros), and parentheses between identifiers.
static b32 checktype(Token *start, Token *end)
{
    if (!start || !end) {
        return 0;
    }
    i32 total  = 0;
    i32 parens = 0;
    i32 words  = 0;
    i32 run    = 0;
    for (; start; start = start->next) {
        if (++total > 30) {
            return 0;
        }
        if (start->type == T_IDENT) {
            run++;
            words++;
            if (run > 4) {
                return 0;
            }
        } else {
            run = 0;
            if (start->type == T_PARENS) {
                if (++parens > 3) {
                    return 0;
                }
                if (total>1 && start->prev->type==T_IDENT && start!=end &&
                        start->next && start->next->type==T_IDENT) {
                    return 0;  // identifier () identifier
                }
            } else if (start->type == T_KEYWORD) {
                words++;
            }
        }
        if (start == end) {
            break;
        }
    }
    return words>=1 && start;
}

// Set the tag's typeref from the range, after leading keywords that are
// not part of types, as "typename:T" or "struct:S" and so on. Returns a
// token holding the text, to free after committing, or null.
static Token *settype(Cxx *c, Tag *tag, Token *start, Token *end)
{
    while (start->type==T_KEYWORD && kwflag(start->kw, KF_NOTTYPE)) {
        if (start == end) {
            return 0;
        }
        start = start->next;
    }

    Str kind = "typename";
    b32 mark = start->type==T_KEYWORD && kwflag(start->kw, KF_TYPEMARK);
    if (start != end) {
        if (mark) {
            kind  = keywords[start->kw].name;
            start = start->next;
        }
    } else if (mark) {
        kind = "meta";  // a lone typename or class: a template parameter
    }

    if (!checktype(start, end)) {
        return 0;
    }
    normalizerange(start, end);
    Token *t = extracttypename(&c->pool, start, end);
    if (t) {
        tag->typekind = kind;
        tag->type     = wordstr(t);
    }
    return t;
}

// An identifier token for an unnamed entity, at the current position.
// Its tags are anonymous extras unless the name has its own prefix.
static Token *anontoken(Cxx *c, i32 kind, Str prefix)
{
    Token *t = newtoken(&c->pool);
    wordcat(&c->pool, &t->word, prefix.data ? prefix : Str("__anon"));
    Str    name = anonname(c->f, c->pool.a, kind);
    wordcat(&c->pool, &t->word, cuthead(name, 6));
    t->type    = T_IDENT;
    t->space   = 1;
    t->anon    = !prefix.data;
    t->line    = cppline(&c->cpp);
    t->linepos = cpppos(&c->cpp);
    return t;
}


// Tokenizer

enum : u32 {
    CC_IDSTART  = 1u<<0,
    CC_IDPART   = 1u<<1,
    CC_DIGIT    = 1u<<2,
    CC_NUMBER   = 1u<<4,  // valid within a number
    CC_SINGLE   = 1u<<5,  // a token of one character
    CC_REPEAT   = 1u<<6,  // a token of one or more of a character
    CC_OPERATOR = 1u<<7,  // merged with neighbors
    CC_CUSTOM   = 1u<<8,
};

struct CharClass {
    u32 cls;
    u32 single;  // token type alone
    u32 repeat;  // token type repeated
};

static CharClass charclass(i32 c)
{
    CharClass r = {};
    if (c>=0x80 && c<=0xff) {
        r.cls = CC_IDSTART|CC_IDPART;
        return r;
    }
    switch (c) {
    case '!': case '%': case '+': case '-': case '/': case '?': case '^':
    case '|':
        r.cls = CC_OPERATOR;
        break;
    case '$': case '_':
        r.cls = CC_IDSTART|CC_IDPART;
        break;
    case '~':
        r.cls = CC_IDSTART;
        break;
    case '&': r = {CC_REPEAT, T_AND, T_ANDS};               break;
    case '.': r = {CC_NUMBER|CC_REPEAT, T_DOT, T_DOTS};     break;
    case ':': r = {CC_REPEAT, T_COLON, T_COLONS};           break;
    case '=': r = {CC_OPERATOR|CC_REPEAT, T_ASSIGN, T_OPERATOR}; break;
    case '(': r = {CC_SINGLE, T_LPAREN, 0};                 break;
    case ')': r = {CC_SINGLE, T_RPAREN, 0};                 break;
    case '*': r = {CC_SINGLE, T_STAR, 0};                   break;
    case ',': r = {CC_SINGLE, T_COMMA, 0};                  break;
    case ';': r = {CC_SINGLE, T_SEMICOLON, 0};              break;
    case '>': r = {CC_SINGLE, T_GT, 0};                     break;  // never ">>"
    case ']': r = {CC_SINGLE, T_RBRACKET, 0};               break;
    case '{': r = {CC_SINGLE, T_LBRACE, 0};                 break;
    case '}': r = {CC_SINGLE, T_RBRACE, 0};                 break;
    case '<': r = {CC_CUSTOM, T_LT, 0};                     break;
    case '[': r = {CC_CUSTOM, T_LBRACKET, 0};               break;
    default:
        if (c>='0' && c<='9') {
            r.cls = CC_IDPART|CC_DIGIT|CC_NUMBER;
        } else if ((c>='a' && c<='f') || (c>='A' && c<='F')) {
            r.cls = CC_IDSTART|CC_IDPART|CC_NUMBER;
        } else if (c=='h' || c=='l' || c=='u' || c=='x' ||
                   c=='H' || c=='L' || c=='U' || c=='X') {
            r.cls = CC_IDSTART|CC_IDPART|CC_NUMBER;
        } else if ((c>='g' && c<='z') || (c>='G' && c<='Z')) {
            r.cls = CC_IDSTART|CC_IDPART;
        }
    }
    return r;
}

static void nextchar(Cxx *c)
{
    c->ch = cppget(&c->cpp);
}

static b32 innumber(u8 b)  // CC_NUMBER, from a bit table
{
    static u64 const bits[2] = {
        0x03ff400000000000,  // . 0-9
        0x0120117e0120117e,  // A-F H L U X a-f h l u x
    };
    return b<0x80 && bits[b>>6]>>(b&63) & 1;
}

static u8 *skipblanks(u8 *s, u8 *end)  // spaces, tabs, and newlines
{
    for (; s < end; s++) {
        u8 b = *s;
        if (b!=' ' && b!='\t' && b!='\n' && !(b=='\r' && s+1<end && s[1]=='\n')) {
            break;
        }
    }
    return s;
}

// Fast path for data tables: at a ',' in a statement already at the
// token limit, where each token replaces the last until one matters,
// skip a run of plain numbers and commas in the source, leaving the
// last ',' as the current character with the state reading it would
// leave. Anything else stops the run, before the number it is in.
static void skipnumbers(Cxx *c)
{
    Cpp *p = &c->cpp;
    if (p->nback || p->state!=D_NONE) {
        return;
    }

    u8 *s    = p->in.p;
    u8 *end  = p->in.end;
    u8 *stop = 0;  // just past the last ',' of a complete number
    for (;;) {
        s = skipblanks(s, end);
        if (s==end || (u8)(*s-'0')>9) {
            break;
        }
        for (s++; s<end && innumber(*s); s++) {}
        if (s<end && (cppident(*s) || *s=='\'')) {
            break;  // more of a token than a number
        }
        s = skipblanks(s, end);
        if (s==end || *s!=',') {
            break;
        }
        stop = ++s;
    }

    if (stop) {
        advance(&p->in, stop);
        cppdrained(p);
        p->innum     = 0;
        p->prevident = 0;
        p->accept    = 0;
        c->ch = ',';
    }
}

static void putch(Cxx *c, Token *t, i32 ch)
{
    if (ch == C_STRING) {
        wordput(&c->pool, &t->word, '"');
        wordcat(&c->pool, &t->word, {c->cpp.lit.data, c->cpp.lit.len});
        wordput(&c->pool, &t->word, '"');
    } else if (ch == C_CHAR) {
        wordput(&c->pool, &t->word, '\'');
        wordcat(&c->pool, &t->word, {c->cpp.lit.data, c->cpp.lit.len});
        wordput(&c->pool, &t->word, '\'');
    } else {
        wordput(&c->pool, &t->word, (u8)ch);
    }
}

// Note keyword-like attributes in __attribute__((...)) and [[...]].
static void analyzeattr(Cxx *c, Chain *attrs)
{
    for (Token *t = attrs ? attrs->head : 0; t; t = t->next) {
        if (t->type == T_IDENT) {
            Str w = wordstr(t);
            if (w=="always_inline" || w=="__always_inline__") {
                c->seen |= SEEN_INLINE;
            } else if (w=="deprecated" || w=="__deprecated__") {
                c->seen |= SEEN_DEPRECATED;
            }
        }
    }
}

// Drop __attribute__((...)) and the like, whose keyword was just read,
// returning the next token instead.
static b32 dropattribute(Cxx *c)
{
    Token *head = chaintakelast(c->chain);
    if (!cxxnext(c)) {
        freetoken(&c->pool, head);
        return 0;
    }
    if (c->tok->type != T_LPAREN) {
        freetoken(&c->pool, head);
        return 1;
    }
    if (!condensesubchain(c, T_LPAREN|T_LBRACKET|T_LBRACE, 0, 0)) {
        freetoken(&c->pool, head);
        destroylast(&c->pool, c->chain);
        return cxxnext(c);
    }
    Token *inner = c->tok->chain->head;
    if (inner && inner->next && inner->next->type==T_PARENS) {
        analyzeattr(c, inner->next->chain);
    }
    Token *args = chaintakelast(c->chain);
    freetoken(&c->pool, head);
    freetoken(&c->pool, args);
    return cxxnext(c);
}

// Drop [[...]], the first '[' already a token and the second current.
static b32 dropcxx11attribute(Cxx *c)
{
    c->inattr = 1;
    if (!condensesubchain(c, T_LPAREN|T_LBRACKET|T_LBRACE, 0, 0)) {
        destroylast(&c->pool, c->chain);
        c->inattr = 0;
        return cxxnext(c);
    }
    Token *inner = chainat(c->tok->chain, 1);
    if (inner && inner->type==T_BRACKETS) {
        analyzeattr(c, inner->chain);
    }
    destroylast(&c->pool, c->chain);
    c->inattr = 0;
    return cxxnext(c);
}

// Macros -I names (LEX-14)

enum : i32 { DROPPED = 2 };  // readtoken: read another in its place

// A macro's replacements since reading last went back to the file.
static Use *uses(Cxx *c, Macro *m)
{
    if (!c->uses) {
        c->uses = alloc<Use>(c->pool.a, c->f->macros->len);
    }
    Use *u = c->uses + m->index;
    if (u->drains != c->cpp.drains) {
        u->count  = 0;
        u->drains = c->cpp.drains;
    }
    return u;
}

// The macro -I names a word that is not a keyword, unless it was
// replaced too often in a row, which ends a loop of replacements.
static Macro *findmacro(Cxx *c, Str w)
{
    Map<Macro> *map = c->f->macros ? c->f->macros->map : 0;
    Macro      *m   = map ? upsert(&map, w, (Arena *)0) : 0;
    if (m && m->repl.data && uses(c, m)->count>=USEMAX) {
        return 0;
    }
    return m;
}

// Drop the statement's last token, a macro -I names, and the group in
// parentheses after it if the macro takes one, then push back its
// replacement, if any, to be read in their place. The statement held
// initial tokens before it. Returns DROPPED, or 0 at the end.
static i32 dropmacro(Cxx *c, Macro *m, iz initial)
{
    Pool *p    = &c->pool;
    i32   line = c->tok->line;
    iz    pos  = c->tok->linepos;
    destroylast(p, c->chain);

    Token *group = 0;
    if (m->parens) {
        while (cppspace(c->ch)) {
            nextchar(c);
        }
        if (c->ch == '(') {
            if (!cxxnext(c) || !condensesubchain(c, T_LPAREN, 0, 0)) {
                return 0;
            }
            group = chaintakelast(c->chain);
        }
    }

    // Unless replacements may be running away
    b32 room = c->nesting<NESTMAX && initial<CHAINMAX &&
               cppbacklen(&c->cpp)<BACKMAX;
    if (m->repl.data && room) {
        unget(&c->cpp, c->ch);
        if (cppident(m->repl[m->repl.len-1]) && cppident(c->ch)) {
            unget(&c->cpp, ' ');  // departure X11: words stay apart
        }
        ungetat(&c->cpp, m->repl, line, pos);
        uses(c, m)->count++;
        nextchar(c);
    }
    freetoken(p, group);
    return DROPPED;
}

// Read a token onto the statement: 0 at the end, 1, or DROPPED.
static i32 readtoken(Cxx *c)
{
    Pool *p       = &c->pool;
    iz    initial = c->chain->count;
    if (initial >= CHAINMAX) {
        destroylast(p, c->chain);
    }

    if (c->unget) {
        chainappend(c->chain, c->unget);
        c->tok   = c->unget;
        c->unget = 0;
        return c->tok->type != T_EOF;
    }

    if (c->skipnums && c->ch==',' && c->chain->count>=CHAINMAX-1 && !c->f->nofast) {
        skipnumbers(c);
    }

    Token *t = newtoken(p);
    chainappend(c->chain, t);
    c->tok = t;

    while (cppspace(c->ch)) {
        nextchar(c);
    }
    c->cpp.resolve = 1;
    t->line    = cppline(&c->cpp);
    t->linepos = cpppos(&c->cpp);

    if (c->ch == C_EOF) {
        t->type  = T_EOF;
        t->space = 0;
        return 0;
    }

    CharClass cc = charclass(c->ch);
    if (cc.cls & CC_IDSTART) {
        t->type  = T_IDENT;
        t->space = 0;
        wordput(p, &t->word, (u8)c->ch);
        if (c->ch == '~') {
            // A destructor's name, possibly after spaces, or an operator
            nextchar(c);
            if (cppspace(c->ch)) {
                t->space = 1;
                do {
                    nextchar(c);
                } while (cppspace(c->ch));
            }
            if (!(charclass(c->ch).cls & CC_IDSTART)) {
                t->type = T_OPERATOR;
                if (!t->space && c->ch=='=') {
                    wordput(p, &t->word, '=');
                    nextchar(c);
                    t->space = cppspace(c->ch);
                }
                return 1;
            }
        } else {
            nextchar(c);
        }
        while (charclass(c->ch).cls & CC_IDPART) {
            wordput(p, &t->word, (u8)c->ch);
            nextchar(c);
        }

        Str    w  = wordstr(t);
        i32    kw = kwlookup(w, c->lang);
        Macro *m  = kw<0 ? findmacro(c, w) : 0;
        if (kw >= 0) {
            if (!kwdisabled(c, kw)) {
                t->type = T_KEYWORD;
                t->kw   = kw;
                if (kwflag(kw, KF_DROP)) {
                    return dropattribute(c);
                }
            }
        } else if (m) {
            return dropmacro(c, m, initial);
        } else if (w=="_Pragma" || w=="__pragma") {
            return dropattribute(c);  // departure: dropped like attributes
        }
        t->space = cppspace(c->ch);
        return 1;
    }

    if (c->ch == '-') {
        wordput(p, &t->word, '-');
        nextchar(c);
        if (c->ch == '>') {
            t->type = T_ARROW;
            wordput(p, &t->word, '>');
            nextchar(c);
        } else {
            t->type = T_OPERATOR;
            if (c->ch == '-') {
                wordput(p, &t->word, '-');
                nextchar(c);
            }
        }
        t->space = cppspace(c->ch);
        return 1;
    }

    if (c->ch==C_STRING || c->ch==C_CHAR) {
        t->type = c->ch==C_STRING ? T_STRING : T_CHAR;
        putch(c, t, c->ch);
        nextchar(c);
        t->space = cppspace(c->ch);
        return 1;
    }

    if (cc.cls & CC_DIGIT) {
        t->type = T_NUMBER;
        wordput(p, &t->word, (u8)c->ch);
        for (;;) {
            nextchar(c);
            if (!(charclass(c->ch).cls & CC_NUMBER)) {
                break;
            }
            wordput(p, &t->word, (u8)c->ch);
        }
        t->space = cppspace(c->ch);
        return 1;
    }

    if (cc.cls & CC_REPEAT) {
        i32 first = c->ch;
        t->type = cc.single;
        wordput(p, &t->word, (u8)first);
        nextchar(c);
        if (c->ch == first) {
            t->type = cc.repeat;
            do {
                wordput(p, &t->word, (u8)c->ch);
                nextchar(c);
            } while (c->ch == first);
        }
        t->space = cppspace(c->ch);
        return 1;
    }

    if (cc.cls & CC_CUSTOM) {
        t->type = cc.single;
        wordput(p, &t->word, (u8)c->ch);
        nextchar(c);
        if (t->type == T_LT) {
            // Only <, <<, <<=, <=, <=>, since < opens templates
            if (c->ch == '<') {
                t->type = T_OPERATOR;
                wordput(p, &t->word, '<');
                nextchar(c);
                if (c->ch == '=') {
                    wordput(p, &t->word, '=');
                    nextchar(c);
                }
            } else if (c->ch == '=') {
                t->type = T_OPERATOR;
                wordput(p, &t->word, '=');
                nextchar(c);
                if (c->ch == '>') {
                    wordput(p, &t->word, '>');
                    nextchar(c);
                }
            }
            t->space = cppspace(c->ch);
        } else {
            // [[ begins an attribute, even with space between
            t->space = cppspace(c->ch);
            if (t->space) {
                do {
                    nextchar(c);
                } while (cppspace(c->ch));
            }
            if (c->ch=='[' && !c->inattr) {
                return dropcxx11attribute(c);
            }
        }
        return 1;
    }

    if (cc.cls & CC_SINGLE) {
        t->type = cc.single;
        wordput(p, &t->word, (u8)c->ch);
        nextchar(c);
        t->space = cppspace(c->ch);
        return 1;
    }

    if (cc.cls & CC_OPERATOR) {
        t->type = T_OPERATOR;
        wordput(p, &t->word, (u8)c->ch);
        nextchar(c);
        while (charclass(c->ch).cls & CC_OPERATOR) {
            wordput(p, &t->word, (u8)c->ch);
            nextchar(c);
        }
        t->space = cppspace(c->ch);
        return 1;
    }

    t->type = T_UNKNOWN;
    putch(c, t, c->ch);
    nextchar(c);
    t->space = cppspace(c->ch);
    return 1;
}

// Read the next token onto the statement. Returns 0 at the end. Each
// identifier -I drops is a level of nesting while the next is read, as
// Universal Ctags reads it by recursion.
static b32 cxxnext(Cxx *c)
{
    i32 levels = 0;
    i32 r      = 0;
    while ((r = readtoken(c)) == DROPPED) {
        c->nesting++;
        levels++;
    }
    c->nesting -= levels;
    return r;
}

// Return the last token to be read again. If one is already waiting,
// it goes back into the character stream, which, as in Universal Ctags,
// puts it after the character already read beyond it.
static void cxxunget(Cxx *c)
{
    if (c->unget) {
        if (c->unget->space) {
            unget(&c->cpp, ' ');
        }
        ungetstr(&c->cpp, wordstr(c->unget));
        freetoken(&c->pool, c->unget);
    }
    c->unget = chaintakelast(c->chain);
    c->tok   = c->chain->tail;
}


// Subchains

// The current token opens a bracket: read to its match, making the
// pair and everything between a subchain, which replaces it. Broken
// input (mismatched or unterminated) leaves an empty pair.
static b32 condensesubchain(Cxx *c, u32 markers, b32 accepteof, b32 reduce)
{
    Pool  *p     = &c->pool;
    Chain *outer = c->chain;
    c->chain = newchain(p);

    Token *opener = chaintakelast(outer);
    chainappend(c->chain, opener);

    Token *ct = newtoken(p);
    ct->line    = opener->line;
    ct->linepos = opener->linepos;
    ct->type    = c->tok->type << 8;
    ct->chain   = c->chain;
    chainappend(outer, ct);

    u32 closer = c->tok->type << 4;
    u32 types  = closer;
    if (accepteof) {
        types |= T_EOF;
    }
    b32 ok     = condenseupto(c, types, markers, reduce);

    if (!ok || c->chain->tail->type!=closer) {
        while (c->chain->count > 1) {
            destroylast(p, c->chain);
        }
        Token *fake = newtoken(p);
        fake->line    = ct->line;
        fake->linepos = ct->linepos;
        fake->type    = closer;
        switch (closer) {
        case T_RBRACE:   wordput(p, &fake->word, '}'); break;
        case T_RPAREN:   wordput(p, &fake->word, ')'); break;
        case T_RBRACKET: wordput(p, &fake->word, ']'); break;
        case T_GT:       wordput(p, &fake->word, '>'); break;
        }
        chainappend(c->chain, fake);
    }

    c->chain = outer;
    c->tok   = outer->tail;
    return ok;
}

// Read up to one of the types, making subchains of the brackets among
// the markers (which are not checked against the types). Returns 0 at
// an unexpected end or a mismatched bracket.
static b32 condenseupto(Cxx *c, u32 types, u32 markers, b32 reduce)
{
    if (!cxxnext(c)) {
        return !!(types & T_EOF);
    }
    u32 closers = markers << 4;
    for (;;) {
        if (c->tok->type & types) {
            if (reduce) {
                reducebackward(&c->pool, c->tok);
            }
            return 1;
        }

        if (c->tok->type & markers) {
            Token *paren = 0;
            if (c->tok->type==T_LBRACE && iscxx(c) && (paren = islambda(c))) {
                if (!handlelambda(c, paren)) {
                    return 0;
                }
            } else {
                if (++c->nesting > NESTMAX) {
                    return 0;
                }
                b32 ok = condensesubchain(c, markers, !!(types&T_EOF), reduce);
                c->nesting--;
                if (!ok) {
                    return 0;
                }
            }
            if (c->tok->type & types) {
                return 1;
            }
            if (!cxxnext(c)) {
                return !!(types & T_EOF);
            }
            continue;
        }

        if (c->tok->type & closers) {
            return 0;  // mismatched
        }
        c->skipnums = !(types & (T_COMMA|T_NUMBER));
        b32 more = cxxnext(c);
        c->skipnums = 0;
        if (!more) {
            return !!(types & T_EOF);
        }
    }
}

// Read up to one of the types, condensing all brackets but angles.
static b32 upto(Cxx *c, u32 types, b32 reduce)
{
    return condenseupto(c, types, T_LBRACE|T_LPAREN|T_LBRACKET, reduce);
}

static b32 skiptosemicolon(Cxx *c)
{
    if (c->tok->type & (T_SEMICOLON|T_EOF)) {
        return 1;
    }
    return upto(c, T_SEMICOLON|T_EOF, 0);
}

// From an identifier or "::", read past a qualified name A::B::C.
static b32 qualifiedname(Cxx *c)
{
    if (c->tok->type==T_IDENT && !cxxnext(c)) {
        return 0;
    }
    while (c->tok->type == T_COLONS) {
        if (!cxxnext(c) || c->tok->type!=T_IDENT || !cxxnext(c)) {
            return 0;
        }
    }
    return 1;
}


// Statements

// At a ';' (or closer or end) that ends a statement with no keyword of
// interest: a variable declaration or a function prototype, perhaps.
static void analyzestatement(Cxx *c)
{
    if (c->chain->count < 2 || c->seen&SEEN_RETURN) {
        return;
    }
    Token *t = c->chain->head;
    if (!(t->type & (T_IDENT|T_KEYWORD|T_COLONS))) {
        return;
    }

    if (scopetype(c) == S_FUNCTION) {
        extractvars(c, c->chain, 0);
        return;
    }

    Sig    sig    = {};
    Params params = {};
    b32    pz     = cxxkind(c, K_PROTOTYPE) && cxxkind(c, K_PARAMETER);
    for (;;) {
        if (!lookforsig(c, c->chain, &sig, pz ? &params : 0) ||
                (sig.typeend && sig.typeend->type==T_ASSIGN)) {
            break;
        }
        Token *end = macrorun(c, c->chain, &sig);
        if (end && macrotrial(c, c->chain, end, 1)) {
            // Departure P1: macro calls before the declaration
            destroyrange(&c->pool, c->chain, c->chain->head, end);
            continue;
        }
        if (!(c->seen & SEEN_FRIEND)) {
            i32 scopes = emitfunction(c, &sig, K_PROTOTYPE, 1);
            if (pz) {
                emitparams(c, &params);
            }
            for (; scopes > 0; scopes--) {
                scopepop(c);
            }
        }
        if (!sig.trailingcomma) {
            return;
        }
        // RetType a(...), b(...); skip past the type name to the next
        Token *beg = firstnotof(c->chain, T_IDENT|T_KEYWORD);
        destroyrange(&c->pool, c->chain, beg, sig.trailingcomma);
    }

    if (c->seen & (SEEN_EXPLICIT|SEEN_OPERATOR|SEEN_VIRTUAL)) {
        return;  // must have been a function
    }
    extractvars(c, c->chain, 0);
}

// After public, protected, or private outside of a class head.
static b32 accessspecifier(Cxx *c)
{
    u8 type = scopetype(c);
    if (type!=S_CLASS && type!=S_UNION && type!=S_STRUCT) {
        if (!c->confirmed) {
            c->tok->type = T_IDENT;
            return 1;
        }
        return 0;  // in the wrong scope
    }
    if (!c->confirmed && c->tok->prev) {
        c->tok->type = T_IDENT;  // likely not an access specifier
        return 1;
    }

    Token *initial = c->tok;
    if (!upto(c, T_COLON|T_SEMICOLON|T_RBRACE|T_EOF, 0)) {
        return 0;
    }
    if (c->tok->type==T_COLON && !initial->prev) {
        c->confirmed = 1;
    }
    switch (initial->kw) {
    case KW_public:    scopesetaccess(c, A_PUBLIC);    break;
    case KW_private:   scopesetaccess(c, A_PRIVATE);   break;
    case KW_protected: scopesetaccess(c, A_PROTECTED); break;
    }
    chainclear(&c->pool, c->chain);
    return 1;
}

// After if, for, while, switch, or catch: variables declared within
// the parentheses.
static b32 ifforwhile(Cxx *c)
{
    i32 kw = c->tok->kw;
    if (!upto(c, T_PARENS|T_SEMICOLON|T_LBRACE|T_EOF, 0)) {
        return 0;
    }
    if (c->tok->type & (T_EOF|T_SEMICOLON|T_LBRACE)) {
        return 1;
    }

    Chain *ch = c->tok->chain;
    b32    ok = kw == KW_catch;
    if (!ok) {
        ok = chainat(ch, 1)->type == T_KEYWORD;
    }
    if (!ok) {
        // Ambiguous with & && * <, unless followed by = or ;
        Token *t = firstof(ch, T_AND|T_ANDS|T_STAR|T_LT|T_ASSIGN|T_SEMICOLON);
        if (!t) {
            ok = 1;
        } else if (t->type & (T_AND|T_ANDS|T_STAR|T_LT)) {
            ok = !!firstof(ch, T_ASSIGN|T_SEMICOLON);
        } else {
            ok = 1;
        }
    }
    if (ok) {
        destroyfirst(&c->pool, ch);
        Token *t = ch->tail;
        t->type     = T_SEMICOLON;
        t->word.len = 0;
        wordput(&c->pool, &t->word, ';');
        extractvars(c, ch, 0);
    }
    return 1;
}


// Blocks

// At '{': an initializer list, a lambda, a function body, or another
// kind of block.
static b32 openingbracket(Cxx *c)
{
    u8     type = scopetype(c);
    Token *prev = c->tok->prev;
    Token *aux  = 0;

    b32 init = 0;
    if (prev && prev->type==T_ASSIGN &&
            (type==S_FUNCTION || type==S_CLASS || type==S_STRUCT ||
             type==S_UNION || type==S_NAMESPACE)) {
        init = 1;  // x = {...}
    } else if (iscxx(c) && prev) {
        if (prev->type==T_IDENT && wordstr(prev)!="override") {
            // T {...}, T x {...}, new T {...}, C::C() : m {...} {
            Token *pp = prev->prev;
            init = !pp || pp->type&(T_COLON|T_COMMA) ||
                   (pp->type&(T_IDENT|T_STAR|T_AND|T_GT|T_KEYWORD) &&
                    !prevof(pp, T_PARENS));
        } else if (prev->type == T_BRACKETS) {
            // type x[]...[] {...}, but not a lambda's [] {...}
            aux  = prevnotof(prev, T_BRACKETS);
            init = aux && aux->type==T_IDENT;
        }
    } else if (!prev && c->seen&SEEN_RETURN) {
        init = 1;  // return {...}
    }
    if (init) {
        return condensesubchain(c, T_LBRACE|T_LPAREN|T_LBRACKET, 0, 1);
    }

    Token *paren = 0;
    if (iscxx(c) && (paren = islambda(c))) {
        return handlelambda(c, paren);  // leaving the statement alive
    }

    Sig sig      = {};
    i32 scopes   = 0;
    b32 exported = 0;
    if (type != S_FUNCTION) {
        // Very likely a function (or a block like extern "C" {...})
        scopes   = sigbeforebracket(c, &sig);
        exported = !!(c->seen & SEEN_EXPORT);
    }
    newstatementfull(c, exported);
    if (!parseblockfull(c, 1, exported)) {
        return 0;
    }
    if (scopes < 1) {
        return 1;
    }

    if (sig.flags & SIG_TRYBLOCK) {
        // A function-try-block's catch blocks belong to it
        for (;;) {
            if (!cxxnext(c)) {
                break;
            }
            if (!iskeyword(c->tok, KW_catch)) {
                cxxunget(c);
                break;
            }
            if (!ifforwhile(c)) {
                return 0;
            }
            newstatement(c);
            if (!cxxnext(c)) {
                break;
            }
            if (c->tok->type != T_LBRACE) {
                cxxunget(c);
                break;
            }
            if (!parseblock(c, 1)) {
                return 0;
            }
        }
    }

    for (; scopes > 0; scopes--) {
        scopepop(c);
    }
    return 1;
}

// Is "module" or "import" here a C++20 keyword?
static b32 modulekeyword(Cxx *c, Str word)
{
    return !(c->seen & ~SEEN_EXPORT) &&
           (!c->tok->prev || iskeyword(c->tok->prev, KW_export)) &&
           iscxx(c) && scopeisglobal(c) && wordstr(c->tok)==word;
}

static b32 parseblockinternal(Cxx *c, b32 closer, b32 exported)
{
    newstatementfull(c, exported);
    if (closer) {
        c->cpp.resolve = 1;
    }

    for (;;) {
        if (!cxxnext(c)) {
        eof:
            return !closer;
        }
    again:
        if (exported) {
            c->seen |= SEEN_EXPORT;
        }

        switch (c->tok->type) {
        case T_KEYWORD:
            switch (c->tok->kw) {
            case KW_namespace: {
                u8 type = scopetype(c);
                if ((type==S_NAMESPACE || type==S_FUNCTION) &&
                        (c->confirmed || !prevof(c->tok, T_STAR|T_AND|T_KEYWORD))) {
                    if (!parsenamespace(c)) {
                        return 0;
                    }
                } else {
                    // In a header, maybe not C++: treat it as an identifier
                    if (!c->confirmed) {
                        c->tok->type = T_IDENT;
                        continue;
                    }
                    return 0;  // out of sync
                }
                newstatement(c);
            } break;

            case KW_template:
                if (!c->tok->prev || !(c->tok->prev->type&(T_DOT|T_ARROW))) {
                    if (!parsetemplateprefix(c)) {
                        return 0;
                    }
                }
                break;

            case KW_typedef:
                c->seen |= SEEN_TYPEDEF;
                chainclear(&c->pool, c->chain);
                break;

            case KW_enum:
                // Departure P5: not in a template argument list
                if (!c->tok->prev || !(c->tok->prev->type&(T_LT|T_COMMA))) {
                    if (!parseenum(c)) {
                        return 0;
                    }
                }
                break;

            case KW_class:
            case KW_struct:
            case KW_union:
                if (!c->tok->prev || !(c->tok->prev->type&(T_LT|T_COMMA))) {
                    i32 kw = c->tok->kw;
                    b32 ok = 0;
                    if (kw == KW_class) {
                        ok = parseclass(c, kw, K_CLASS, S_CLASS);
                    } else if (kw == KW_struct) {
                        ok = parseclass(c, kw, K_STRUCT, S_STRUCT);
                    } else {
                        ok = parseclass(c, kw, K_UNION, S_UNION);
                    }
                    if (!ok) {
                        return 0;
                    }
                }
                break;

            case KW_public:
            case KW_protected:
            case KW_private:
                if (!accessspecifier(c)) {
                    return 0;
                }
                break;

            case KW_using:
                if (!parseusing(c)) {
                    return 0;
                }
                newstatement(c);
                break;

            case KW_if:
            case KW_for:
            case KW_while:
            case KW_switch:
            case KW_catch:
                if (!ifforwhile(c)) {
                    return 0;
                }
                newstatement(c);
                c->cpp.resolve = 1;  // just before the block
                break;

            case KW_try:
                // Out of a function, or after tokens, a function-try-block
                if (scopetype(c)!=S_FUNCTION || c->tok->prev) {
                    continue;
                }
                newstatement(c);
                c->cpp.resolve = 1;
                break;

            case KW_else:
            case KW_do:
                newstatement(c);
                c->cpp.resolve = 1;
                break;

            case KW_return:
                if (iscxx(c)) {
                    // Maybe a lambda follows
                    newstatement(c);
                    c->seen |= SEEN_RETURN;
                } else {
                    if (!upto(c, T_SEMICOLON|T_EOF, 0)) {
                        return 0;
                    }
                    newstatement(c);
                }
                break;

            case KW_continue:
            case KW_break:
            case KW_goto:
                if (!upto(c, T_SEMICOLON|T_EOF, 0)) {
                    return 0;
                }
                newstatement(c);
                break;

            case KW_throw:
                // Skip throw expressions, but not exception specifications
                if (scopetype(c)==S_FUNCTION || c->chain->count<3) {
                    if (!upto(c, T_SEMICOLON|T_EOF, 0)) {
                        return 0;
                    }
                    newstatement(c);
                }
                break;

            case KW_case:
                if (!upto(c, T_SEMICOLON|T_EOF|T_COLON, 0)) {
                    return 0;
                }
                newstatement(c);
                break;

            case KW_extern:
                c->seen |= SEEN_EXTERN;
                destroylast(&c->pool, c->chain);
                if (!cxxnext(c)) {
                    goto eof;
                }
                if (c->tok->type == T_STRING) {
                    destroylast(&c->pool, c->chain);  // extern "C"
                } else {
                    goto again;
                }
                break;

            case KW_static:
                c->seen |= SEEN_STATIC;
                destroylast(&c->pool, c->chain);
                break;

            case KW_inline:
            case KW__INLINE:
            case KW__INLINE__:
            case KW__FORCEINLINE:
            case KW__FORCEINLINE__:
                c->seen |= SEEN_INLINE;
                destroylast(&c->pool, c->chain);
                break;

            case KW_explicit:
                c->seen |= SEEN_EXPLICIT;
                destroylast(&c->pool, c->chain);
                break;

            case KW_operator:
                c->seen |= SEEN_OPERATOR;
                break;

            case KW_virtual:
                c->seen |= SEEN_VIRTUAL;
                destroylast(&c->pool, c->chain);
                break;

            case KW_mutable:
                c->seen |= SEEN_MUTABLE;
                destroylast(&c->pool, c->chain);
                break;

            case KW_friend:      c->seen |= SEEN_FRIEND;      break;
            case KW_volatile:    c->seen |= SEEN_VOLATILE;    break;
            case KW_const:       c->seen |= SEEN_CONST;       break;
            case KW_constexpr:   c->seen |= SEEN_CONSTEXPR;   break;
            case KW_consteval:   c->seen |= SEEN_CONSTEVAL;   break;
            case KW_constinit:   c->seen |= SEEN_CONSTINIT;   break;
            case KW_thread_local:
            case KW__THREAD:
            case KW_THREAD_LOCAL: c->seen |= SEEN_THREADLOCAL; break;
            case KW_export:      c->seen |= SEEN_EXPORT;      break;

            default:
                if (c->seen & SEEN_TYPEDEF) {
                    c->seen &= ~SEEN_TYPEDEF;
                    if (!parsegenerictypedef(c)) {
                        return 0;
                    }
                    newstatement(c);
                }
            }
            break;

        case T_SEMICOLON:
            if (isc(c) && scopeisglobal(c) && !(c->seen&(SEEN_EXTERN|SEEN_TYPEDEF))) {
                // Maybe a K&R function: type f(a, b) int a; int b; {
                switch (knrfunction(c)) {
                case 1:  break;
                case 0:  analyzestatement(c); break;
                default: return 0;
                }
            } else {
                analyzestatement(c);
            }
            newstatement(c);
            break;

        case T_COLON:
            if (c->chain->count==2 && c->chain->head->type==T_IDENT) {
                Tag *tag = cxxtag(c, K_LABEL, c->chain->head);
                if (tag) {
                    tag->filescope = 1;
                    cxxcommit(c, tag);
                }
                // Departure P4: a label, or a macro used as an access
                // specifier, is not part of the next declaration
                chainclear(&c->pool, c->chain);
            }
            break;

        case T_LBRACE:
            if (!openingbracket(c)) {
                return 0;
            }
            break;

        case T_RBRACE:
            if (!closer) {
                return 0;  // unexpected: probably preprocessing confusion
            }
            newstatement(c);
            return 1;

        case T_LPAREN:
        case T_LBRACKET:
            if (!condensesubchain(c, T_LBRACE|T_LPAREN|T_LBRACKET, 1, 0)) {
                return 0;
            }
            if (c->tok->type == T_EOF) {
                return !closer;
            }
            break;

        case T_IDENT:
            if (modulekeyword(c, "module")) {
                if (!parsemodule(c)) {
                    return 0;
                }
            } else if (modulekeyword(c, "import")) {
                if (!parseimport(c)) {
                    return 0;
                }
            } else if (c->seen & SEEN_TYPEDEF) {
                c->seen &= ~SEEN_TYPEDEF;
                if (!parsegenerictypedef(c)) {
                    return 0;
                }
                newstatement(c);
            }
            break;
        }
    }
}

static b32 parseblockfull(Cxx *c, b32 closer, b32 exported)
{
    if (++c->nesting > NESTMAX) {
        return 0;  // and, as in Universal Ctags, stays counted
    }
    c->cpp.depth++;
    b32 ok = parseblockinternal(c, closer, exported);
    c->cpp.depth--;
    c->nesting--;
    return ok;
}

static b32 parseblock(Cxx *c, b32 closer)
{
    return parseblockfull(c, closer, 0);
}


// Driver

static b32 cxxtags(File *f, b32 brace, Arena scratch)
{
    Arena *a = &scratch;
    Cxx   *c = alloc<Cxx>(a);
    c->f      = f;
    c->pool.a = a;
    c->lang   = f->src->lang==LANG_C ? KL_C : KL_CXX;
    c->chain  = newchain(&c->pool);
    c->scope  = newchain(&c->pool);
    c->ch     = ' ';
    cppinit(&c->cpp, f, a, brace);

    // In headers, which may be C, public, protected, and private are
    // identifiers until the code shows itself to be C++
    c->confirmed = iscxx(c) && !f->src->header;
    c->pppon     = c->confirmed;
    c->finalon   = 0;

    newstatement(c);
    return parseblock(c, 0);
}

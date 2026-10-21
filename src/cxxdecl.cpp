// C and C++ declarations: enum, struct, class, union, typedef, namespace,
// using, templates, lambdas, and modules
// This is free and unencumbered software released into the public domain.

// The keyword is a C++ keyword and not a C keyword.
static b32 cxxonly(Token *t)
{
    return t->type==T_KEYWORD && keywords[t->kw].langs==KL_CXX;
}

// Before a declaration's keyword, keep only it, const, and volatile.
static void cleanupprefix(Cxx *c, i32 kw, Token *last)
{
    Pool  *p = &c->pool;
    Token *t = c->chain->head;
    while (t && t!=last) {
        if (t->type==T_KEYWORD && (t->kw==kw || t->kw==KW_const || t->kw==KW_volatile)) {
            t = t->next;
        } else if (t->prev) {
            Token *prev = t->prev;
            chaintake(c->chain, t);
            freetoken(p, t);
            t = prev->next;
        } else {
            destroyfirst(p, c->chain);
            t = c->chain->head;
        }
    }
}

// Push the qualifiers of a name, A::B::name, as class scopes, taking
// them from the chain. Returns the number pushed.
static i32 pushqualifiers(Cxx *c, Token *name, b32 push)
{
    Token *beg  = name;
    Token *prev = name->prev;
    while (prev) {
        if (prev->type != T_COLONS) {
            break;
        }
        prev = prev->prev;
        if (!prev || prev->type!=T_IDENT) {
            break;
        }
        beg  = prev;
        prev = prev->prev;
    }

    i32 pushed = 0;
    while (beg != name) {
        Token *next = beg->next;
        chaintake(c->chain, beg);
        if (push) {
            scopepush(c, beg, S_CLASS, A_UNKNOWN);
            pushed++;
        } else {
            freetoken(&c->pool, beg);
        }
        beg = next->next;
    }
    return pushed;
}

// After the closing brace of an enum, struct, class, or union: what
// follows declares variables or typedefs of it, or is a function.
static b32 declarationtrailer(Cxx *c, u32 seen, i32 kw, Str name)
{
    Pool *p = &c->pool;
    chainclear(p, c->chain);

    i32 line    = cppline(&c->cpp);
    iz  linepos = cpppos(&c->cpp);
    u32 types   = T_EOF|T_SEMICOLON|T_ASSIGN;
    if (!c->confirmed) {
        types |= T_LBRACE;  // perhaps a C function returning it
    }
    if (!upto(c, types, 0)) {
        return 0;
    }
    if (c->tok->type==T_EOF || c->chain->count<2) {
        return 1;
    }

    // Prefix the type: [volatile] [const] keyword name
    Token *id = newtoken(p);
    id->line    = line;
    id->linepos = linepos;
    id->type    = T_IDENT;
    id->space   = 1;
    wordcat(p, &id->word, name);
    chainprepend(c->chain, id);
    chainprepend(c->chain, keywordtoken(p, line, linepos, kw));
    if (seen & SEEN_CONST) {
        chainprepend(c->chain, keywordtoken(p, line, linepos, KW_const));
    }
    if (seen & SEEN_VOLATILE) {
        chainprepend(c->chain, keywordtoken(p, line, linepos, KW_volatile));
    }

    if (c->tok->type == T_LBRACE) {
        // static struct S {...} f(...) {...}
        c->seen |= seen & (SEEN_STATIC|SEEN_INLINE|SEEN_EXTERN|SEEN_DEPRECATED);
        return openingbracket(c);
    }

    if (c->tok->type==T_ASSIGN && !upto(c, T_EOF|T_SEMICOLON, 0)) {
        return 0;
    }

    if (seen & SEEN_TYPEDEF) {
        extracttypedef(c, c->chain, 1, 0);
    } else {
        c->seen |= seen & (SEEN_STATIC|SEEN_EXTERN|SEEN_MUTABLE|SEEN_INLINE|
                           SEEN_DEPRECATED|SEEN_CONSTEXPR|SEEN_CONSTINIT|
                           SEEN_THREADLOCAL);
        if (firstkeyword(c->chain, KW_static)) {
            c->seen |= SEEN_STATIC;  // departure P9: } static x;
        }
        extractvars(c, c->chain, 0);
    }
    return 1;
}

// Is it worth trying a K&R function at this ';' after a declaration's
// keyword? Its parameter declarations may begin with struct or enum.
static b32 maybeknr(Cxx *c, u32 seen, iz count)
{
    return c->tok->type==T_SEMICOLON && count>3 && isc(c) &&
           scopeisglobal(c) && !(seen&SEEN_TYPEDEF);
}


// enum

static b32 parseenum(Cxx *c)
{
    Pool  *p     = &c->pool;
    u32    seen  = c->seen;
    iz     count = c->chain->count;
    Token *last  = c->chain->tail;

    // Departure P5: also stop at '=', a variable: enum T x[] = {...};
    u32 terms = T_EOF|T_SEMICOLON|T_COLON|T_PARENS|T_LBRACE|T_ASSIGN;
    if (!upto(c, terms|T_KEYWORD, 0)) {
        return 0;
    }

    b32 scoped = 0;
    if (c->tok->type == T_KEYWORD) {
        scoped = c->tok->kw==KW_struct || c->tok->kw==KW_class;
        if (!upto(c, terms, 0)) {
            return 0;
        }
    }

    switch (c->tok->type) {
    case T_EOF:
        newstatement(c);  // a forward declaration
        return 1;
    case T_PARENS:
        if (seen & SEEN_TYPEDEF) {
            c->seen &= ~SEEN_TYPEDEF;
            b32 ok = parsegenerictypedef(c);
            newstatement(c);
            return ok;
        }
        return 1;  // enum x f(...), so keep the statement
    }

    if (maybeknr(c, seen, count)) {
        switch (knrfunction(c)) {
        case 1:  return 1;
        case 0:  break;
        default: return 0;
        }
    }

    if (count > 1) {
        cleanupprefix(c, KW_enum, last);
    }

    if (c->tok->type == T_ASSIGN) {
        // enum X Y = ...;
        b32 vars = c->chain->count > 3;
        if (!upto(c, T_EOF|T_SEMICOLON, 0)) {
            return 0;
        }
        if (vars) {
            extractvars(c, c->chain, 0);
        }
        newstatement(c);
        return 1;
    }

    if (c->tok->type == T_SEMICOLON) {
        // [typedef] enum X Y;
        if (!scoped && c->chain->count>3) {
            if (c->seen & SEEN_TYPEDEF) {
                extracttypedef(c, c->chain, 1, 0);
            } else {
                extractvars(c, c->chain, 0);
            }
        }
        newstatement(c);
        return 1;
    }

    Token *name     = lastof(c->chain, T_IDENT);
    Token *typestart = 0;
    Token *typeend   = 0;
    if (c->tok->type == T_COLON) {
        // The underlying type
        typestart = c->tok;
        if (!upto(c, T_EOF|T_SEMICOLON|T_LBRACE, 0)) {
            return 0;
        }
        if (c->tok->type == T_EOF) {
            newstatement(c);
            return 1;
        }
        if (c->tok->type == T_SEMICOLON) {
            // A member with a bit width: struct { enum E m : 2; }
            if (scopevarkind(c) == K_MEMBER) {
                extractvars(c, c->chain, 0);
            }
            newstatement(c);
            return 1;
        }
        if (c->tok->prev != typestart) {
            typestart = typestart->next;
            typeend   = c->tok->prev;
        }
    }

    i32 pushed = 0;
    if (name) {
        if (iscxx(c)) {
            pushed += pushqualifiers(c, name, 1);
        }
        chaintake(c->chain, name);
    } else {
        name = anontoken(c, K_ENUM, {});
    }

    b32  exported = 0;
    Tag *tag      = cxxtag(c, K_ENUM, name);
    if (tag) {
        tag->filescope = !isheader(c);
        Token *type = typeend ? settype(c, tag, typestart, typeend) : 0;
        exported = !!(c->seen & SEEN_EXPORT);
        if (exported || scopeisexported(c)) {
            tag->filescope = 0;
        }
        cxxcommit(c, tag);
        freetoken(p, type);
    }

    scopepushexported(c, name, S_ENUM, A_PUBLIC, exported);
    pushed++;
    Str fullname = clone(p->a, scopefullname(c));

    for (;;) {
        chainclear(p, c->chain);
        if (!upto(c, T_COMMA|T_RBRACE|T_EOF, 0)) {
            return 0;
        }
        Token *first = c->chain->head;
        if (c->chain->count>1 && first->type==T_IDENT) {
            tag = cxxtag(c, K_ENUMERATOR, first);
            if (tag) {
                tag->filescope = !isheader(c) && !scopeisexported(c);
                cxxcommit(c, tag);
            }
        }
        if (c->tok->type & (T_EOF|T_RBRACE)) {
            break;
        }
    }

    for (; pushed > 0; pushed--) {
        scopepop(c);
    }
    b32 ok = declarationtrailer(c, seen, KW_enum, fullname);
    newstatement(c);
    return ok;
}


// struct, class, union

// An inherits field's value from the base clause in the statement: its
// tokens without access keywords or virtual, joined without spaces,
// though a group keeps those inside it (FLD-9). Null if none are left.
// Valid until the next signature.
static Str baseclasses(Cxx *c)
{
    Word *w = &c->sig;
    iz    n = 0;
    w->len = 0;
    for (Token *t = c->chain->head; t; t = t->next) {
        if (t->type == T_KEYWORD) {
            switch (t->kw) {
            case KW_public: case KW_protected: case KW_private: case KW_virtual:
                continue;
            }
        }
        tokenappend(&c->pool, w, t);
        n++;
    }
    return n ? Str{w->data, w->len} : Str{};
}

static b32 parseclassinternal(Cxx *c, i32 kw, i32 kind, u8 type)
{
    Pool  *p     = &c->pool;
    u32    seen  = c->seen;
    iz     count = c->chain->count;
    Token *last  = c->chain->tail;

    // The name, perhaps after attributes, before a base clause
    b32 macro      = 0;
    b32 conversion = last->prev && iskeyword(last->prev, KW_operator);
    c->finalon = 1;
    u32 terms = T_EOF|T_COLON|T_SEMICOLON|T_LBRACE|T_LT|T_PARENS;
    if (iscxx(c)) {
        terms |= T_KEYWORD;
    }
    if (kind != K_CLASS) {
        terms |= T_ASSIGN;
    }
    for (;;) {
        if (!upto(c, terms, 0)) {
            c->finalon = 0;
            return 0;
        }

        if (c->tok->type == T_KEYWORD) {
            if (c->tok->kw == KW_operator) {
                return 1;  // declaring an operator (leaves final enabled)
            }
            continue;
        }

        if (c->tok->type == T_PARENS) {
            Token *prev = c->tok->prev;
            Token *inner = chainat(c->tok->chain, 1);
            if (!macro && !conversion && prev && prev->type==T_IDENT &&
                    prev->prev==last && !(inner->type&(T_STAR|T_AND|T_ANDS))) {
                // Departure P7: struct MACRO(x) Name {, as struct Name {,
                // but not struct S (*f)(...) or struct S (f)(...)
                cxxnext(c);
                b32 named = !!(c->tok->type & (T_IDENT|T_LBRACE));
                cxxunget(c);
                if (named) {
                    macro = 1;
                    destroylast(p, c->chain);
                    destroylast(p, c->chain);
                    c->tok = c->chain->tail;
                    continue;
                }
            }
            if ((prev && iskeyword(prev, KW_alignas)) ||
                    !looksparams(c, c->tok->chain, 0)) {
                continue;  // struct alignas(n) S, or a macro call
            }
        }

        if (c->tok->type != T_LT) {
            break;
        }

        // template<typename T> struct X<int> {...}
        if (!iscxx(c)) {
            c->finalon = 0;
            return 0;
        }
        freechain(p, c->spec);
        c->spec = templatetochain(c, 0);
        if (!c->spec) {
            c->finalon = 0;
            return 0;
        }
    }
    c->finalon = 0;

    if (c->tok->type == T_PARENS) {
        if (seen & SEEN_TYPEDEF) {
            c->seen &= ~SEEN_TYPEDEF;
            b32 ok = parsegenerictypedef(c);
            newstatement(c);
            return ok;
        }
        return 1;  // struct x *f(...), so keep the statement
    }

    if (maybeknr(c, seen, count)) {
        switch (knrfunction(c)) {
        case 1:  return 1;
        case 0:  break;
        default: return 0;
        }
    }

    if (count > 1) {
        cleanupprefix(c, kw, last);
    }

    if (c->tok->type == T_SEMICOLON) {
        // [typedef] struct X Y; but (departure P6) not a templated
        // forward declaration: template<class T> class EXPORT X;
        if (c->chain->count>3 && !c->tmpl) {
            if (seen & SEEN_TYPEDEF) {
                extracttypedef(c, c->chain, 1, 0);
            } else if (!(c->seen & SEEN_FRIEND)) {
                extractvars(c, c->chain, 0);
            }
        }
        newstatement(c);
        return 1;
    }

    if (c->tok->type == T_ASSIGN) {
        // struct X Y = ...;
        b32 vars = c->chain->count > 3;
        if (!upto(c, T_EOF|T_SEMICOLON, 0)) {
            return 0;
        }
        if (vars) {
            extractvars(c, c->chain, 0);
        }
        newstatement(c);
        return 1;
    }

    if (c->tok->type == T_EOF) {
        newstatement(c);
        return 1;
    }

    Token *name = lastof(c->chain, T_IDENT);
    if (!name && iscxx(c) && !c->confirmed && kw!=KW_class &&
            c->chain->count>=3 && cxxonly(c->tok->prev)) {
        // In a header that may be C: struct new {...}
        name = c->tok->prev;
        name->type = T_IDENT;
    }

    i32 pushed = 0;
    if (name) {
        pushed += pushqualifiers(c, name, iscxx(c));
        chaintake(c->chain, name);
    } else {
        name = anontoken(c, kind, {});
    }

    if (c->tok->type == T_COLON) {
        // Base classes
        chainclear(p, c->chain);
        if (!upto(c, T_EOF|T_SEMICOLON|T_LBRACE, 0)) {
            freetoken(p, name);
            return 0;
        }
        if (c->tok->type & (T_SEMICOLON|T_EOF)) {
            // A syntax error. As in Universal Ctags, the qualifiers'
            // scopes stay pushed.
            freetoken(p, name);
            newstatement(c);
            return 1;
        }
        destroylast(p, c->chain);
    } else {
        chainclear(p, c->chain);
    }

    if (kind == K_CLASS) {
        c->confirmed = 1;
    }

    Tag *tag      = cxxtag(c, kind, name);
    b32  tmpl     = c->tmpl && c->tmpl->count>0 && iscxx(c);
    b32  exported = !!(seen & SEEN_EXPORT);
    if (tag) {
        if (c->chain->count && wantfield(c->f, FIELD_INHERITS)) {
            tag->inherits = baseclasses(c);
        }
        tag->filescope = !isheader(c) && !exported && !scopeisexported(c);
        cxxcommit(c, tag);
    }

    u8 access = kind==K_CLASS ? A_PRIVATE : A_PUBLIC;
    scopepushexported(c, name, type, access, exported);
    if (tmpl && cxxkind(c, K_TPARAM)) {
        emittemplateparams(c);
    }
    Str fullname = clone(p->a, scopefullname(c));

    if (!parseblock(c, 1)) {
        return 0;
    }

    for (pushed++; pushed > 0; pushed--) {
        scopepop(c);
    }
    b32 ok = declarationtrailer(c, seen, kw, fullname);
    newstatement(c);
    return ok;
}

static b32 parseclass(Cxx *c, i32 kw, i32 kind, u8 type)
{
    // Within one, public, protected, and private are surely keywords
    b32 ppp  = c->pppon;
    c->pppon = 1;
    b32 ok = parseclassinternal(c, kw, kind, type);
    c->pppon = ppp || c->confirmed;
    return ok;
}


// typedef

// After typedef and a keyword or identifier: read the statement, then
// extract its definitions.
static b32 parsegenerictypedef(Cxx *c)
{
    for (;;) {
        if (!upto(c, T_SEMICOLON|T_EOF|T_RBRACE|T_KEYWORD, 0)) {
            return 0;
        }
        if (c->tok->type != T_KEYWORD) {
            if (c->tok->type != T_SEMICOLON) {
                return 1;  // EOF or '}'
            }
            break;
        }
        i32 kw = c->tok->kw;
        if (kw==KW_extern || kw==KW_typedef || kw==KW_static) {
            return 1;  // as if a ';', but too confusing to tag
        }
    }
    extracttypedef(c, c->chain, 1, 0);
    return 1;
}

// Tag the names a typedef statement defines, after the typedef keyword
// and before an optional terminator.
static void extracttypedef(Cxx *c, Chain *ch, b32 terminator, b32 tmpl)
{
    Pool *p = &c->pool;
    if (ch->count < (terminator ? 3 : 2)) {
        return;
    }

    b32 exported = !!(c->seen & SEEN_EXPORT);
    if (terminator) {
        destroylast(p, ch);
    }

    while (ch->count >= 2) {
        // To the comma ending this definition, skipping templates
        Token *t     = ch->head;
        Token *paren = 0;
        u32    types = T_COMMA|T_LT|T_PARENS;
        Token *comma = 0;
        for (;;) {
            comma = nextof(t, types);
            if (!comma) {
                break;
            } else if (comma->type == T_LT) {
                t = skipangles(comma);
                if (!t) {
                    return;
                }
            } else if (comma->type == T_PARENS) {
                types &= ~T_PARENS;
                paren  = comma;
            } else {
                break;
            }
        }

        if (comma) {
            if (!comma->prev || !comma->prev->prev) {
                return;
            }
            t = comma->prev;
        } else {
            t = ch->tail;
        }

        Chain *parent = 0;
        Token *lookup = t;
        if (t->type == T_IDENT) {
            parent = ch;
        } else if (paren) {
            if (paren->next && paren->next->type&(T_PARENS|T_BRACKETS)) {
                // A function pointer: typedef int (*name)(...)
                t = lastnestedof(paren->chain, T_IDENT, &parent);
            } else if (paren->prev && paren->prev->type==T_IDENT) {
                // A function: typedef int name(...). Turn the name into
                // a chain (name) to take it from.
                t = paren->prev;
                parent = newchain(p);

                Token *x = newtoken(p);
                x->type    = T_LPAREN;
                x->line    = t->line;
                x->linepos = t->linepos;
                wordput(p, &x->word, '(');
                chainappend(parent, x);

                Token *id = newtoken(p);
                id->type    = T_IDENT;
                id->line    = t->line;
                id->linepos = t->linepos;
                wordcat(p, &id->word, wordstr(t));
                chainappend(parent, id);

                t->type     = T_PARENS;
                t->chain    = parent;
                t->word.len = 0;
                t = id;

                x = newtoken(p);
                x->type    = T_RPAREN;
                x->line    = t->line;
                x->linepos = t->linepos;
                wordput(p, &x->word, ')');
                chainappend(parent, x);
            } else {
                return;
            }
        } else {
            parent = ch;
            t = prevof(t, T_IDENT);
        }

        if (!t) {
            if (iscxx(c) && !c->confirmed && cxxonly(lookup)) {
                lookup->type = T_IDENT;
                t = lookup;
            } else {
                return;
            }
        }

        Tag *tag = cxxtag(c, K_TYPEDEF, t);
        if (tag) {
            chaintake(parent, t);

            // No type for functions or templates
            Token *type   = 0;
            u32    notype = T_PARENS|T_ANGLES;
            Token *x = comma ? prevof(comma, notype) : lastof(ch, notype);
            if (parent!=ch || !x) {
                type = settype(c, tag, ch->head, comma ? comma->prev : ch->tail);
            }
            tag->filescope = !isheader(c) && !exported && !scopeisexported(c);
            cxxcommit(c, tag);

            if (tmpl && cxxkind(c, K_TPARAM)) {
                scopepush(c, t, S_TYPEDEF, A_UNKNOWN);
                emittemplateparams(c);
                scopepop(c);
            } else {
                freetoken(p, t);
            }
            freetoken(p, type);
        }

        if (!comma) {
            break;
        }

        // Keep the type for the next name: typedef int a, *b;
        while (comma->prev && !(comma->prev->type&(T_IDENT|T_KEYWORD|T_GT|T_ANGLES))) {
            Token *x = comma->prev;
            chaintake(ch, x);
            freetoken(p, x);
        }
        chaintake(ch, comma);
        freetoken(p, comma);
    }
}


// namespace

static b32 parsenamespace(Cxx *c)
{
    Pool *p        = &c->pool;
    b32   exported = !!(c->seen & SEEN_EXPORT);

    newstatement(c);
    c->cpp.resolve = 1;  // but within it

    i32 scopes = 0;
    if (!cxxnext(c)) {
        return 1;
    }

    if (c->tok->type == T_IDENT) {
        Token *first = c->tok;
        Token *last  = c->tok;
        if (!cxxnext(c)) {
            return 1;
        }

        switch (c->tok->type) {
        case T_ASSIGN: {
            // namespace alias = qualified::name;
            if (!cxxnext(c)) {
                return 1;
            }
            if (!(c->tok->type & (T_IDENT|T_COLONS))) {
                return skiptosemicolon(c);
            }
            Token *alias = first;
            first = c->tok;
            if (!qualifiedname(c)) {
                return skiptosemicolon(c);
            }
            last = c->tok->prev;
            Tag *tag = cxxtag(c, K_ALIAS, alias);
            if (tag) {
                tag->filescope = !isheader(c) && !exported && !scopeisexported(c);
                Token *aliased = extractrange(p, first, last, 1);
                tag->aliased = wordstr(aliased);
                cxxcommit(c, tag);
                freetoken(p, aliased);
            }
            return skiptosemicolon(c);
        }

        case T_COLONS:
            // namespace a::b {
            if (!qualifiedname(c)) {
                return skiptosemicolon(c);
            }
            last = c->tok->prev;
            if (c->tok->type != T_LBRACE) {
                if (!upto(c, T_LBRACE|T_SEMICOLON|T_EOF, 0)) {
                    return 0;
                }
                if (c->tok->type != T_LBRACE) {
                    return 1;
                }
            }
            break;

        case T_LBRACE:
            break;

        case T_SEMICOLON:
            return 1;

        case T_IDENT:
            // namespace name MACRO {
            if (!upto(c, T_LBRACE|T_SEMICOLON|T_EOF, 0)) {
                return 0;
            }
            if (c->tok->type != T_LBRACE) {
                return 1;
            }
            break;

        default:
            return skiptosemicolon(c);
        }

        for (Token *t = first; t;) {
            Tag *tag = cxxtag(c, K_NAMESPACE, t);
            if (tag) {
                tag->filescope = !isheader(c) && !exported && !scopeisexported(c);
                cxxcommit(c, tag);
            }
            Token *next = t==last ? 0 : t->next->next;
            chaintake(c->chain, t);
            scopepushexported(c, t, S_NAMESPACE, A_UNKNOWN, exported);
            scopes++;
            t = next;
        }

    } else if (c->tok->type == T_LBRACE) {
        Token *t   = anontoken(c, K_NAMESPACE, {});
        t->scopeanon = 1;
        Tag   *tag = cxxtag(c, K_NAMESPACE, t);
        if (tag) {
            tag->filescope = !isheader(c);
            cxxcommit(c, tag);
        }
        scopepush(c, t, S_NAMESPACE, A_UNKNOWN);
        scopes++;

    } else {
        return skiptosemicolon(c);
    }

    c->confirmed = 1;
    if (!parseblock(c, 1)) {
        return 0;
    }
    for (; scopes > 0; scopes--) {
        scopepop(c);
    }
    return 1;
}


// using

static b32 parseusing(Cxx *c)
{
    Pool *p    = &c->pool;
    u32   seen = c->seen;

    chainclear(p, c->chain);
    if (!upto(c, T_SEMICOLON|T_RBRACE|T_EOF, 0)) {
        return 0;
    }
    if (c->tok->type != T_SEMICOLON) {
        return 1;  // a tolerated syntax error
    }
    destroylast(p, c->chain);
    if (c->chain->count < 1) {
        return 1;
    }

    Token *assign = firstof(c->chain, T_ASSIGN);
    if (assign) {
        // using name = type; is typedef type name;
        Token *first = c->chain->head;
        b32    tmpl  = c->tmpl && c->tmpl->count>0 && iscxx(c);
        if (first->type == T_IDENT) {
            chaintake(c->chain, first);
            while (c->chain->head != assign) {
                destroyfirst(p, c->chain);
            }
            destroyfirst(p, c->chain);
            chainappend(c->chain, first);
            extracttypedef(c, c->chain, 0, tmpl);
        }

    } else {
        Token *t  = c->chain->head;
        b32    ns = 0;
        if (iskeyword(t, KW_namespace)) {
            ns = 1;
            destroyfirst(p, c->chain);
        } else if (iskeyword(t, KW_typename)) {
            destroyfirst(p, c->chain);
        }

        if (c->chain->count > 0) {
            Tag *tag = 0;
            if (ns) {
                // using namespace a::b;
                condense(p, c->chain, 0);
                tag = cxxtag(c, K_USING, c->chain->head);
            } else {
                // using a::b; and using a::operator+;
                t = c->chain->tail;
                if (t->type == T_IDENT) {
                    tag = cxxtag(c, K_NAME, t);
                } else {
                    Token *op = firstkeyword(c->chain, KW_operator);
                    if (op) {
                        Token *name = copytoken(p, op);
                        name->word.len = 0;
                        joinrange(p, op, t, &name->word, " ", 1);
                        tag = cxxtag(c, K_NAME, name);
                    }
                }
            }

            if (tag) {
                tag->filescope = scopetype(c)==S_NAMESPACE && !isheader(c);
                if (seen&SEEN_EXPORT || scopeisexported(c)) {
                    tag->filescope = 0;
                }
                cxxcommit(c, tag);
            }
        }
    }

    c->confirmed = 1;
    return 1;
}


// Templates

enum : i32 { TA_OK, TA_EARLY, TA_FAILED, TA_RECOVERABLE };

// Is the identifier a template parameter that is not a type?
static b32 nontypeparam(Cxx *c, Token *t)
{
    if (!t->prev) {
        return 0;
    }
    if (t->prev->type==T_KEYWORD && kwflag(t->prev->kw, KF_TYPEMARK)) {
        return 0;
    }
    for (i32 i = 0; i < c->tparams.count; i++) {
        if (wordstr(t) == wordstr(c->tparams.ids[i])) {
            return 1;
        }
    }
    return 0;
}

// Is the identifier, after typename or class, a template parameter?
static b32 typeparam(Cxx *c, Token *t)
{
    if (!t->prev || t->prev->type!=T_KEYWORD || !kwflag(t->prev->kw, KF_TYPEMARK)) {
        return 0;
    }
    for (i32 i = 0; i < c->tparams.count; i++) {
        if (wordstr(t) == wordstr(c->tparams.ids[i])) {
            return 1;
        }
    }
    return 0;
}

// Record the template parameter from start to just before the current
// token, if it looks like a type and a name.
static void captureparam(Cxx *c, Token *start)
{
    Params *tp = &c->tparams;
    if (tp->count>=PARAMMAX || start->next==c->tok) {
        return;
    }

    Token *t = start;
    for (; !(t->type & (T_COMMA|T_GT|T_ASSIGN)); t = t->next) {
        b32 typepart = t->type==T_KEYWORD && kwflag(t->kw, KF_TYPEPART);
        if (!typepart && !(t->type & (T_IDENT|T_STAR|T_AND|T_ANDS|T_DOTS))) {
            return;
        }
    }
    if (t->prev->type != T_IDENT) {
        return;
    }
    tp->ids[tp->count]       = t->prev;
    tp->typestart[tp->count] = start;
    tp->typeend[tp->count]   = t->prev->prev;
    tp->count++;
}

// Read template angle brackets, the '<' already the chain's last token,
// to the matching '>', telling apart '>' and '>>' as best it can.
static i32 templateangles(Cxx *c, b32 capture, i32 nested)
{
    i32    level  = 1;
    Token *before = c->tok;  // before the current parameter
    for (;;) {
        u32 types = T_GT|T_LT|T_LBRACE|T_SEMICOLON|T_COMMA|T_EOF|T_KEYWORD;
        if (!condenseupto(c, types, T_LPAREN|T_LBRACKET, 0)) {
            return TA_FAILED;
        }

        switch (c->tok->type) {
        case T_COMMA:
            if (capture && !nested && level==1 && before->next!=c->tok) {
                captureparam(c, before->next);
            }
            if (level == 1) {
                before = c->tok;
            }
            break;

        case T_LT: {
            Token *lt = c->tok;
            if (!cxxnext(c)) {
                return TA_RECOVERABLE;
            }
            // Not after a number or non-type parameter: a < b
            if (lt->prev->type!=T_NUMBER &&
                    !(lt->prev->type==T_IDENT && nontypeparam(c, lt->prev))) {
                level++;
            }
            if (c->tok->type & (T_LPAREN|T_LBRACKET)) {
                cxxunget(c);  // to be condensed
            }
        } break;

        case T_GT: {
            b32 firstspace = c->tok->space;
            i32 gts    = 1;
            i32 maxgts = level<3 ? 3 : level;
            while (gts < maxgts) {
                if (!cxxnext(c)) {
                    return TA_RECOVERABLE;
                }
                if (c->tok->type != T_GT) {
                    break;
                }
                gts++;
            }

            // Shifts: a >> 2, a >> (b), a >> param, a >> b
            if (!firstspace && gts==2 &&
                    (c->tok->type&(T_NUMBER|T_LPAREN) ||
                     (c->tok->type==T_IDENT && nontypeparam(c, c->tok)) ||
                     (!nested && level==1 && c->tok->type==T_IDENT &&
                      !typeparam(c, c->tok)))) {
                if (c->tok->type == T_LPAREN) {
                    cxxunget(c);
                }
                continue;
            }

            if (c->tok->type != T_GT) {
                b32 keyword = c->tok->type==T_KEYWORD &&
                              !kwflag(c->tok->kw, KF_CONSTANT);
                if (keyword || (c->tok->type==T_IDENT && typeparam(c, c->tok))) {
                    // Surely the end, as in "> class" or "> T"
                    cxxunget(c);
                    if (capture && !nested && before->next!=c->tok) {
                        captureparam(c, before->next);
                    }
                    return gts>level ? TA_EARLY : TA_OK;
                }
                cxxunget(c);
            }

            for (; gts > level; gts--) {
                cxxunget(c);
            }
            level -= gts;
            if (!level) {
                if (capture && !nested && before->next!=c->tok) {
                    captureparam(c, before->next);
                }
                return TA_OK;
            }
        } break;

        case T_KEYWORD:
            if (iskeyword(c->tok, KW_template)) {
                if (!cxxnext(c)) {
                    return TA_RECOVERABLE;
                }
                if (c->tok->type != T_LT) {
                    // Only after "::" may template not begin a list
                    if (c->tok->prev->prev->type != T_COLONS) {
                        return TA_FAILED;
                    }
                    continue;
                }
                i32 r = templateangles(c, 0, nested+1);
                if (r != TA_OK) {
                    return r;
                }
            }
            break;

        case T_EOF:
            return TA_RECOVERABLE;

        case T_SEMICOLON:
            newstatement(c);
            return TA_RECOVERABLE;

        case T_LBRACE:
            // Perhaps C++11 syntax beyond this: skip the block
            if (!upto(c, T_RBRACE|T_EOF, 0)) {
                return TA_FAILED;
            }
            newstatement(c);
            return TA_RECOVERABLE;

        default:
            return TA_FAILED;
        }
    }
}

// Move the '<' at the chain's end and the rest of its template list
// into a new chain, or null on failure.
static Chain *templatetochain(Cxx *c, b32 capture)
{
    Pool  *p   = &c->pool;
    Chain *out = newchain(p);
    chainappend(out, chaintakelast(c->chain));

    Chain *save = c->chain;
    c->chain = out;
    i32 r = templateangles(c, capture, 0);
    c->chain = save;

    if (r == TA_FAILED) {
        freechain(p, out);
        return 0;
    }
    return out;
}

static b32 parsetemplateprefix(Cxx *c)
{
    Pool *p     = &c->pool;
    b32   first = !c->tok->prev;
    destroylast(p, c->chain);  // the template keyword

    if (first) {
        // Departure P6: an explicit instantiation, perhaps after extern,
        // declares nothing: template class X<int>;
        if (!cxxnext(c)) {
            return 1;
        }
        if (c->tok->type == T_KEYWORD &&
                (c->tok->kw==KW_class || c->tok->kw==KW_struct || c->tok->kw==KW_union)) {
            if (!skiptosemicolon(c)) {
                return 0;
            }
            newstatement(c);
            return 1;
        }
        cxxunget(c);
    }

    if (!upto(c, T_LT|T_EOF|T_SEMICOLON, 0)) {
        return 0;
    }
    if (c->tok->type & (T_EOF|T_SEMICOLON)) {
        newstatement(c);
        return 1;
    }

    c->tparams.count = 0;
    Chain *out = templatetochain(c, 1);
    if (!out) {
        return 0;
    }
    freechain(p, c->tmpl);
    c->tmpl = out;
    c->tparams.chain = out;
    freechain(p, c->spec);
    c->spec = 0;
    return 1;
}

static void emittemplateparams(Cxx *c)
{
    for (i32 i = 0; i < c->tparams.count; i++) {
        Tag *tag = cxxtag(c, K_TPARAM, c->tparams.ids[i]);
        if (tag) {
            Token *type = settype(c, tag, c->tparams.typestart[i], c->tparams.typeend[i]);
            cxxcommit(c, tag);
            freetoken(&c->pool, type);
        }
    }
}


// Lambdas

// At '{', the lambda's parameters (or captures without parameters), or
// null if it is not a lambda.
static Token *islambda(Cxx *c)
{
    Token *t = c->tok->prev;
    if (!t) {
        return 0;
    }

    if (t->type == T_BRACKETS) {
        // [...] {, but not type var[] {
        if (t->prev && t->prev->type==T_IDENT) {
            return 0;
        }
        return t;
    }

    if (t->type == T_PARENS) {
        // [...] (...) {
        t = t->prev;
        if (!t || t->type!=T_BRACKETS) {
            return 0;
        }
        if (t->prev && t->prev->type==T_KEYWORD) {
            return 0;
        }
        return t->next;
    }

    // [...] (...) mutable -> type {
    t = prevof(t, T_BRACKETS|T_BRACES|T_ASSIGN|T_OPERATOR);
    if (!t || t->type!=T_BRACKETS) {
        return 0;
    }
    if (t->prev && t->prev->type==T_KEYWORD) {
        return 0;
    }
    t = t->next;
    return t->type==T_PARENS ? t : 0;
}

// Tag a lambda as an anonymous function and parse its body, leaving
// an empty chain token in place of its '{'.
static b32 handlelambda(Cxx *c, Token *paren)
{
    Pool  *p    = &c->pool;
    Token *id   = anontoken(c, K_FUNCTION, {});
    Chain *save = c->chain;
    c->chain = newchain(p);

    Tag   *tag   = cxxtag(c, K_FUNCTION, id);
    Token *after = paren ? paren->next : 0;
    if (after && iskeyword(after, KW_const)) {
        after = after->next;
    }

    Token *typestart = 0;
    Token *typeend   = 0;
    if (after && after->type==T_ARROW && after->next && after->next->type!=T_LBRACE) {
        typestart = typeend = after->next;
        while (typeend->next && typeend->next->type!=T_LBRACE) {
            typeend = typeend->next;
        }
    }

    if (tag) {
        tag->filescope = 1;
        Token *type = typestart ? settype(c, tag, typestart, typeend) : 0;
        if (paren && paren->type==T_PARENS && wantfield(c->f, FIELD_SIGNATURE)) {
            // As written, unlike a function's (FLD-8)
            tag->signature = joinsig(c, paren->chain, 0, 0);
        }
        cxxcommit(c, tag);
        freetoken(p, type);
    }

    scopepush(c, id, S_FUNCTION, A_UNKNOWN);
    if (paren && paren->type==T_PARENS && cxxkind(c, K_PARAMETER)) {
        Params params = {};
        if (looksparams(c, paren->chain, &params)) {
            emitparams(c, &params);
        }
    }

    b32 ok = parseblock(c, 1);
    scopepop(c);

    Chain *body = c->chain;
    c->chain = save;
    c->tok   = save->tail;
    c->tok->type  = T_ANGLES;  // sic
    c->tok->chain = body;
    chainclear(p, body);

    Token *t = newtoken(p);
    t->type = T_LBRACE;
    wordput(p, &t->word, '{');
    chainappend(body, t);
    t = newtoken(p);
    t->type = T_RBRACE;
    wordput(p, &t->word, '}');
    chainappend(body, t);
    return ok;
}


// C++20 modules

// A token naming a dotted module or partition, from beg to end.
static Token *moduletoken(Cxx *c, Token *beg, Token *end)
{
    Token *r = copytoken(&c->pool, beg);
    if (beg != end) {
        joinrange(&c->pool, beg->next, end, &r->word, "", 1);
    }
    return r;
}

static b32 parsemodule(Cxx *c)
{
    Pool *p = &c->pool;
    newstatement(c);
    if (!upto(c, T_SEMICOLON|T_EOF, 0)) {
        return 0;
    }

    Token *first = c->chain->head;
    if (first->type & (T_SEMICOLON|T_EOF)) {
        // module; begins the global module fragment
        Token *m   = anontoken(c, K_MODULE, "__gmod_");
        Tag   *tag = cxxtag(c, K_MODULE, m);
        if (tag) {
            cxxcommit(c, tag);
        }
        freetoken(p, m);

    } else if (first->type==T_COLON && first->next && iskeyword(first->next, KW_private)) {
        // module :private;
        if (c->module) {
            scopepush(c, copytoken(p, c->module), S_MODULE, A_UNKNOWN);
        }
        Tag *tag = cxxtag(c, K_PARTITION, first->next);
        if (tag) {
            cxxcommit(c, tag);
        }
        if (c->module) {
            scopepop(c);
        }

    } else if (first->type == T_IDENT) {
        // module name; or module name:partition;
        Token *sep  = nextof(first, T_COLON|T_SEMICOLON|T_EOF);
        b32    part = sep->type==T_COLON && sep->next && sep->next->type==T_IDENT &&
                      c->tok->prev && c->tok->prev!=sep;
        Token *m    = moduletoken(c, first, sep->prev);
        if (cxxkind(c, K_MODULE)) {
            if (!part) {
                // With a partition, a reference to the module
                cxxcommit(c, cxxtag(c, K_MODULE, m));
            }
            c->module = copytoken(p, m);
            scopepush(c, m, S_MODULE, A_UNKNOWN);
        } else {
            c->module = m;
            m = 0;
        }

        if (m && part) {
            Token *pt  = moduletoken(c, sep->next, c->tok->prev);
            Tag   *tag = cxxtag(c, K_PARTITION, pt);
            if (tag) {
                cxxcommit(c, tag);
            }
            freetoken(p, pt);
        }
        if (m) {
            scopepop(c);
        }
    }

    chainclear(p, c->chain);
    return 1;
}

// An import makes only reference tags, which are not written.
static b32 parseimport(Cxx *c)
{
    newstatement(c);
    if (!upto(c, T_SEMICOLON|T_EOF, 0)) {
        return 0;
    }
    chainclear(&c->pool, c->chain);
    return 1;
}

// C and C++ functions, parameters, and variables
//
// A statement is analyzed once it ends: does it hold a function
// signature, or variable declarations? Both are heuristic, and Universal
// Ctags' order of tests is the behavior, so it is followed exactly.
//
// This is free and unencumbered software released into the public domain.

// A K&R function definition, the ';' just read: int f(a, b) int a; ...
// Returns 1 if it was one (and its body was parsed), 0 if not, -1 on
// failure.
static i32 knrfunction(Cxx *c)
{
    Pool *p = &c->pool;
    if (c->chain->count < 5) {
        return 0;
    }

    Token *paren = firstof(c->chain, T_PARENS);
    if (!paren) {
        return 0;
    }
    Token *id = paren->prev;
    if (!id || id->type!=T_IDENT) {
        return 0;
    }
    Token *x = paren->next;
    if (!x || !x->next || !x->next->next) {
        return 0;
    }

    // A declaration should follow the parentheses
    u32 decl = T_IDENT|T_KEYWORD|T_BRACKETS|T_STAR|T_COMMA|T_COLON|T_NUMBER;
    x = nextnotof(paren, decl);
    if (!x || x->type!=T_SEMICOLON) {
        return 0;
    }
    x = prevnotof(x, T_BRACKETS|T_COLON|T_NUMBER);
    if (!x || x->type!=T_IDENT) {
        return 0;
    }

    Chain *parent   = c->chain;
    Token *firstarg = paren->next;
    if (paren->chain->count==3 && chainat(paren->chain, 1)->type==T_PARENS &&
            id->prev && id->prev->type==T_IDENT) {
        // type f MACRO((a, b))
        id     = id->prev;
        parent = paren->chain;
        paren  = chainat(paren->chain, 1);
    }

    // Only identifiers, perhaps ending in "..."
    if (paren->chain->count < 3) {
        return 0;
    }
    x = paren->chain->head->next;
    i32 nparams = 0;
    b32 dots    = 0;
    for (;;) {
        if (x->type == T_IDENT) {
            nparams++;
        } else if (x->type == T_DOTS) {
            dots = 1;
        } else {
            return 0;
        }
        x = x->next;
        if (x->type == T_RPAREN) {
            break;
        }
        if (dots || x->type!=T_COMMA) {
            return 0;
        }
        x = x->next;
    }
    if (nparams < 1) {
        return 0;
    }

    chaintake(c->chain, id);
    chaintake(parent, paren);
    while (c->chain->head != firstarg) {
        destroyfirst(p, c->chain);
    }

    // Read the remaining parameter declarations, to the body
    enum { KNRMAX = 10 };
    Token *starts[KNRMAX];
    i32    nstarts = 0;
    for (; nparams > 0; nparams--) {
        Token *tail = c->chain->tail;
        if (!upto(c, T_SEMICOLON|T_LBRACE|T_EOF, 0)) {
            return -1;
        }
        if (c->tok->type == T_EOF) {
            newstatement(c);
            return 1;
        }
        if (nstarts < KNRMAX) {
            starts[nstarts++] = tail->next;
        }
        if (c->tok->type == T_LBRACE) {
            break;
        }
    }
    if (c->tok->type != T_LBRACE) {
        newstatement(c);
        return 1;
    }

    Tag *tag = cxxtag(c, K_FUNCTION, id);
    if (tag) {
        tag->filescope = c->seen&SEEN_STATIC && !isheader(c);
        cxxcommit(c, tag);
    }
    freetoken(p, paren);

    scopepush(c, id, S_FUNCTION, A_UNKNOWN);
    if (cxxkind(c, K_PARAMETER)) {
        for (i32 i = 0;; i++) {
            extractvars(c, c->chain, 1);
            if (i >= nstarts) {
                break;
            }
            while (c->chain->head != starts[i]) {
                destroyfirst(p, c->chain);
            }
        }
    }

    newstatement(c);
    if (!parseblock(c, 1)) {
        return -1;
    }
    scopepop(c);
    return 1;
}

// Does the (...) or {...} chain look like the arguments of a call?
static b32 callargs(Cxx *c, Chain *ch)
{
    Token *t      = ch->head;
    Token *last   = ch->tail;
    u32    closer = t->type << 4;
    b32    parens = closer == T_RPAREN;

    for (t = t->next; t != last;) {
        u32 values = T_NUMBER|T_STRING|T_CHAR|T_ARROW|T_DOT|T_OPERATOR|T_DOTS;
        if (parens && t->type&values) {
            return 1;
        }

        if (t->type == T_KEYWORD) {
            if (kwflag(t->kw, KF_TYPEPART)) {
                return 0;
            }
            if (parens && (kwflag(t->kw, KF_CONSTANT) || t->kw==KW_new)) {
                return 1;
            }
            u32 decl = T_KEYWORD|T_STAR|T_AND|T_ANDS|T_IDENT;
            if (t->kw!=KW_new && t->next->type&decl) {
                return 0;
            }
        } else if (t->type == T_IDENT) {
            if (t->next->type & (T_KEYWORD|T_IDENT)) {
                return 0;  // type name
            }
        } else if (t->type == T_GT) {
            if (t->next->type & (T_AND|T_STAR|T_ANDS|T_COMMA|closer)) {
                return 0;  // template type
            }
            if (t->prev->type & T_KEYWORD) {
                return 0;
            }
        } else if (t->type==T_PARENS &&
                   t->prev->type&(T_IDENT|T_KEYWORD|T_STAR|T_AND|T_GT) &&
                   t->next->type==T_PARENS &&
                   chainat(t->chain, 1)->type==T_STAR &&
                   looksparams(c, t->next->chain, 0)) {
            return 0;  // a function pointer: type (*name)(params)
        }

        if (t->type == T_ASSIGN) {
            // A default value
            t = nextof(t, closer|T_COMMA);
            if (t->type == T_COMMA) {
                t = t->next;
            }
        } else {
            t = t->next;
        }
    }
    return 1;
}

static b32 ctorargs(Cxx *c, Chain *ch)
{
    if (ch->count < 3) {
        return ch->head->type == T_LBRACE;  // type var{} but not type var()
    }
    return callargs(c, ch);
}

// Accept the parentheses and name as a signature if they look like one,
// including the form MACRO name((params)).
static b32 sigparens(Cxx *c, Token *paren, Chain *idchain, Token *idstart,
                     Token *idend, Sig *sig, Params *params)
{
    Token *inner = chainat(paren->chain, 1);
    if (paren->chain->count==3 && inner->type==T_PARENS &&
            idend->type==T_IDENT && idend->prev && idend->prev->type==T_IDENT &&
            looksparams(c, inner->chain, params)) {
        sig->parencontainer = paren->chain;
        sig->idend   = idend->prev;
        sig->idstart = sig->idend;
        sig->idchain = idchain;
        sig->paren   = inner;
        return 1;
    }

    if (looksparams(c, paren->chain, params)) {
        sig->parencontainer = idchain;
        sig->idstart = idstart;
        sig->idend   = idend;
        sig->idchain = idchain;
        sig->paren   = paren;
        return 1;
    }
    return 0;
}

// Within a class, is the name its constructor's?
static b32 isconstructor(Cxx *c, Str name)
{
    if (scopeisglobal(c)) {
        return 0;
    }
    switch (scopetype(c)) {
    case S_CLASS:
    case S_ENUM:
    case S_UNION:
    case S_STRUCT:
        break;
    default:
        return 0;
    }

    // Its last occurrence in the scope's name, at a component's start
    Str scope = wordstr(c->scope->tail);
    for (iz i = scope.len - name.len; i >= 0; i--) {
        if (!__builtin_memcmp(scope.data+i, name.data, (uz)name.len)) {
            return !i || scope[i-1]==':';
        }
    }
    return 0;
}

// Look for a function signature in the statement. Fills in the
// signature, and the parameters if requested.
static b32 lookforsig(Cxx *c, Chain *ch, Sig *sig, Params *params)
{
    if (ch->count < 1) {
        return 0;
    }
    if (params) {
        params->count = 0;
    }

    sig->flags         = 0;
    sig->paren         = 0;
    sig->trailingcomma = 0;
    sig->specstart     = 0;

    Token *idstart = 0;
    Token *idend   = 0;
    Token *top     = 0;  // the top-level parentheses
    b32    angles  = 0;  // skipped template angle brackets

    Token *t = ch->head;
    for (; t; t = t->next) {
        if (t->type & (T_LBRACE|T_SEMICOLON|T_EOF)) {
            break;
        }
        if (t->type == T_COMMA) {
            sig->trailingcomma = t;  // another may follow
            break;
        }
        if (iscxx(c) && t->type&(T_COLON|T_ASSIGN|T_ARROW)) {
            break;  // initializers, = 0, trailing return type
        }

        u32 bad = T_OPERATOR|T_ARROW|T_BRACES|T_STRING|T_CHAR|T_DOTS|
                  T_RBRACE|T_RPAREN|T_RBRACKET;
        if (t->type & bad) {
            return 0;
        }

        if (t->type == T_LT) {
            t = skipangles(t);
            if (!t) {
                return 0;
            }
            angles = 1;
            continue;
        }

        if (sig->paren) {
            continue;  // already found
        }

        if (iskeyword(t, KW_operator)) {
            // operator X (params)
            idstart = t;
            for (t = t->next; t; t = t->next) {
                if (t->type == T_PARENS) {
                    if (t->next && t->next->type==T_PARENS) {
                        t = t->next;  // operator ()(params)
                    }
                    break;
                } else if (t->type == T_KEYWORD) {
                    if (t->kw!=KW_new && t->kw!=KW_delete && t->kw!=KW_volatile &&
                            !kwflag(t->kw, KF_TYPEPART)) {
                        return 0;
                    }
                } else if (t->type == T_LT) {
                    if (t->prev == idstart) {
                        // operator <
                    } else if (t->prev->type == T_IDENT) {
                        t = skipangles(t);  // operator type<...>
                        if (!t) {
                            return 0;
                        }
                    } else {
                        return 0;
                    }
                } else if (t->type == T_STRING) {
                    if (wordstr(t) != "\"\"") {
                        return 0;  // operator "" suffix
                    }
                } else {
                    u32 ok = T_AND|T_ASSIGN|T_COMMA|T_DOT|T_ANGLES|T_GT|
                             T_OPERATOR|T_ARROW|T_COLON|T_BRACKETS|T_COLONS|
                             T_STAR|T_ANDS|T_IDENT;
                    if (!(t->type & ok)) {
                        return 0;
                    }
                }
            }
            if (!t) {
                return 0;
            }
            top   = t;
            idend = t->prev;
            sigparens(c, top, ch, idstart, idend, sig, params);
            continue;
        }

        if (t->type!=T_PARENS || !t->prev) {
            continue;
        }
        top = t;

        if (t->prev->type == T_IDENT) {
            // name (params)
            idstart = idend = t->prev;
            if (sigparens(c, top, ch, idstart, idend, sig, params)) {
                // But MACRO(x) name(...) and MACRO(x) name; are not,
                // nor (departure P3) MACRO(x) *name(...)
                Token *n = sig->paren->next;
                while (n && n->type&(T_STAR|T_AND|T_ANDS)) {
                    n = n->next;
                }
                if (!idstart->prev && n && n->type==T_IDENT && n->next &&
                        n->next->type&(T_PARENS|T_SEMICOLON) &&
                        !firstof(sig->paren->chain, T_COMMA)) {
                    sig->paren = 0;
                }
                continue;
            }
        }

        if (t->prev->type==T_GT && angles) {
            // name<specialization> (params)
            Token *spec = skipanglesback(t->prev);
            if (spec && spec->prev && spec->prev->type==T_IDENT &&
                    (c->tmpl || isconstructor(c, wordstr(spec->prev)))) {
                idstart = idend = spec->prev;
                sig->flags    |= SIG_SPEC;
                sig->specstart = spec;
                sig->specend   = t->prev;
                if (sigparens(c, top, ch, idstart, idend, sig, params)) {
                    continue;
                }
            }
        }

        // type (*name)(params) and such, with the name nested, but
        // (departure P2) not within arguments: v("x", f(y)), v(1, f(y))
        Chain *idchain = 0;
        Token *first   = chainat(t->chain, 1);
        if (firstof(t->chain, T_COMMA) || first->type&(T_STRING|T_CHAR|T_NUMBER)) {
            continue;
        }
        idstart = firstnestedid(c, t->chain, &idchain);
        if (idstart && idstart->next && idstart->next->type==T_PARENS &&
                looksparams(c, idstart->next->chain, 0)) {
            idend = idstart;
            top   = idstart->next;
            sigparens(c, top, idchain, idstart, idend, sig, params);
        }
    }

    if (!sig->paren) {
        return 0;
    }

    // Space the name canonically
    if (sig->idstart != sig->idend) {
        sig->idstart->space = 1;
        for (Token *x = sig->idstart->next; x != sig->idend; x = x->next) {
            u32 word = T_IDENT|T_KEYWORD;
            x->space = x->type&word && x->next->type&word;
        }
    } else {
        sig->idstart->space = 0;
    }
    sig->idend->space = 0;

    // Qualifiers: A::B<T>::name
    sig->scopestart = 0;
    sig->sigconst   = 0;
    if (iscxx(c)) {
        for (Token *x = sig->idstart->prev; x;) {
            if (x->type != T_COLONS) {
                break;
            }
            x = x->prev;
            if (!x) {
                break;
            }
            if (x->type != T_IDENT) {
                if (x->type == T_GT) {
                    Token *lt = skipanglesback(x);
                    if (!lt || !lt->prev || lt->prev->type!=T_IDENT) {
                        break;
                    }
                    x = lt->prev;
                    sig->flags |= SIG_SCOPESPEC;
                } else if (x->type == T_ANGLES) {
                    if (!x->prev || x->prev->type!=T_IDENT) {
                        break;
                    }
                    x = x->prev;
                    sig->flags |= SIG_SCOPESPEC;
                } else {
                    break;
                }
            }
            sig->scopestart = x;
            x = x->prev;
        }

        if (top->next) {
            if (iskeyword(top->next, KW_const)) {
                sig->sigconst = top->next;
            }
            for (Token *x = nextof(top, T_IDENT|T_KEYWORD); x; x = nextof(x, T_IDENT|T_KEYWORD)) {
                if (iskeyword(x, KW_try)) {
                    sig->flags |= SIG_TRYBLOCK;
                }
            }
        }
    }

    // The return type
    if (sig->idchain != ch) {
        sig->typestart   = ch->head;
        sig->typeend     = t ? t->prev : ch->tail;
        sig->typewrapsid = 1;
    } else {
        Token *x = sig->scopestart ? sig->scopestart : sig->idstart;
        sig->typestart   = 0;
        sig->typeend     = 0;
        sig->typewrapsid = 0;
        if (x->prev) {
            Token *pc = sig->sigconst ? sig->sigconst : sig->paren;
            if (iscxx(c) && iskeyword(x->prev, KW_auto) && pc->next &&
                    pc->next->type==T_ARROW && pc->next->next &&
                    !(pc->next->next->type & (T_SEMICOLON|T_LBRACE))) {
                // auto name(params) -> type
                sig->typestart = sig->typeend = pc->next->next;
                while (sig->typeend->next &&
                        !(sig->typeend->next->type & (T_SEMICOLON|T_LBRACE))) {
                    sig->typeend = sig->typeend->next;
                }
            } else {
                sig->typestart = ch->head;
                sig->typeend   = x->prev;
                Token *e = sig->typeend;
                if (e->type==T_PARENS && e->chain->count>=3 &&
                        e->prev==sig->typestart && sig->typestart->type==T_IDENT) {
                    // MACRO(type) name(params)
                    sig->typestart = e->chain->head->next;
                    sig->typeend   = e->chain->tail->prev;
                }
            }
        }
    }
    return 1;
}

// Tag the function the signature describes. With pushscopes, its scope
// (with any qualifiers) is pushed, and the count returned.
static i32 emitfunction(Cxx *c, Sig *sig, i32 kind, b32 pushscopes)
{
    Pool *p      = &c->pool;
    i32   pushed = 0;
    u8    outer  = scopetype(c);

    if (sig->typestart && (sig->typestart==sig->scopestart || sig->typestart==sig->idstart)) {
        sig->typestart = sig->idend->next;
    }

    if (sig->scopestart) {
        if (pushscopes) {
            while (sig->scopestart != sig->idstart) {
                Token *id = sig->scopestart;
                sig->scopestart = nextof(sig->scopestart, T_COLONS)->next;
                destroyrange(p, sig->idchain, id->next, sig->scopestart->prev);
                chaintake(sig->idchain, id);
                scopepush(c, id, S_CLASS, A_UNKNOWN);
                pushed++;
            }
        } else {
            destroyrange(p, sig->idchain, sig->scopestart, sig->idstart->prev);
        }
    }

    Token *id = extractrange(p, sig->idstart, sig->idend, 0);
    destroyrange(p, sig->idchain, sig->idstart, sig->idend);
    id->type = T_IDENT;

    // A friend function belongs to the enclosing scope
    Token *saved = 0;
    if (kind==K_FUNCTION && c->seen&SEEN_FRIEND && !scopeisglobal(c)) {
        saved = scopetaketop(c);
    }
    Tag *tag  = cxxtag(c, kind, id);
    b32  tmpl = c->tmpl && c->tmpl->count>0 && iscxx(c);

    if (tag) {
        normalizechain(sig->paren->chain);
        sig->paren->chain->tail->space = 0;

        if (kind == K_PROTOTYPE) {
            tag->filescope = !isheader(c);
        } else if (outer == S_NAMESPACE) {
            tag->filescope = (c->seen&SEEN_STATIC || inanon(c)) && !isheader(c);
        } else {
            tag->filescope = !isheader(c);
        }
        if (c->seen&SEEN_EXPORT || scopeisexported(c)) {
            tag->filescope = 0;
        }

        Token *type = 0;
        if (sig->typestart && sig->typewrapsid) {
            // Without the parameters: type (*name)(params)
            if (sig->typestart == sig->paren) {
                sig->typestart = sig->paren->next;
            }
            if (sig->typeend == sig->paren) {
                sig->typeend = sig->paren->prev;
            }
            if (sig->typestart && sig->typeend) {
                Token *before = sig->paren->prev;
                chaintake(sig->parencontainer, sig->paren);
                type = settype(c, tag, sig->typestart, sig->typeend);
                chaininsertafter(sig->parencontainer, before, sig->paren);
            }
        } else if (sig->typestart) {
            type = settype(c, tag, sig->typestart, sig->typeend);
        }

        if (tmpl && sig->specstart) {
            normalizerange(sig->specstart, sig->specend);
            sig->specstart->space = 0;
        }

        cxxcommit(c, tag);
        freetoken(p, type);
    }

    if (saved) {
        scopepushtop(c, saved);
    }

    if (pushscopes) {
        scopepush(c, id, kind==K_PROTOTYPE ? S_PROTOTYPE : S_FUNCTION, A_UNKNOWN);
        pushed++;
    } else {
        freetoken(p, id);
    }

    if (tag && tmpl && cxxkind(c, K_TPARAM)) {
        emittemplateparams(c);
    }
    return pushed;
}

// At '{' outside a function: tag the function it begins, if any.
// Returns the number of scopes pushed.
static i32 sigbeforebracket(Cxx *c, Sig *sig)
{
    destroylast(&c->pool, c->chain);  // the '{'

    Params params = {};
    b32    want   = cxxkind(c, K_PARAMETER);
    if (!lookforsig(c, c->chain, sig, want ? &params : 0)) {
        return 0;
    }
    i32 scopes = emitfunction(c, sig, K_FUNCTION, 1);
    if (want) {
        emitparams(c, &params);
    }
    return scopes;
}

static void emitparams(Cxx *c, Params *ps)
{
    for (i32 i = 0; i < ps->count; i++) {
        Token *id  = ps->ids[i];
        Tag   *tag = cxxtag(c, K_PARAMETER, id);
        if (!tag) {
            break;
        }

        Token *type  = 0;
        Token *start = ps->typestart[i];
        Token *end   = ps->typeend[i];
        if (start && end && start!=end) {
            if (start == id) {
                start = start->next;
            } else if (end == id) {
                end = end->prev;
            }
            chaintakenested(ps->chain, id);
            type = settype(c, tag, start, end);
        }

        tag->filescope = 1;
        cxxcommit(c, tag);
        freetoken(&c->pool, type);
    }
}

// Does the (...) chain look like a parameter list? If so, and params
// is given, record each parameter's type and name, an anonymous name
// for those without one.
static b32 looksparams(Cxx *c, Chain *tc, Params *ps)
{
    if (ps) {
        ps->count = 0;
        ps->chain = tc;
    }
    if (tc->count == 2) {
        return 1;  // ()
    }

    // Tokens not expected in a parameter before its default value
    u32 bad = T_ARROW|T_OPERATOR|T_DOT|T_NUMBER|T_STRING|T_CHAR|T_ANGLES|T_COLON;

    for (Token *t = chainat(tc, 1);;) {
        Token *start = t;
        if (!(t->type & (T_IDENT|T_KEYWORD|T_DOTS|T_COLONS))) {
            return 0;
        }

        for (;;) {
            u32 types = T_RPAREN|T_COMMA|T_ASSIGN|T_LT|T_GT|T_PARENS|bad;
            t = nextof(t, types);
            if (t->type == T_PARENS) {
                // (*name), (&name), (args), ((x)(y))
                b32 ok = !firstof(t->chain, bad) &&
                         (firstof(t->chain, T_STAR|T_AND) ||
                          looksparams(c, t->chain, 0) ||
                          !firstnotof(t->chain, T_LPAREN|T_PARENS|T_RPAREN));
                if (!ok) {
                    return 0;
                }
            } else if (t->type == T_LT) {
                t = skipangles(t);
                if (!t) {
                    return 0;
                }
            } else {
                break;
            }
        }

        if (t->type==T_GT || t->type&bad) {
            return 0;
        }

        if (ps && t->prev!=start) {
            if (ps->count < PARAMMAX) {
                // Find the name. As in Universal Ctags, a failed test
                // may leave the name it found.
                Token *id   = 0;
                Token *prev = t->prev;
                if (prev->type == T_IDENT) {
                    id = prev;
                } else if (prev->prev) {
                    Token *nonsq = prevnotof(t, T_BRACKETS);
                    b32    sq    = nonsq && nonsq!=prev;
                    if (sq && nonsq->type==T_IDENT) {
                        id = nonsq;  // name[]
                    } else if (sq && nonsq->type==T_PARENS &&
                               (id = firstof(nonsq->chain, T_IDENT))) {
                        // (*name)[]
                    } else if (prev->type==T_NUMBER && prev->prev->type==T_IDENT) {
                        id = prev->prev;  // name : 3
                    } else if (prev->type==T_PARENS &&
                               ((prev->prev->type==T_PARENS &&
                                 (id = lastnestedof(prev->prev->chain, T_IDENT, 0)) &&
                                 id->prev && id->prev->type==T_STAR) ||
                                ((id = lastnestedof(prev->chain, T_IDENT, 0)) &&
                                 id->prev && id->prev->type&(T_STAR|T_AND)))) {
                        // (*name)(params) and (&name)
                    }
                }

                i32 n = ps->count;
                ps->typestart[n] = start;
                ps->typeend[n]   = prev;
                ps->anon &= ~(1u << n);
                if (!id) {
                    id = anontoken(c, K_PARAMETER, {});
                    id->line    = prev->line;
                    id->linepos = prev->linepos;
                    ps->anon |= 1u << n;
                }
                ps->ids[n] = id;
                ps->count++;
            } else {
                ps = 0;
            }

        } else if (ps && ps->count<PARAMMAX && !iskeyword(start, KW_void) &&
                   start->type!=T_DOTS) {
            // An unnamed parameter of one token: int f(int)
            Token *type = copytoken(&c->pool, start);
            Token *id   = anontoken(c, K_PARAMETER, {});
            id->line    = start->line;
            id->linepos = start->linepos;
            type->next  = id;
            id->prev    = type;

            i32 n = ps->count;
            ps->typestart[n] = type;
            ps->typeend[n]   = id;
            ps->ids[n]       = id;
            ps->anon |= 1u << n;
            ps->count++;
        }

        if (t->type == T_RPAREN) {
            return 1;
        }
        if (t->type == T_COMMA) {
            t = t->next;
            continue;
        }

        // A default value, which C does not have
        if (isc(c)) {
            return 0;
        }
        t = nextof(t, T_RPAREN|T_COMMA);
        if (t->type == T_RPAREN) {
            return 1;
        }
        t = t->next;
    }
}

// The first identifier, perhaps within nested parentheses, and in C++
// the last part of a qualified name.
static Token *firstnestedid(Cxx *c, Chain *ch, Chain **parent)
{
    Token *id = firstnestedof(ch, T_IDENT, parent);
    if (!id || !iscxx(c) || !id->next || id->next->type!=T_COLONS) {
        return id;
    }
    return nextof(id, T_IDENT);
}

// Within parentheses, after stars and qualifiers, a declared name:
// type (*name)[n] or type (*(*name))
static Token *vardefinparens(Token *t, i32 depth)
{
    for (t = t->next; t; t = t->next) {
        b32 skip = t->type==T_STAR || iskeyword(t, KW_const) || iskeyword(t, KW_volatile);
        if (!skip) {
            break;
        }
    }
    if (!t) {
        return 0;
    }
    if (t->type == T_PARENS) {
        return vardefinparens(t->chain->head, depth+1);
    }
    if (t->type==T_IDENT && t->next &&
            (t->next->type&(T_BRACKETS|T_RPAREN) || depth>0)) {
        return t;
    }
    return 0;
}

// Tag the variables a statement declares, or K&R parameters. Returns
// whether it found any. The statement ends in ';' (or such).
static b32 extractvars(Cxx *c, Chain *ch, b32 knr)
{
    Pool *p = &c->pool;
    if (ch->count < 1) {
        return 0;
    }

    Token *t     = ch->head;
    u8     scope = scopetype(c);
    if (!(t->type & (T_IDENT|T_KEYWORD))) {
        return 0;
    }
    if (t->type==T_KEYWORD && !kwflag(t->kw, KF_VARDECL)) {
        return 0;
    }

    Token *specstart = 0;
    Token *specend   = 0;
    b32    got       = 0;
    while (t) {
        // To a token that ends the type and name
        for (; t; t = t->next) {
            u32 notable = T_COLON|T_PARENS|T_BRACKETS|T_BRACES|T_ASSIGN|
                          T_COMMA|T_SEMICOLON|T_LBRACE;
            if (t->type & notable) {
                break;
            }
            u32 bad = T_OPERATOR|T_ANDS|T_ARROW|T_STRING|T_ANGLES|T_CHAR|
                      T_DOTS|T_RBRACE|T_RPAREN|T_RBRACKET|T_GT;
            if (t->type & bad) {
                return got;
            }
            if (t->type == T_LT) {
                specstart = t;
                t = skipangles(t);
                if (!t) {
                    return got;
                }
                specend = t;
            }
        }
        if (!t || !t->prev) {
            return got;
        }

        Token *id          = 0;
        Token *before      = 0;  // the token before the name
        Token *removestart = t;
        b32    varspec     = 0;  // a variable template specialization

        switch (t->type) {
        case T_PARENS:
            // type (*name)(params) or type (*name)[n]
            if (t->next &&
                    ((t->next->type==T_PARENS && looksparams(c, t->next->chain, 0)) ||
                     t->next->type==T_BRACKETS) &&
                    (id = firstnestedid(c, t->chain, 0)) &&
                    (!id->next || id->next->type!=T_PARENS)) {
                before = t->prev;
                t = t->next->next;
                removestart = t;
                goto gotid;
            }

            // type MACRO((...)) is no variable
            if (t->chain->count==3 && chainat(t->chain, 1)->type==T_PARENS &&
                    t->prev && t->prev->type==T_IDENT &&
                    t->prev->prev && t->prev->prev->type==T_IDENT) {
                return got;
            }

            // type name(args), a constructor call
            if (t->prev->type==T_IDENT && (scope==S_NAMESPACE || scope==S_FUNCTION) &&
                    iscxx(c) && ctorargs(c, t->chain)) {
                id     = t->prev;
                before = id->prev;
                goto gotid;
            }

            // type (*name), type (name)
            if ((t->prev->type&(T_STAR|T_AND) ||
                 (t->prev->type==T_KEYWORD && kwflag(t->prev->kw, KF_TYPEPART) &&
                  !kwflag(t->prev->kw, KF_DECLTYPE))) &&
                    (id = vardefinparens(t->chain->head, 0))) {
                before = t->prev;
                t = t->next;
                goto gotid;
            }

            // decltype(...) name
            if (t->prev->type==T_KEYWORD && kwflag(t->prev->kw, KF_DECLTYPE) && t->next) {
                t = t->next;
                continue;
            }
            return got;

        case T_BRACES:
            // type name{args}
            if (t->prev->type==T_IDENT && iscxx(c) && ctorargs(c, t->chain)) {
                id     = t->prev;
                before = id->prev;
                goto gotid;
            }
            return got;

        case T_COLON:
            // A bit field: type name : n
            if (t->next && t->next->type&(T_NUMBER|T_IDENT)) {
                id     = t->prev;
                before = id->prev;
                if (t->next->type==T_NUMBER && t->next->next &&
                        t->next->next->type&(T_COMMA|T_SEMICOLON|T_ASSIGN)) {
                    t = t->next->next;
                }
                goto gotid;
            }
            return got;

        case T_BRACKETS:
            // type name[n]...
            varspec = t->prev && t->prev==specend;
            id      = varspec ? specstart->prev : t->prev;
            before  = id->prev;
            while (t->next && t->next->type==T_BRACKETS) {
                t = t->next;
            }
            if (!t->next) {
                return got;
            }
            // Attributes, perhaps macros with arguments
            while (t->next && t->next->type==T_IDENT) {
                t = t->next;
                if (t->next && t->next->type==T_PARENS) {
                    t = t->next;
                }
            }
            if (!t->next) {
                return got;
            }
            if (!(t->next->type & (T_COMMA|T_SEMICOLON|T_ASSIGN|T_BRACES))) {
                return got;
            }
            t = t->next;
            break;

        default:
            varspec = t->prev && t->prev==specend;
            if (varspec ? specstart->prev->type!=T_IDENT : t->prev->type!=T_IDENT) {
                return got;
            }
            id     = varspec ? specstart->prev : t->prev;
            before = id->prev;
            if (varspec) {
                t = specend->next;
            }
            break;
        }

        if (varspec) {
            specend->space = 0;
            Token *spec = extractrange(p, specstart, specend, 0);
            if (c->spec) {
                chainclear(p, c->spec);
            } else {
                c->spec = newchain(p);
            }
            chainappend(c->spec, spec);
            destroyrange(p, ch, specstart, specend);
            specstart = specend = 0;
        }

    gotid:
        if (!before) {
            // MACRO(x) name;
            if (t->type==T_PARENS && t->next && t->next->type==T_IDENT &&
                    t->next->next && t->next->next->type==T_SEMICOLON) {
                id     = t->next;
                before = t;
                t = t->next->next;
            } else {
                return got;
            }
        }

        // Qualifiers: type A::B::name
        Token *scopeend   = before->next;
        Token *scopestart = 0;
        while (before->type == T_COLONS) {
            if (!iscxx(c)) {
                return 0;
            }
            before = before->prev;
            if (!before) {
                return got;
            }
            if (before->type == T_GT) {
                Token *lt = skipanglesback(before);
                if (!lt || !lt->prev) {
                    return got;
                }
                before = lt->prev;
            }
            if (before->type != T_IDENT) {
                return got;
            }
            scopestart = before;
            before = before->prev;
            if (!before) {
                return got;
            }
        }

        if (!got) {
            // The first must follow a plausible type
            if (!(before->type & (T_IDENT|T_KEYWORD|T_STAR|T_AND))) {
                if (before->type == T_GT) {
                    i32 level = 1;
                    for (Token *x = before->prev; x; x = x->prev) {
                        if (x->type == T_GT) {
                            level++;
                        } else if (x->type == T_LT) {
                            level--;
                        }
                    }
                    if (level) {
                        return got;
                    }
                } else if (before->type==T_PARENS && before->prev &&
                           (!before->prev->prev ||
                            (before->prev->prev->type==T_KEYWORD &&
                             kwflag(before->prev->prev->kw, KF_VARDECL))) &&
                           (before->prev->type==T_IDENT ||
                            (before->prev->type==T_KEYWORD &&
                             kwflag(before->prev->kw, KF_DECLTYPE)))) {
                    // MACRO(x) and decltype(x)
                } else {
                    return got;
                }
            }
            got = 1;
        }

        i32 pushed = 0;
        while (scopestart && scopestart!=scopeend) {
            Token *partend = nextof(scopestart, T_COLONS);
            Token *sid     = extractrange(p, scopestart, partend->prev, 0);
            scopepush(c, sid, S_CLASS, A_UNKNOWN);
            Token *next = partend->next;
            destroyrange(p, ch, scopestart, partend);
            scopestart = next;
            pushed++;
        }

        chaintakenested(ch, id);

        b32 tmpl = c->tmpl && c->tmpl->count>0 && iscxx(c);
        i32 kind = knr ? K_PARAMETER :
                   c->seen&SEEN_EXTERN ? K_EXTERNVAR :
                   scopevarkind(c);
        Tag *tag = cxxtag(c, kind, id);
        if (tag) {
            // Array dimensions in the type, except a lone number
            for (Token *x = t->prev; x && x->type==T_BRACKETS; x = x->prev) {
                Chain *dims = x->chain;
                if (dims->count>3 || (dims->count==3 && chainat(dims, 1)->type!=T_NUMBER)) {
                    destroyrange(p, dims, dims->head->next, dims->tail->prev);
                }
            }

            Token *type = settype(c, tag, ch->head, t->prev);
            if (knr) {
                tag->filescope = 1;
            } else {
                b32 exported = c->seen&SEEN_EXPORT || scopeisexported(c);
                tag->filescope =
                    (scope==S_NAMESPACE && (c->seen&SEEN_STATIC || inanon(c)) &&
                     !exported && !isheader(c)) ||
                    scope==S_FUNCTION ||
                    (scope!=S_NAMESPACE && scope!=S_FUNCTION && !exported && !isheader(c));
            }
            cxxcommit(c, tag);
            freetoken(p, type);
        }

        if (tmpl && cxxkind(c, K_TPARAM)) {
            scopepush(c, id, S_VARIABLE, A_PUBLIC);
            emittemplateparams(c);
            scopepop(c);
        } else {
            freetoken(p, id);
        }
        for (; pushed > 0; pushed--) {
            scopepop(c);
        }

        if (!t) {
            return got;
        }
        if (!(t->type & (T_COMMA|T_SEMICOLON|T_LBRACE))) {
            t = nextof(t, T_COMMA|T_SEMICOLON|T_LBRACE|T_LT);
            if (!t || t->type==T_LT) {
                return got;
            }
        }
        if (t->type & (T_SEMICOLON|T_LBRACE)) {
            return got;
        }

        // Past the comma, keeping the type for the next name
        t = t->next;
        if (!removestart) {
            return got;
        }
        while (removestart->prev &&
                removestart->prev->type&(T_STAR|T_AND|T_ANDS|T_BRACKETS)) {
            removestart = removestart->prev;
        }
        destroyrange(p, ch, removestart, t->prev);
    }
    return got;
}

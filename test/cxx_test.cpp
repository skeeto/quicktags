// C and C++: tag cases
// This is free and unencumbered software released into the public domain.

static TagCase ccases[] = {
    {SHARED, "macros", "m.c", "--kinds-C=+D",
     "#define LIMIT 10\n"
     "#define MAX(a, b) ((a) > (b) ? (a) : (b))\n"
     "#define VA(fmt, args...) printf(fmt, args)\n",
     "LIMIT\tm.c\t/^#define LIMIT /;\"\td\tline:1\tfile:\n"
     "MAX\tm.c\t/^#define MAX(/;\"\td\tline:2\tfile:\n"
     "VA\tm.c\t/^#define VA(/;\"\td\tline:3\tfile:\n"
     "a\tm.c\t/^#define MAX(a, b) ((a) > (b) ? (a) : (b))$/;\"\tD\tline:2\tmacro:MAX\n"
     "args\tm.c\t/^#define VA(fmt, args...) printf(fmt, args)$/;\"\tD\tline:3\tmacro:VA\n"
     "b\tm.c\t/^#define MAX(a, b) ((a) > (b) ? (a) : (b))$/;\"\tD\tline:2\tmacro:MAX\n"
     "fmt\tm.c\t/^#define VA(fmt, args...) printf(fmt, args)$/;\"\tD\tline:3\tmacro:VA\n",
     ""},
    {SHARED, "struct typedef", "s.c", "",
     "typedef struct node {\n"
     "    int value;\n"
     "    struct node *next;\n"
     "} node, *nodeptr;\n",
     "next\ts.c\t/^    struct node *next;$/;\"\tm\tline:3\tstruct:node\ttyperef:struct:node *\tfile:\n"
     "node\ts.c\t/^typedef struct node {$/;\"\ts\tline:1\tfile:\n"
     "node\ts.c\t/^} node, *nodeptr;$/;\"\tt\tline:4\ttyperef:struct:node\tfile:\n"
     "nodeptr\ts.c\t/^} node, *nodeptr;$/;\"\tt\tline:4\ttyperef:struct:node *\tfile:\n"
     "value\ts.c\t/^    int value;$/;\"\tm\tline:2\tstruct:node\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "enum variable", "e.c", "",
     "enum color { RED, GREEN = 2, BLUE } favorite;\n",
     "BLUE\te.c\t/^enum color { RED, GREEN = 2, BLUE } favorite;$/;\"\te\tline:1\tenum:color\tfile:\n"
     "GREEN\te.c\t/^enum color { RED, GREEN = 2, BLUE } favorite;$/;\"\te\tline:1\tenum:color\tfile:\n"
     "RED\te.c\t/^enum color { RED, GREEN = 2, BLUE } favorite;$/;\"\te\tline:1\tenum:color\tfile:\n"
     "color\te.c\t/^enum color { RED, GREEN = 2, BLUE } favorite;$/;\"\tg\tline:1\tfile:\n"
     "favorite\te.c\t/^enum color { RED, GREEN = 2, BLUE } favorite;$/;\"\tv\tline:1\ttyperef:enum:color\n",
     ""},
    {SHARED, "functions and locals", "f.c", "--kinds-C=+lpz",
     "static int helper(void);\n"
     "static int helper(void) { return 0; }\n"
     "int api(int x, char *name)\n"
     "{\n"
     "    int local = x, other[4];\n"
     "    return local + helper();\n"
     "}\n",
     "api\tf.c\t/^int api(int x, char *name)$/;\"\tf\tline:3\ttyperef:typename:int\n"
     "helper\tf.c\t/^static int helper(void) { return 0; }$/;\"\tf\tline:2\ttyperef:typename:int\tfile:\n"
     "helper\tf.c\t/^static int helper(void);$/;\"\tp\tline:1\ttyperef:typename:int\tfile:\n"
     "local\tf.c\t/^    int local = x, other[4];$/;\"\tl\tline:5\tfunction:api\ttyperef:typename:int\tfile:\n"
     "name\tf.c\t/^int api(int x, char *name)$/;\"\tz\tline:3\tfunction:api\ttyperef:typename:char *\tfile:\n"
     "other\tf.c\t/^    int local = x, other[4];$/;\"\tl\tline:5\tfunction:api\ttyperef:typename:int[4]\tfile:\n"
     "x\tf.c\t/^int api(int x, char *name)$/;\"\tz\tline:3\tfunction:api\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "K&R definition", "k.c", "--kinds-C=+z",
     "int add(a, b)\n"
     "    int a;\n"
     "    int b;\n"
     "{\n"
     "    return a + b;\n"
     "}\n",
     "a\tk.c\t/^    int a;$/;\"\tz\tline:2\tfunction:add\ttyperef:typename:int\tfile:\n"
     "add\tk.c\t/^int add(a, b)$/;\"\tf\tline:1\n"
     "b\tk.c\t/^    int b;$/;\"\tz\tline:3\tfunction:add\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "anonymous struct", "a.c", "",
     "struct { int x; } s;\n"
     "union { int i; float f; } u;\n",
     "__anon0b8855d70108\ta.c\t/^struct { int x; } s;$/;\"\ts\tline:1\tfile:\n"
     "__anon0b8855d7020a\ta.c\t/^union { int i; float f; } u;$/;\"\tu\tline:2\tfile:\n"
     "f\ta.c\t/^union { int i; float f; } u;$/;\"\tm\tline:2\tunion:__anon0b8855d7020a\ttyperef:typename:float\tfile:\n"
     "i\ta.c\t/^union { int i; float f; } u;$/;\"\tm\tline:2\tunion:__anon0b8855d7020a\ttyperef:typename:int\tfile:\n"
     "s\ta.c\t/^struct { int x; } s;$/;\"\tv\tline:1\ttyperef:struct:__anon0b8855d70108\n"
     "u\ta.c\t/^union { int i; float f; } u;$/;\"\tv\tline:2\ttyperef:union:__anon0b8855d7020a\n"
     "x\ta.c\t/^struct { int x; } s;$/;\"\tm\tline:1\tstruct:__anon0b8855d70108\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "bit fields", "b.c", "",
     "struct flags {\n"
     "    unsigned a : 1, b : 2;\n"
     "    int c : 3;\n"
     "};\n",
     "a\tb.c\t/^    unsigned a : 1, b : 2;$/;\"\tm\tline:2\tstruct:flags\ttyperef:typename:unsigned:1\tfile:\n"
     "b\tb.c\t/^    unsigned a : 1, b : 2;$/;\"\tm\tline:2\tstruct:flags\ttyperef:typename:unsigned:2\tfile:\n"
     "c\tb.c\t/^    int c : 3;$/;\"\tm\tline:3\tstruct:flags\ttyperef:typename:int:3\tfile:\n"
     "flags\tb.c\t/^struct flags {$/;\"\ts\tline:1\tfile:\n",
     ""},
    {SHARED, "conditional branches", "c.c", "",
     "#if 0\n"
     "int dead;\n"
     "#else\n"
     "int live;\n"
     "#endif\n"
     "#ifdef X\n"
     "int first;\n"
     "#else\n"
     "int second;\n"
     "#endif\n",
     "first\tc.c\t/^int first;$/;\"\tv\tline:7\ttyperef:typename:int\n"
     "live\tc.c\t/^int live;$/;\"\tv\tline:4\ttyperef:typename:int\n"
     "second\tc.c\t/^int second;$/;\"\tv\tline:9\ttyperef:typename:int\n",
     ""},
    {SHARED, "labels", "l.c", "--kinds-C=+L",
     "void spin(void)\n"
     "{\n"
     "again:\n"
     "    goto again;\n"
     "}\n",
     "again\tl.c\t/^again:$/;\"\tL\tline:3\tfunction:spin\tfile:\n"
     "spin\tl.c\t/^void spin(void)$/;\"\tf\tline:1\ttyperef:typename:void\n",
     ""},
    {SHARED, "header scope", "h.h", "--kinds-C=+px",
     "static int counter;\n"
     "extern int shared;\n"
     "int compute(int);\n"
     "#define H_H\n",
     "H_H\th.h\t/^#define H_H$/;\"\td\tline:4\n"
     "compute\th.h\t/^int compute(int);$/;\"\tp\tline:3\ttyperef:typename:int\n"
     "counter\th.h\t/^static int counter;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "shared\th.h\t/^extern int shared;$/;\"\tx\tline:2\ttyperef:typename:int\n",
     ""},
    {SHARED, "function pointers", "p.c", "",
     "typedef int (*handler)(int, char **);\n"
     "void (*callback)(void);\n"
     "int (*table[4])(int);\n",
     "callback\tp.c\t/^void (*callback)(void);$/;\"\tv\tline:2\ttyperef:typename:void (*)(void)\n"
     "handler\tp.c\t/^typedef int (*handler)(int, char **);$/;\"\tt\tline:1\ttyperef:typename:int (*)(int,char **)\tfile:\n"
     "table\tp.c\t/^int (*table[4])(int);$/;\"\tv\tline:3\ttyperef:typename:int (* [4])(int)\n",
     ""},
    {SHARED, "long pattern", "w.c", "",
     "int a_variable_with_a_rather_long_name_that_goes_on = 1; /* and a comment that pushes the line past the limit */\n",
     "a_variable_with_a_rather_long_name_that_goes_on\tw.c\t/^int a_variable_with_a_rather_long_name_that_goes_on = 1; \\/* and a comment that pushes the line /;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "escapes in patterns", "x.c", "",
     "char *path = \"a/b\\\\c\";\n"
     "int end$;\n",
     "end$\tx.c\t/^int end$;$/;\"\tv\tline:2\ttyperef:typename:int\n"
     "path\tx.c\t/^char *path = \"a\\/b\\\\\\\\c\";$/;\"\tv\tline:1\ttyperef:typename:char *\n",
     ""},
    {IMPROVED, "P9 static after a struct body", "p9.c", "",
     "struct S { int a; } static x;\n",
     "S\tp9.c\t/^struct S { int a; } static x;$/;\"\ts\tline:1\tfile:\n"
     "a\tp9.c\t/^struct S { int a; } static x;$/;\"\tm\tline:1\tstruct:S\ttyperef:typename:int\tfile:\n"
     "x\tp9.c\t/^struct S { int a; } static x;$/;\"\tv\tline:1\ttyperef:struct:S\tfile:\n",
     "S\tp9.c\t/^struct S { int a; } static x;$/;\"\ts\tline:1\tfile:\n"
     "a\tp9.c\t/^struct S { int a; } static x;$/;\"\tm\tline:1\tstruct:S\ttyperef:typename:int\tfile:\n"
     "x\tp9.c\t/^struct S { int a; } static x;$/;\"\tv\tline:1\ttyperef:struct:S \n"},
    {IMPROVED, "X10 macro parameters keep their scope", "x10.c",
     "--kinds-C=+D --extras=-F",
     "#define MAX(a, b) a\n",
     "a\tx10.c\t/^#define MAX(a, b) a$/;\"\tD\tline:1\tmacro:MAX\n"
     "b\tx10.c\t/^#define MAX(a, b) a$/;\"\tD\tline:1\tmacro:MAX\n",
     "a\tx10.c\t/^#define MAX(a, b) a$/;\"\tD\tline:1\n"
     "b\tx10.c\t/^#define MAX(a, b) a$/;\"\tD\tline:1\n"},
    {IMPROVED, "X10 without the macro kind", "x10.h", "--kinds-C=-d+D",
     "#define MAX(a, b) a\n",
     "a\tx10.h\t/^#define MAX(a, b) a$/;\"\tD\tline:1\tmacro:MAX\n"
     "b\tx10.h\t/^#define MAX(a, b) a$/;\"\tD\tline:1\tmacro:MAX\n",
     "a\tx10.h\t/^#define MAX(a, b) a$/;\"\tD\tline:1\n"
     "b\tx10.h\t/^#define MAX(a, b) a$/;\"\tD\tline:1\n"},
    {IMPROVED, "P3 macro type with a pointer", "p3.c", "",
     "STACK_OF(X509) *load_certs(const char *path) { return 0; }\n"
     "_Atomic(int) *counter(void) { return 0; }\n",
     "counter\tp3.c\t/^_Atomic(int) *counter(void) { return 0; }$/;\"\tf\tline:2\ttyperef:typename:_Atomic (int)*\n"
     "load_certs\tp3.c\t/^STACK_OF(X509) *load_certs(const char *path) { return 0; }$/;\"\tf\tline:1\ttyperef:typename:STACK_OF (X509)*\n",
     "STACK_OF\tp3.c\t/^STACK_OF(X509) *load_certs(const char *path) { return 0; }$/;\"\tf\tline:1\n"
     "_Atomic\tp3.c\t/^_Atomic(int) *counter(void) { return 0; }$/;\"\tf\tline:2\n"},
    {IMPROVED, "P8 keywords dropped from types", "p8.c", "",
     "int * restrict p;\n"
     "int * restrict * q;\n"
     "static inline int (*handler)(void);\n",
     "handler\tp8.c\t/^static inline int (*handler)(void);$/;\"\tv\tline:3\ttyperef:typename:int (*)(void)\tfile:\n"
     "p\tp8.c\t/^int * restrict p;$/;\"\tv\tline:1\ttyperef:typename:int *\n"
     "q\tp8.c\t/^int * restrict * q;$/;\"\tv\tline:2\ttyperef:typename:int **\n",
     "handler\tp8.c\t/^static inline int (*handler)(void);$/;\"\tv\tline:3\ttyperef:typename:int (*)(void)\tfile:\n"
     "p\tp8.c\t/^int * restrict p;$/;\"\tv\tline:1\ttyperef:typename:int * \n"
     "q\tp8.c\t/^int * restrict * q;$/;\"\tv\tline:2\ttyperef:typename:int * *\n"},
    {SHARED, "qualified C members", "q.c", "--extras=+q",
     "struct point { int x, y; };\n"
     "union value { int i; struct { int a; } s; };\n"
     "enum color { RED };\n"
     "typedef struct { int z; } zt;\n",
     "RED\tq.c\t/^enum color { RED };$/;\"\te\tline:3\tenum:color\tfile:\n"
     "__anon0b8899e70108\tq.c\t/^union value { int i; struct { int a; } s; };$/;\"\ts\tline:2\tunion:value\tfile:\n"
     "__anon0b8899e70208\tq.c\t/^typedef struct { int z; } zt;$/;\"\ts\tline:4\tfile:\n"
     "__anon0b8899e70208::z\tq.c\t/^typedef struct { int z; } zt;$/;\"\tm\tline:4\tstruct:__anon0b8899e70208\ttyperef:typename:int\tfile:\n"
     "a\tq.c\t/^union value { int i; struct { int a; } s; };$/;\"\tm\tline:2\tstruct:value::__anon0b8899e70108\ttyperef:typename:int\tfile:\n"
     "color\tq.c\t/^enum color { RED };$/;\"\tg\tline:3\tfile:\n"
     "i\tq.c\t/^union value { int i; struct { int a; } s; };$/;\"\tm\tline:2\tunion:value\ttyperef:typename:int\tfile:\n"
     "point\tq.c\t/^struct point { int x, y; };$/;\"\ts\tline:1\tfile:\n"
     "point::x\tq.c\t/^struct point { int x, y; };$/;\"\tm\tline:1\tstruct:point\ttyperef:typename:int\tfile:\n"
     "point::y\tq.c\t/^struct point { int x, y; };$/;\"\tm\tline:1\tstruct:point\ttyperef:typename:int\tfile:\n"
     "s\tq.c\t/^union value { int i; struct { int a; } s; };$/;\"\tm\tline:2\tunion:value\ttyperef:struct:value::__anon0b8899e70108\tfile:\n"
     "value\tq.c\t/^union value { int i; struct { int a; } s; };$/;\"\tu\tline:2\tfile:\n"
     "value::__anon0b8899e70108\tq.c\t/^union value { int i; struct { int a; } s; };$/;\"\ts\tline:2\tunion:value\tfile:\n"
     "value::__anon0b8899e70108::a\tq.c\t/^union value { int i; struct { int a; } s; };$/;\"\tm\tline:2\tstruct:value::__anon0b8899e70108\ttyperef:typename:int\tfile:\n"
     "value::i\tq.c\t/^union value { int i; struct { int a; } s; };$/;\"\tm\tline:2\tunion:value\ttyperef:typename:int\tfile:\n"
     "value::s\tq.c\t/^union value { int i; struct { int a; } s; };$/;\"\tm\tline:2\tunion:value\ttyperef:struct:value::__anon0b8899e70108\tfile:\n"
     "x\tq.c\t/^struct point { int x, y; };$/;\"\tm\tline:1\tstruct:point\ttyperef:typename:int\tfile:\n"
     "y\tq.c\t/^struct point { int x, y; };$/;\"\tm\tline:1\tstruct:point\ttyperef:typename:int\tfile:\n"
     "z\tq.c\t/^typedef struct { int z; } zt;$/;\"\tm\tline:4\tstruct:__anon0b8899e70208\ttyperef:typename:int\tfile:\n"
     "zt\tq.c\t/^typedef struct { int z; } zt;$/;\"\tt\tline:4\ttyperef:struct:__anon0b8899e70208\tfile:\n",
     ""},
    {SHARED, "extras without fileScope", "f.c", "--extras=-F --kinds-C=+l",
     "#define LIMIT 10\n"
     "static int count;\n"
     "int total;\n"
     "struct pair { int a, b; };\n"
     "static void helper(void) { int i; }\n"
     "int api(void) { return 0; }\n",
     "api\tf.c\t/^int api(void) { return 0; }$/;\"\tf\tline:6\ttyperef:typename:int\n"
     "total\tf.c\t/^int total;$/;\"\tv\tline:3\ttyperef:typename:int\n",
     ""},

    // -I (LEX-14, OPT-17)
    {SHARED, "-I kernel macros", "k.c",
     "--kinds-C=+p -I __init,__read_mostly,EXPORT_SYMBOL+,__printf+",
     "static int __init setup(void)\n"
     "{\n"
     "    return 0;\n"
     "}\n"
     "EXPORT_SYMBOL(setup)\n"
     "int counter __read_mostly = 1;\n"
     "void log_msg(const char *fmt, ...) __printf(1, 2);\n"
     "void after(void) {}\n",
     "after\tk.c\t/^void after(void) {}$/;\"\tf\tline:8\ttyperef:typename:void\n"
     "counter\tk.c\t/^int counter __read_mostly = 1;$/;\"\tv\tline:6\ttyperef:typename:int\n"
     "log_msg\tk.c\t/^void log_msg(const char *fmt, ...) __printf(1, 2);$/;\"\tp\tline:7\ttyperef:typename:void\tfile:\n"
     "setup\tk.c\t/^static int __init setup(void)$/;\"\tf\tline:1\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "-I replacements", "r.c",
     "-I DECL=int,ARRAY+=long,TYPE+=char,_Pragma=short",
     "DECL x;\n"
     "ARRAY(4) y;\n"
     "TYPE (a,\n"
     "  b) z;\n"
     "DECL\n"
     "w;\n"
     "_Pragma q;\n",
     "q\tr.c\t/^_Pragma q;$/;\"\tv\tline:7\ttyperef:typename:short\n"
     "w\tr.c\t/^w;$/;\"\tv\tline:6\ttyperef:typename:int\n"
     "x\tr.c\t/^DECL x;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "y\tr.c\t/^ARRAY(4) y;$/;\"\tv\tline:2\ttyperef:typename:long\n"
     "z\tr.c\t/^  b) z;$/;\"\tv\tline:4\ttyperef:typename:char\n",
     ""},
    {IMPROVED, "X11 -I replacements end words", "j.c", "-I X+=int,TYPE+=char",
     "TYPE v;\n"
     "unsigned X(a)long t;\n"
     "X(1)x;\n"
     "X(1)*p;\n",
     "p\tj.c\t/^X(1)*p;$/;\"\tv\tline:4\ttyperef:typename:int *\n"
     "t\tj.c\t/^unsigned X(a)long t;$/;\"\tv\tline:2\ttyperef:typename:unsigned int long\n"
     "v\tj.c\t/^TYPE v;$/;\"\tv\tline:1\ttyperef:typename:char\n"
     "x\tj.c\t/^X(1)x;$/;\"\tv\tline:3\ttyperef:typename:int\n",
     "p\tj.c\t/^X(1)*p;$/;\"\tv\tline:4\ttyperef:typename:int *\n"
     "t\tj.c\t/^unsigned X(a)long t;$/;\"\tv\tline:2\ttyperef:typename:unsigned intlong\n"},
    {SHARED, "-I in C, of C++ keywords", "kc.c", "-I class=long,final=char,int=short",
     "class x;\n"
     "final y;\n"
     "int z;\n",
     "x\tkc.c\t/^class x;$/;\"\tv\tline:1\ttyperef:typename:long\n"
     "y\tkc.c\t/^final y;$/;\"\tv\tline:2\ttyperef:typename:char\n"
     "z\tkc.c\t/^int z;$/;\"\tv\tline:3\ttyperef:typename:int\n",
     ""},
    {SHARED, "-I with defines", "d.c", "-I __init,WRAP=int --kinds-C=+D",
     "#define __init __attribute__((section(\".init\")))\n"
     "#define WRAP(x) x\n"
     "WRAP y;\n"
     "int __init f(void) { return 0; }\n",
     "WRAP\td.c\t/^#define WRAP(/;\"\td\tline:2\tfile:\n"
     "__init\td.c\t/^#define __init /;\"\td\tline:1\tfile:\n"
     "f\td.c\t/^int __init f(void) { return 0; }$/;\"\tf\tline:4\ttyperef:typename:int\n"
     "x\td.c\t/^#define WRAP(x) x$/;\"\tD\tline:2\tmacro:WRAP\n"
     "y\td.c\t/^WRAP y;$/;\"\tv\tline:3\ttyperef:typename:int\n",
     ""},
    {SHARED, "-I loops", "lp.c", "-I a=b,b=a,x=x,p=q(p)",
     "a x;\n"
     "b y;\n"
     "x z;\n"
     "p w;\n",
     "x\tlp.c\t/^a x;$/;\"\tv\tline:1\ttyperef:typename:a\n"
     "y\tlp.c\t/^b y;$/;\"\tv\tline:2\ttyperef:typename:b\n"
     "z\tlp.c\t/^x z;$/;\"\tv\tline:3\ttyperef:typename:x\n",
     ""},
    {SHARED, "-I replacements ending in hex digits", "hx.c", "-I X+=B,Y+=int*F,Z+=g",
     "struct X\n"
     "{ int m; };\n"  // B is read past, so it takes the line of '{'
     "Y\n"
     "\n"
     "\n"
     ";\n"
     "int Z\n"  // g is not a hex digit
     "(1)\n"
     ";\n",
     "B\thx.c\t/^{ int m; };$/;\"\ts\tline:2\tfile:\n"
     "F\thx.c\t/^;$/;\"\tv\tline:6\ttyperef:typename:int *\n"
     "g\thx.c\t/^int Z$/;\"\tv\tline:7\ttyperef:typename:int\n"
     "m\thx.c\t/^{ int m; };$/;\"\tm\tline:2\tstruct:B\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "-I counts restart as the file is read", "rs.c", "-I DECL=int",
     "DECL v1;\n"
     "DECL v2;\n"
     "DECL v3;\n"
     "DECL v4;\n"
     "DECL v5;\n"
     "DECL v6;\n"
     "DECL v7;\n"
     "DECL v8;\n"
     "DECL v9;\n"
     "DECL v10;\n",
     "v1\trs.c\t/^DECL v1;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "v10\trs.c\t/^DECL v10;$/;\"\tv\tline:10\ttyperef:typename:int\n"
     "v2\trs.c\t/^DECL v2;$/;\"\tv\tline:2\ttyperef:typename:int\n"
     "v3\trs.c\t/^DECL v3;$/;\"\tv\tline:3\ttyperef:typename:int\n"
     "v4\trs.c\t/^DECL v4;$/;\"\tv\tline:4\ttyperef:typename:int\n"
     "v5\trs.c\t/^DECL v5;$/;\"\tv\tline:5\ttyperef:typename:int\n"
     "v6\trs.c\t/^DECL v6;$/;\"\tv\tline:6\ttyperef:typename:int\n"
     "v7\trs.c\t/^DECL v7;$/;\"\tv\tline:7\ttyperef:typename:int\n"
     "v8\trs.c\t/^DECL v8;$/;\"\tv\tline:8\ttyperef:typename:int\n"
     "v9\trs.c\t/^DECL v9;$/;\"\tv\tline:9\ttyperef:typename:int\n",
     ""},
    {SHARED, "-I replacements in a row", "pp.c", "-I P=*P",
     "int P;\n",  // 8 times, then P stays
     "P\tpp.c\t/^int P;$/;\"\tv\tline:1\ttyperef:typename:int ********\n",
     ""},
    // Universal Ctags never finishes these
    {OURS, "X12 -I loops at the end, looking for a group", "le.c", "-I S+=;S",
     "int a;\n"
     "S\n",
     "a\tle.c\t/^int a;$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {OURS, "X12 -I loops at the end", "le.c", "-I T=int*v;T",
     "int b;\n"
     "T",
     "b\tle.c\t/^int b;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "v\tle.c\t/^T/;\"\tv\tline:2\ttyperef:typename:int *\n",
     ""},
    {SHARED, "-I groups", "g.c", "-I M+,N+",
     "int M(a[1], {b], <c) v;\n"
     "int N(x) (y) u;\n"
     "int M(\n",
     "v\tg.c\t/^int M(a[1], {b], <c) v;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "y\tg.c\t/^int N(x) (y) u;$/;\"\tv\tline:2\ttyperef:typename:int ()\n",
     ""},

    // Line formats
    {SHARED, "FLD-5 fields by name, roles, and extras", "fld.c",
     "--fields=+lKzZEr --extras=+q --kinds-C=+l",
     "#define LIMIT 10\n"
     "static int count;\n"
     "struct pair { int a; };\n"
     "static void helper(void) { int i; }\n"
     "struct { int z; } anon;\n",
     "LIMIT\tfld.c\t/^#define LIMIT /;\"\tkind:macro\tline:1\tlanguage:C\tfile:\troles:def\textras:fileScope\n"
     "__anon0f71ac2c0108\tfld.c\t/^struct { int z; } anon;$/;\"\tkind:struct\tline:5\tlanguage:C\tfile:\troles:def\textras:fileScope,anonymous\n"
     "__anon0f71ac2c0108::z\tfld.c\t/^struct { int z; } anon;$/;\"\tkind:member\tline:5\tlanguage:C\tscope:struct:__anon0f71ac2c0108\ttyperef:typename:int\tfile:\troles:def\textras:fileScope,qualified\n"
     "a\tfld.c\t/^struct pair { int a; };$/;\"\tkind:member\tline:3\tlanguage:C\tscope:struct:pair\ttyperef:typename:int\tfile:\troles:def\textras:fileScope\n"
     "anon\tfld.c\t/^struct { int z; } anon;$/;\"\tkind:variable\tline:5\tlanguage:C\ttyperef:struct:__anon0f71ac2c0108\troles:def\n"
     "count\tfld.c\t/^static int count;$/;\"\tkind:variable\tline:2\tlanguage:C\ttyperef:typename:int\tfile:\troles:def\textras:fileScope\n"
     "helper\tfld.c\t/^static void helper(void) { int i; }$/;\"\tkind:function\tline:4\tlanguage:C\ttyperef:typename:void\tfile:\troles:def\textras:fileScope\n"
     "i\tfld.c\t/^static void helper(void) { int i; }$/;\"\tkind:local\tline:4\tlanguage:C\tscope:function:helper\ttyperef:typename:int\tfile:\troles:def\textras:fileScope\n"
     "pair\tfld.c\t/^struct pair { int a; };$/;\"\tkind:struct\tline:3\tlanguage:C\tfile:\troles:def\textras:fileScope\n"
     "pair::a\tfld.c\t/^struct pair { int a; };$/;\"\tkind:member\tline:3\tlanguage:C\tscope:struct:pair\ttyperef:typename:int\tfile:\troles:def\textras:fileScope,qualified\n"
     "z\tfld.c\t/^struct { int z; } anon;$/;\"\tkind:member\tline:5\tlanguage:C\tscope:struct:__anon0f71ac2c0108\ttyperef:typename:int\tfile:\troles:def\textras:fileScope\n",
     ""},
    {SHARED, "FLD-5 no fields, so no ;\"", "fld.c", "--fields=",
     "#define LIMIT 10\n"
     "static int count;\n"
     "struct pair { int a; };\n",
     "LIMIT\tfld.c\t/^#define LIMIT /\n"
     "a\tfld.c\t/^struct pair { int a; };$/\n"
     "count\tfld.c\t/^static int count;$/\n"
     "pair\tfld.c\t/^struct pair { int a; };$/\n",
     ""},
    {SHARED, "OUT-6 line numbers", "fld.c", "--excmd=number",
     "#define LIMIT 10\n"
     "static int count;\n"
     "struct pair { int a; };\n",
     "LIMIT\tfld.c\t1;\"\td\tline:1\tfile:\n"
     "a\tfld.c\t3;\"\tm\tline:3\tstruct:pair\ttyperef:typename:int\tfile:\n"
     "count\tfld.c\t2;\"\tv\tline:2\ttyperef:typename:int\tfile:\n"
     "pair\tfld.c\t3;\"\ts\tline:3\tfile:\n",
     ""},
    {SHARED, "OUT-6 combined addresses", "fld.c", "--excmd=combine --fields=-n",
     "#define LIMIT 10\n"
     "static int count;\n"
     "struct pair { int a; };\n",
     "LIMIT\tfld.c\t1;/^#define LIMIT /;\"\td\tfile:\n"
     "a\tfld.c\t3;/^struct pair { int a; };$/;\"\tm\tstruct:pair\ttyperef:typename:int\tfile:\n"
     "count\tfld.c\t2;/^static int count;$/;\"\tv\ttyperef:typename:int\tfile:\n"
     "pair\tfld.c\t3;/^struct pair { int a; };$/;\"\ts\tfile:\n",
     ""},
    {SHARED, "OUT-7 a pattern length limit", "lim.c", "--pattern-length-limit=12",
     "int value_with_a_long_name = 1;\n"
     "int long_line[] = {0000000000, 1111111111, 2222222222, 3333333333, 4444444444, 5555555555, 6666666666, 7777777777};\n",
     "long_line\tlim.c\t/^int long_lin/;\"\tv\tline:2\ttyperef:typename:int[]\n"
     "value_with_a_long_name\tlim.c\t/^int value_wi/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "OUT-7 no pattern length limit", "lim.c", "--pattern-length-limit=0",
     "int value_with_a_long_name = 1;\n"
     "int long_line[] = {0000000000, 1111111111, 2222222222, 3333333333, 4444444444, 5555555555, 6666666666, 7777777777};\n",
     "long_line\tlim.c\t/^int long_line[] = {0000000000, 1111111111, 2222222222, 3333333333, 4444444444, 5555555555, 6666666666, 7777777777};$/;\"\tv\tline:2\ttyperef:typename:int[]\n"
     "value_with_a_long_name\tlim.c\t/^int value_with_a_long_name = 1;$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "OUT-8 the original format", "fld.c", "--format=1",
     "#define LIMIT 10\n"
     "static int count;\n"
     "struct pair { int a; };\n",
     "LIMIT\tfld.c\t/^#define LIMIT /\n"
     "a\tfld.c\t/^struct pair { int a; };$/\n"
     "count\tfld.c\t/^static int count;$/\n"
     "pair\tfld.c\t/^struct pair { int a; };$/\n",
     ""},
    {SHARED, "FLD-6 e-ctags", "z.c", "--fields=+zZ-n --output-format=e-ctags",
     "struct s { int m; };\n",
     "m\tz.c\t/^struct s { int m; };$/;\"\tkind:m\tscope:struct:s\ttyperef:typename:int\tfile:\n"
     "s\tz.c\t/^struct s { int m; };$/;\"\tkind:s\tfile:\n",
     ""},
    {SHARED, "FLD-7 FLD-8 FLD-10 access and signatures in C", "s.c",
     "--fields=+aS --kinds-C=+p",
     "#define MAX(a, b) ((a) > (b) ? (a) : (b))\n"
     "#define LOG(fmt, args...) printf(fmt, args)\n"
     "#define TRACE(...) 0\n"
     "#define NONE( ) 0\n"
     "#define OBJ 1\n"
     "struct point { int x, y; };\n"
     "enum color { RED, GREEN };\n"
     "int area(struct point *p, int scale);\n"
     "static int knr(a, b) int a; char *b; { return a; }\n"
     "int main(int argc, char **argv) { int n; return 0; }\n"
     "#define CUT(a, b\n",
     "CUT\ts.c\t/^#define CUT(/;\"\td\tline:11\tfile:\n"
     "GREEN\ts.c\t/^enum color { RED, GREEN };$/;\"\te\tline:7\tenum:color\tfile:\taccess:public\n"
     "LOG\ts.c\t/^#define LOG(/;\"\td\tline:2\tfile:\tsignature:(fmt,args...)\n"
     "MAX\ts.c\t/^#define MAX(/;\"\td\tline:1\tfile:\tsignature:(a,b)\n"
     "NONE\ts.c\t/^#define NONE(/;\"\td\tline:4\tfile:\tsignature:()\n"
     "OBJ\ts.c\t/^#define OBJ /;\"\td\tline:5\tfile:\n"
     "RED\ts.c\t/^enum color { RED, GREEN };$/;\"\te\tline:7\tenum:color\tfile:\taccess:public\n"
     "TRACE\ts.c\t/^#define TRACE(/;\"\td\tline:3\tfile:\tsignature:(...)\n"
     "area\ts.c\t/^int area(struct point *p, int scale);$/;\"\tp\tline:8\ttyperef:typename:int\tfile:\tsignature:(struct point * p,int scale)\n"
     "color\ts.c\t/^enum color { RED, GREEN };$/;\"\tg\tline:7\tfile:\n"
     "knr\ts.c\t/^static int knr(a, b) int a; char *b; { return a; }$/;\"\tf\tline:9\tfile:\tsignature:(a,b)\n"
     "main\ts.c\t/^int main(int argc, char **argv) { int n; return 0; }$/;\"\tf\tline:10\ttyperef:typename:int\tsignature:(int argc,char ** argv)\n"
     "point\ts.c\t/^struct point { int x, y; };$/;\"\ts\tline:6\tfile:\n"
     "x\ts.c\t/^struct point { int x, y; };$/;\"\tm\tline:6\tstruct:point\ttyperef:typename:int\tfile:\taccess:public\n"
     "y\ts.c\t/^struct point { int x, y; };$/;\"\tm\tline:6\tstruct:point\ttyperef:typename:int\tfile:\taccess:public\n",
     ""},
    {OURS, "FLD-5 end and nth write nothing here", "z.c", "--fields=+eo",
     "struct s { int m; };\n",
     "m\tz.c\t/^struct s { int m; };$/;\"\tm\tline:1\tstruct:s\ttyperef:typename:int\tfile:\n"
     "s\tz.c\t/^struct s { int m; };$/;\"\ts\tline:1\tfile:\n",
     ""},

    // The inputFile extra (EX-7), without the epoch, which varies
    {SHARED, "EX-7 a file tag", "a.c", "--extras=+f --fields=-T",
     "int a;\n"
     "static int b;\n",
     "a\ta.c\t/^int a;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "a.c\ta.c\t1;\"\tF\tline:1\n"
     "b\ta.c\t/^static int b;$/;\"\tv\tline:2\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "EX-7 an empty file's tag", "e.c", "--extras=+f --fields=-T",
     "",
     "e.c\te.c\t1;\"\tF\tline:1\n",
     ""},
    {SHARED, "EX-7 the file tag last", "r.c", "--extras=+f --fields=-T --sort=no",
     "#ifdef A\n"
     "int f(void) {\n"
     "#else\n"
     "int f(int x) {\n"
     "#endif\n"
     "    return x;\n"
     "}\n"
     "}\n"
     "int g;\n",
     "f\tr.c\t/^int f(void) {$/;\"\tf\tline:2\ttyperef:typename:int\n"
     "g\tr.c\t/^int g;$/;\"\tv\tline:9\ttyperef:typename:int\n"
     "r.c\tr.c\t1;\"\tF\tline:1\n",
     ""},
    {SHARED, "EX-7 a header's file tag fields", "h.h",
     "--extras=+fq --fields=+KzlErSZ-T --kinds-C=+p",
     "struct s { int m; };\n"
     "void f(int);\n"
     "struct { int n; } v;\n",
     "__anon0b8873a30108\th.h\t/^struct { int n; } v;$/;\"\tkind:struct\tline:3\tlanguage:C++\troles:def\textras:anonymous\n"
     "__anon0b8873a30108::n\th.h\t/^struct { int n; } v;$/;\"\tkind:member\tline:3\tlanguage:C++\tscope:struct:__anon0b8873a30108\ttyperef:typename:int\troles:def\textras:qualified\n"
     "f\th.h\t/^void f(int);$/;\"\tkind:prototype\tline:2\tlanguage:C++\ttyperef:typename:void\tsignature:(int)\troles:def\n"
     "h.h\th.h\t1;\"\tkind:file\tline:1\tlanguage:C++\troles:def\textras:inputFile\n"
     "m\th.h\t/^struct s { int m; };$/;\"\tkind:member\tline:1\tlanguage:C++\tscope:struct:s\ttyperef:typename:int\troles:def\n"
     "n\th.h\t/^struct { int n; } v;$/;\"\tkind:member\tline:3\tlanguage:C++\tscope:struct:__anon0b8873a30108\ttyperef:typename:int\troles:def\n"
     "s\th.h\t/^struct s { int m; };$/;\"\tkind:struct\tline:1\tlanguage:C++\troles:def\n"
     "s::m\th.h\t/^struct s { int m; };$/;\"\tkind:member\tline:1\tlanguage:C++\tscope:struct:s\ttyperef:typename:int\troles:def\textras:qualified\n"
     "v\th.h\t/^struct { int n; } v;$/;\"\tkind:variable\tline:3\tlanguage:C++\ttyperef:struct:__anon0b8873a30108\troles:def\n",
     ""},
    {SHARED, "EX-7 other extras leave the file tag", "h.h",
     "--extras=+f-F-{anonymous} --fields=-T",
     "struct s { int m; };\n"
     "struct { int n; } v;\n",
     "h.h\th.h\t1;\"\tF\tline:1\n"
     "m\th.h\t/^struct s { int m; };$/;\"\tm\tline:1\tstruct:s\ttyperef:typename:int\n"
     "n\th.h\t/^struct { int n; } v;$/;\"\tm\tline:2\tstruct:__anon0b8873a30108\ttyperef:typename:int\n"
     "s\th.h\t/^struct s { int m; };$/;\"\ts\tline:1\n"
     "v\th.h\t/^struct { int n; } v;$/;\"\tv\tline:2\ttyperef:struct:__anon0b8873a30108\n",
     ""},
    {SHARED, "EX-7 a file tag's address in pattern mode", "a.c",
     "--extras=+f --fields=-T --excmd=pattern",
     "int a;\n",
     "a\ta.c\t/^int a;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "a.c\ta.c\t1;\"\tF\tline:1\n",
     ""},
    {SHARED, "EX-7 a file tag's address in combine mode", "a.c",
     "--extras=+f --fields=-T --excmd=combine",
     "int a;\n",
     "a\ta.c\t1;/^int a;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "a.c\ta.c\t1;\"\tF\tline:1\n",
     ""},
    {SHARED, "EX-7 a file tag in the original format", "a.c",
     "--extras=+f --format=1",
     "int a;\n",
     "a\ta.c\t/^int a;$/\n"
     "a.c\ta.c\t1\n",
     ""},
    {SHARED, "EX-7 a file tag in e-ctags", "a.c",
     "--extras=+f --fields=-T --output-format=e-ctags",
     "int a;\n",
     "a\ta.c\t/^int a;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "a.c\ta.c\t1;\"\tF\tline:1\n",
     ""},
    {SHARED, "EX-7 the file kind disabled", "a.c", "--extras=+f --kinds-C=-F",
     "int a;\n",
     "a\ta.c\t/^int a;$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "EX-7 the file kind alone", "a.c",
     "--kinds-C= --extras=f --fields=n",
     "int a;\n",
     "a.c\ta.c\t1;\"\tline:1\n",
     ""},
    {SHARED, "EX-7 a file tag's name escaped", "!x.c", "--extras=+f --fields=-T",
     "int z;\n",
     "\\x21x.c\t!x.c\t1;\"\tF\tline:1\n"
     "z\t!x.c\t/^int z;$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},

    // Emacs tags (OPT-21): the raw line, a CR before its LF dropped, a
    // macro's cut after its name, offsets in bytes, in the parser's order
    {SHARED, "OPT-21 etags entries", "e.c", "-e",
     "#define LIMIT 10\r\n"
     "int x;\r\r\n"
     "int a; /* \r */ int b;\n"
     "#define M(a) \\\n"
     "  a\n"
     "#define E\n"
     "int last;",
     "\f\ne.c,153\n"
     "#define LIMIT \x7fLIMIT\x01" "1,0\n"
     "int x;\r\x7fx\x01" "2,18\n"
     "int a; /* \r */ int b;\x7f" "a\x01" "3,27\n"
     "int a; /* \r */ int b;\x7f" "b\x01" "3,27\n"
     "#define M(\x7fM\x01" "4,49\n"
     "#define E\x7f" "E\x01" "6,68\n"
     "int last;\x7flast\x01" "7,78\n",
     ""},
    {SHARED, "OPT-21 etags cut to the limit, not within a character", "e.c",
     "-e --pattern-length-limit=12",
     "int abc; /*\xc3\xa9\xc3\xa9 */\n"
     "int d; /*  \xf0\x9f\x98\x80 */\n"
     "int e; /*  \x80\x80\x80\x80\x80 */\n"
     "int f;\n",
     "\f\ne.c,82\n"
     "int abc; /*\xc3\xa9\x7f" "abc\x01" "1,0\n"
     "int d; /*  \xf0\x9f\x98\x80\x7f" "d\x01" "2,19\n"
     "int e; /*  \x80\x80\x80\x80\x7f" "e\x01" "3,38\n"
     "int f;\x7f" "f\x01" "4,58\n",
     ""},
    {SHARED, "OPT-21 etags offsets count the BOM, file tags", "e.c",
     "-e --extras=+f --pattern-length-limit=0",
     "\xef\xbb\xbfint a;\n"
     "int b;\n",
     "\f\ne.c,36\n"
     "int a;\x7f" "a\x01" "1,3\n"
     "int b;\x7f" "b\x01" "2,10\n"
     "\x7f" "e.c\x01" "1,0\n",
     ""},
    {IMPROVED, "X4 a NUL in an etags entry's text", "n.c", "-e",
     "int a; /* \x00 */\n"
     "int b;\n",
     "\f\nn.c,35\n"
     "int a; /* \x00 */\x7f" "a\x01" "1,0\n"
     "int b;\x7f" "b\x01" "2,15\n",
     "\f\nn.c,23\n"
     "int a; /* int b;\x7f" "a\x01" "1,0\n"},
    {SHARED, "OPT-21 etags of no tags, whatever the fields", "e.c",
     "--fields=+lK --format=1 --excmd=number --extras=+p --output-format=etags",
     "#include <stdio.h>\n",
     "\f\ne.c,0\n",
     ""},
};

static TagCase cxxcases[] = {
    {SHARED, "class members", "c.cpp", "--kinds-C++=+p",
     "namespace geo {\n"
     "class Point {\n"
     "public:\n"
     "    Point(int x, int y);\n"
     "    int x() const { return x_; }\n"
     "private:\n"
     "    int x_, y_;\n"
     "};\n"
     "}\n",
     "Point\tc.cpp\t/^    Point(int x, int y);$/;\"\tp\tline:4\tclass:geo::Point\tfile:\n"
     "Point\tc.cpp\t/^class Point {$/;\"\tc\tline:2\tnamespace:geo\tfile:\n"
     "geo\tc.cpp\t/^namespace geo {$/;\"\tn\tline:1\tfile:\n"
     "x\tc.cpp\t/^    int x() const { return x_; }$/;\"\tf\tline:5\tclass:geo::Point\ttyperef:typename:int\tfile:\n"
     "x_\tc.cpp\t/^    int x_, y_;$/;\"\tm\tline:7\tclass:geo::Point\ttyperef:typename:int\tfile:\n"
     "y_\tc.cpp\t/^    int x_, y_;$/;\"\tm\tline:7\tclass:geo::Point\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "qualified definitions", "q.cpp", "",
     "geo::Point::Point(int x, int y) : x_(x), y_(y) {}\n"
     "int geo::Point::area() const { return 0; }\n"
     "int geo::counter = 0;\n",
     "Point\tq.cpp\t/^geo::Point::Point(int x, int y) : x_(x), y_(y) {}$/;\"\tf\tline:1\tclass:geo::Point\n"
     "area\tq.cpp\t/^int geo::Point::area() const { return 0; }$/;\"\tf\tline:2\tclass:geo::Point\ttyperef:typename:int\n"
     "counter\tq.cpp\t/^int geo::counter = 0;$/;\"\tm\tline:3\tclass:geo\ttyperef:typename:int\n",
     ""},
    {SHARED, "templates", "t.cpp", "--kinds-C++=+Z",
     "template<typename T, int N>\n"
     "struct Array {\n"
     "    T data[N];\n"
     "    T &at(int i) { return data[i]; }\n"
     "};\n"
     "template<class T> T max(T a, T b) { return a < b ? b : a; }\n",
     "Array\tt.cpp\t/^struct Array {$/;\"\ts\tline:2\tfile:\n"
     "N\tt.cpp\t/^template<typename T, int N>$/;\"\tZ\tline:1\tstruct:Array\ttyperef:typename:int\n"
     "T\tt.cpp\t/^template<class T> T max(T a, T b) { return a < b ? b : a; }$/;\"\tZ\tline:6\tfunction:max\ttyperef:meta:class\n"
     "T\tt.cpp\t/^template<typename T, int N>$/;\"\tZ\tline:1\tstruct:Array\ttyperef:meta:typename\n"
     "at\tt.cpp\t/^    T &at(int i) { return data[i]; }$/;\"\tf\tline:4\tstruct:Array\ttyperef:typename:T &\tfile:\n"
     "data\tt.cpp\t/^    T data[N];$/;\"\tm\tline:3\tstruct:Array\ttyperef:typename:T[]\tfile:\n"
     "max\tt.cpp\t/^template<class T> T max(T a, T b) { return a < b ? b : a; }$/;\"\tf\tline:6\ttyperef:typename:T\n",
     ""},
    {SHARED, "lambdas", "l.cpp", "",
     "auto twice = [](int a) { return 2*a; };\n"
     "void run() {\n"
     "    auto inner = [&](int b) -> int { return b; };\n"
     "}\n",
     "__anon0fbc44020102\tl.cpp\t/^auto twice = [](int a) { return 2*a; };$/;\"\tf\tline:1\tfile:\n"
     "__anon0fbc44020202\tl.cpp\t/^    auto inner = [&](int b) -> int { return b; };$/;\"\tf\tline:3\tfunction:run\ttyperef:typename:int\tfile:\n"
     "run\tl.cpp\t/^void run() {$/;\"\tf\tline:2\ttyperef:typename:void\n"
     "twice\tl.cpp\t/^auto twice = [](int a) { return 2*a; };$/;\"\tv\tline:1\ttyperef:typename:auto\n",
     ""},
    {SHARED, "operators", "o.cpp", "--kinds-C++=+p",
     "struct V {\n"
     "    V operator+(const V &o) const;\n"
     "    bool operator==(const V &) const;\n"
     "    int operator()(int) { return 0; }\n"
     "    explicit operator bool() const;\n"
     "};\n",
     "V\to.cpp\t/^struct V {$/;\"\ts\tline:1\tfile:\n"
     "operator ()\to.cpp\t/^    int operator()(int) { return 0; }$/;\"\tf\tline:4\tstruct:V\ttyperef:typename:int\tfile:\n"
     "operator +\to.cpp\t/^    V operator+(const V &o) const;$/;\"\tp\tline:2\tstruct:V\ttyperef:typename:V\tfile:\n"
     "operator ==\to.cpp\t/^    bool operator==(const V &) const;$/;\"\tp\tline:3\tstruct:V\ttyperef:typename:bool\tfile:\n"
     "operator bool\to.cpp\t/^    explicit operator bool() const;$/;\"\tp\tline:5\tstruct:V\tfile:\n",
     ""},
    {SHARED, "using and aliases", "u.cpp", "--kinds-C++=+ANU",
     "using Int = int;\n"
     "namespace a { namespace b { int v; } }\n"
     "namespace c = a::b;\n"
     "using namespace a;\n"
     "using a::b::v;\n",
     "Int\tu.cpp\t/^using Int = int;$/;\"\tt\tline:1\ttyperef:typename:int\tfile:\n"
     "a\tu.cpp\t/^namespace a { namespace b { int v; } }$/;\"\tn\tline:2\tfile:\n"
     "a\tu.cpp\t/^using namespace a;$/;\"\tU\tline:4\tfile:\n"
     "b\tu.cpp\t/^namespace a { namespace b { int v; } }$/;\"\tn\tline:2\tnamespace:a\tfile:\n"
     "c\tu.cpp\t/^namespace c = a::b;$/;\"\tA\tline:3\tfile:\tname:a::b\n"
     "v\tu.cpp\t/^namespace a { namespace b { int v; } }$/;\"\tv\tline:2\tnamespace:a::b\ttyperef:typename:int\n"
     "v\tu.cpp\t/^using a::b::v;$/;\"\tN\tline:5\tfile:\n",
     ""},
    {SHARED, "enums", "e.cpp", "",
     "enum class Mode : unsigned char { Off, On };\n"
     "enum { Anonymous };\n"
     "typedef enum { X, Y } Axis;\n",
     "Anonymous\te.cpp\t/^enum { Anonymous };$/;\"\te\tline:2\tenum:__anon0f3d987b0103\tfile:\n"
     "Axis\te.cpp\t/^typedef enum { X, Y } Axis;$/;\"\tt\tline:3\ttyperef:enum:__anon0f3d987b0203\tfile:\n"
     "Mode\te.cpp\t/^enum class Mode : unsigned char { Off, On };$/;\"\tg\tline:1\ttyperef:typename:unsigned char\tfile:\n"
     "Off\te.cpp\t/^enum class Mode : unsigned char { Off, On };$/;\"\te\tline:1\tenum:Mode\tfile:\n"
     "On\te.cpp\t/^enum class Mode : unsigned char { Off, On };$/;\"\te\tline:1\tenum:Mode\tfile:\n"
     "X\te.cpp\t/^typedef enum { X, Y } Axis;$/;\"\te\tline:3\tenum:__anon0f3d987b0203\tfile:\n"
     "Y\te.cpp\t/^typedef enum { X, Y } Axis;$/;\"\te\tline:3\tenum:__anon0f3d987b0203\tfile:\n"
     "__anon0f3d987b0103\te.cpp\t/^enum { Anonymous };$/;\"\tg\tline:2\tfile:\n"
     "__anon0f3d987b0203\te.cpp\t/^typedef enum { X, Y } Axis;$/;\"\tg\tline:3\tfile:\n",
     ""},
    {IMPROVED, "anonymous namespace", "n.cpp", "",
     "namespace {\n"
     "int hidden;\n"
     "void helper() {}\n"
     "}\n",
     "__anon0fe075040111\tn.cpp\t/^namespace {$/;\"\tn\tline:1\tfile:\n"
     "helper\tn.cpp\t/^void helper() {}$/;\"\tf\tline:3\tnamespace:__anon0fe075040111\ttyperef:typename:void\tfile:\n"
     "hidden\tn.cpp\t/^int hidden;$/;\"\tv\tline:2\tnamespace:__anon0fe075040111\ttyperef:typename:int\tfile:\n",
     "__anon0fe075040111\tn.cpp\t/^namespace {$/;\"\tn\tline:1\tfile:\n"
     "helper\tn.cpp\t/^void helper() {}$/;\"\tf\tline:3\tnamespace:__anon0fe075040111\ttyperef:typename:void\n"
     "hidden\tn.cpp\t/^int hidden;$/;\"\tv\tline:2\tnamespace:__anon0fe075040111\ttyperef:typename:int\n"},
    {SHARED, "header access", "h.hpp", "",
     "struct S {\n"
     "    int a;\n"
     "    static int count;\n"
     "    void method();\n"
     "};\n"
     "inline int twice(int x) { return 2*x; }\n",
     "S\th.hpp\t/^struct S {$/;\"\ts\tline:1\n"
     "a\th.hpp\t/^    int a;$/;\"\tm\tline:2\tstruct:S\ttyperef:typename:int\n"
     "count\th.hpp\t/^    static int count;$/;\"\tm\tline:3\tstruct:S\ttyperef:typename:int\n"
     "twice\th.hpp\t/^inline int twice(int x) { return 2*x; }$/;\"\tf\tline:6\ttyperef:typename:int\n",
     ""},
    {SHARED, "modules", "m.cpp", "",
     "module;\n"
     "export module m:part;\n"
     "export int f();\n",
     "__gmod_0fce5c830116\tm.cpp\t/^module;$/;\"\tM\tline:1\n"
     "part\tm.cpp\t/^export module m:part;$/;\"\tP\tline:2\tmodule:m\n",
     ""},
    {SHARED, "function try block", "y.cpp", "--kinds-C++=+l",
     "int f() try { int a = 1; return a; } catch (int e) { int b = e; return b; }\n",
     "a\ty.cpp\t/^int f() try { int a = 1; return a; } catch (int e) { int b = e; return b; }$/;\"\tl\tline:1\tfunction:f\ttyperef:typename:int\tfile:\n"
     "b\ty.cpp\t/^int f() try { int a = 1; return a; } catch (int e) { int b = e; return b; }$/;\"\tl\tline:1\tfunction:f\ttyperef:typename:int\tfile:\n"
     "e\ty.cpp\t/^int f() try { int a = 1; return a; } catch (int e) { int b = e; return b; }$/;\"\tl\tline:1\tfunction:f\ttyperef:typename:int\tfile:\n"
     "f\ty.cpp\t/^int f() try { int a = 1; return a; } catch (int e) { int b = e; return b; }$/;\"\tf\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "friends", "fr.cpp", "",
     "class K {\n"
     "    friend void touch(K &k) {}\n"
     "    friend class Other;\n"
     "};\n",
     "K\tfr.cpp\t/^class K {$/;\"\tc\tline:1\tfile:\n"
     "touch\tfr.cpp\t/^    friend void touch(K &k) {}$/;\"\tf\tline:2\ttyperef:typename:void\tfile:\n",
     ""},
    {IMPROVED, "pragma operators", "p.cpp", "",
     "__pragma(warning(push))\n"
     "static __inline void f(unsigned id)\n"
     "__pragma(warning(pop))\n"
     "{}\n"
     "_Pragma(\"once\") int g;\n",
     "f\tp.cpp\t/^static __inline void f(unsigned id)$/;\"\tf\tline:2\ttyperef:typename:void\tfile:\n"
     "g\tp.cpp\t/^_Pragma(\"once\") int g;$/;\"\tv\tline:5\ttyperef:typename:int\n",
     "__pragma\tp.cpp\t/^__pragma(warning(push))$/;\"\tf\tline:1\tfile:\n"},
    {IMPROVED, "u8 character literals", "u.cpp", "",
     "char a = u8'x';\n"
     "struct After { int b; };\n",
     "After\tu.cpp\t/^struct After { int b; };$/;\"\ts\tline:2\tfile:\n"
     "a\tu.cpp\t/^char a = u8'x';$/;\"\tv\tline:1\ttyperef:typename:char\n"
     "b\tu.cpp\t/^struct After { int b; };$/;\"\tm\tline:2\tstruct:After\ttyperef:typename:int\tfile:\n",
     "After\tu.cpp\t/^struct After { int b; };$/;\"\ts\tline:2\tfile:\n"
     "b\tu.cpp\t/^struct After { int b; };$/;\"\tm\tline:2\tstruct:After\ttyperef:typename:int\tfile:\n"},
    {IMPROVED, "unterminated strings", "s.c", "",
     "#define BAD \"open\n"
     "int after;\n",
     "BAD\ts.c\t/^#define BAD /;\"\td\tline:1\tfile:\n"
     "after\ts.c\t/^int after;$/;\"\tv\tline:2\ttyperef:typename:int\n",
     "BAD\ts.c\t/^#define BAD /;\"\td\tline:1\tfile:\n"},
    {IMPROVED, "identifiers beyond ASCII", "i.cpp", "",
     "int café = 1;\n"
     "int λ(int x) { return x; }\n",
     "café\ti.cpp\t/^int café = 1;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "λ\ti.cpp\t/^int λ(int x) { return x; }$/;\"\tf\tline:2\ttyperef:typename:int\n",
     ""},
    {IMPROVED, "header templates", "config.h.in", "",
     "#define VERSION \"@VERSION@\"\n"
     "struct Config { int level; };\n",
     "Config\tconfig.h.in\t/^struct Config { int level; };$/;\"\ts\tline:2\n"
     "VERSION\tconfig.h.in\t/^#define VERSION \"@VERSION@/;\"\td\tline:1\n"
     "level\tconfig.h.in\t/^struct Config { int level; };$/;\"\tm\tline:2\tstruct:Config\ttyperef:typename:int\n",
     "Config\tconfig.h.in\t/^struct Config { int level; };$/;\"\ts\tline:2\tfile:\n"
     "VERSION\tconfig.h.in\t/^#define VERSION \"@VERSION@/;\"\td\tline:1\tfile:\n"
     "level\tconfig.h.in\t/^struct Config { int level; };$/;\"\tm\tline:2\tstruct:Config\ttyperef:typename:int\tfile:\n"},
    {IMPROVED, "upper-case C extension", "k.C", "",
     "class Thing { int v; };\n"
     "int Thing::get() { return v; }\n",
     "Thing\tk.C\t/^class Thing { int v; };$/;\"\tc\tline:1\tfile:\n"
     "get\tk.C\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\tclass:Thing\ttyperef:typename:int\n"
     "v\tk.C\t/^class Thing { int v; };$/;\"\tm\tline:1\tclass:Thing\ttyperef:typename:int\tfile:\n",
     "get\tk.C\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\ttyperef:typename:int Thing::\n"
     "v\tk.C\t/^class Thing { int v; };$/;\"\tv\tline:1\ttyperef:typename:int\n"},
    {IMPROVED, "P4 macro as an access label", "p4.cpp", "",
     "class Json {\n"
     "JSON_PRIVATE_UNLESS_TESTED:\n"
     "    int depth;\n"
     "    void dump() {}\n"
     "};\n",
     "Json\tp4.cpp\t/^class Json {$/;\"\tc\tline:1\tfile:\n"
     "depth\tp4.cpp\t/^    int depth;$/;\"\tm\tline:3\tclass:Json\ttyperef:typename:int\tfile:\n"
     "dump\tp4.cpp\t/^    void dump() {}$/;\"\tf\tline:4\tclass:Json\ttyperef:typename:void\tfile:\n",
     "Json\tp4.cpp\t/^class Json {$/;\"\tc\tline:1\tfile:\n"
     "dump\tp4.cpp\t/^    void dump() {}$/;\"\tf\tline:4\tclass:Json\ttyperef:typename:void\tfile:\n"},
    {IMPROVED, "P5 enum types in declarators", "p5.cpp", "",
     "enum color { RED, GREEN };\n"
     "enum color palette[] = { RED, GREEN };\n"
     "struct S {\n"
     "    std::vector<enum color> colors = { RED };\n"
     "};\n",
     "GREEN\tp5.cpp\t/^enum color { RED, GREEN };$/;\"\te\tline:1\tenum:color\tfile:\n"
     "RED\tp5.cpp\t/^enum color { RED, GREEN };$/;\"\te\tline:1\tenum:color\tfile:\n"
     "S\tp5.cpp\t/^struct S {$/;\"\ts\tline:3\tfile:\n"
     "color\tp5.cpp\t/^enum color { RED, GREEN };$/;\"\tg\tline:1\tfile:\n"
     "colors\tp5.cpp\t/^    std::vector<enum color> colors = { RED };$/;\"\tm\tline:4\tstruct:S\ttyperef:typename:std::vector<enum color>\tfile:\n"
     "palette\tp5.cpp\t/^enum color palette[] = { RED, GREEN };$/;\"\tv\tline:2\ttyperef:enum:color[]\n",
     "GREEN\tp5.cpp\t/^enum color palette[] = { RED, GREEN };$/;\"\te\tline:2\tenum:palette\tfile:\n"
     "GREEN\tp5.cpp\t/^enum color { RED, GREEN };$/;\"\te\tline:1\tenum:color\tfile:\n"
     "RED\tp5.cpp\t/^    std::vector<enum color> colors = { RED };$/;\"\te\tline:4\tenum:S::colors\tfile:\n"
     "RED\tp5.cpp\t/^enum color palette[] = { RED, GREEN };$/;\"\te\tline:2\tenum:palette\tfile:\n"
     "RED\tp5.cpp\t/^enum color { RED, GREEN };$/;\"\te\tline:1\tenum:color\tfile:\n"
     "S\tp5.cpp\t/^struct S {$/;\"\ts\tline:3\tfile:\n"
     "color\tp5.cpp\t/^enum color { RED, GREEN };$/;\"\tg\tline:1\tfile:\n"
     "colors\tp5.cpp\t/^    std::vector<enum color> colors = { RED };$/;\"\tg\tline:4\tstruct:S\tfile:\n"
     "palette\tp5.cpp\t/^enum color palette[] = { RED, GREEN };$/;\"\tg\tline:2\tfile:\n"},
    {IMPROVED, "P6 explicit instantiations", "p6.cpp", "",
     "template<typename T> struct Box { T v; };\n"
     "template struct Box<int>;\n"
     "extern template struct Box<long>;\n"
     "template<typename T> class EXPORT Later;\n",
     "Box\tp6.cpp\t/^template<typename T> struct Box { T v; };$/;\"\ts\tline:1\tfile:\n"
     "v\tp6.cpp\t/^template<typename T> struct Box { T v; };$/;\"\tm\tline:1\tstruct:Box\ttyperef:typename:T\tfile:\n",
     "Box\tp6.cpp\t/^template struct Box<int>;$/;\"\tv\tline:2\ttyperef:meta:struct\n"
     "Box\tp6.cpp\t/^template<typename T> struct Box { T v; };$/;\"\ts\tline:1\tfile:\n"
     "Later\tp6.cpp\t/^template<typename T> class EXPORT Later;$/;\"\tv\tline:4\ttyperef:class:EXPORT\n"
     "v\tp6.cpp\t/^template<typename T> struct Box { T v; };$/;\"\tm\tline:1\tstruct:Box\ttyperef:typename:T\tfile:\n"},
    {IMPROVED, "P7 macro after struct", "p7.cpp", "",
     "struct ALIGNAS(16) Vec { float v[4]; };\n"
     "class CONSUMABLE(unconsumed) Handle { int fd; };\n"
     "struct PACKED(1) { char c; } loose;\n"
     "typedef struct Vec (*maker)(int);\n",
     "Handle\tp7.cpp\t/^class CONSUMABLE(unconsumed) Handle { int fd; };$/;\"\tc\tline:2\tfile:\n"
     "Vec\tp7.cpp\t/^struct ALIGNAS(16) Vec { float v[4]; };$/;\"\ts\tline:1\tfile:\n"
     "__anon111efc5d0108\tp7.cpp\t/^struct PACKED(1) { char c; } loose;$/;\"\ts\tline:3\tfile:\n"
     "c\tp7.cpp\t/^struct PACKED(1) { char c; } loose;$/;\"\tm\tline:3\tstruct:__anon111efc5d0108\ttyperef:typename:char\tfile:\n"
     "fd\tp7.cpp\t/^class CONSUMABLE(unconsumed) Handle { int fd; };$/;\"\tm\tline:2\tclass:Handle\ttyperef:typename:int\tfile:\n"
     "loose\tp7.cpp\t/^struct PACKED(1) { char c; } loose;$/;\"\tv\tline:3\ttyperef:struct:__anon111efc5d0108\n"
     "maker\tp7.cpp\t/^typedef struct Vec (*maker)(int);$/;\"\tt\tline:4\ttyperef:struct:Vec (*)(int)\tfile:\n"
     "v\tp7.cpp\t/^struct ALIGNAS(16) Vec { float v[4]; };$/;\"\tm\tline:1\tstruct:Vec\ttyperef:typename:float[4]\tfile:\n",
     "CONSUMABLE\tp7.cpp\t/^class CONSUMABLE(unconsumed) Handle { int fd; };$/;\"\tf\tline:2\ttyperef:meta:class\n"
     "PACKED\tp7.cpp\t/^struct PACKED(1) { char c; } loose;$/;\"\ts\tline:3\tfile:\n"
     "Vec\tp7.cpp\t/^struct ALIGNAS(16) Vec { float v[4]; };$/;\"\ts\tline:1\tfile:\n"
     "c\tp7.cpp\t/^struct PACKED(1) { char c; } loose;$/;\"\tm\tline:3\tstruct:PACKED\ttyperef:typename:char\tfile:\n"
     "loose\tp7.cpp\t/^struct PACKED(1) { char c; } loose;$/;\"\tv\tline:3\ttyperef:struct:PACKED\n"
     "maker\tp7.cpp\t/^typedef struct Vec (*maker)(int);$/;\"\tt\tline:4\ttyperef:struct:Vec (*)(int)\tfile:\n"
     "v\tp7.cpp\t/^struct ALIGNAS(16) Vec { float v[4]; };$/;\"\tm\tline:1\tstruct:Vec\ttyperef:typename:float[4]\tfile:\n"},
    {IMPROVED, "P2 constructor arguments with calls", "p2.cpp", "",
     "static cl::opt<bool> PrintStats(\"print-stats\", cl::init(false), cl::Hidden);\n"
     "static std::string Name(\"x\", make(1));\n",
     "Name\tp2.cpp\t/^static std::string Name(\"x\", make(1));$/;\"\tv\tline:2\ttyperef:typename:std::string\tfile:\n"
     "PrintStats\tp2.cpp\t/^static cl::opt<bool> PrintStats(\"print-stats\", cl::init(false), cl::Hidden);$/;\"\tv\tline:1\ttyperef:typename:cl::opt<bool>\tfile:\n",
     "Name\tp2.cpp\t/^static std::string Name(\"x\", make(1));$/;\"\tv\tline:2\ttyperef:typename:std::string\tfile:\n"},
    {IMPROVED, "P10 anonymous namespace", "p10.cpp", "",
     "namespace {\n"
     "int counter;\n"
     "void helper() {}\n"
     "struct Local { static int n; };\n"
     "}\n"
     "int Local::n = 0;\n"
     "namespace outer { namespace { int inner; } }\n",
     "Local\tp10.cpp\t/^struct Local { static int n; };$/;\"\ts\tline:4\tnamespace:__anon27067e470111\tfile:\n"
     "__anon27067e470111\tp10.cpp\t/^namespace {$/;\"\tn\tline:1\tfile:\n"
     "__anon27067e470211\tp10.cpp\t/^namespace outer { namespace { int inner; } }$/;\"\tn\tline:7\tnamespace:outer\tfile:\n"
     "counter\tp10.cpp\t/^int counter;$/;\"\tv\tline:2\tnamespace:__anon27067e470111\ttyperef:typename:int\tfile:\n"
     "helper\tp10.cpp\t/^void helper() {}$/;\"\tf\tline:3\tnamespace:__anon27067e470111\ttyperef:typename:void\tfile:\n"
     "inner\tp10.cpp\t/^namespace outer { namespace { int inner; } }$/;\"\tv\tline:7\tnamespace:outer::__anon27067e470211\ttyperef:typename:int\tfile:\n"
     "n\tp10.cpp\t/^int Local::n = 0;$/;\"\tm\tline:6\tclass:Local\ttyperef:typename:int\n"
     "n\tp10.cpp\t/^struct Local { static int n; };$/;\"\tm\tline:4\tstruct:__anon27067e470111::Local\ttyperef:typename:int\tfile:\n"
     "outer\tp10.cpp\t/^namespace outer { namespace { int inner; } }$/;\"\tn\tline:7\tfile:\n",
     "Local\tp10.cpp\t/^struct Local { static int n; };$/;\"\ts\tline:4\tnamespace:__anon27067e470111\tfile:\n"
     "__anon27067e470111\tp10.cpp\t/^namespace {$/;\"\tn\tline:1\tfile:\n"
     "__anon27067e470211\tp10.cpp\t/^namespace outer { namespace { int inner; } }$/;\"\tn\tline:7\tnamespace:outer\tfile:\n"
     "counter\tp10.cpp\t/^int counter;$/;\"\tv\tline:2\tnamespace:__anon27067e470111\ttyperef:typename:int\n"
     "helper\tp10.cpp\t/^void helper() {}$/;\"\tf\tline:3\tnamespace:__anon27067e470111\ttyperef:typename:void\n"
     "inner\tp10.cpp\t/^namespace outer { namespace { int inner; } }$/;\"\tv\tline:7\tnamespace:outer::__anon27067e470211\ttyperef:typename:int\n"
     "n\tp10.cpp\t/^int Local::n = 0;$/;\"\tm\tline:6\tclass:Local\ttyperef:typename:int\n"
     "n\tp10.cpp\t/^struct Local { static int n; };$/;\"\tm\tline:4\tstruct:__anon27067e470111::Local\ttyperef:typename:int\tfile:\n"
     "outer\tp10.cpp\t/^namespace outer { namespace { int inner; } }$/;\"\tn\tline:7\tfile:\n"},
    {IMPROVED, "P1 macro calls before a definition", "p1.cpp", "",
     "INITIALIZE_PASS_BEGIN(Lint, \"lint\", \"Lint\", false, true)\n"
     "INITIALIZE_PASS_DEPENDENCY(AAResults)\n"
     "INITIALIZE_PASS_END(Lint, \"lint\", \"Lint\", false, true)\n"
     "\n"
     "FunctionPass *createLintPass() { return 0; }\n"
     "\n"
     "struct Flags {\n"
     "    PREFERRED_TYPE(bool)\n"
     "    unsigned Verbose : 1;\n"
     "};\n"
     "\n"
     "Foo::Foo()\n"
     "    noexcept {}\n",
     "Flags\tp1.cpp\t/^struct Flags {$/;\"\ts\tline:7\tfile:\n"
     "Foo\tp1.cpp\t/^Foo::Foo()$/;\"\tf\tline:12\tclass:Foo\n"
     "Verbose\tp1.cpp\t/^    unsigned Verbose : 1;$/;\"\tm\tline:9\tstruct:Flags\ttyperef:typename:unsigned:1\tfile:\n"
     "createLintPass\tp1.cpp\t/^FunctionPass *createLintPass() { return 0; }$/;\"\tf\tline:5\ttyperef:typename:FunctionPass *\n",
     "Flags\tp1.cpp\t/^struct Flags {$/;\"\ts\tline:7\tfile:\n"
     "Foo\tp1.cpp\t/^Foo::Foo()$/;\"\tf\tline:12\tclass:Foo\n"
     "INITIALIZE_PASS_DEPENDENCY\tp1.cpp\t/^INITIALIZE_PASS_DEPENDENCY(AAResults)$/;\"\tf\tline:2\ttyperef:typename:Lint,\"lint\",\"Lint\",false,true\n"},
    {SHARED, "qualified extra", "q.cpp", "--kinds-C++=+plz --extras=+q",
     "namespace geo {\n"
     "class Shape {\n"
     "public:\n"
     "    Shape(int n);\n"
     "    int sides() const { return n; }\n"
     "    enum Fill { Solid, Hollow };\n"
     "    enum class Edge { Sharp };\n"
     "    struct Box { int w; } box;\n"
     "    typedef int Count;\n"
     "private:\n"
     "    int n;\n"
     "};\n"
     "enum Unit { Mm };\n"
     "void draw(int scale) { int done = scale; }\n"
     "}\n"
     "enum Global { G1 };\n"
     "void geo::Shape::grow() {}\n",
     "Box\tq.cpp\t/^    struct Box { int w; } box;$/;\"\ts\tline:8\tclass:geo::Shape\tfile:\n"
     "Count\tq.cpp\t/^    typedef int Count;$/;\"\tt\tline:9\tclass:geo::Shape\ttyperef:typename:int\tfile:\n"
     "Edge\tq.cpp\t/^    enum class Edge { Sharp };$/;\"\tg\tline:7\tclass:geo::Shape\tfile:\n"
     "Fill\tq.cpp\t/^    enum Fill { Solid, Hollow };$/;\"\tg\tline:6\tclass:geo::Shape\tfile:\n"
     "G1\tq.cpp\t/^enum Global { G1 };$/;\"\te\tline:16\tenum:Global\tfile:\n"
     "Global\tq.cpp\t/^enum Global { G1 };$/;\"\tg\tline:16\tfile:\n"
     "Hollow\tq.cpp\t/^    enum Fill { Solid, Hollow };$/;\"\te\tline:6\tenum:geo::Shape::Fill\tfile:\n"
     "Mm\tq.cpp\t/^enum Unit { Mm };$/;\"\te\tline:13\tenum:geo::Unit\tfile:\n"
     "Shape\tq.cpp\t/^    Shape(int n);$/;\"\tp\tline:4\tclass:geo::Shape\tfile:\n"
     "Shape\tq.cpp\t/^class Shape {$/;\"\tc\tline:2\tnamespace:geo\tfile:\n"
     "Sharp\tq.cpp\t/^    enum class Edge { Sharp };$/;\"\te\tline:7\tenum:geo::Shape::Edge\tfile:\n"
     "Solid\tq.cpp\t/^    enum Fill { Solid, Hollow };$/;\"\te\tline:6\tenum:geo::Shape::Fill\tfile:\n"
     "Unit\tq.cpp\t/^enum Unit { Mm };$/;\"\tg\tline:13\tnamespace:geo\tfile:\n"
     "box\tq.cpp\t/^    struct Box { int w; } box;$/;\"\tm\tline:8\tclass:geo::Shape\ttyperef:struct:geo::Shape::Box\tfile:\n"
     "done\tq.cpp\t/^void draw(int scale) { int done = scale; }$/;\"\tl\tline:14\tfunction:geo::draw\ttyperef:typename:int\tfile:\n"
     "draw\tq.cpp\t/^void draw(int scale) { int done = scale; }$/;\"\tf\tline:14\tnamespace:geo\ttyperef:typename:void\n"
     "geo\tq.cpp\t/^namespace geo {$/;\"\tn\tline:1\tfile:\n"
     "geo::Mm\tq.cpp\t/^enum Unit { Mm };$/;\"\te\tline:13\tenum:geo::Unit\tfile:\n"
     "geo::Shape\tq.cpp\t/^class Shape {$/;\"\tc\tline:2\tnamespace:geo\tfile:\n"
     "geo::Shape::Box\tq.cpp\t/^    struct Box { int w; } box;$/;\"\ts\tline:8\tclass:geo::Shape\tfile:\n"
     "geo::Shape::Box::w\tq.cpp\t/^    struct Box { int w; } box;$/;\"\tm\tline:8\tstruct:geo::Shape::Box\ttyperef:typename:int\tfile:\n"
     "geo::Shape::Count\tq.cpp\t/^    typedef int Count;$/;\"\tt\tline:9\tclass:geo::Shape\ttyperef:typename:int\tfile:\n"
     "geo::Shape::Edge\tq.cpp\t/^    enum class Edge { Sharp };$/;\"\tg\tline:7\tclass:geo::Shape\tfile:\n"
     "geo::Shape::Fill\tq.cpp\t/^    enum Fill { Solid, Hollow };$/;\"\tg\tline:6\tclass:geo::Shape\tfile:\n"
     "geo::Shape::Hollow\tq.cpp\t/^    enum Fill { Solid, Hollow };$/;\"\te\tline:6\tenum:geo::Shape::Fill\tfile:\n"
     "geo::Shape::Shape\tq.cpp\t/^    Shape(int n);$/;\"\tp\tline:4\tclass:geo::Shape\tfile:\n"
     "geo::Shape::Sharp\tq.cpp\t/^    enum class Edge { Sharp };$/;\"\te\tline:7\tenum:geo::Shape::Edge\tfile:\n"
     "geo::Shape::Solid\tq.cpp\t/^    enum Fill { Solid, Hollow };$/;\"\te\tline:6\tenum:geo::Shape::Fill\tfile:\n"
     "geo::Shape::box\tq.cpp\t/^    struct Box { int w; } box;$/;\"\tm\tline:8\tclass:geo::Shape\ttyperef:struct:geo::Shape::Box\tfile:\n"
     "geo::Shape::grow\tq.cpp\t/^void geo::Shape::grow() {}$/;\"\tf\tline:17\tclass:geo::Shape\ttyperef:typename:void\n"
     "geo::Shape::n\tq.cpp\t/^    int n;$/;\"\tm\tline:11\tclass:geo::Shape\ttyperef:typename:int\tfile:\n"
     "geo::Shape::sides\tq.cpp\t/^    int sides() const { return n; }$/;\"\tf\tline:5\tclass:geo::Shape\ttyperef:typename:int\tfile:\n"
     "geo::Unit\tq.cpp\t/^enum Unit { Mm };$/;\"\tg\tline:13\tnamespace:geo\tfile:\n"
     "geo::draw\tq.cpp\t/^void draw(int scale) { int done = scale; }$/;\"\tf\tline:14\tnamespace:geo\ttyperef:typename:void\n"
     "grow\tq.cpp\t/^void geo::Shape::grow() {}$/;\"\tf\tline:17\tclass:geo::Shape\ttyperef:typename:void\n"
     "n\tq.cpp\t/^    Shape(int n);$/;\"\tz\tline:4\tprototype:geo::Shape::Shape\ttyperef:typename:int\tfile:\n"
     "n\tq.cpp\t/^    int n;$/;\"\tm\tline:11\tclass:geo::Shape\ttyperef:typename:int\tfile:\n"
     "scale\tq.cpp\t/^void draw(int scale) { int done = scale; }$/;\"\tz\tline:14\tfunction:geo::draw\ttyperef:typename:int\tfile:\n"
     "sides\tq.cpp\t/^    int sides() const { return n; }$/;\"\tf\tline:5\tclass:geo::Shape\ttyperef:typename:int\tfile:\n"
     "w\tq.cpp\t/^    struct Box { int w; } box;$/;\"\tm\tline:8\tstruct:geo::Shape::Box\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "qualified module partition", "m.cpp", "--extras=+q",
     "export module shapes:circle;\n"
     "export int radius;\n",
     "circle\tm.cpp\t/^export module shapes:circle;$/;\"\tP\tline:1\tmodule:shapes\n"
     "radius\tm.cpp\t/^export int radius;$/;\"\tv\tline:2\ttyperef:typename:int\n"
     "shapes:circle\tm.cpp\t/^export module shapes:circle;$/;\"\tP\tline:1\tmodule:shapes\n",
     ""},
    {SHARED, "extras reset drops anonymous", "a.cpp", "--kinds-C++=+pz --extras=q",
     "int f(int, char *);\n"
     "void g(int (*)(int), int x) {}\n"
     "struct { int a; } v;\n"
     "enum { E1 } e;\n"
     "namespace ns { struct { int b; } w; }\n"
     "auto l = [](int y) { return y; };\n",
     "e\ta.cpp\t/^enum { E1 } e;$/;\"\tv\tline:4\ttyperef:enum:__anon0ef536770203\n"
     "g\ta.cpp\t/^void g(int (*)(int), int x) {}$/;\"\tf\tline:2\ttyperef:typename:void\n"
     "l\ta.cpp\t/^auto l = [](int y) { return y; };$/;\"\tv\tline:6\ttyperef:typename:auto\n"
     "ns::w\ta.cpp\t/^namespace ns { struct { int b; } w; }$/;\"\tv\tline:5\tnamespace:ns\ttyperef:struct:ns::__anon0ef536770308\n"
     "v\ta.cpp\t/^struct { int a; } v;$/;\"\tv\tline:3\ttyperef:struct:__anon0ef536770108\n"
     "w\ta.cpp\t/^namespace ns { struct { int b; } w; }$/;\"\tv\tline:5\tnamespace:ns\ttyperef:struct:ns::__anon0ef536770308\n",
     ""},
    {SHARED, "anonymous extra off", "a.cpp", "--kinds-C++=+pz --extras=-{anonymous}",
     "int f(int, char *);\n"
     "struct { int a; } v;\n"
     "auto l = [](int y) { return y; };\n",
     "a\ta.cpp\t/^struct { int a; } v;$/;\"\tm\tline:2\tstruct:__anon0ef536770108\ttyperef:typename:int\tfile:\n"
     "f\ta.cpp\t/^int f(int, char *);$/;\"\tp\tline:1\ttyperef:typename:int\tfile:\n"
     "l\ta.cpp\t/^auto l = [](int y) { return y; };$/;\"\tv\tline:3\ttyperef:typename:auto\n"
     "v\ta.cpp\t/^struct { int a; } v;$/;\"\tv\tline:2\ttyperef:struct:__anon0ef536770108\n"
     "y\ta.cpp\t/^auto l = [](int y) { return y; };$/;\"\tz\tline:3\tfunction:__anon0ef536770202\ttyperef:typename:int\tfile:\n",
     ""},

    // Languages chosen by options (LANG-6): the same source as C++, then
    // as C, by its name or forced
    {SHARED, "forced C++", "k.c", "--language-force=C++",
     "class Thing { int v; };\n"
     "int Thing::get() { return v; }\n",
     "Thing\tk.c\t/^class Thing { int v; };$/;\"\tc\tline:1\tfile:\n"
     "get\tk.c\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\tclass:Thing\ttyperef:typename:int\n"
     "v\tk.c\t/^class Thing { int v; };$/;\"\tm\tline:1\tclass:Thing\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "forced C++ without an extension", "vector", "--language-force=c++",
     "class Thing { int v; };\n"
     "int Thing::get() { return v; }\n",
     "Thing\tvector\t/^class Thing { int v; };$/;\"\tc\tline:1\tfile:\n"
     "get\tvector\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\tclass:Thing\ttyperef:typename:int\n"
     "v\tvector\t/^class Thing { int v; };$/;\"\tm\tline:1\tclass:Thing\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "forced C", "k.cpp", "--language-force=C",
     "class Thing { int v; };\n"
     "int Thing::get() { return v; }\n",
     "get\tk.cpp\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\ttyperef:typename:int Thing::\n"
     "v\tk.cpp\t/^class Thing { int v; };$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "an extension moved to C++", "k.c", "--langmap=c++:+.c",
     "class Thing { int v; };\n"
     "int Thing::get() { return v; }\n",
     "Thing\tk.c\t/^class Thing { int v; };$/;\"\tc\tline:1\tfile:\n"
     "get\tk.c\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\tclass:Thing\ttyperef:typename:int\n"
     "v\tk.c\t/^class Thing { int v; };$/;\"\tm\tline:1\tclass:Thing\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "an extension shared, C first", "k.c", "--map-C++=+.c",
     "class Thing { int v; };\n"
     "int Thing::get() { return v; }\n",
     "get\tk.c\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\ttyperef:typename:int Thing::\n"
     "v\tk.c\t/^class Thing { int v; };$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "an extension shared, C disabled", "k.c",
     "--map-C++=+.c --languages=-C",
     "class Thing { int v; };\n"
     "int Thing::get() { return v; }\n",
     "Thing\tk.c\t/^class Thing { int v; };$/;\"\tc\tline:1\tfile:\n"
     "get\tk.c\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\tclass:Thing\ttyperef:typename:int\n"
     "v\tk.c\t/^class Thing { int v; };$/;\"\tm\tline:1\tclass:Thing\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "a header as C", "k.h", "--langmap=c:+.h",
     "class Thing { int v; };\n"
     "int Thing::get() { return v; }\n",
     "get\tk.h\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\ttyperef:typename:int Thing::\n"
     "v\tk.h\t/^class Thing { int v; };$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "a pattern before extensions", "k.h", "--map-C=+(k*)",
     "class Thing { int v; };\n"
     "int Thing::get() { return v; }\n",
     "get\tk.h\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\ttyperef:typename:int Thing::\n"
     "v\tk.h\t/^class Thing { int v; };$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "C disabled", "k.c", "--languages=-C",
     "class Thing { int v; };\n",
     "",
     ""},
    {SHARED, "headers by -h", "s.c", "-h .c",
     "static int s;\n"
     "#define M 1\n",
     "M\ts.c\t/^#define M /;\"\td\tline:2\n"
     "s\ts.c\t/^static int s;$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "headers added by -h", "s.h", "-h +.c",
     "static int s;\n"
     "#define M 1\n",
     "M\ts.h\t/^#define M /;\"\td\tline:2\n"
     "s\ts.h\t/^static int s;$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "headers replaced by -h", "s.h", "-h .c",
     "static int s;\n"
     "#define M 1\n",
     "M\ts.h\t/^#define M /;\"\td\tline:2\tfile:\n"
     "s\ts.h\t/^static int s;$/;\"\tv\tline:1\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "an extension in capitals shared, C first", "k.h", "--map-C=+.H",
     "class Thing { int v; };\n"
     "int Thing::get() { return v; }\n",
     "get\tk.h\t/^int Thing::get() { return v; }$/;\"\tf\tline:2\ttyperef:typename:int Thing::\n"
     "v\tk.h\t/^class Thing { int v; };$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "an extension in capitals, ignoring case", "k.x", "--langmap=c:.X",
     "int v;\n",
     "v\tk.x\t/^int v;$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "a tie with Fortran decided by the template", "k.c.in",
     "--map-Fortran=+.in --map-C++=+.in",
     "class Thing { int v; };\n",
     "v\tk.c.in\t/^class Thing { int v; };$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {SHARED, "a tie with Fortran left to the first by name", "k.in",
     "--map-Fortran=+.in --map-C++=+.in",
     "class Thing { int v; };\n",
     "Thing\tk.in\t/^class Thing { int v; };$/;\"\tc\tline:1\tfile:\n"
     "v\tk.in\t/^class Thing { int v; };$/;\"\tm\tline:1\tclass:Thing\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "an extension taken by a defined language first by name", "k.c",
     "--langdef=Aaa --map-Aaa=+.c",
     "int v;\n",
     "",
     ""},
    {SHARED, "an extension shared with a defined language, C first", "k.c",
     "--langdef=Zzz --map-Zzz=+.c",
     "int v;\n",
     "v\tk.c\t/^int v;$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {IMPROVED, "D1 C++'s own C without C's", "k.C", "--map-C=-.c",
     "class Thing { int v; };\n",
     "Thing\tk.C\t/^class Thing { int v; };$/;\"\tc\tline:1\tfile:\n"
     "v\tk.C\t/^class Thing { int v; };$/;\"\tm\tline:1\tclass:Thing\ttyperef:typename:int\tfile:\n",
     ""},
    {SHARED, "C++'s own C removed", "k.C", "--map-C++=-.C",
     "class Thing { int v; };\n",
     "v\tk.C\t/^class Thing { int v; };$/;\"\tv\tline:1\ttyperef:typename:int\n",
     ""},
    {IMPROVED, "D2 a forced header template", "k.x.in", "-h .x --language-force=C",
     "static int s;\n",
     "s\tk.x.in\t/^static int s;$/;\"\tv\tline:1\ttyperef:typename:int\n",
     "s\tk.x.in\t/^static int s;$/;\"\tv\tline:1\ttyperef:typename:int\tfile:\n"},
    {OURS, "forced to another language", "k.c", "--language-force=Python",
     "int v;\n",
     "",
     ""},

    // -I (LEX-14, OPT-17)
    {SHARED, "-I Qt macros", "qt.cpp",
     "--kinds-C++=+p -I Q_DECL_EXPORT,Q_OBJECT,Q_PROPERTY+,Q_SLOTS,Q_SIGNALS=public",
     "class Q_DECL_EXPORT Widget : public QObject\n"
     "{\n"
     "    Q_OBJECT\n"
     "    Q_PROPERTY(int size READ size NOTIFY resized)\n"
     "public Q_SLOTS:\n"
     "    void resize(int);\n"
     "Q_SIGNALS:\n"
     "    void resized();\n"
     "private:\n"
     "    int m_size;\n"
     "};\n",
     "Widget\tqt.cpp\t/^class Q_DECL_EXPORT Widget : public QObject$/;\"\tc\tline:1\tfile:\n"
     "m_size\tqt.cpp\t/^    int m_size;$/;\"\tm\tline:10\tclass:Widget\ttyperef:typename:int\tfile:\n"
     "resize\tqt.cpp\t/^    void resize(int);$/;\"\tp\tline:6\tclass:Widget\ttyperef:typename:void\tfile:\n"
     "resized\tqt.cpp\t/^    void resized();$/;\"\tp\tline:8\tclass:Widget\ttyperef:typename:void\tfile:\n",
     ""},
    {SHARED, "-I keywords stay", "kw.h", "-I public=long,final=char,int=short,struct=union",
     "struct S {\n"
     "    public x;\n"
     "    final y;\n"
     "    int z;\n"
     "};\n"
     "class C final : B { };\n",
     "C\tkw.h\t/^class C final : B { };$/;\"\tc\tline:6\n"
     "S\tkw.h\t/^struct S {$/;\"\ts\tline:1\n"
     "y\tkw.h\t/^    final y;$/;\"\tm\tline:3\tstruct:S\ttyperef:typename:final\n"
     "z\tkw.h\t/^    int z;$/;\"\tm\tline:4\tstruct:S\ttyperef:typename:int\n",
     ""},
    {SHARED, "-I replacement positions", "st.cpp", "--kinds-C++=+p -I X+=virtual,NAME=n,F+=var",
     "struct A {\n"
     "    X(1,\n"
     "      2)~A();\n"  // ~ keeps its line, under the replacement
     "    X(3,\n"
     "      4)(*fp)(void);\n"
     "    int NAME\n"
     "      ;\n"
     "};\n"
     "int F(a,\n"
     "      b) = 1;\n",
     "A\tst.cpp\t/^struct A {$/;\"\ts\tline:1\tfile:\n"
     "n\tst.cpp\t/^    int NAME$/;\"\tm\tline:6\tstruct:A\ttyperef:typename:int\tfile:\n"
     "var\tst.cpp\t/^int F(a,$/;\"\tv\tline:9\ttyperef:typename:int\n"
     "~A\tst.cpp\t/^      2)~A();$/;\"\tp\tline:3\tstruct:A\tfile:\n",
     ""},
    {SHARED, "-I parameter", "pi.cpp", "--param-CPreProcessor.ignore=a=int,b=char",
     "a x;\n"
     "b y;\n",
     "y\tpi.cpp\t/^b y;$/;\"\tv\tline:2\ttyperef:typename:b\n",
     ""},

    // Line formats
    {SHARED, "FLD-8 FLD-9 fields around -I macros", "i.cpp",
     "-I MAC,PACKED+,ARG+,MAC2+ --fields=+SiaKz --kinds-C++=+p",
     "MAC int g(void);\n"
     "int MAC h;\n"
     "struct S PACKED(4) : B { int m; };\n"
     "int f2(int a ARG(x), char *b) MAC2(1, 2) { return 0; }\n",
     "S\ti.cpp\t/^struct S PACKED(4) : B { int m; };$/;\"\tkind:struct\tline:3\tfile:\tinherits:B\n"
     "f2\ti.cpp\t/^int f2(int a ARG(x), char *b) MAC2(1, 2) { return 0; }$/;\"\tkind:function\tline:4\ttyperef:typename:int\tsignature:(int a,char * b)\n"
     "g\ti.cpp\t/^MAC int g(void);$/;\"\tkind:prototype\tline:1\ttyperef:typename:int\tfile:\tsignature:(void)\n"
     "h\ti.cpp\t/^int MAC h;$/;\"\tkind:variable\tline:2\ttyperef:typename:int\n"
     "m\ti.cpp\t/^struct S PACKED(4) : B { int m; };$/;\"\tkind:member\tline:3\tstruct:S\ttyperef:typename:int\tfile:\taccess:public\n",
     ""},
    {SHARED, "FLD-5 long kinds, languages, scope keys", "k.cpp",
     "--fields=K+Zsl --kinds-C++=+A",
     "namespace geo {\n"
     "class Shape { int sides; };\n"
     "namespace g2 = geo;\n"
     "}\n"
     "static auto f = [](int x) { return x; };\n",
     "Shape\tk.cpp\t/^class Shape { int sides; };$/;\"\tclass\tlanguage:C++\tscope:namespace:geo\n"
     "__anon0faa2b810102\tk.cpp\t/^static auto f = [](int x) { return x; };$/;\"\tfunction\tlanguage:C++\n"
     "f\tk.cpp\t/^static auto f = [](int x) { return x; };$/;\"\tvariable\tlanguage:C++\n"
     "g2\tk.cpp\t/^namespace g2 = geo;$/;\"\talias\tlanguage:C++\tscope:namespace:geo\tname:geo\n"
     "geo\tk.cpp\t/^namespace geo {$/;\"\tnamespace\tlanguage:C++\n"
     "sides\tk.cpp\t/^class Shape { int sides; };$/;\"\tmember\tlanguage:C++\tscope:class:geo::Shape\n",
     ""},
    {SHARED, "FLD-5 extras of qualified and anonymous tags", "k.cpp",
     "--fields=+E --extras=+q --kinds-C++=+Az",
     "namespace geo {\n"
     "class Shape { int sides; };\n"
     "namespace g2 = geo;\n"
     "}\n"
     "static auto f = [](int x) { return x; };\n",
     "Shape\tk.cpp\t/^class Shape { int sides; };$/;\"\tc\tline:2\tnamespace:geo\tfile:\textras:fileScope\n"
     "__anon0faa2b810102\tk.cpp\t/^static auto f = [](int x) { return x; };$/;\"\tf\tline:5\tfile:\textras:fileScope,anonymous\n"
     "f\tk.cpp\t/^static auto f = [](int x) { return x; };$/;\"\tv\tline:5\ttyperef:typename:auto\n"
     "g2\tk.cpp\t/^namespace g2 = geo;$/;\"\tA\tline:3\tnamespace:geo\tfile:\textras:fileScope\tname:geo\n"
     "geo\tk.cpp\t/^namespace geo {$/;\"\tn\tline:1\tfile:\textras:fileScope\n"
     "geo::Shape\tk.cpp\t/^class Shape { int sides; };$/;\"\tc\tline:2\tnamespace:geo\tfile:\textras:fileScope,qualified\n"
     "geo::Shape::sides\tk.cpp\t/^class Shape { int sides; };$/;\"\tm\tline:2\tclass:geo::Shape\ttyperef:typename:int\tfile:\textras:fileScope,qualified\n"
     "geo::g2\tk.cpp\t/^namespace g2 = geo;$/;\"\tA\tline:3\tnamespace:geo\tfile:\textras:fileScope,qualified\tname:geo\n"
     "sides\tk.cpp\t/^class Shape { int sides; };$/;\"\tm\tline:2\tclass:geo::Shape\ttyperef:typename:int\tfile:\textras:fileScope\n"
     "x\tk.cpp\t/^static auto f = [](int x) { return x; };$/;\"\tz\tline:5\tfunction:__anon0faa2b810102\ttyperef:typename:int\tfile:\textras:fileScope\n",
     ""},
    {IMPROVED, "D10 ;\" before a parser's field alone", "a.cpp",
     "--fields= --kinds-C++=+A",
     "namespace N { int x; }\n"
     "namespace A = N;\n",
     "A\ta.cpp\t/^namespace A = N;$/;\"\tname:N\n"
     "N\ta.cpp\t/^namespace N { int x; }$/\n"
     "x\ta.cpp\t/^namespace N { int x; }$/\n",
     "A\ta.cpp\t/^namespace A = N;$/\tname:N\n"
     "N\ta.cpp\t/^namespace N { int x; }$/\n"
     "x\ta.cpp\t/^namespace N { int x; }$/\n"},
    {SHARED, "FLD-5 the C++ name field off", "k.cpp",
     "--fields-C++=-{name} --kinds-C++=+A",
     "namespace geo {\n"
     "namespace g2 = geo;\n"
     "}\n",
     "g2\tk.cpp\t/^namespace g2 = geo;$/;\"\tA\tline:2\tnamespace:geo\tfile:\n"
     "geo\tk.cpp\t/^namespace geo {$/;\"\tn\tline:1\tfile:\n",
     ""},
    {SHARED, "OUT-8 no name field in the original format", "k.cpp",
     "--format=1 --kinds-C++=+A",
     "namespace geo {\n"
     "namespace g2 = geo;\n"
     "}\n",
     "g2\tk.cpp\t/^namespace g2 = geo;$/\n"
     "geo\tk.cpp\t/^namespace geo {$/\n",
     ""},
    {SHARED, "FLD-7 access by scope", "a.cpp", "--fields=+a --kinds-C++=+pZ",
     "class Shape {\n"
     "    int id;\n"
     "    enum Kind { Round, Square };\n"
     "public:\n"
     "    virtual double area() const = 0;\n"
     "protected:\n"
     "    struct Cache { int hits; };\n"
     "    friend void dump(Shape &s) {}\n"
     "};\n"
     "template<class T> class Box { T value; };\n"
     "struct Point { int x; class In {} in; };\n"
     "namespace geo { int count; }\n"
     "double Shape::area() const { int local; return 0; }\n",
     "Box\ta.cpp\t/^template<class T> class Box { T value; };$/;\"\tc\tline:10\tfile:\n"
     "Cache\ta.cpp\t/^    struct Cache { int hits; };$/;\"\ts\tline:7\tclass:Shape\tfile:\taccess:protected\n"
     "In\ta.cpp\t/^struct Point { int x; class In {} in; };$/;\"\tc\tline:11\tstruct:Point\tfile:\taccess:public\n"
     "Kind\ta.cpp\t/^    enum Kind { Round, Square };$/;\"\tg\tline:3\tclass:Shape\tfile:\taccess:private\n"
     "Point\ta.cpp\t/^struct Point { int x; class In {} in; };$/;\"\ts\tline:11\tfile:\n"
     "Round\ta.cpp\t/^    enum Kind { Round, Square };$/;\"\te\tline:3\tenum:Shape::Kind\tfile:\taccess:public\n"
     "Shape\ta.cpp\t/^class Shape {$/;\"\tc\tline:1\tfile:\n"
     "Square\ta.cpp\t/^    enum Kind { Round, Square };$/;\"\te\tline:3\tenum:Shape::Kind\tfile:\taccess:public\n"
     "T\ta.cpp\t/^template<class T> class Box { T value; };$/;\"\tZ\tline:10\tclass:Box\ttyperef:meta:class\taccess:private\n"
     "area\ta.cpp\t/^    virtual double area() const = 0;$/;\"\tp\tline:5\tclass:Shape\ttyperef:typename:double\tfile:\taccess:public\n"
     "area\ta.cpp\t/^double Shape::area() const { int local; return 0; }$/;\"\tf\tline:13\tclass:Shape\ttyperef:typename:double\n"
     "count\ta.cpp\t/^namespace geo { int count; }$/;\"\tv\tline:12\tnamespace:geo\ttyperef:typename:int\n"
     "dump\ta.cpp\t/^    friend void dump(Shape &s) {}$/;\"\tf\tline:8\ttyperef:typename:void\tfile:\n"
     "geo\ta.cpp\t/^namespace geo { int count; }$/;\"\tn\tline:12\tfile:\n"
     "hits\ta.cpp\t/^    struct Cache { int hits; };$/;\"\tm\tline:7\tstruct:Shape::Cache\ttyperef:typename:int\tfile:\taccess:public\n"
     "id\ta.cpp\t/^    int id;$/;\"\tm\tline:2\tclass:Shape\ttyperef:typename:int\tfile:\taccess:private\n"
     "in\ta.cpp\t/^struct Point { int x; class In {} in; };$/;\"\tm\tline:11\tstruct:Point\ttyperef:class:Point::In\tfile:\taccess:public\n"
     "value\ta.cpp\t/^template<class T> class Box { T value; };$/;\"\tm\tline:10\tclass:Box\ttyperef:typename:T\tfile:\taccess:private\n"
     "x\ta.cpp\t/^struct Point { int x; class In {} in; };$/;\"\tm\tline:11\tstruct:Point\ttyperef:typename:int\tfile:\taccess:public\n",
     ""},
    {SHARED, "FLD-8 signatures of functions and lambdas", "s.cpp",
     "--fields=+S --kinds-C++=+p",
     "struct Vec {\n"
     "    Vec(int x, int y);\n"
     "    int dot(const Vec &o) const;\n"
     "    int len() volatile noexcept;\n"
     "    Vec &operator+=(const Vec &) &;\n"
     "    virtual ~Vec() = default;\n"
     "};\n"
     "void apply(int (*fn)(int, char), unsigned long n = 3, ...) {}\n"
     "template<typename T> T pick(T &&a, std::vector<T> const &v) { return a; }\n"
     "auto add = [](int a, int b) { return a + b; };\n"
     "auto none = []{ return 0; };\n"
     "auto tight = [](int q){ return q; };\n",
     "Vec\ts.cpp\t/^    Vec(int x, int y);$/;\"\tp\tline:2\tstruct:Vec\tfile:\tsignature:(int x,int y)\n"
     "Vec\ts.cpp\t/^struct Vec {$/;\"\ts\tline:1\tfile:\n"
     "__anon103aef890102\ts.cpp\t/^auto add = [](int a, int b) { return a + b; };$/;\"\tf\tline:10\tfile:\tsignature:(int a, int b) \n"
     "__anon103aef890202\ts.cpp\t/^auto none = []{ return 0; };$/;\"\tf\tline:11\tfile:\n"
     "__anon103aef890302\ts.cpp\t/^auto tight = [](int q){ return q; };$/;\"\tf\tline:12\tfile:\tsignature:(int q)\n"
     "add\ts.cpp\t/^auto add = [](int a, int b) { return a + b; };$/;\"\tv\tline:10\ttyperef:typename:auto\n"
     "apply\ts.cpp\t/^void apply(int (*fn)(int, char), unsigned long n = 3, ...) {}$/;\"\tf\tline:8\ttyperef:typename:void\tsignature:(int (* fn)(int,char),unsigned long n=3,...)\n"
     "dot\ts.cpp\t/^    int dot(const Vec &o) const;$/;\"\tp\tline:3\tstruct:Vec\ttyperef:typename:int\tfile:\tsignature:(const Vec & o) const\n"
     "len\ts.cpp\t/^    int len() volatile noexcept;$/;\"\tp\tline:4\tstruct:Vec\ttyperef:typename:int\tfile:\tsignature:()\n"
     "none\ts.cpp\t/^auto none = []{ return 0; };$/;\"\tv\tline:11\ttyperef:typename:auto\n"
     "operator +=\ts.cpp\t/^    Vec &operator+=(const Vec &) &;$/;\"\tp\tline:5\tstruct:Vec\ttyperef:typename:Vec &\tfile:\tsignature:(const Vec &)\n"
     "pick\ts.cpp\t/^template<typename T> T pick(T &&a, std::vector<T> const &v) { return a; }$/;\"\tf\tline:9\ttyperef:typename:T\tsignature:(T && a,std::vector<T> const & v)\n"
     "tight\ts.cpp\t/^auto tight = [](int q){ return q; };$/;\"\tv\tline:12\ttyperef:typename:auto\n"
     "~Vec\ts.cpp\t/^    virtual ~Vec() = default;$/;\"\tp\tline:6\tstruct:Vec\tfile:\tsignature:()\n",
     ""},
    {SHARED, "FLD-9 base classes", "i.cpp", "--fields=+i",
     "struct Base {};\n"
     "class Derived : public Base, protected virtual Mixin, private Tmpl<unsigned int, 2> {};\n"
     "struct Nested : ::ns::Outer<std::pair<int, int> > {};\n"
     "class Odd : decltype(make()) {};\n"
     "union U {};\n",
     "Base\ti.cpp\t/^struct Base {};$/;\"\ts\tline:1\tfile:\n"
     "Derived\ti.cpp\t/^class Derived : public Base, protected virtual Mixin, private Tmpl<unsigned int, 2> {};$/;\"\tc\tline:2\tfile:\tinherits:Base,Mixin,Tmpl<unsignedint,2>\n"
     "Nested\ti.cpp\t/^struct Nested : ::ns::Outer<std::pair<int, int> > {};$/;\"\ts\tline:3\tfile:\tinherits:::ns::Outer<std::pair<int,int>>\n"
     "Odd\ti.cpp\t/^class Odd : decltype(make()) {};$/;\"\tc\tline:4\tfile:\tinherits:decltype(make()) \n"
     "U\ti.cpp\t/^union U {};$/;\"\tu\tline:5\tfile:\n",
     ""},
    {SHARED, "FLD-8 FLD-9 e-ctags signatures as they are", "e.cpp",
     "--fields=+Si --output-format=e-ctags",
     "void f(const char *s = \"x\\\\y\") {}\n"
     "void g(const char *t = \"a\tb\") {}\n"
     "class K : public X<\"a\\\\b\"> {};\n",
     "K\te.cpp\t/^class K : public X<\"a\\\\\\\\b\"> {};$/;\"\tc\tline:3\tfile:\tinherits:X<\"a\\\\\\\\b\">\n"
     "f\te.cpp\t/^void f(const char *s = \"x\\\\\\\\y\") {}$/;\"\tf\tline:1\ttyperef:typename:void\tsignature:(const char * s=\"x\\\\y\")\n",
     ""},
    {IMPROVED, "X4 FLD-6 e-ctags NUL in a signature", "n.cpp",
     "--fields=+S --output-format=e-ctags",
     "int a;\n"
     "void f(char c = '\0', int d) {}\n"
     "int b;\n",
     "a\tn.cpp\t/^int a;$/;\"\tv\tline:1\ttyperef:typename:int\n"
     "b\tn.cpp\t/^int b;$/;\"\tv\tline:3\ttyperef:typename:int\n",
     "a\tn.cpp\t/^int a;$/;\"\tv\tline:1\ttyperef:typename:int\n"},

    {SHARED, "EX-7 a C++ file tag", "n.cpp",
     "--extras=+f --fields=+l-T --sort=no",
     "namespace n { int v; }\n",
     "n\tn.cpp\t/^namespace n { int v; }$/;\"\tn\tline:1\tlanguage:C++\tfile:\n"
     "v\tn.cpp\t/^namespace n { int v; }$/;\"\tv\tline:1\tlanguage:C++\tnamespace:n\ttyperef:typename:int\n"
     "n.cpp\tn.cpp\t1;\"\tF\tline:1\tlanguage:C++\n",
     ""},

    {SHARED, "OPT-21 etags of qualified and anonymous tags", "e.cpp",
     "-e --extras=+q",
     "namespace N { struct S { int m; }; }\n"
     "auto l = [](){};\n",
     "\f\ne.cpp,289\n"
     "namespace N { struct S { int m; }; }\x7fN\x01" "1,0\n"
     "namespace N { struct S { int m; }; }\x7fS\x01" "1,0\n"
     "namespace N { struct S { int m; }; }\x7fN::S\x01" "1,0\n"
     "namespace N { struct S { int m; }; }\x7fm\x01" "1,0\n"
     "namespace N { struct S { int m; }; }\x7fN::S::m\x01" "1,0\n"
     "auto l = [](){};\x7f__anon0f3d987b0102\x01" "2,37\n"
     "auto l = [](){};\x7fl\x01" "2,37\n",
     ""},
};

// head, n spaces, then tail.
static Str padded(Arena *a, Str head, iz n, Str tail)
{
    Buf b(a, head.len + n + tail.len);
    print(&b, head);
    for (iz i = 0; i < n; i++) {
        putbyte(&b, ' ');
    }
    print(&b, tail);
    return finish(&b);
}

// The limits on -I replacements (LEX-14), with inputs too large for tag
// cases. Universal Ctags gives the same output.
static void test_macrolimits(Test *t)
{
    Arena a  = t->perm;
    Os   *os = newos(&a);
    vfsfile(os, "m.c", "X\n");

    // A replacement's buffer is 8 bytes longer than it, and none is made
    // while the buffer being read has 65536 bytes, which a replacement
    // nested in it does not change once read
    struct { Str list; Str want; } bs[] = {
        {padded(&a, "Y=int\nX=", 65523, "Y v;\n"),
         "v\tm.c\t/^X$/;\"\tv\tline:1\ttyperef:typename:int\n"},
        {padded(&a, "Y=int\nX=", 65524, "Y v;\n"), ""},
        {padded(&a, "Y=int\nZ=", 65540, "char\nX=Z Y v;\n"),
         "v\tm.c\t/^X$/;\"\tv\tline:1\ttyperef:typename:char int\n"},
    };
    for (iz i = 0; i < countof(bs); i++) {
        vfsfile(os, "list", bs[i].list);
        Run r = run(os, a, "-I @list -f - m.c");
        expect(t, "-I buffer size", r.out, bs[i].want);
    }

    // Nor is one made in a statement at the token limit, here one that
    // would end it
    Buf big(&a, 1<<16);
    print(&big, "int v(");
    for (i32 i = 0; i < 8200; i++) {
        print(&big, "a, ");
    }
    print(&big, "X);\n");
    vfsfile(os, "list", "X=0);int w;int u(0\n");
    vfsfile(os, "s.c", "int v(a, X);\n");
    vfsfile(os, "big.c", finish(&big));
    Run r = run(os, a, "-I @list -f - s.c");
    expect(t, "-I below the token limit", r.out,
           "w\ts.c\t/^int v(a, X);$/;\"\tv\tline:1\ttyperef:typename:int\n");
    r = run(os, a, "-I @list -f - big.c");
    expect(t, "-I at the token limit", r.out, "");
}

static void test_c(Test *t)
{
    runcases(t, ccases, countof(ccases));
    if (!t->exportdir) {
        test_macrolimits(t);
    }
}

// The keyword spelled s in the language, by searching them all.
static i32 kwsearch(Str s, u32 lang)
{
    for (i32 kw = 0; kw < NKEYWORDS; kw++) {
        Str name = keywords[kw].name;
        if (name == s) {
            return keywords[kw].langs&lang ? kw : -1;
        }
    }
    return -1;
}

// Keyword lookup by hash against a search: each keyword, its prefixes,
// its extensions, and words one byte from it, in each language.
static void test_keywords(Test *t)
{
    Arena a    = t->perm;
    Str   fail = {};
    u64   rng  = 1;
    for (i32 kw = 0; kw<NKEYWORDS && !fail.data; kw++) {
        Str name = keywords[kw].name;
        for (i32 trial = 0; trial < 64; trial++) {
            Buf b(&a, 32);
            if (trial < name.len) {
                print(&b, takehead(name, trial));  // a prefix
            } else {
                print(&b, name);
                rng = rng*0x3243f6a8885a308d + 1;
                u32 r = (u32)(rng >> 32);
                iz  i = r % (u32)name.len;
                switch (r>>8 & 3) {
                case 0:  break;  // itself
                case 1:  putbyte(&b, "_xs1"[r>>12 & 3]); break;
                case 2:  b.data[i] = (u8)"_aeiouAZ09t"[(r>>12) % 11]; break;
                case 3:  b.data[i] ^= 0x20; break;  // case, or not
                }
            }
            Str word = finish(&b);
            for (u32 lang = KL_C; lang <= KL_CXX; lang++) {
                if (kwlookup(word, lang) != kwsearch(word, lang)) {
                    fail = word;
                }
            }
        }
    }
    expect(t, "keyword lookup", fail.data ? fail : Str("ok"), "ok");
}

static void test_cxx(Test *t)
{
    runcases(t, cxxcases, countof(cxxcases));
    if (!t->exportdir) {
        test_keywords(t);
    }
}

// A data table long enough to reach the token limit, with things in
// it the fast path must stop at, then a tag whose line is checked.
static Str bigtable(Arena *a, Str head, Str tail, i32 *afterline)
{
    Buf b(a, 1<<20);
    print(&b, head);
    for (i32 i = 0; i < 40000; i++) {
        print(&b, "0x");
        printhex(&b, (u32)i&0xff, 2);
        switch (i % 5000) {
        case 1000: print(&b, " /* , */"); break;
        case 1500: print(&b, "\n#if 1\n,0x1\n#endif\n"); break;
        case 2000: print(&b, ", 1'000"); break;
        case 2500: print(&b, ", 1.5e3f, 0x1p3, .5, 077uLL"); break;
        case 3000: print(&b, ", u8'x', \"s,1\""); break;
        case 3500: print(&b, ", \v1"); break;
        case 4000: print(&b, ", ID(2), x"); break;
        }
        print(&b, i%12==11 ? (i%24==11 ? Str(",\r\n") : Str(",\n")) : Str(", "));
    }
    print(&b, tail);
    *afterline = 1;
    for (iz i = 0; i < b.len; i++) {
        *afterline += b.data[i] == '\n';
    }
    print(&b, "int after;\n");
    return finish(&b);
}

// Inputs at the edges of the lexer's fast paths, where reading ahead,
// splices, line endings, and literals meet them.
static Str fastedges[] = {
    "int a = 1'000;\nint b;\n",
    "  #define X 1\nint y;\n",
    "/*/ int x; */ int y;\n",
    "int a; // x",
    "int a; // x\\",
    "int a; /* x",
    "int a; \"x",
    "int a; R\"(x",
    "int f() { return ~0; }\nstruct S { ~S(); ~ T(); int z; };\n",
    "int g = 1ga'b';\nint h;\n",
    "int ab\\\nR\"(x)\";\nint z;\n",
    "#include\n\"str\" int w;\n",
    "int fo\\\no;\nint fo\\\r\no2;\nint fo\\\r\ro3;\n",
    "// a\\\nint hidden;\nint shown;\n",
    "// a\\\r\nint hidden;\n// b\\\\\nint shown;\n// c\\\r\r\nint shown2;\n",
    "char *s = \"a\\\nb\";\nint after;\nchar *t = \"a\\\r\nb\";\nint after2;\n",
    "int a;\r\n// c\r\nint b;\r\nint c\r = 1;\nint d;\r\r\nint e;\n",
    "?\?=define T 1\nint a?\?(3?\?);\n?\?/\nint b;\nint c ?\?- ?\?! ?\?' ?\?< ?\?> ?\?) ?\?x;\n",
    "<: :> <% %> %:define D 2\nint a<::b> c;\nx<::>y;\nint e<:: ;\nint g;\n",
    "char *r = R\"d(raw)d\";\nint after;\n",
    "char *u = u8R\"(x)\";\nint v;\nchar *l = LR\"(y)\";\nint fooR\"x\";\nint w;\n",
    "char *s = u8R\"x(\"; } {)x\";\nint after;\nchar *t = R\"(;}{)\";\nint after2;\n"
    "char *u = uR\"(\";)\"; int after3; char *v = xR\"(\";)\"; int after4;\n",
    "int x = 0x1F'FF'u + 1'0.5e3f + 0x1'f + 1.'a' + 0x1\\\n'2';\nint y;\n",
    "/**/int b;/***/int c;/* * / */int d;/*\r\n*/int e;\n",
    "#if 0\n'\n#endif\nint c;\n#if 0\n#define\nint x;\n#endif\nint d;\n",
    "int x\\\n\\\ny;\n#define M(a) \\\n a\nint z;\n",
    "\"open\nint after;\n'x\nint after2;\n",
    "int \x80\xff\x80z;\nint z\x00y;\nint \x01q\x7f;\n",
    "int a\v= 1;\fint b;\n  \t #  define Y 2\nint c;\n",
    "int f(int a = 1? 2 : 3, int b = a<b, char c = '\\'', char *d = \"\\\"\");\n",
    "u8Rx q;\nint after;\n",
};

static void test_fastpaths(Test *t)
{
    // Each edge, as C and C++, with the default options, with every
    // kind and the signatures built from tokens' text, and with -I
    // replacements, the same with and without the fast paths
    {
        Str opts[] = {"", "--kinds-C=* --kinds-C++=* --fields=+S",
                      "-I u8Rx=int,ab+,x=0x2"};
        Str files[] = {"e.c", "e.cpp"};
        for (iz i = 0; i < countof(fastedges); i++) {
            for (iz f = 0; f < countof(files); f++) {
                for (iz o = 0; o < countof(opts); o++) {
                    Arena a  = t->perm;
                    Os   *os = newos(&a);
                    vfsfile(os, files[f], fastedges[i]);
                    Buf fast(&a, 64);
                    print(&fast, opts[o]);
                    print(&fast, " -f - ");
                    print(&fast, files[f]);
                    Buf slow(&a, 64);
                    print(&slow, "--_nofast ");
                    print(&slow, opts[o]);
                    print(&slow, " -f - ");
                    print(&slow, files[f]);
                    Run x = run(os, a, finish(&fast));
                    Run y = run(os, a, finish(&slow));
                    Buf name(&a, 64);
                    print(&name, "fast edge ");
                    print(&name, (i64)i);
                    print(&name, " ");
                    print(&name, files[f]);
                    print(&name, " ");
                    print(&name, opts[o]);
                    expect(t, finish(&name), x.out, y.out);
                }
            }
        }
    }

    // A string longer than LITMAX, then a raw string, whose token text
    // is the last literal's kept contents, in a signature
    {
        Arena a = t->perm;
        Buf   b(&a, 4096);
        print(&b, "void f(const char *s = \"");
        for (i32 i = 0; i < 1100; i++) {
            putbyte(&b, (u8)('a' + i%26));
            if (i == 1022) {
                print(&b, "\\n");  // an escape across the limit
            }
        }
        print(&b, "\", const char *t = R\"(r)\") {}\n");
        Os *os = newos(&a);
        vfsfile(os, "l.cpp", finish(&b));
        Run x = run(os, a, "--fields=+S -f - l.cpp");
        Run y = run(os, a, "--_nofast --fields=+S -f - l.cpp");
        expect(t, "fast edge LITMAX", x.out, y.out);
    }

    struct {
        Str name;
        Str head;
        Str tail;
    } tables[] = {
        {"table", "unsigned char data[] = {\n", "};\n"},
        {"nested", "int f(void) { static int v[] = {\n", "}; return 0; }\n"},
        {"call", "int x = f(\n", "0);\n"},
    };
    for (iz i = 0; i < countof(tables); i++) {
        Arena a     = t->perm;
        i32   line  = 0;
        Str   src   = bigtable(&a, tables[i].head, tables[i].tail, &line);
        Os   *os    = newos(&a);
        vfsfile(os, "big.c", src);
        Run fast = run(os, a, "-f - big.c");
        Run slow = run(os, a, "--_nofast -f - big.c");
        expect(t, tables[i].name, fast.out, slow.out);

        // Replacements leave the pushback for the fast path to drain
        Run ifast = run(os, a, "-I ID+=1,x=0x2 -f - big.c");
        Run islow = run(os, a, "-I ID+=1,x=0x2 --_nofast -f - big.c");
        expect(t, tables[i].name, ifast.out, islow.out);

        Buf want(&a, 64);
        print(&want, "after\tbig.c\t/^int after;$/;\"\tv\tline:");
        print(&want, (i64)line);
        Str got = {};
        for (Cut c = cut(fast.out, '\n'); c.ok; c = cut(c.tail, '\n')) {
            if (startswith(c.head, "after\t")) {
                got = cuttail(c.head, c.head.len - want.len);
            }
        }
        expect(t, tables[i].name, got, finish(&want));
    }
}

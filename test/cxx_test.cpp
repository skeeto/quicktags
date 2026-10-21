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
};

static void test_c(Test *t)
{
    runcases(t, ccases, countof(ccases));
}

static void test_cxx(Test *t)
{
    runcases(t, cxxcases, countof(cxxcases));
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

static void test_fastpaths(Test *t)
{
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

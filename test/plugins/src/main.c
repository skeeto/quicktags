// A small C program for the plugin tests. Public domain.
#include <stdio.h>
#include "util.h"

#define COUNT 4
#define SQUARE(x) ((x) * (x))

typedef struct point {
    int x;
    int y;
} Point;

enum color { RED, GREEN = 2, BLUE };

union value {
    int i;
    double d;
};

struct list;
int extern_count;
static int counter = 0;
const char *names[COUNT] = {"a", "b", "c", "d"};

static int helper(int a, int b);

static int helper(int a, int b)
{
    return SQUARE(a) + b;
}

int point_sum(Point p)
{
    return p.x + p.y;
}

int main(int argc, char **argv)
{
    Point p = {1, 2};
    enum color c = GREEN;
    union value v;
    v.i = helper(argc, c);
    counter += point_sum(p) + v.i;
    printf("%s %d\n", argv[0], counter);
    return 0;
}

// Declarations shared by the C files, with kernel-style annotations
// that the kernel's tags.sh drops with -I. Public domain.
#ifndef UTIL_H
#define UTIL_H

#define __init
#define __read_mostly
#define __must_check
#define EXPORT_SYMBOL(sym)

struct device;

struct ops {
    int (*open)(struct device *dev);
    void (*close)(struct device *dev);
};

typedef unsigned long ulong;

extern int util_level;
int __must_check util_init(int level);
void util_free(void *p);

static inline int util_max(int a, int b)
{
    return a > b ? a : b;
}

#endif

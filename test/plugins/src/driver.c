// Kernel-style code, as scripts/tags.sh expects it. Public domain.
#include "util.h"

static int __read_mostly driver_debug;
int util_level;

static const struct ops driver_ops;

int __init util_init(int level)
{
    util_level = level;
    return 0;
}
EXPORT_SYMBOL(util_init);

void util_free(void *p)
{
    (void)p;
}
EXPORT_SYMBOL(util_free);

static int driver_open(struct device *dev)
{
    (void)dev;
    return driver_debug;
}

static void driver_close(struct device *dev)
{
    (void)dev;
}

static const struct ops driver_ops = {
    .open  = driver_open,
    .close = driver_close,
};

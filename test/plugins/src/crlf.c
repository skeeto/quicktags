// CRLF lines, as from a Windows editor. Public domain.
#define CRLF_LIMIT 80

struct line {
    char text[CRLF_LIMIT];
    int length;
};

static int line_count;

int line_width(const struct line *l)
{
    return l->length + line_count;
}

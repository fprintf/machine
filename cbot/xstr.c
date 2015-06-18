#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

/*
 * Portably allocate enough space for a sprintf()
 * TODO vsnprintf() is not portable, fix that piece
 * This functions deals with both systems that return
 * 0 (or less than 0) on failure and systems that
 * return the size needed to print the full string.
 *
 * We do this by simply incraesing our block_size geometrically
 * until the vsprintf() is successful. This means we will
 * likely allocate some extra space, usually this is negligible
 */
size_t xsprintf(char ** buf , const char * fmt, ...)
{
    size_t over, block_sz = 16;
    va_list args;

    do {
        if ( !(*buf = malloc(sizeof **buf * block_sz)) ) {
            block_sz = 0;
            break;
        }

        va_start(args, fmt);
        over = vsnprintf(*buf, block_sz, fmt, args);
        va_end(args);
        if (over >= block_sz || over <= 0) {
            block_sz <<= 2; /* Increase block geometrically */
            free(*buf);
        } else 
            over = 0; /* done */
    } while (over > 0);

    return block_sz;
}

/* 
 * Writes atmost sz+1 bytes to dest, guaranteeing NULL byte
 */
char * xstrncpy(char * dest, const char * src, size_t sz)
{
    strncpy(dest,src,sz);
    dest[sz] = 0;
    return dest;
}

char * xstrchrnul(const char * s, int c)
{
    char * p;

    for (p = (char *)s; *p && *p != c; ++p)
        ;

    return p;
}

char * xstrdup(const char * s)
{
    char * n;
    size_t sz = strlen(s)+1;

    if ( (n = malloc(sz * sizeof *n)) )
        strcpy(n,s);

    return n;
}

/*
 * Allocates sz+1 then copies 'sz' items
 * out of string and returns string
 * (NULL on error)
 */
char * xstrndup(const char * s, size_t sz)
{
    char * n;

    if ( (n = malloc(sz+1 * sizeof *n)) ) 
        xstrncpy(n,s,sz);

    return n;
}

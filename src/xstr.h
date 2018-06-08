#ifndef XXSTR_HEAD__H_
#define XXSTR_HEAD__H_

size_t xsprintf(char ** buf, const char * fmt, ...);
char * xstrncpy(char * dest, const char * src, size_t sz);
char * xstrchrnul(const char * s, int c);
char * xstrdup(const char * s);
char * xstrndup(const char * s, size_t sz);

#endif

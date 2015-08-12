#include <string.h>
#include <stdlib.h>

char *strdup(const char *s)
{
    char *d = malloc(strlen(s)+1);
    if (d) strcpy(d,s);
    return d;
}

char *strndup(const char *s, size_t n)
{
    size_t l;
    char *d;
    
    l = strlen(s);
    if (n > l) n = l;
    d = malloc(n+1);
    strncpy(d,s,l);
    d[n] = 0;
    return d;
}

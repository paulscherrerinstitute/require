#include <stdio.h>
#include <stdlib.h>
#include "asprintf.h"

int vasprintf(char** pbuffer, const char* format, va_list ap)
{
    va_list ap2;
    int len;
    FILE* f;
    
    /* print to null device to get required buffer length */
    f = fopen("/null","w");
    if (f == NULL) return -1;
    __va_copy(ap2, ap);
    len = vfprintf(f, format, ap2);
    va_end(ap2);
    fclose(f);
    if (len < 0) return len;

    *pbuffer = malloc(len+1);
    if (*pbuffer == NULL) return -1;
    return vsprintf(*pbuffer, format, ap);
}

int asprintf(char** pbuffer, const char* format, ...)
{
    va_list ap;
    int len;

    va_start(ap, format);
    len = vasprintf(pbuffer, format, ap);
    va_end(ap);
    return len;
}

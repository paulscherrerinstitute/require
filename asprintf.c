#include <stdio.h>
#include <stdlib.h>
#include "asprintf.h"

#ifndef va_copy
#define va_copy __va_copy
#endif

int vasprintf(char** pbuffer, const char* format, va_list ap)
{
    va_list ap2;
    size_t len;
    
#if defined(vxWorks)
    FILE* f;
    /* print to null device to get required buffer length */
    if ((f = fopen("/null","w")) != NULL)
    {
        len = vfprintf(f, format, ap2);
        fclose(f);
    }
#elif defined(_WIN32)
    len = _vscprintf(format, ap);
#else
    len = vsnprintf(NULL, 0, format, ap);
#endif
    va_end(ap2);
    if (len <= 0)
    {
        fprintf(stderr, "vasprintf: too old version on vsnprintf\n");
        return len;
    }
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "asprintf.h"

/* some implementations have __va_copy instead of va_copy */
#if !defined(va_copy) && defined (__va_copy)
#define va_copy __va_copy
#endif

int vasprintf(char** pbuffer, const char* format, va_list ap)
{
    int len = -1;

#ifdef va_copy
    va_list ap2;
    va_copy(ap2, ap);
#else
    /* if we have no va_copy, we probably don't need one */
    #define ap2 ap
#endif

#if defined(vxWorks)
    {
        FILE* f;
        /* print to null device to get required buffer length */
        if ((f = fopen("/null","w")) != NULL)
        {
            len = vfprintf(f, format, ap2);
            fclose(f);
        }
    }
#elif defined(_WIN32)
    len = _vscprintf(format, ap2);
#else
    len = vsnprintf(NULL, 0, format, ap2);
#endif

#ifdef va_copy
    va_end(ap2);
#endif
    
    if (len <= 0)
    {
        fprintf(stderr, "vasprintf: error calculating needed size\n");
        return -1;
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

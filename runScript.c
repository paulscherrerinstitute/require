#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <macLib.h>
#include <epicsVersion.h>

#ifdef vxWorks
#include "asprintf.h"
#ifdef _WRS_VXWORKS_MAJOR
/* vxWorks 6+ */
#include <private/shellLibP.h>
#else
/* vxWorks 5 */
#include <shellLib.h>
#include "strdup.h"
#endif
#endif

#ifdef BASE_VERSION
#define EPICS_3_13
extern char** ppGlobalEnviron;
#else
#include <iocsh.h>
epicsShareFunc int epicsShareAPI iocshCmd(const char *cmd);
#include <epicsExport.h>
#endif

#include "require.h"

int runScriptDebug=0;

static int parseExpr(const char** pp, int* v);

static int parseValue(const char** pp, int* v)
{
    int val;
    const char *p = *pp;
    int neg = 0;

    while (isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') neg = *p++ == '-';
    while (isspace((unsigned char)*p)) p++;
    if (*p == '(')
    {
        p++;
        if (!parseExpr(&p, &val)) return 0;
        if (*p++ != ')') return 0;
    }
    else
    {
        char* e;
        val = strtol(p, &e, 0);
        if (e == p) return 0;
        p = e;
    }
    if (neg) val = -val;
    if (*p == '?')
    {
        p++;
        val = (val != 0);
    }
    *pp = p;
    *v = val;
    if (runScriptDebug > 1) printf("parseValue: %d rest=\"%s\"\n", *v, p);
    return 1;
}

static int parseExpr(const char** pp, int* v)
{
    const char *p = *pp;
    const char *q;
    int o;
    int val;
    int val2;
    int status = 0;

    *v = 0;
    do {
        if (!parseValue(&p, &val)) return status;
        if (runScriptDebug > 1) printf("parseExp val=%d rest=%s\n", val, p);
        q = p;
        while (isspace((unsigned char)*q)) q++;
        o = *q;
        while (o == '*' || o == '/' || o == '%')
        {
            q++;
            if (!parseValue(&q, &val2)) break;
            if (o == '*') val *= val2;
            else if (val2 == 0) val = 0;
            else if (o == '/') val /= val2;
            else val %= val2;
            p = q;
            while (isspace((unsigned char)*p)) p++;
            o = *p;
        }
        status = 1;
        *v += val;
        *pp = p;
        if (runScriptDebug > 1) printf("parseExpr: sum %d rest=\"%s\"\n", *v, p);
    } while (o == '+' || o == '-');
    return 1;
}

const char* getFormat(const char** pp)
{
    static char format [20];
    const char* p = *pp;
    int i = 1;
    if (runScriptDebug > 1) printf ("getFormat %s\n", p);
    if ((format[0] = *p++) == '%')
    {
        if (runScriptDebug > 1) printf ("getFormat0 %s\n", p);
        while (i < sizeof(format) && strchr(" #-+0", *p))
            format[i++] = *p++;
        if (runScriptDebug > 1) printf ("getFormat1 %s\n", p);
        while (i < sizeof(format) && strchr("0123456789", *p))
            format[i++] = *p++;
        if (runScriptDebug > 1) printf ("getFormat2 %s\n", p);
        if (i < sizeof(format) && strchr("diouxXc", *p))
        {
            format[i++] = *p++;
            format[i] = 0;
            *pp = p;
            if (runScriptDebug > 1) printf ("format=%s\n", format);
            return format;
        }
    }
    if (runScriptDebug > 1) printf ("no format\n");
    return NULL;
}

int runScript(const char* filename, const char* args)
{
    MAC_HANDLE *mac = NULL;
    FILE* file = NULL;
    char* line_raw = NULL;
    char* line_exp = NULL;
    long line_raw_size = 256;
    long line_exp_size = line_raw_size;
    char** pairs;
    int status = 0;
    
    if (!filename)
    {
        fprintf(stderr, "Usage: runScript filename [macro=value,...]\n");
        return -1;
    }
    
    pairs = (char*[]){ "", "environ", NULL, NULL };

    if ((file = fopen(filename, "r")) == NULL) { perror(filename); return errno; }
    if (macCreateHandle(&mac, pairs) != 0) goto error;
    macSuppressWarning(mac, 1);
    #ifdef EPICS_3_13
    /* Have no environment macro substitution, thus load envionment explicitly */
    /* Actually, environmant macro substitution was introduced in 3.14.3 */
    for (pairs = ppGlobalEnviron; *pairs; pairs++)
    {
        char* var, *eq;
        if (runScriptDebug)
            printf("runScript: environ %s\n", *pairs);

        /* take a copy to replace '=' with null byte */
        if ((var = strdup(*pairs)) == NULL) goto error;
        eq = strchr(var, '=');
        if (eq)
        {
            *eq = 0;
            macPutValue(mac, var, eq+1);
        }
        free(var);            
    }
    #endif

    if (args)
    {
        if (runScriptDebug)
            printf("runScript: macParseDefns \"%s\"\n", args);
        macParseDefns(mac, (char*)args, &pairs);
        macInstallMacros(mac, pairs);
        free(pairs);
    }

    /*  line by line after expanding macros with arguments or environment */
    if ((line_raw = malloc(line_raw_size)) == NULL) goto error;
    if ((line_exp = malloc(line_exp_size)) == NULL) goto error;
    while (fgets(line_raw, line_raw_size, file))
    {
        char* p, *x;
        long len;

        /* check if we have a line longer than the buffer size */
        while (line_raw[(len = (long)strlen(line_raw))-1] != '\n' && !feof(file))
        {
            if (runScriptDebug)
                    printf("runScript partial line: \"%s\"\n", line_raw);
            if ((line_raw = realloc(line_raw, line_raw_size *= 2)) == NULL) goto error;
            if (fgets(line_raw + len, line_raw_size - len, file) == NULL) break;
        }
        if (line_raw[len-1] == '\n') line_raw[--len] = 0; /* get rid of '\n' */
        if (runScriptDebug)
                printf("runScript raw line (%ld chars): '%s'\n", len, line_raw);
        /* expand and check the buffer size (different epics versions write different may number of bytes)*/
        while ((len = labs(macExpandString(mac, line_raw, line_exp, 
#ifdef EPICS_3_13
        /* 3.13 version of macExpandString is broken and may write more than allowed */
                line_exp_size/2))) >= line_exp_size/2)
#else       
                line_exp_size-1))) >= line_exp_size-2)
#endif
        {
            if (runScriptDebug)
                printf("runScript: grow expand buffer: len=%ld size=%ld\n", len, line_exp_size);
            free(line_exp);
            if ((line_exp = malloc(line_exp_size *= 2)) == NULL) goto error;
        }
        if (runScriptDebug)
                printf("runScript expanded line (%ld chars): '%s'\n", len, line_exp);
        printf("%s\n", line_exp);
        p = line_exp;
        while (isspace((unsigned char)*p)) p++;
        if (*p == 0 || *p == '#') continue;
        
        /* find local variable assignments */
        if ((x = strpbrk(p, "=(, \t\n")) != NULL && *x=='=')
        {
            const char* r;
            char* w;
            int val;

            *x++ = 0;
            r = x;
            w = line_raw;
            while (*r)
            {
                if (runScriptDebug > 1) printf ("expr %s\n", r);
                if (*r == '%')
                {
                    const char* r2 = r;
                    const char* f;
                    if ((f = getFormat(&r2)) && parseExpr(&r2, &val))
                    {
                        w += sprintf(w, f , val);
                        r = r2;
                    }
                    else
                    {
                        if (runScriptDebug > 1) printf ("skip %c\n", *r);
                        *w++ = *r++;
                    }
                    continue;
                }
                if (parseExpr(&r, &val))
                {
                    if (runScriptDebug > 1) printf ("val=%d, rest=%s\n", val, r);
                    w += sprintf(w, "%d", val);
                    if (runScriptDebug > 1) printf ("rest=%s\n", r);
                }
                else if (*r == '(' || *r == '+')
                {
                    if (runScriptDebug > 1) printf ("skip %c\n", *r);
                    *w++ = *r++;
                    continue;
                }
                while (1)
                {
                    if ((*r >= '0' && *r <= '9') || *r == '(' || *r == '%') break;
                    if (*r == '"' || *r == '\'')
                    {
                        char c = *r++;
                        if (runScriptDebug > 1) printf ("string %c\n", c);
                        while (*r && *r != c) {
                            *w++ = *r++;
                        }
                        *w = 0;
                        if (*r) r++;
                        if (*r == '+')
                        {
                            if (runScriptDebug > 1) printf ("skip %c\n", *r);
                            *w++ = *r++;
                        }
                        break;
                    }
                    if (runScriptDebug > 1) printf ("copy %c\n", *r);
                    if (!(*w++ = *r)) break;
                    r++;
                };
            }
            if (runScriptDebug)
                printf("runScript: assign %s=%s\n", p, line_raw);
            macPutValue(mac, p, line_raw);
            continue;
        }
#ifdef _WRS_VXWORKS_MAJOR
        if (strlen(line_exp) >= 255)
        {
            fprintf(stderr, "runScript: Line too long (>=255):\n%s\n", line_exp);
            return -1;
        }
        else
        {
            SHELL_EVAL_VALUE result;
            status = shellInterpEvaluate(line_exp, "C", &result);
        }
#elif defined(vxWorks)
        if (strlen(line_exp) >= 120)
        {
            fprintf(stderr, "runScript: Line too long (>=120):\n%s\n", line_exp);
            return -1;
        }
        status = execute(line_exp);
#else
        if (runScriptDebug)
            printf("runScript: iocshCmd: '%s'\n", line_exp);
        status = iocshCmd(line_exp);
#endif
        if (status != 0) break;
    }
    goto end;
error:
    if (errno)
    {
        status = errno;
        perror("runScript");
    }
end:
    free(line_raw);
    free(line_exp);
    if (mac) macDeleteHandle(mac);
    if (file) fclose(file);
    return status;
}

#ifndef EPICS_3_13
epicsExportAddress(int, runScriptDebug);

static const iocshArg runScriptArg0 = { "filename", iocshArgString };
static const iocshArg runScriptArg1 = { "substitutions", iocshArgString };
static const iocshArg * const runScriptArgs[2] = { &runScriptArg0, &runScriptArg1 };
static const iocshFuncDef runScriptDef = { "runScript", 2, runScriptArgs };
static void runScriptFunc (const iocshArgBuf *args)
{
    runScript(args[0].sval, args[1].sval);
}

static void runScriptRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister (&runScriptDef, runScriptFunc);
    }
}
epicsExportRegistrar(runScriptRegister);
#endif

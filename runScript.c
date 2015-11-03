#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <macLib.h>
#include <epicsVersion.h>

#ifdef vxWorks
extern int execute(const char*);
#endif

#ifdef BASE_VERSION
#define EPICS_3_13
#include <strdup.h>
extern char** environ;
#else
#include <iocsh.h>
epicsShareFunc int epicsShareAPI iocshCmd(const char *cmd);
#include <epicsExport.h>
#endif

#include "require.h"

int runScriptDebug=0;
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
    for (pairs = environ; *pairs; pairs++)
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
        const unsigned char* p;
        long len;

        /* check if we have a line longer than the buffer size */
        while (line_raw[(len = (long)strlen(line_raw))-1] != '\n' && !feof(file))
        {
            if (runScriptDebug)
                    printf("runScript partial line: \"%s\"\n", line_raw);
            if ((line_raw = realloc(line_raw, line_raw_size *= 2)) == NULL) goto error;
            if (fgets(line_raw + len, line_raw_size - len, file) == NULL) break;
        }
        line_raw[--len] = 0; /* get rid of '\n' */
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
        printf("%s\n", line_exp);
        p=(unsigned char*)line_exp;
        while (isspace(*p)) p++;
        if (*p == 0 || *p == '#') continue;
#ifdef vxWorks
        status = execute(line_exp);
#else
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

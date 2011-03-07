#include <vxWorks.h>
#include <symLib.h>
#include <sysSymTbl.h>
#include <sysLib.h>
#include <symLib.h>
#include <loadLib.h>
#include <shellLib.h>
#include <usrLib.h>
#include <taskLib.h>
#include <stat.h>
#include <stdio.h>
#include <ioLib.h>
#include <string.h>
#include <ctype.h>
#include <require.h>
#include <epicsVersion.h>
#ifdef BASE_VERSION
#define EPICS_3_13
int dbLoadDatabase(char *filename, char *path, char *substitutions);
#else
#define EPICS_3_14
#include <iocsh.h>
extern int iocshCmd (const char *cmd);
#include <dbAccess.h>
#include <epicsExit.h>
#include <epicsExport.h>
#endif

static int validate(char* version, char* loaded)
{
    int lmajor, lminor, lpatch, lmatches;
    int major, minor, patch, matches;
    
    if (!version || !*version || strcmp(loaded, version) == 0)
    {
        /* no version requested or exact match */
        return OK;
    }
    if (strcmp(loaded, "test") == 0)
    {
        /* test version already loaded */
        printf("Warning: version is test where %s was requested\n",
            version);
        return OK;
    }
    /* non-numerical versions must match exactly
       numerical versions must have exact match in major version and
       backward-compatible match in minor version and patch level
    */

    lmatches = sscanf(loaded, "%d.%d.%d", &lmajor, &lminor, &lpatch);
    matches = sscanf(version, "%d.%d.%d", &major, &minor, &patch);
    if (((matches == 0 || lmatches == 0) &&
            strcmp(loaded, version) != 0)             
        || major != lmajor
        || (matches >= 2 && minor > lminor)
        || (matches > 2 && minor == lminor && patch > lpatch))
    {
        return ERROR;
    }
    return OK;
}

int require(char* lib, char* vers)
{
    char** path;
    char* loaded;
    SYM_TYPE type;
    struct stat filestat;
    char version[20];
    int fd;
    MODULE_ID libhandle = NULL;
    
    if (symFindByName(sysSymTbl, "LIB", (char**)&path, &type) != OK)
    {
        static char* here = ".";
        path = &here;
    }
    if (!lib)
    {
        printf("Usage: require \"<libname>\" [, \"<version>\"]\n");
        printf("Loads <libname>Lib[-<version>] and dbd/<libname>[-<version>].dbd\n");
        printf("Directory is LIB = %s\n", *path);
        return ERROR;
    }
    
    bzero(version, sizeof(version));
    if (vers) strncpy(version, vers, sizeof(version));
    
    loaded = getLibVersion(lib);
    if (!loaded)
    {
        char libname[256];
        char dbdname[256];
        char depname[256];

        if (version[strlen(version)-1] == '+')
        {
            char* p = strrchr(version, '.');
            if (!p) p = version;
            *p = 0;
        }
        
        /* try to find module in local /bin directory first, then in path
           prefer *Lib.munch file over *Lib file
           check for dependencies in *.dep file
           load *.dbd file if it exists
        */

        /* first try local library */
        if (version && *version)
        {
            sprintf(libname, "bin/%sLib-%s.munch", lib, version);
            sprintf(depname, "bin/%s-%s.dep", lib, version);
            sprintf(dbdname, "dbd/%s-%s.dbd", lib, version);
        }
        else
        {
            sprintf(libname, "bin/%sLib.munch", lib);
            sprintf(depname, "bin/%s.dep", lib);
            sprintf(dbdname, "dbd/%s.dbd", lib);
        }
        if (stat(libname, &filestat) == ERROR)
        {
            /* no munched local lib */
            libname[strlen(libname)-6]=0;  /* skip ".munch" */
        }
        if (stat(libname, &filestat) == ERROR)
        {
            /* no local lib at all */
            libname[strlen(libname)-6]=0;  /* skip ".munch" */
            if (version && *version)
            {
                sprintf(libname, "%s/%sLib-%s.munch", *path, lib, version);
                sprintf(depname, "%s/%s-%s.dep", *path, lib, version);
                sprintf(dbdname, "%s/dbd/%s-%s.dbd", *path, lib, version);
            }
            else
            {
                sprintf(libname, "%s/%sLib.munch", *path, lib);
                sprintf(depname, "%s/%s.dep", *path, lib);
                sprintf(dbdname, "%s/dbd/%s.dbd", *path, lib);
            }
            if (stat(libname, &filestat) == ERROR)
            {
                /* no munched lib */
                libname[strlen(libname)-6]=0;  /* skip ".munch" */
            }
            if (stat(libname, &filestat) == ERROR &&
                /* allow alias without library */
                stat(depname, &filestat) == ERROR)
            {
                /* still no library found */
                printf("Library %s not found\n", libname);
                printf("Aborting startup stript.\n");
                shellScriptAbort();
                return ERROR;
            }
        }
        errno = 0;
        
        /* check dependencies */
        if (stat(depname, &filestat) != ERROR)
        {
            FILE* depfile;
            char buffer[40];
            char *l; /* required library */
            char *v; /* required version */
            char *e; /* end */
            
            depfile = fopen(depname, "r");
            while (fgets(buffer, sizeof(buffer), depfile))
            {
                l = buffer;
                while (isspace((int)*l)) l++;
                if (*l == 0 || *l == '#') continue;
                v = l;
                while (*v && !isspace((int)*v)) v++;
                *v++ = 0;
                while (isspace((int)*v)) v++;
                e = v;
                while (*e && !isspace((int)*e)) e++;
                *e++ = '+';
                *e = 0;
                printf ("%s depends on %s %s\n", lib, l, v);
                if (require(l, v) != OK)
                {
                    fclose(depfile);
                    return ERROR;
                }
            }
            fclose(depfile);
        }
        
        /* load library */
        printf("Loading %s\n", libname);
        fd = open(libname, O_RDONLY, 0);
        if (fd >= 0)
        {
            errno = 0;
            libhandle = loadModule(fd, LOAD_GLOBAL_SYMBOLS);
            close (fd);
        }
        if (libhandle == NULL || errno == S_symLib_SYMBOL_NOT_FOUND)
        {
            printf("Loading %s library failed: %s\n", lib, strerror(errno));
            printf("Aborting startup stript.\n");
            shellScriptAbort();
            return ERROR;
        }
        loaded = getLibVersion(lib);

        /* load dbd file */
        if (stat(dbdname, &filestat) != ERROR && filestat.st_size > 0)
        {
            /* If file exists and is not empty */
            printf("Loading %s\n", dbdname);
            if (dbLoadDatabase(dbdname, NULL, NULL) != OK)
            {
                taskDelay(sysClkRateGet());
                printf ("Aborting startup stript.\n");
                shellScriptAbort();
                return ERROR;
            }
#ifdef EPICS_3_14
            /* call register function for R3.14 */
            {
                char initfunc[256];

                sprintf (initfunc, "%s_registerRecordDeviceDriver", lib);
                printf("calling %s\n", initfunc);
                iocshCmd (initfunc);
            }
#endif        
        }
        if (validate(vers, loaded) == ERROR)
        {
            printf("Requested %s version %s not available, found only %s.\n"
                "Aborting startup stript.\n",
                lib, vers, loaded);
            shellScriptAbort();
            return ERROR;
        }
        if (loaded) printf("%s version is %s\n", lib, loaded);
        return OK;
    }
    else
    {
        /* Library already loaded. Check Version. */
        if (validate(version, loaded) == ERROR)
        {
            printf("Conflict between requested %s version %s\n"
                "and already loaded version %s.\n"
                "Aborting startup stript.\n",
                lib, version, loaded);
            shellScriptAbort();
            return ERROR;
        }
        /* Loaded version is ok */
        printf("%sLib-%s already loaded\n", lib, loaded);
        return OK;
    }
}

char* getLibVersion(char* lib)
{
    char symbol[256];
    char* loaded;
    SYM_TYPE type;
    
    sprintf(symbol, "_%sLibRelease", lib);
    if (symFindByName(sysSymTbl, symbol, &loaded, &type) != OK) return NULL;
    return loaded;
}

static BOOL printIfLibversion(char* name, int val,
    SYM_TYPE type, int arg, UINT16 group)
{
    int l;
    char* pattern = (char*) arg;
    
    l = strlen(name);
    if (l > 10 && strcmp(name+l-10, "LibRelease") == 0)
    {
        if (pattern && !strstr(name, pattern)) return TRUE;
        printf("%15.*s %s\n", l-11, name+1, (char*)val);
    }
    return TRUE;
}

int libversionShow(char* pattern)
{
    symEach(sysSymTbl, (FUNCPTR)printIfLibversion, (int)pattern);
    return OK;
}

#ifdef EPICS_3_14
static const iocshArg requireArg0 = { "module", iocshArgString };
static const iocshArg requireArg1 = { "version", iocshArgString };
static const iocshArg * const requireArgs[2] = { &requireArg0, &requireArg1 };
static const iocshFuncDef requireDef = { "require", 2, requireArgs };
static void requireFunc (const iocshArgBuf *args)
{
    if (require (args[0].sval, args[1].sval) != 0
        && !interruptAccept)
    {
        /* require failed in startup script before iocInit */
        fprintf (stderr, "Aborting startup script\n");
        epicsExit (1);
    }
}

static void requireRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        iocshRegister (&requireDef, requireFunc);
        firstTime = 0;
    }
}

epicsExportRegistrar(requireRegister);

#endif

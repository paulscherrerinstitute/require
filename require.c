/*
* ld - load code dynamically
*
* $Author: zimoch $
* $ID$
* $Date: 2015/04/10 10:06:44 $
*
* DISCLAIMER: Use at your own risc and so on. No warranty, no refund.
*/
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <epicsVersion.h>
#ifdef BASE_VERSION
#define EPICS_3_13
#define epicsStdoutPrintf printf
int dbLoadDatabase(char *filename, char *path, char *substitutions);
extern volatile int interruptAccept;
#else
#define EPICS_3_14
#include <iocsh.h>
#include <dbAccess.h>
epicsShareFunc int epicsShareAPI iocshCmd (const char *cmd);
#include <epicsExit.h>
#include <epicsStdio.h>
#include <epicsExport.h>
#endif

#include "require.h"

int requireDebug=0;

static int firstTime = 1;

#define DIRSEP "/"
#define PATHSEP ":"
#define PREFIX
#define INFIX

#if defined (vxWorks)

    #include <symLib.h>
    #include <sysSymTbl.h>
    #include <sysLib.h>
    #include <symLib.h>
    #include <loadLib.h>
    #include <shellLib.h>
    #include <usrLib.h>
    #include <taskLib.h>
    #include <ioLib.h>
    #include <errno.h>

    #define HMODULE MODULE_ID
    #undef  INFIX
    #define INFIX "Lib"
    #define EXT ".munch"
    
    #define getAddress(module,name) __extension__ \
        ({SYM_TYPE t; char* a=NULL; symFindByName(sysSymTbl, (name), &a, &t); a;})

#elif defined (__unix)

    #define __USE_GNU
    #include <dlfcn.h>
    #define HMODULE void *

    #define getAddress(module,name) (dlsym(module, name))

    #ifdef CYGWIN32

        #define EXT ".dll"

    #else

        #undef  PREFIX
        #define PREFIX "lib"
        #define EXT ".so"

    #endif

#elif defined (_WIN32)

    #include <windows.h>
    #undef  DIRSEP
    #define DIRSEP "\\"
    #undef  PATHSEP
    #define PATHSEP ";"
    #define EXT ".dll"

    #define getAddress(module,name) (GetProcAddress(module, name))
#else

    #warning unknwn OS
    #define getAddress(module,name) NULL

#endif

/* loadlib (library)
Find a loadable library by name and load it.
*/

static HMODULE loadlib(const char* libname)
{
    HMODULE libhandle = NULL;

    if (!libname)
    {
        fprintf (stderr, "missing library name\n");
        return NULL;
    }

#if defined (__unix)
    if (!(libhandle = dlopen(libname, RTLD_NOW|RTLD_GLOBAL)))
    {
        fprintf (stderr, "Loading %s library failed: %s\n",
            libname, dlerror());
    }
#elif defined (_WIN32)
    if (!(libhandle = LoadLibrary(libname)))
    {
        LPVOID lpMsgBuf;

        FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL );
        fprintf (stderr, "Loading %s library failed: %s\n",
            libname, lpMsgBuf);
        LocalFree(lpMsgBuf);
    }
#elif defined (vxWorks)
    {
        int fd, loaderror;
        fd = open(libname, O_RDONLY, 0);
        loaderror = errno;
        if (fd >= 0)
        {
            errno = 0;
            libhandle = loadModule(fd, LOAD_GLOBAL_SYMBOLS);
            if (errno == S_symLib_SYMBOL_NOT_FOUND)
            {
                libhandle = NULL;
            }
            loaderror = errno;
            close (fd);
        }
        if (libhandle == NULL)
        {
            fprintf(stderr, "Loading %s library failed: %s\n",
                libname, strerror(loaderror));
        }
    }
#else
    fprintf (stderr, "cannot load libraries on this OS.\n");
#endif    
    return libhandle;
}

typedef struct moduleitem
{
    struct moduleitem* next;
    char name[100];
    char version[20];
} moduleitem;

moduleitem* loadedModules = NULL;

static void registerModule(const char* module, const char* version)
{
    moduleitem* m = (moduleitem*) calloc(sizeof (moduleitem),1);
    if (!m)
    {
        printf ("require: out of memory\n");
    }
    else
    {
        strncpy (m->name, module, sizeof(m->name));
        strncpy (m->version, version, sizeof(m->version));
        m->next = loadedModules;
        loadedModules = m;
    }
}

#if defined (vxWorks)
BOOL findLibRelease (
    char      *name,  /* symbol name       */
    int       val,    /* value of symbol   */
    SYM_TYPE  type,   /* symbol type       */
    int       arg,    /* user-supplied arg */
    UINT16    group   /* group number      */
) {
    char libname [20];
    int e;
    if (name[0] != '_') return TRUE;
    e = strlen(name) - 10;
    if (e <= 0 || e > 20) return TRUE;
    if (strcmp(name+e, "LibRelease") != 0) return TRUE;
    strncpy(libname, name+1, e-1);
    libname[e-1]=0;
    if (!getLibVersion(libname))
    {
        registerModule(libname, (char*)val);
    }
    return TRUE;
}

static void registerExternalModules()
{
    symEach(sysSymTbl, (FUNCPTR)findLibRelease, 0);
}

#elif defined (__linux)
#include <link.h>

int findLibRelease (
    struct dl_phdr_info *info, /* shared library info */
    size_t size,               /* size of info structure */
    void *data                 /* user-supplied arg */
) {
    void *handle;
    char symname [80];
    const char* p;
    char* q;
    char* version;
    
    if (!info->dlpi_name || !info->dlpi_name[0]) return 0;
    p = strrchr(info->dlpi_name, '/');
    if (p) p+=4; else p=info->dlpi_name + 3;
    symname[0] = '_';
    for (q=symname+1; *p && *p != '.' && *p != '-' && q < symname+11; p++, q++) *q=*p;
    strcpy(q, "LibRelease");
    handle = dlopen(info->dlpi_name, RTLD_NOW|RTLD_GLOBAL);
    version = dlsym(handle, symname);
    dlclose(handle);
    *q = 0;
    if (version)
    {
        registerModule(symname+1, version);
    }
    return 0;
}

static void registerExternalModules()
{
    dl_iterate_phdr(findLibRelease, NULL);
}

#elif defined (_WIN32)

static void registerExternalModules()
{
    ;
}


#else
static void registerExternalModules()
{
    ;
}
#endif


const char* getLibVersion(const char* libname)
{
    moduleitem* m;

    for (m = loadedModules; m; m=m->next)
    {
        if (strncmp(m->name, libname, sizeof(m->name)) == 0)
        {
            return m->version;
        }
    }
    return NULL;
}

int libversionShow(const char* pattern)
{
    moduleitem* m;
    
    if (firstTime)
    {
        firstTime=0;
        registerExternalModules();
    }

    for (m = loadedModules; m; m=m->next)
    {
        if (pattern && !strstr(m->name, pattern)) return 0;
        epicsStdoutPrintf("%15s %s\n", m->name, m->version);
    }
    return 0;
}

static int validate(const char* module, const char* version, const char* loaded)
{
    int lmajor, lminor, lpatch, lmatches;
    int major, minor, patch, matches;
    
    if (!version || !*version || strcmp(loaded, version) == 0)
    {
        /* no version requested or exact match */
        return 0;
    }
    if (!isdigit((unsigned char)loaded[0]))
    {
        /* test version already loaded */
        printf("Warning: %s test version %s already loaded where %s was requested\n",
            module, loaded, version);
        return 0;
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
        return -1;
    }
    return 0;
}

/* require (module)
Look if module is already loaded.
If module is already loaded check for version mismatch.
If module is not yet loaded load the library with ld,
load <module>.dbd with dbLoadDatabase (if file exists)
and call <module>_registerRecordDeviceDriver function.

If require is called from the iocsh before iocInit and fails,
it calls epicsExit to abort the application.
*/

/* wrapper to abort statup script */
static int require_priv(const char* module, const char* ver);

int require(const char* module, const char* ver)
{
    if (firstTime)
    {
        firstTime=0;
        registerExternalModules();
    }
    
    if (require_priv(module, ver) != 0 && !interruptAccept)
    {
        /* require failed in startup script before iocInit */
        fprintf(stderr, "Aborting startup script\n");
#ifdef vxWorks
        shellScriptAbort();
#else
        epicsExit(1);
#endif
        return -1;
    }
    return 0;
}

static int require_priv(const char* module, const char* vers)
{
    char* driverpath = ".";
    char version[20];
    const char* loaded;
    struct stat filestat;
    HMODULE libhandle;
    char* p;
    char *end; /* end of string */
    const char sep[1] = PATHSEP;
    
    if (requireDebug)
        printf("require: checking module %s version %s\n",
            module, vers);
    driverpath = getenv("EPICS_DRIVER_PATH");
    if (requireDebug)
        printf("require: searchpath=%s\n",
            driverpath);
    if (!module)
    {
        printf("Usage: require \"<module>\" [, \"<version>\"]\n");
        printf("Loads " PREFIX "<module>" INFIX "[-<version>]" EXT " and dbd/<libname>[-<version>].dbd\n");
#ifdef EPICS_3_14
        printf("And calls <module>_registerRecordDeviceDriver\n");
#endif
        printf("Search path is %s\n", driverpath);
        return -1;
    }
    
    memset(version, 0, sizeof(version));
    if (vers) strncpy(version, vers, sizeof(version));
    
    loaded = getLibVersion(module);
    if (loaded)
    {
        if (requireDebug)
            printf("require: loaded version of %s is %s\n",
                module, loaded);
        /* Library already loaded. Check Version. */
        if (validate(module, version, loaded) != 0)
        {
            printf("Conflict between requested %s version %s\n"
                "and already loaded version %s.\n",
                module, version, loaded);
            return -1;
        }
        /* Loaded version is ok */
        printf ("%s %s already loaded\n", module, loaded);
        return 0;
    }
    else
    {
        char libname[256];
        char dbdname[256];
        char depname[256];
        char libdir[256];
        char fulllibname[256];
        char fulldbdname[256];
        char fulldepname[256];
        char symbolname[256];

        /* user may give a minimal version (e.g. "1.2.4+")
           load highest matching version (here "1.2") and check later
        */
        if (isdigit((unsigned char)version[0]) && version[strlen(version)-1] == '+')
        {
            char* p = strrchr(version, '.');
            if (!p) p = version;
            *p = 0;
        }
        
        /* make filenames with or without version string */
        
        if (version[0])
        {
            sprintf(libname, PREFIX "%s" INFIX "-%s" EXT, module, version);
            sprintf(depname, "%s-%s.dep", module, version);
        }
        else
        {
            sprintf(libname, PREFIX "%s" INFIX EXT, module);
            sprintf(depname, "%s.dep", module);
        }
        if (requireDebug)
        {
            printf("require: libname is %s\n", libname);
            printf("require: depname is %s\n", depname);
        }

        /* search for library in driverpath */
        for (p = driverpath; p != NULL; p = end)
        {            
            end = strchr(p, sep[0]);
            if (end)
            {
                sprintf (libdir, "%.*s", (int)(end-p), p);
                end++;
            }
            else
            {
                sprintf (libdir, "%s", p);
            }
            /* ignore empty driverpath elements */
            if (libdir[0] == 0) continue;

            sprintf (fulllibname, "%s" DIRSEP "%s", libdir, libname);
            sprintf (fulldepname, "%s" DIRSEP "%s", libdir, depname);
            if (requireDebug)
                printf("require: looking for %s\n", fulllibname);
            if (stat(fulllibname, &filestat) == 0) break;
#ifdef vxWorks
            /* now without the .munch */
            fulllibname[strlen(fulllibname)-6] = 0;
            if (requireDebug)
                printf("require: looking for %s\n", fulllibname);
            if (stat(fulllibname, &filestat) == 0) break;
#endif            
            /* allow dependency without library for aliasing */
            if (requireDebug)
                printf("require: looking for %s\n", fulldepname);
            if (stat(fulldepname, &filestat) == 0) break;
        }
        if (!p)
        {
            fprintf(stderr, "Library %s not found in EPICS_DRIVER_PATH=%s\n",
                libname, driverpath);
            return -1;
        }
        if (requireDebug)
            printf("require: found in %s\n", p);
        
        /* parse dependency file if exists */
        if (stat(fulldepname, &filestat) == 0)
        {
            FILE* depfile;
            char buffer[40];
            char *rmodule; /* required module */
            char *rversion; /* required version */
            
            if (requireDebug)
                printf("require: parsing dependency file %s\n", fulldepname);
            depfile = fopen(fulldepname, "r");
            while (fgets(buffer, sizeof(buffer)-1, depfile))
            {
                rmodule = buffer;
                /* ignore leading spaces */
                while (isspace((int)*rmodule)) rmodule++;
                /* ignore empty lines and comment lines */
                if (*rmodule == 0 || *rmodule == '#') continue;
                /* rmodule at start of module name */
                rversion = rmodule;
                /* find end of module name */
                while (*rversion && !isspace((int)*rversion)) rversion++;
                /* terminate module name */
                *rversion++ = 0;
                /* ignore spaces */
                while (isspace((int)*rversion)) rversion++;
                /* rversion at start of version */
                end = rversion;
                /* find end of version */
                while (*end && !isspace((int)*end)) end++;
                /* append + to version to allow newer compaible versions */
                *end++ = '+';
                /* terminate version */
                *end = 0;
                printf("%s depends on %s %s\n", module, rmodule, rversion);
                if (require(rmodule, rversion) != 0)
                {
                    fclose(depfile);
                    return -1;
                }
            }
            fclose(depfile);
        }
        
        if (stat(fulllibname, &filestat) != 0)
        {
            /* no library, dep file was an alias */
            if (requireDebug)
                printf("require: no library to load\n");
            return 0;
        }
        
        /* load library */
        if (requireDebug)
            printf("require: loading library %s\n", fulllibname);
        if (!(libhandle = loadlib(fulllibname)))
        {
            if (requireDebug)
                printf("require: loading failed\n");
            return -1;
        }
        
        /* now check if we got what we wanted (with original version number) */
        sprintf (symbolname, "_%sLibRelease", module);
        loaded = getAddress(libhandle, symbolname);

        if (loaded)
        {
            printf("Loading %s (version %s)\n", fulllibname, loaded);
            /* make sure we get the dbd that matches the library version */
            sprintf(dbdname, "%s-%s.dbd", module, loaded);
        }
        else
        {
            printf("Loading %s (no version)\n", fulllibname);
            sprintf(dbdname, "%s.dbd", module);
            loaded = "(no version)";
        }

        if (validate(module, vers, loaded) != 0)
        {
            fprintf(stderr, "Requested %s version %s not available, found only %s.\n",
                module, vers, loaded);
            return -1;
        }
        
        if (requireDebug)
        {
            printf("require: dbdname is %s\n", dbdname);
        }
        /* look for dbd in . ./dbd ../dbd ../../dbd (relative to lib dir) */
        p = PATHSEP DIRSEP "dbd"
            PATHSEP DIRSEP ".." DIRSEP "dbd"
            PATHSEP DIRSEP ".." DIRSEP ".." DIRSEP "dbd";
        while (p)
        {
            end = strchr(p, sep[0]);
            if (end)
            {
                sprintf(fulldbdname, "%s%.*s" DIRSEP "%s",
                    libdir, (int)(end-p), p, dbdname);
                end++;
            }
            else
            {
                sprintf(fulldbdname, "%s%s" DIRSEP "%s",
                    libdir, p, dbdname);
            }
            if (requireDebug)
                printf("require: Looking for %s\n", fulldbdname);
            if (stat(fulldbdname, &filestat) == 0) break;
            p=end;
        }
        
        /* if dbd file exists and is not empty load it */
        if (p && filestat.st_size > 0)
        {
            printf("Loading %s\n", fulldbdname);
            if (dbLoadDatabase(fulldbdname, NULL, NULL) != 0)
            {
                fprintf (stderr, "require: can't load %s\n", fulldbdname);
                return -1;
            }
            
            /* when dbd is loaded call register function for 3.14 */
#ifdef EPICS_3_14
            sprintf (symbolname, "%s_registerRecordDeviceDriver", module);
            printf ("Calling %s function\n", symbolname);
#ifdef vxWorks
            {
                FUNCPTR f = (FUNCPTR) getAddress(NULL, symbolname);
                if (f)
                    f(pdbbase);
                else
                    fprintf (stderr, "require: can't find %s function\n", symbolname);
            }        
#else
            iocshCmd(symbolname);
#endif
#endif        
        }
        else
        {
            /* no dbd file, but that might be OK */
            printf("no dbd file %s\n", dbdname);
        }
        
        registerModule(module, loaded);
        return 0;
    }
}

#ifdef EPICS_3_14
static const iocshArg requireArg0 = { "module", iocshArgString };
static const iocshArg requireArg1 = { "version", iocshArgString };
static const iocshArg * const requireArgs[2] = { &requireArg0, &requireArg1 };
static const iocshFuncDef requireDef = { "require", 2, requireArgs };
static void requireFunc (const iocshArgBuf *args)
{
    require(args[0].sval, args[1].sval);
}

static const iocshArg libversionShowArg0 = { "pattern", iocshArgString };
static const iocshArg * const libversionArgs[1] = { &libversionShowArg0 };
static const iocshFuncDef libversionShowDef = { "libversionShow", 1, libversionArgs };
static void libversionShowFunc (const iocshArgBuf *args)
{
    libversionShow(args[0].sval);
}

static const iocshArg ldArg0 = { "library", iocshArgString };
static const iocshArg * const ldArgs[1] = { &ldArg0 };
static const iocshFuncDef ldDef = { "ld", 1, ldArgs };
static void ldFunc (const iocshArgBuf *args)
{
    loadlib(args[0].sval);
}

static void requireRegister(void)
{
    if (firstTime) {
        firstTime = 0;
        iocshRegister (&ldDef, ldFunc);
        iocshRegister (&libversionShowDef, libversionShowFunc);
        iocshRegister (&requireDef, requireFunc);
        registerExternalModules();
    }
}

epicsExportRegistrar(requireRegister);
epicsExportAddress(int, requireDebug);
#endif

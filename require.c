/*
* ld - load code dynamically
*
* $Author: zimoch $
* $ID$
* $Date: 2015/06/29 09:47:30 $
*
* DISCLAIMER: Use at your own risc and so on. No warranty, no refund.
*/

#ifdef __unix
/* for vasprintf and dl_iterate_phdr */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

/* for 64 bit (NFS) file systems */
#define _FILE_OFFSET_BITS 64

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <recSup.h>
#include <initHooks.h>
#include <osiFileName.h>
#include <epicsVersion.h>

#ifdef BASE_VERSION
#define EPICS_3_13

#define epicsGetStdout() stdout
extern int dbLoadDatabase(const char *filename, const char *path, const char *substitutions);
int dbLoadRecords(const char *filename, const char *substitutions)
{
    /* This implementation uses EPICS_DB_INCLUDE_PATH */
    return dbLoadDatabase(filename, NULL, substitutions);
}
extern volatile int interruptAccept;

#else /* 3.14+ */

#include <iocsh.h>
#include <dbAccess.h>
/* This prototype is missing in older EPICS versions */
epicsShareFunc int epicsShareAPI iocshCmd(const char *cmd);
#include <epicsExit.h>
#include <epicsStdio.h>
#include <osiFileName.h>
#include <epicsExport.h>

#endif

#include "require.h"

int requireDebug;

#if defined(vxWorks)
    #ifndef OS_CLASS
        #define OS_CLASS "vxWorks"
    #endif

    #include <symLib.h>
    #include <sysSymTbl.h>
    #include <loadLib.h>
    #include <shellLib.h>
    #include <ioLib.h>
    #include <envLib.h>
    #include <epicsAssert.h>
    #include "strdup.h"
    #include "asprintf.h"

    #define HMODULE MODULE_ID
    #define PREFIX
    #define INFIX "Lib"
    #define EXT ".munch"

    #define getAddress(module, name) __extension__ \
        ({SYM_TYPE t; char* a = NULL; symFindByName(sysSymTbl, (name), &a, &t); a;})

    /* vxWorks has no snprintf() */
    #define snprintf(s, maxchars, f, args...) __extension__ \
        ({int printed=sprintf(s, f, ## args); assert(printed < maxchars); printed;})

    /* vxWorks has no realpath() -- at least make directory absolute */
    static char* realpath(const char* path, char* buf)
    {
        size_t len = 0;
        if (!buf) buf = malloc(MAX_FILENAME_LENGTH);
        if (!buf) return NULL;
        if (path[0] != OSI_PATH_SEPARATOR[0])
        {
            getcwd(buf, MAX_FILENAME_LENGTH);
            len = strlen(buf);
            if (len && buf[len-1] != OSI_PATH_SEPARATOR[0])
                buf[len++] = OSI_PATH_SEPARATOR[0];
        }
        strcpy(buf+len, path);
        return buf;
    }

#elif defined(__unix)

    #ifndef OS_CLASS
        #ifdef __linux
            #define OS_CLASS "Linux"
        #endif

        #ifdef SOLARIS
            #define OS_CLASS "solaris"
        #endif
        
        #ifdef __rtems__
            #define OS_CLASS "RTEMS"
        #endif
        
        #ifdef CYGWIN32
            #define OS_CLASS "cygwin32"
        #endif

        #ifdef freebsd
            #define OS_CLASS "freebsd"
        #endif

        #ifdef darwin
            #define OS_CLASS "Darwin"
        #endif

        #ifdef _AIX32
            #define OS_CLASS "AIX"
        #endif
    #endif

    #include <dlfcn.h>
    #define HMODULE void *

    #define getAddress(module, name) dlsym(module, name)

    #ifdef CYGWIN32
        #define PREFIX
        #define INFIX
        #define EXT ".dll"
    #else
        #define PREFIX "lib"
        #define INFIX
        #define EXT ".so"
    #endif

#elif defined (_WIN32)

    #ifndef OS_CLASS
        #define OS_CLASS "WIN32"
    #endif

    #include <windows.h>
    #include <Psapi.h>
    #pragma comment(lib, "kernel32.lib")
    #pragma comment(lib, "psapi.lib")
    #include "asprintf.h"
    #define snprintf _snprintf
    #define setenv(name,value,overwrite) _putenv_s(name,value)
    #define NAME_MAX MAX_PATH

    #define PREFIX
    #define INFIX
    #define EXT ".dll"

    #define getAddress(module, name) GetProcAddress(module, name)
    
    static char* realpath(const char* path, char* buffer)
    {
        int len = MAX_PATH;
        if (buffer == NULL)
        {
            len = GetFullPathName(path, 0, NULL, NULL);
            if (len == 0) return NULL;
            buffer = malloc(len);
            if (buffer == NULL) return NULL;
        }
        GetFullPathName(path, len, buffer, NULL);
        return buffer;
    }
    
#else

    #warning unknown OS
    #define PREFIX
    #define INFIX
    #define EXT
    #define getAddress(module, name) NULL

#endif

/* for readdir: Windows or Posix */
#if defined(_WIN32)
    #define DIR_HANDLE HANDLE
    #define DIR_ENTRY WIN32_FIND_DATA
    #define IF_OPEN_DIR(f) if(snprintf(f+modulediroffs, sizeof(f)-modulediroffs, "\\*.*"), (dir=FindFirstFile(filename, &direntry)) != INVALID_HANDLE_VALUE || (FindClose(dir), 0))
    #define START_DIR_LOOP do
    #define END_DIR_LOOP while(FindNextFile(dir,&direntry)); FindClose(dir);
    #define SKIP_NON_DIR(e) if (!(e.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (e.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) continue;
    #define FILENAME(e) e.cFileName

#else
    #include <dirent.h>
    #define DIR_HANDLE DIR*
    #define IF_OPEN_DIR(f) if ((dir = opendir(f)))
    #define DIR_ENTRY struct dirent*
    #define START_DIR_LOOP while ((errno = 0, direntry = readdir(dir)) != NULL)
    #define END_DIR_LOOP if (!direntry && errno) fprintf(stderr, "error reading directory %s: %s\n", filename, strerror(errno)); if (dir) closedir(dir);
    #ifdef _DIRENT_HAVE_D_TYPE
    #define SKIP_NON_DIR(e) if (e->d_type != DT_DIR && e->d_type != DT_UNKNOWN) continue;
    #else
    #define SKIP_NON_DIR(e)
    #endif
    #define FILENAME(e) e->d_name

#endif

#define LIBDIR "lib" OSI_PATH_SEPARATOR
#define TEMPLATEDIR "db"
 
#define TOSTR(s) TOSTR2(s)
#define TOSTR2(s) #s
const char epicsRelease[] = TOSTR(EPICS_VERSION)"."TOSTR(EPICS_REVISION)"."TOSTR(EPICS_MODIFICATION);
const char epicsBasetype[] = TOSTR(EPICS_VERSION)"."TOSTR(EPICS_REVISION);

#ifndef T_A
#error T_A not defined: Compile with USR_CFLAGS += -DT_A=${T_A}
#endif
const char targetArch[] = TOSTR(T_A);

#ifndef OS_CLASS
#error OS_CLASS not defined: Try to compile with USR_CFLAGS += -DOS_CLASS='"${OS_CLASS}"'
#endif
const char osClass[] = OS_CLASS;

/* loadlib (library)
Find a loadable library by name and load it.
*/

static HMODULE loadlib(const char* libname)
{
    HMODULE libhandle = NULL;

    if (libname == NULL)
    {
        fprintf (stderr, "missing library name\n");
        return NULL;
    }

#if defined (__unix)
    if ((libhandle = dlopen(libname, RTLD_NOW|RTLD_GLOBAL)) == NULL)
    {
        fprintf (stderr, "Loading %s library failed: %s\n",
            libname, dlerror());
    }
#elif defined (_WIN32)
    if ((libhandle = LoadLibrary(libname)) == NULL)
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
#ifndef _WRS_VXWORKS_MAJOR
/* vxWorks 5 */
            if (errno == S_symLib_SYMBOL_NOT_FOUND)
            {
                libhandle = NULL;
            }
#endif
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
    char content[0];
} moduleitem;

static moduleitem* loadedModules = NULL;
static unsigned long moduleCount = 0;
static unsigned long moduleListBufferSize = 1;
static unsigned long maxModuleNameLength = 0;

int putenvprintf(const char* format, ...)
{
    va_list ap;
    char *var;
    char *val;
    int status = 0;

    if (!format) return -1;
    va_start(ap, format);
    if (vasprintf(&var, format, ap) < 0)
    {
        perror("require putenvprintf");
        return errno;
    }
    va_end(ap);

    if (requireDebug)
        printf("require: putenv(\"%s\")\n", var);

    val = strchr(var, '=');
    if (!val) 
    {
        fprintf(stderr, "putenvprintf: string contains no =: %s\n", var);
        status = -1;
    }
    else
    {
#ifdef vxWorks
        if (putenv(var) != 0) /* vxWorks putenv() makes a copy */
        {
            perror("require putenvprintf: putenv failed");
            status = errno;
        }
#else
        *val++ = 0;
        if (setenv(var, val, 1) != 0)
        {
            perror("require putenvprintf: setenv failed");
            status = errno;
        }
#endif
    }
    free(var);
    return status;
}

void pathAdd(const char* varname, const char* dirname)
{
    char* old_path;           

    if (!varname || !dirname) {
        fprintf(stderr, "usage: pathAdd \"ENVIRONMENT_VARIABLE\",\"directory\"\n");
        fprintf(stderr, "       Adds or moves the directory to the front of the ENVIRONMENT_VARIABLE\n");
        fprintf(stderr, "       but after a leading \".\".\n");
        return;
    }

    /* add directory to front */
    old_path = getenv(varname);
    if (old_path == NULL)
        putenvprintf("%s=." OSI_PATH_LIST_SEPARATOR "%s", varname, dirname);
    else
    {
        size_t len = strlen(dirname);
        char* p;

        /* skip over "." at the beginning */
        if (old_path[0] == '.' && old_path[1] == OSI_PATH_LIST_SEPARATOR[0])
            old_path += 2;

        /* If directory is already in path, move it to front */
        p = old_path;
        while ((p = strstr(p, dirname)) != NULL)
        {
            if ((p == old_path || *(p-1) == OSI_PATH_LIST_SEPARATOR[0]) &&
                (p[len] == 0 || p[len] == OSI_PATH_LIST_SEPARATOR[0]))
            {
                if (p == old_path) break; /* already at front, nothing to do */
                memmove(old_path+len+1, old_path, p-old_path-1);
                strcpy(old_path, dirname);
                old_path[len] = OSI_PATH_LIST_SEPARATOR[0];
                if (requireDebug)
                    printf("require: modified %s=%s\n", varname, old_path);
                break;
            }
            p += len;
        }
        if (p == NULL)
            /* add new directory to the front (after "." )*/
            putenvprintf("%s=." OSI_PATH_LIST_SEPARATOR "%s" OSI_PATH_LIST_SEPARATOR "%s",
                 varname, dirname, old_path);
    }
}

static int setupDbPath(const char* module, const char* dbdir)
{
    char* absdir = realpath(dbdir, NULL); /* so we can change directory later safely */
    if (absdir == NULL)
    {
        if (requireDebug)
            printf("require: cannot resolve %s\n", dbdir);
        return -1;
    }

    if (requireDebug)
        printf("require: found template directory %s\n", absdir);

    /* set up db search path environment variables
      <module>_TEMPLATES      template path of <module>
      <module>_DB             template path of <module>
      TEMPLATES               template path of the current module (overwritten)
      EPICS_DB_INCLUDE_PATH   template path of all loaded modules (last in front after ".")
    */

    putenvprintf("%s_DB=%s", module, absdir);
    putenvprintf("%s_TEMPLATES=%s", module, absdir);
    putenvprintf("TEMPLATES=%s", absdir);
    pathAdd("EPICS_DB_INCLUDE_PATH", absdir);
    free(absdir);
    return 0;
}

static int getRecordHandle(const char* namepart, short type, long minsize, DBADDR* paddr)
{
    char recordname[PVNAME_STRINGSZ];

    sprintf(recordname, "%.*s%s", (int)(PVNAME_STRINGSZ-strlen(namepart)-1), getenv("IOC"), namepart);
    if (dbNameToAddr(recordname, paddr) != 0)
    {
        fprintf(stderr, "require: record %s not found\n",
            recordname);
        return -1;
    }
    if (paddr->field_type != type)
    {
        fprintf(stderr, "require: record %s has wrong type %s instead of %s\n",
            recordname, pamapdbfType[paddr->field_type].strvalue, pamapdbfType[type].strvalue);
        return -1;
    }
    if (paddr->no_elements < minsize)
    {
        fprintf(stderr, "require: record %s has not enough elements: %lu instead of %lu\n",
            recordname, paddr->no_elements, minsize);
        return -1;
    }
    if (paddr->pfield == NULL)
    {
        fprintf(stderr, "require: record %s has not yet allocated memory\n",
            recordname);
        return -1;
    }
    return 0;
}

/*
We can fill the records only after they have been initialized, at initHookAfterFinishDevSup.
But use double indirection here because in 3.13 we must
wait until initHooks is loaded before we can register the hook.
*/

static void fillModuleListRecord(initHookState state)
{
    if (state == initHookAfterFinishDevSup) /* MODULES record exists and has allocated memory */
    {
        DBADDR modules, versions, modver;
        int have_modules, have_versions, have_modver;
        moduleitem *m;
        int i = 0;
        long c = 0;
        
        if (requireDebug)
            printf("require: fillModuleListRecord\n");

        have_modules  = (getRecordHandle(":MODULES",  DBF_STRING, moduleCount, &modules) == 0);
        have_versions = (getRecordHandle(":VERSIONS", DBF_STRING, moduleCount, &versions) == 0);
        
        moduleListBufferSize += moduleCount * maxModuleNameLength;
        have_modver   = (getRecordHandle(":MOD_VER",  DBF_CHAR, moduleListBufferSize, &modver) == 0);

        for (m = loadedModules, i = 0; m; m=m->next, i++)
        {
            size_t lm = strlen(m->content)+1;
            if (have_modules)
            {
                if (requireDebug)
                    printf("require: %s[%d] = \"%.*s\"\n",
                    modules.precord->name, i, 
                    MAX_STRING_SIZE-1, m->content);
                sprintf((char*)(modules.pfield) + i * MAX_STRING_SIZE, "%.*s",
                    MAX_STRING_SIZE-1, m->content);
            }
            if (have_versions)
            {
                if (requireDebug)
                    printf("require: %s[%d] = \"%.*s\"\n",
                    versions.precord->name, i, 
                    MAX_STRING_SIZE-1, m->content+lm);
                sprintf((char*)(versions.pfield) + i * MAX_STRING_SIZE, "%.*s",
                    MAX_STRING_SIZE-1, m->content+lm);
            }
            if (have_modver)
            {
                if (requireDebug)
                    printf("require: %s+=\"%-*s%s\"\n",
                        modver.precord->name,
                        (int)maxModuleNameLength, m->content, m->content+lm);
                c += sprintf((char*)(modver.pfield) + c, "%-*s%s\n",
                        (int)maxModuleNameLength, m->content, m->content+lm);
            }
        }
        if (have_modules) dbGetRset(&modules)->put_array_info(&modules, i);
        if (have_versions) dbGetRset(&versions)->put_array_info(&versions, i);
        if (have_modver) dbGetRset(&modver)->put_array_info(&modver, c+1);
    }
}

void registerModule(const char* module, const char* version, const char* location)
{
    moduleitem *m, **pm;
    size_t lm = strlen(module) + 1;
    size_t lv = (version ? strlen(version) : 0) + 1;
    size_t ll = 1;
    char* abslocation = NULL;
    char* argstring = NULL;
    int addSlash=0;
    const char *mylocation;
    static int firstTime = 1;
    
    if (requireDebug)
        printf("require: registerModule(%s,%s,%s)\n", module, version, location);
        
    if (firstTime)
    {
#ifdef EPICS_3_13
        int (*initHookRegister)() = NULL;
        SYM_TYPE type;
        symFindByName(sysSymTbl, "initHookRegister", (char**)&initHookRegister, &type);
        if (initHookRegister)
#endif
        {
            initHookRegister(fillModuleListRecord);
            if (requireDebug)
                printf("require: initHookRegister\n");
        }
        firstTime = 0;
    }
    
    if (!version) version="";
        
    if (location)
    {
        abslocation = realpath(location, NULL);
        if (!abslocation) abslocation = (char*)location;
        ll = strlen(abslocation) + 1;
        /* linux realpath removes trailing slash */
        if (abslocation[ll-1-strlen(OSI_PATH_SEPARATOR)] != OSI_PATH_SEPARATOR[0])
        {
            addSlash = strlen(OSI_PATH_SEPARATOR);
        }
    }
    m = (moduleitem*) malloc(sizeof(moduleitem) + lm + lv + ll + addSlash);
    if (m == NULL)
    {
        fprintf(stderr, "require: out of memory\n");
        return;
    }
    m->next = NULL;
    strcpy (m->content, module);
    strcpy (m->content+lm, version);
    strcpy (m->content+lm+lv, abslocation ? abslocation : "");
    if (addSlash) strcpy (m->content+lm+lv+ll-1, OSI_PATH_SEPARATOR);
    if (abslocation != location) free(abslocation);
    for (pm = &loadedModules; *pm != NULL; pm = &(*pm)->next);
    *pm = m;
    if (lm > maxModuleNameLength) maxModuleNameLength = lm;
    moduleListBufferSize += lv;
    moduleCount++;

    putenvprintf("MODULE=%s", module);
    putenvprintf("%s_VERSION=%s", module, version);
    if (location)
    {
        putenvprintf("%s_DIR=%s", module, m->content+lm+lv);
        pathAdd("SCRIPT_PATH", m->content+lm+lv);
    }
    
    /* only do registration register stuff at init */
    if (interruptAccept) return;
    
    /* create a record with the version string */
    mylocation = getenv("require_DIR");
    if (mylocation == NULL) return;
    if (asprintf(&abslocation, "%s" OSI_PATH_SEPARATOR "db" OSI_PATH_SEPARATOR "moduleversion.template", mylocation) < 0) return;
    if (asprintf(&argstring, "IOC=%.30s, MODULE=%.24s, VERSION=%.39s, MODULE_COUNT=%lu, BUFFER_SIZE=%lu",
        getenv("IOC"), module, version, moduleCount,
        moduleListBufferSize+maxModuleNameLength*moduleCount) < 0) return;
    printf("Loading module info records for %s\n", module);
    dbLoadRecords(abslocation, argstring);
    free(argstring);
    free(abslocation);
}

#if defined (vxWorks)
static BOOL findLibRelease (
    char      *name,  /* symbol name       */
    int       val,    /* value of symbol   */
    SYM_TYPE  type,   /* symbol type       */
    int       arg,    /* user-supplied arg */
    UINT16    group   /* group number      */
) {
    /* find symbols with a name like "_<module>LibRelease" */
    char* module;
    size_t lm;

    if (name[0] != '_') return TRUE;
    lm = strlen(name);
    if (lm <= 10) /* strlen("LibRelease") */ return TRUE;
    lm -= 10;
    if (strcmp(name+lm, "LibRelease") != 0) return TRUE;
    module = strdup(name+1);                  /* remove '_' */
    module[lm-1]=0;                           /* remove "libRelase" */
    if (getLibVersion(module) == NULL)
        registerModule(module, (char*)val, NULL);
    free(module);
    return TRUE;
}

void registerExternalModules()
{
    /* iterate over all symbols */
    symEach(sysSymTbl, (FUNCPTR)findLibRelease, 0);
}

#elif defined (__linux)
/* This is the Linux link.h, not the EPICS link.h ! */
#include <link.h>

static int findLibRelease (
    struct dl_phdr_info *info, /* shared library info */
    size_t size,               /* size of info structure */
    void *data                 /* user-supplied arg */
) {
    void *handle;
    char* location = NULL;
    char* p;
    char* version;
    char* symname;
    char name[NAME_MAX + 11];                               /* get space for library path + "LibRelease" */

    /* find a symbol with a name like "_<module>LibRelease"
       where <module> is from the library name "<location>/lib<module>.so" */
    if (info->dlpi_name == NULL || info->dlpi_name[0] == 0) return 0;  /* no library name */
    strcpy(name, info->dlpi_name);                          /* get a modifiable copy of the library name */
    handle = dlopen(info->dlpi_name, RTLD_LAZY);            /* re-open already loaded library */
    p = strrchr(name, '/');                                 /* find file name part in "<location>/lib<module>.so" */
    if (p) {location = name; *++p=0;} else p=name;          /* terminate "<location>/" (if exists) */
    *(symname = p+2) = '_';                                 /* replace "lib" with "_" */
    p = strchr(symname, '.');                               /* find ".so" extension */
    if (p == NULL) p = symname + strlen(symname);           /* no file extension ? */
    strcpy(p, "LibRelease");                                /* append "LibRelease" to module name */
    version = dlsym(handle, symname);                       /* find symbol "_<module>LibRelease" */
    if (version)
    {
        *p=0; symname++;                                    /* get "<module>" from "_<module>LibRelease" */
        if ((p = strstr(name, "/" LIBDIR)) != NULL) p[1]=0; /* cut "<location>" before LIBDIR */
        if (getLibVersion(symname) == NULL)
            registerModule(symname, version, location);
    }
    dlclose(handle);
    return 0;
}

static void registerExternalModules()
{
    /* iterate over all loaded libraries */
    dl_iterate_phdr(findLibRelease, NULL);
}

#elif defined (_WIN32)

static void registerExternalModules()
{
    HMODULE hMods[100];
    HANDLE hProcess = GetCurrentProcess();
    DWORD cbNeeded;
    char* location = NULL;
    char* p;
    char* version;
    char* symname;
    unsigned int i;
    char name [MAX_PATH+11];                                     /* get space for library path + "LibRelease" */
    
    /* iterate over all loaded libraries */
    if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) return;
    for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
    {
        /* Get the full path to the module's file. */
        if (!GetModuleFileName(hMods[i], name, MAX_PATH)) continue;  /* no library name */
        name[sizeof(name)-1] = 0;                                /* WinXP may not terminate the string */
        p = strrchr(name, '\\');                                 /* find file name part in "<location>/<module>.dll" */
        if (p) { location = name; } else p=name;                 /* find end of "<location>\\" (if exists) */
        symname = p;
        p = strchr(symname, '.');                                /* find ".dll" */
        if (p == NULL) p = symname + strlen(symname);            /* no file extension ? */
        memmove(symname+2, symname, p - symname);                /* make room for 0 and '_' */
        *symname++ = 0;                                          /* terminate "<location>/" */
        *symname = '_';                                          /* prefix module name with '_' */
        strcpy((p+=2), "LibRelease");                            /* append "LibRelease" to module name */

        version = (char*)GetProcAddress(hMods[i], symname);      /* find symbol "_<module>LibRelease" */
        if (version)
        {
            *p=0; symname++;                                     /* get "<module>" from "_<module>LibRelease" */
            if ((p = strstr(name, "\\" LIBDIR)) != NULL) p[1]=0; /* cut "<location>" before LIBDIR */
            if (getLibVersion(symname) == NULL)
                registerModule(symname, version, location);
        }
    }
}


#else
static void registerExternalModules()
{
    ;
}
#endif

size_t foreachLoadedLib(size_t (*func)(const char* name, const char* version, const char* path, void* arg), void* arg)
{
    moduleitem* m;
    int result;

    for (m = loadedModules; m; m=m->next)
    {
        const char* name = m->content;
        const char* version = name + strlen(name)+1;
        const char* path = version + strlen(version)+1;
        result = func(name, version, path, arg);
        if (result) return result;
    }
    return 0;
}

const char* getLibVersion(const char* libname)
{
    moduleitem* m;

    for (m = loadedModules; m; m=m->next)
    {
        if (strcmp(m->content, libname) == 0)
        {
            return m->content+strlen(m->content)+1;
        }
    }
    return NULL;
}

const char* getLibLocation(const char* libname)
{
    moduleitem* m;
    char *v;

    for (m = loadedModules; m; m=m->next)
    {
        if (strcmp(m->content, libname) == 0)
        {
            v = m->content+strlen(m->content)+1;
            return v+strlen(v)+1;
        }
    }
    return NULL;
}

int libversionShow(const char* outfile)
{
    moduleitem* m;
    size_t lm, lv;
    
    FILE* out = epicsGetStdout();

    if (outfile)
    {
        out = fopen(outfile, "w");
        if (out == NULL)
        {
            fprintf(stderr, "can't open %s: %s\n",
                outfile, strerror(errno));
            return -1;
        }
    }
    for (m = loadedModules; m; m=m->next)
    {
        lm = strlen(m->content)+1;
        lv = strlen(m->content+lm)+1;
        fprintf(out, "%-*s%-20s %s\n",
            (int)maxModuleNameLength, m->content,
            m->content+lm, m->content+lm+lv);
    }
    if (fflush(out) < 0 && outfile)
    {
        fprintf(stderr, "can't write to %s: %s\n",
            outfile, strerror(errno));
        return -1;
    }
    if (outfile)
        fclose(out);
    return 0;
}

#define MISMATCH -1
#define EXACT 0
#define MATCH 1
#define TESTVERS 2
#define HIGHER 3

static int compareVersions(const char* found, const char* request)
{
    int found_major, found_minor=0, found_patch=0, found_parts = 0;
    int req_major, req_minor, req_patch, req_parts;
    const char* found_extra;
    const char* req_extra;
    int n;
    
    if (requireDebug)
        printf("require: compareVersions(found=%s, request=%s)\n", found, request);
        
    if (found == NULL || found[0] == 0)                /* no version found: any requested? */
    {
        if (request == NULL || request[0] == 0)
        {
            if (requireDebug)
                printf("require: compareVersions: EXACT both empty\n");
            return EXACT;
        }
        else
        {
            if (requireDebug)
                printf("require: compareVersions: MISMATCH version requested, empty version found\n");
            return MISMATCH;
        }
    }
    n = 0;
    found_parts = sscanf(found, "%d%n.%d%n.%d%n", &found_major, &n, &found_minor, &n, &found_patch, &n);
    found_extra = found + n;
    if (request == NULL || request[0] == 0)            /* no particular version request: match anything */
    {
        if (found_parts == 0 || found_extra[0] != 0)
        {
            if (requireDebug)
                printf("require: compareVersions: TESTVERS nothing requested, test version found\n");
            return TESTVERS;
        }
        else
        {
            if (requireDebug)
                printf("require: compareVersions: MATCH no version requested, numeric version found\n");
            return MATCH;
        }
    }

    if (strcmp(found, request) == 0)
    {
        if (requireDebug)
            printf("require: compareVersions: MATCH exactly\n");
        return EXACT;
    }

    /* Numerical version compare. Format is major.minor.patch
       Numerical requests must have exact match in major and
       backward-compatible number in minor and patch
    */
    n = 0;
    req_parts = sscanf(request, "%d%n.%d%n.%d%n", &req_major, &n, &req_minor, &n, &req_patch, &n);
    req_extra = request + n;
    if (req_parts == 0 || (req_extra[0] != 0 && strcmp(req_extra, "+") != 0))
    {
        if (requireDebug)
            printf("require: compareVersions: MISMATCH test version requested, different version found\n");
        return MISMATCH;
    }
    if (found_parts == 0 || (found_extra[0] != 0 && strcmp(found_extra, "+") != 0))
    {
        if (requireDebug)
            printf("require: compareVersions: TESTVERS numeric requested, test version found");
        if (req_extra[0] == '+')
            return TESTVERS;
        else
            return MISMATCH;
    }
    if (found_major < req_major)
    {
        if (requireDebug)
            printf("require: compareVersions: MISMATCH too low major number\n");
        return MISMATCH;
    }
    if (found_major > req_major)
    {
        if (requireDebug)
            printf("require: compareVersions: HIGHER major number\n");
        return HIGHER;
    }
    if (req_parts == 1)
    {
        if (requireDebug)
            printf("require: compareVersions: MATCH only major number requested\n");
        return MATCH;
    }
    if (found_minor < req_minor)
    {
        if (requireDebug)
            printf("require: compareVersions: MISMATCH minor number too low\n");
        return MISMATCH;
    }
    if (found_minor > req_minor)                        /* minor larger than required */
    {
        if (req_extra[0] == '+')
        {
            if (requireDebug)
                printf("require: compareVersions: MATCH minor number higher than requested with +\n");
            return MATCH;
        }
        else
        {
            if (requireDebug)
                printf("require: compareVersions: HIGHER minor number\n");
            return HIGHER;
        }
    }
    if (req_parts == 2)
    {
        if (requireDebug)
            printf("require: compareVersions: MATCH only major.minor number requested\n");
        return MATCH;
    }
    if (found_patch < req_patch)
    {
        if (requireDebug)
            printf("require: compareVersions: MISMATCH patch level too low\n");
        return MISMATCH;
    }
    if (found_patch == req_patch)
    {
        if (requireDebug)
            printf("require: compareVersions: MATCH patch level matches exactly requested\n");
        return MATCH;
    }
    if (req_extra[0] == '+')
    {
        if (requireDebug)
            printf("require: compareVersions: MATCH patch level higher than requested with +\n");
        return MATCH;
    }
    if (requireDebug)
        printf("require: compareVersions: HIGHER patch level\n");
    return HIGHER; 
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
static int require_priv(const char* module, const char* version, const char* args, const char* versionstr);

int require(const char* module, const char* version, const char* args)
{
    int status;
    char* versionstr;
    static int firstTime = 1;

    if (firstTime)
    {
        firstTime = 0;
        putenvprintf("T_A=%s", targetArch);
        putenvprintf("EPICS_HOST_ARCH=%s", targetArch);
        putenvprintf("EPICS_RELEASE=%s", epicsRelease);
        putenvprintf("EPICS_BASETYPE=%s", epicsBasetype);
        putenvprintf("OS_CLASS=%s", osClass);
    }

    if (module == NULL)
    {
        printf("Usage: require \"<module>\" [, \"<version>\" | \"ifexists\"] [, \"<args>\"]\n");
        printf("Loads " PREFIX "<module>" INFIX EXT " and <libname>.dbd\n");
#ifndef EPICS_3_13
        printf("And calls <module>_registerRecordDeviceDriver\n");
#endif
        printf("If available, runs startup script snippet (only before iocInit)\n");
        return -1;
    }
    
    /* either order for version and args, either may be empty or NULL */
    if (version && strchr(version, '='))
    {
        const char *v = version;
        version = args;
        args = v;
        if (requireDebug)
            printf("require: swap version and args\n");
    }

    if (version && version[0] == 0) version = NULL;

    if (version && strcmp(version, "none") == 0)
    {
        if (requireDebug)
            printf("require: skip version=none\n");
        return 0;
    }

    if (version)
    {
        /* needed for old style only: */
        if (asprintf(&versionstr, "-%s", version) < 0) return errno;
        if (isdigit((unsigned char)version[0]) && version[strlen(version)-1] == '+')
        {
            /*
                user may give a minimal version (e.g. "1.2.4+")
                load highest matching version (here "1.2") and check later
            */
            char* p = strrchr(versionstr, '.');
            if (p == NULL) p = versionstr;
            *p = 0;
        }
    }
    else
        versionstr = "";
    if (requireDebug)
        printf("require: versionstr = \"%s\"\n", versionstr);

    status = require_priv(module, version, args, versionstr);

    if (version) free(versionstr);

    if (status == 0) return 0;
    if (status != -1) perror("require");
    if (interruptAccept) return status;

    /* require failed in startup script before iocInit */
    fprintf(stderr, "Aborting startup script\n");
    #ifdef vxWorks
    shellScriptAbort();
    #else
    epicsExit(1);
    #endif
    return status;
}

static off_t fileSize(const char* filename)
{
    struct stat filestat;
    if (stat(
#ifdef vxWorks
        (char*) /* vxWorks has buggy stat prototype */
#endif
        filename, &filestat) != 0)
    {
        if (requireDebug)
            printf("require: %s does not exist\n", filename);
        return -1;
    }
    switch (filestat.st_mode & S_IFMT)
    {
        case S_IFREG:
            if (requireDebug)
                printf("require: file %s exists, size %lld bytes\n",
                    filename, (unsigned long long)filestat.st_size);
            return filestat.st_size;
        case S_IFDIR:
            if (requireDebug)
                printf("require: directory %s exists\n",
                    filename);
            return 0;
        #ifdef S_IFBLK
        case S_IFBLK:
            if (requireDebug)
                printf("require: %s is a block device\n",
                    filename);
            return -1;
        #endif
        #ifdef S_IFCHR
        case S_IFCHR:
            if (requireDebug)
                printf("require: %s is a character device\n",
                    filename);
            return -1;
        #endif
        #ifdef S_IFIFO
        case S_IFIFO:
            if (requireDebug)
                printf("require: %s is a FIFO/pipe\n",
                    filename);
            return -1;
        #endif
        #ifdef S_IFSOCK
        case S_IFSOCK:
            if (requireDebug)
                printf("require: %s is a socket\n",
                    filename);
            return -1;
        #endif
        default:
            if (requireDebug)
                printf("require: %s is an unknown type of special file\n",
                    filename);
            return -1;
    }
}
#define fileExists(filename) (fileSize(filename)>=0)
#define fileNotEmpty(filename) (fileSize(filename)>0)

static int handleDependencies(const char* module, char* depfilename)
{
    FILE* depfile;
    char buffer[40];
    char *end; /* end of string */
    char *rmodule; /* required module */
    char *rversion; /* required version */

    if (requireDebug)
        printf("require: parsing dependency file %s\n", depfilename);
    depfile = fopen(depfilename, "r");
    while (fgets(buffer, sizeof(buffer)-1, depfile))
    {
        rmodule = buffer;
        /* ignore leading spaces */
        while (isspace((unsigned char)*rmodule)) rmodule++;
        /* ignore empty lines and comment lines */
        if (*rmodule == 0 || *rmodule == '#') continue;
        /* rmodule at start of module name */
        rversion = rmodule;
        /* find end of module name */
        while (*rversion && !isspace((unsigned char)*rversion)) rversion++;
        /* terminate module name */
        *rversion++ = 0;
        /* ignore spaces */
        while (isspace((unsigned char)*rversion)) rversion++;
        /* rversion at start of version */
        
        if (*rversion)
        {
            end = rversion;
            /* find end of version */
            while (*end && !isspace((unsigned char)*end)) end++;

            /* add + to numerial versions if not yet there */
            if (*(end-1) != '+' && strspn(rversion, "0123456789.") == end-rversion) *end++ = '+';

            /* terminate version */
            *end = 0;
        }
        printf("Module %s depends on %s %s\n", module, rmodule, rversion);
        if (require(rmodule, rversion, NULL) != 0)
        {
            fclose(depfile);
            return -1;
        }
    }
    fclose(depfile);
    return 0;
}

static int require_priv(const char* module, const char* version, const char* args,
    const char* versionstr  /* "-<version>" or "" (for old style only */ )
{
    int status;
    const char* loaded = NULL;
    const char* found = NULL;
    HMODULE libhandle;
    int ifexists = 0;
    const char* driverpath;
    const char* dirname;
    const char *end;

    int releasediroffs;
    int libdiroffs;
    int extoffs;
    char* founddir = NULL;
    char* symbolname;
    char filename[NAME_MAX];
    
    int someVersionFound = 0;
    int someArchFound = 0;
    
    static char* globalTemplates = NULL;

    if (requireDebug)
        printf("require: module=\"%s\" version=\"%s\" args=\"%s\"\n", module, version, args);

#if defined __GNUC__ && __GNUC__ < 3
    #define TRY_FILE(offs, args...) \
        (snprintf(filename + offs, sizeof(filename) - offs, args) && fileExists(filename))

    #define TRY_NONEMPTY_FILE(offs, args...) \
        (snprintf(filename + offs, sizeof(filename) - offs, args) && fileNotEmpty(filename))
#else
    #define TRY_FILE(offs, ...) \
        (snprintf(filename + offs, sizeof(filename) - offs, __VA_ARGS__) && fileExists(filename))

    #define TRY_NONEMPTY_FILE(offs, ...) \
        (snprintf(filename + offs, sizeof(filename) - offs, __VA_ARGS__) && fileNotEmpty(filename))
#endif

#if defined (_WIN32)
    /* enable %n in printf */
    _set_printf_count_output(1);
#endif

    driverpath = getenv("EPICS_DRIVER_PATH");
    if (!globalTemplates)
    {
        char *t = getenv("TEMPLATES");
        if (t) globalTemplates = strdup(t);
    }
    
    if (driverpath == NULL) driverpath = ".";
    if (requireDebug)
        printf("require: searchpath=%s\n", driverpath);

    if (version && strcmp(version,"ifexists") == 0)
    {
        ifexists = 1;
        version = NULL;
        versionstr = "";
    }

    /* check already loaded verion */
    loaded = getLibVersion(module);
    if (loaded)
    {
        /* Library already loaded. Check Version. */
        switch (compareVersions(loaded, version))
        {
            case TESTVERS:
                if (version)
                    printf("Warning: Module %s test version %s already loaded where %s was requested\n",
                        module, loaded, version);
            case EXACT:
            case MATCH:
                printf ("Module %s version %s already loaded\n", module, loaded);
                break;
            default:
                printf("Conflict between requested %s version %s and already loaded version %s.\n",
                    module, version, loaded);
                return -1;
        }
        dirname = getLibLocation(module);
        if (dirname[0] == 0) return 0;
        if (requireDebug)
            printf("require: library found in %s\n", dirname);
        snprintf(filename, sizeof(filename), "%s%n", dirname, &releasediroffs);
        putenvprintf("MODULE=%s", module);
        pathAdd("SCRIPT_PATH", dirname);
    }
    else
    {
        if (requireDebug)
            printf("require: no %s version loaded yet\n", module);

        /* Search for module in driverpath */
        for (dirname = driverpath; dirname != NULL; dirname = end)
        {
            /* get one directory from driverpath */
            int dirlen;
            int modulediroffs;
            DIR_HANDLE dir;
            DIR_ENTRY direntry;

            end = strchr(dirname, OSI_PATH_LIST_SEPARATOR[0]);
            if (end && end[1] == OSI_PATH_SEPARATOR[0] && end[2] == OSI_PATH_SEPARATOR[0])   /* "http://..." and friends */
                end = strchr(end+2, OSI_PATH_LIST_SEPARATOR[0]);
            if (end) dirlen = (int)(end++ - dirname);
            else dirlen = (int)strlen(dirname);
            if (dirlen == 0) continue; /* ignore empty driverpath elements */

            if (requireDebug)
                printf("require: trying %.*s\n", dirlen, dirname);

            snprintf(filename, sizeof(filename), "%.*s" OSI_PATH_SEPARATOR "%s" OSI_PATH_SEPARATOR "%n", 
                dirlen, dirname, module, &modulediroffs);
            dirlen++;
            /* filename = "<dirname>/[dirlen]<module>/[modulediroffs]" */

            /* Does the module directory exist? */
            IF_OPEN_DIR(filename)
            {
                if (requireDebug)
                    printf("require: found directory %s\n", filename);
                    
                /* Now look for versions. */
                START_DIR_LOOP
                {
                    char* currentFilename = FILENAME(direntry);
                    
                    SKIP_NON_DIR(direntry)
                    if (currentFilename[0] == '.') continue;  /* ignore hidden directories */

                    someVersionFound = 1;

                    /* Look for highest matching version. */
                    if (requireDebug)
                        printf("require: checking version %s against required %s\n",
                                currentFilename, version);

                    switch ((status = compareVersions(currentFilename, version)))
                    {
                        case EXACT: /* exact match found */
                        case MATCH: /* all given numbers match. */
                        {
                            someArchFound = 1;

                            if (requireDebug)
                                printf("require: %s %s may match %s\n",
                                    module, currentFilename, version);

                            /* Check if it has our EPICS version and architecture. */
                            /* Even if it has no library, at least it has a dep file in the lib dir */

                            /* filename = "<dirname>/[dirlen]<module>/[modulediroffs]" */
                            if (!TRY_FILE(modulediroffs, "%s" OSI_PATH_SEPARATOR "R%s" OSI_PATH_SEPARATOR LIBDIR "%s" OSI_PATH_SEPARATOR,
                                currentFilename, epicsRelease, targetArch))
                            /* filename = "<dirname>/[dirlen]<module>/[modulediroffs]<version>/R<epicsRelease>/lib/<targetArch>/" */
                            {
                                if (requireDebug)
                                    printf("require: %s %s has no support for %s %s\n",
                                        module, currentFilename, epicsRelease, targetArch);
                                continue;
                            }

                            if (status == EXACT)
                            {
                                if (requireDebug)
                                    printf("require: %s %s matches %s exactly\n",
                                        module, currentFilename, version);
                                /* We are done. */
                                end = NULL;
                                break;
                            }

                            /* Is it higher than the one we found before? */
                            if (found && requireDebug)
                                printf("require: %s %s support for %s %s found, compare against previously found %s\n",
                                    module, currentFilename, epicsRelease, targetArch, found);
                            if (!found || compareVersions(currentFilename, found) == HIGHER)
                            {
                                if (requireDebug)
                                    printf("require: %s %s looks promising\n", module, currentFilename);
                                break;
                            }
                            if (requireDebug)
                                printf("require: version %s is lower than %s \n", currentFilename, found);
                            continue;
                        }
                        default:
                        {
                            if (requireDebug)
                                printf("require: %s %s does not match %s\n",
                                    module, currentFilename, version);
                            continue;
                        }
                    }
                    /* we have found something (EXACT or MATCH) */
                    free(founddir);
                    /* filename = "<dirname>/[dirlen]<module>/[modulediroffs]..." */
                    if (asprintf(&founddir, "%.*s%s", modulediroffs, filename, currentFilename) < 0)
                        return errno;
                    /* founddir = "<dirname>/[dirlen]<module>/[modulediroffs]<version>" */
                    found = founddir + modulediroffs; /* version part in the path */
                    if (status == EXACT) break;
                }
                END_DIR_LOOP
            }
            else
            {
                /* filename = "<dirname>/[dirlen]<module>/" */
                if (requireDebug)
                    printf("require: no %s directory\n", filename);

                /* try local/old style module only if no new style candidate has been found */
                if (!found)
                {
                    /* look for dep file */
                    releasediroffs = libdiroffs = dirlen;
                    if (TRY_FILE(dirlen, "%s%s.dep", module, versionstr))
                    /* filename = "<dirname>/[dirlen][releasediroffs][libdiroffs]<module>(-<version>)?.dep" */
                    {
                        if (requireDebug)
                            printf("require: found old style %s\n", filename);
                        printf ("Module %s%s found in %.*s\n", module,
                            versionstr, dirlen, filename);
                        goto checkdep;
                    }

                    /* look for library file */
                    if (TRY_FILE(dirlen, PREFIX "%s" INFIX "%s%n" EXT, module, versionstr, &extoffs)
                    /* filename = "<dirname>/[dirlen][releasediroffs][libdiroffs]PREFIX<module>INFIX(-<version>)?[extoffs]EXT" */
                    #ifdef vxWorks
                        /* try without extension */
                        || (filename[dirlen + extoffs] = 0, fileExists(filename))
                    #endif
                        )
                    {
                        if (requireDebug)
                            printf("require: found old style %s\n", filename);
                        printf ("Module %s%s found in %.*s\n", module,
                            versionstr, dirlen, filename);
                        goto loadlib;
                    }
                }
            }
            /* filename = "<dirname>/[dirlen]..." */
            if (!found && requireDebug)
                printf("require: no matching version in %.*s\n", dirlen, filename);
        }

        if (!found)
        {
            if (someArchFound)
                fprintf(stderr, "Module %s%s%s not available for %s\n(but maybe for other EPICS versions or architectures)\n",
                    module, version ? " version " : "", version ? version : "", targetArch);
            else
            if (someVersionFound)
                fprintf(stderr, "Module %s%s%s not available (but other versions are available)\n",
                    module, version ? " version " : "", version ? version : "");
            else
                fprintf(stderr, "Module %s%s%s not available\n",
                    module, version ? " version " : "", version ? version : "");
            return ifexists ? 0 : -1;
        }

        versionstr = "";

        /* founddir = "<dirname>/[dirlen]<module>/<version>" */
        printf ("Module %s version %s found in %s" OSI_PATH_SEPARATOR "\n", module, found, founddir);

        if (requireDebug)
            printf("require: looking for dependency file\n");

        if (!TRY_FILE(0, "%s" OSI_PATH_SEPARATOR "R%s" OSI_PATH_SEPARATOR "%n" LIBDIR "%s" OSI_PATH_SEPARATOR "%n%s.dep",
            founddir, epicsRelease, &releasediroffs, targetArch, &libdiroffs, module))
        /* filename = "<dirname>/[dirlen]<module>/<version>/R<epicsRelease>/[releasediroffs]/lib/<targetArch>/[libdiroffs]/module.dep" */
        {
            fprintf(stderr, "Dependency file %s not found\n", filename);
        }
        else
        {
checkdep:
            /* filename = "<dirname>/[dirlen]<module>/<version>/R<epicsRelease>/[releasediroffs]/lib/<targetArch>/[libdiroffs]/module.dep" */
            /* or (old)   "<dirname>/[dirlen]][releasediroffs][libdiroffs]<module>(-<version>)?.dep" */
            if (handleDependencies(module, filename) == -1)
                return -1;
        }

        if (requireDebug)
            printf("require: looking for library file\n");

        if (!(TRY_FILE(libdiroffs, PREFIX "%s" INFIX "%s%n" EXT, module, versionstr, &extoffs)
        #ifdef vxWorks
            /* try without extension */
            || (filename[libdiroffs + extoffs] = 0, fileExists(filename))
        #endif
            ))
        /* filename = "<dirname>/[dirlen]<module>/<version>/R<epicsRelease>/[releasediroffs]/lib/<targetArch>/[libdiroffs]/PREFIX<module>INFIX[extoffs](EXT)?" */
        /* or  (old)  "<dirname>/[dirlen][releasediroffs][libdiroffs]PREFIX<module>INFIX(-<version>)?[extoffs](EXT)?" */
        {
            printf("Module %s has no library\n", module);
        }
        else
        {
loadlib:
            /* filename = "<dirname>/[dirlen]<module>/<version>/R<epicsRelease>/[releasediroffs]/lib/<targetArch>/[libdiroffs]/PREFIX<module>INFIX[extoffs]EXT" */
            /* or  (old)  "<dirname>/[dirlen][releasediroffs][libdiroffs]PREFIX<module>INFIX(-<version>)?[extoffs]EXT" */
            printf("Loading library %s\n", filename);
            if ((libhandle = loadlib(filename)) == NULL)
                return -1;

            /* now check what version we really got (with compiled-in version number) */
            if (asprintf (&symbolname, "_%sLibRelease", module) < 0)
                return errno;

            found = (const char*) getAddress(libhandle, symbolname);
            free(symbolname);
            printf("Loaded %s version %s\n", module, found);

            /* check what we got */
            if (requireDebug)
                printf("require: compare requested version %s with loaded version %s\n", version, found);
            if (compareVersions(found, version) == MISMATCH)
            {
                fprintf(stderr, "Requested %s version %s not available, found only %s.\n",
                    module, version, found);
                return -1;
            }

            /* load dbd file */
            if (TRY_NONEMPTY_FILE(releasediroffs, "dbd" OSI_PATH_SEPARATOR "%s%s.dbd", module, versionstr) ||
                TRY_NONEMPTY_FILE(releasediroffs, "%s%s.dbd", module, versionstr) ||
                TRY_NONEMPTY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "dbd" OSI_PATH_SEPARATOR "%s%s.dbd", module, versionstr) ||
                TRY_NONEMPTY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "%s%s.dbd", module, versionstr) ||
                TRY_NONEMPTY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR ".." OSI_PATH_SEPARATOR "dbd" OSI_PATH_SEPARATOR "%s.dbd", module)) /* org EPICSbase */
            {
                printf("Loading dbd file %s\n", filename);
                if (dbLoadDatabase(filename, NULL, NULL) != 0)
                {
                    fprintf (stderr, "Error loading %s\n", filename);
                    return -1;
                }

                #ifndef EPICS_3_13
                /* when dbd is loaded call register function */
                if (asprintf(&symbolname, "%s_registerRecordDeviceDriver", module) < 0)
                    return errno;

                printf ("Calling function %s\n", symbolname);
                #ifdef vxWorks
                {
                    FUNCPTR f = (FUNCPTR) getAddress(NULL, symbolname);
                    if (f)
                        f(pdbbase);
                    else
                        fprintf (stderr, "require: can't find %s function\n", symbolname);
                }        
                #else /* !vxWorks */
                iocshCmd(symbolname);
                #endif /* !vxWorks */
                free(symbolname);
                #endif /* !EPICS_3_13 */
            }
            else
            {
                /* no dbd file, but that might be OK */
                printf("%s has no dbd file\n", module);
            }
        }
        /* register module with path */
        filename[releasediroffs] = 0;
        registerModule(module, found, filename);
    }

    status = 0;       

    if (requireDebug)
        printf("require: looking for template directory\n");
    /* filename = "<dirname>/[dirlen]<module>/<version>/R<epicsRelease>/[releasediroffs]..." */
    if (!((TRY_FILE(releasediroffs, TEMPLATEDIR) ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR TEMPLATEDIR)) && setupDbPath(module, filename) == 0))
    {
        /* if no template directory found, restore TEMPLATES to initial value */
        char *t;
        t = getenv("TEMPLATES");
        if (globalTemplates && (!t || strcmp(globalTemplates, t) != 0))
            putenvprintf("TEMPLATES=%s", globalTemplates);
    }

    if (loaded && args == NULL) return 0; /* no need to execute startup script twice if not with new arguments */

    /* load startup script */
    if (requireDebug)
        printf("require: looking for startup script\n");
    /* filename = "<dirname>/[dirlen]<module>/<version>/R<epicsRelease>/[releasediroffs]db" */
    if (TRY_FILE(releasediroffs, "%s-%s.cmd", targetArch, epicsRelease) ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "%s-%s.cmd", targetArch, epicsRelease) ||
        TRY_FILE(releasediroffs, "%s-%s.cmd", targetArch, epicsBasetype) ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "%s-%s.cmd", targetArch, epicsBasetype) ||
        TRY_FILE(releasediroffs, "%s-%s.cmd", osClass, epicsRelease) ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "%s-%s.cmd", osClass, epicsRelease) ||
        TRY_FILE(releasediroffs, "%s-%s.cmd", osClass, epicsBasetype) ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "%s-%s.cmd", osClass, epicsBasetype) ||
        TRY_FILE(releasediroffs, "startup-%s.cmd", epicsRelease) ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "startup-%s.cmd", epicsRelease) ||
        TRY_FILE(releasediroffs, "startup-%s.cmd", epicsBasetype) ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "startup-%s.cmd", epicsBasetype) ||
        TRY_FILE(releasediroffs, "%s.cmd", targetArch) ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "%s.cmd", targetArch) ||
        TRY_FILE(releasediroffs, "%s.cmd", osClass) ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "%s.cmd", osClass) ||
        TRY_FILE(releasediroffs, "startup.cmd") ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "startup.cmd")
        )
    {
        if (args)
            printf("Executing %s with \"%s\"\n", filename, args);
        else if (interruptAccept)
        {
            printf("Not executing %s after iocInit\n", filename);
            return 0;
        }
        else
            printf("Executing %s\n", filename);
        if (runScript(filename, args) != 0)
            fprintf (stderr, "Error executing %s\n", filename);
        else
            printf("Done with %s\n", filename);
    }
    return status;
}

#ifndef EPICS_3_13
static const iocshFuncDef requireDef = {
    "require", 3, (const iocshArg *[]) {
        &(iocshArg) { "module", iocshArgString },
        &(iocshArg) { "[version]", iocshArgString },
        &(iocshArg) { "[substitutions]", iocshArgString },
}};
    
static void requireFunc (const iocshArgBuf *args)
{
    require(args[0].sval, args[1].sval, args[2].sval);
}

static const iocshFuncDef libversionShowDef = {
    "libversionShow", 1, (const iocshArg *[]) {
        &(iocshArg) { "outputfile", iocshArgString },
}};

static void libversionShowFunc (const iocshArgBuf *args)
{
    libversionShow(args[0].sval);
}

static const iocshFuncDef ldDef = {
    "ld", 1, (const iocshArg *[]) {
        &(iocshArg) { "library", iocshArgString },
}};

static void ldFunc (const iocshArgBuf *args)
{
    loadlib(args[0].sval);
}

static const iocshFuncDef pathAddDef = {
    "pathAdd", 2, (const iocshArg *[]) {
        &(iocshArg) { "ENV_VARIABLE", iocshArgString },
        &(iocshArg) { "directory", iocshArgString },
}};

static void pathAddFunc (const iocshArgBuf *args)
{
    pathAdd(args[0].sval, args[1].sval);
}

static void requireRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        firstTime = 0;
        iocshRegister (&requireDef, requireFunc);
        iocshRegister (&libversionShowDef, libversionShowFunc);
        iocshRegister (&ldDef, ldFunc);
        iocshRegister (&pathAddDef, pathAddFunc);
        registerExternalModules();
    }
}

epicsExportRegistrar(requireRegister);
epicsExportAddress(int, requireDebug);
#endif

/*
* ld - load code dynamically
*
* $Author: zimoch $
* $ID$
* $Date: 2015/06/29 09:47:30 $
*
* DISCLAIMER: Use at your own risc and so on. No warranty, no refund.
*/
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <epicsVersion.h>

#ifdef BASE_VERSION
#define EPICS_3_13
#define epicsGetStdout() stdout
extern int dbLoadDatabase(const char *filename, const char *path, const char *substitutions);
extern int dbLoadTemplate(const char *filename);
int dbLoadRecords(const char *filename, const char *substitutions)
{
    /* This implementation respects EPICS_DB_INCLUDE_PATH */
    return dbLoadDatabase(filename, NULL, substitutions);
}
extern volatile int interruptAccept;
#define OSI_PATH_SEPARATOR "/"
#define OSI_PATH_LIST_SEPARATOR ":"

#else /* 3.14+ */

#include <iocsh.h>
#include <dbAccess.h>
epicsShareFunc int epicsShareAPI iocshCmd(const char *cmd);
#include <epicsExit.h>
#include <epicsStdio.h>
#include <dbLoadTemplate.h>
#include <osiFileName.h>
#include <epicsExport.h>

#endif

#include "require.h"

int requireDebug=0;

#if defined(vxWorks)
    #ifndef OS_CLASS
        #define OS_CLASS "vxWorks"
    #endif

    #include <symLib.h>
    #include <sysSymTbl.h>
    #include <sysLib.h>
    #include <symLib.h>
    #include <loadLib.h>
    #include <shellLib.h>
    #include <usrLib.h>
    #include <taskLib.h>
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

    #warning unknwn OS
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
    #define DIR_ENTRY struct dirent*
    #define IF_OPEN_DIR(f) if ((dir = opendir(f)))
    #define START_DIR_LOOP while ((direntry = readdir(dir)) != NULL)
    #define END_DIR_LOOP if (dir) closedir(dir);
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
#define EPICS_RELEASE TOSTR(EPICS_VERSION)"."TOSTR(EPICS_REVISION)"."TOSTR(EPICS_MODIFICATION)
const char epicsRelease[] = EPICS_RELEASE;

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
    char content[0];
} moduleitem;

moduleitem* loadedModules = NULL;

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

static int runLoadScript(const char* script, const char* module, const char* version)
{
    char *scriptpath = NULL;
    char *subst = NULL;
    const char *mylocation = getenv("require_DIR");
    
    if (requireDebug)
        printf("require: runLoadScript %s, loc=%s\n", script, mylocation);
    if (!mylocation) return 0;
    if (asprintf(&scriptpath, "%s/%s", mylocation, script) < 0) return -1;
    runScript(scriptpath,NULL);
    free(subst);
    free(scriptpath);
    return 0;
}

static int setupDbPath(const char* module, const char* dbdir)
{
    char* old_path;           
    char* p;
    size_t len;

    char* absdir = realpath(dbdir, NULL); /* so we can change directory later safely */
    if (absdir == NULL)
    {
        if (requireDebug)
            printf("require: cannot resolve %s\n", dbdir);
        return -1;
    }
    len = strlen(absdir);

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

    /* add directory to front of EPICS_DB_INCLUDE_PATH */
    old_path = getenv("EPICS_DB_INCLUDE_PATH");
    if (old_path == NULL)
        putenvprintf("EPICS_DB_INCLUDE_PATH=." OSI_PATH_LIST_SEPARATOR "%s", absdir);
    else
    {
        /* skip over "." at the beginning */
        if (old_path[0] == '.' && old_path[1] == OSI_PATH_LIST_SEPARATOR[0])
            old_path += 2;

        /* If directory is already in path, move it to front */
        p = old_path;
        while ((p = strstr(p, absdir)) != NULL)
        {
            if ((p == old_path || *(p-1) == OSI_PATH_LIST_SEPARATOR[0]) &&
                (p[len] == 0 || p[len] == OSI_PATH_LIST_SEPARATOR[0]))
            {
                if (p == old_path) break; /* already at front, nothing to do */
                memmove(old_path+len+1, old_path, p-old_path-1);
                strcpy(old_path, absdir);
                old_path[len] = OSI_PATH_LIST_SEPARATOR[0];
                if (requireDebug)
                    printf("require: modified EPICS_DB_INCLUDE_PATH=%s\n", old_path);
                break;
            }
            p += len;
        }
        if (p == NULL)
            /* add new directory to the front (after "." )*/
            putenvprintf("EPICS_DB_INCLUDE_PATH=." OSI_PATH_LIST_SEPARATOR "%s" OSI_PATH_LIST_SEPARATOR "%s",
                 absdir, old_path);
    }
    free(absdir);
    return 0;
}

void registerModule(const char* module, const char* version, const char* location)
{
    moduleitem* m;
    size_t lm = strlen(module) + 1;
    size_t lv = (version ? strlen(version) : 0) + 1;
    size_t ll = 1;
    char* abslocation;
    
    if (requireDebug)
        printf("require: registerModule(%s,%s,%s)\n", module, version, location);
        
    if (location)
    {
        abslocation = realpath(location, NULL);
        if (!abslocation) abslocation = (char*)location;
        ll +=  strlen(abslocation);
    }
    m = (moduleitem*) malloc(sizeof(moduleitem) + lm + lv + ll);
    if (m == NULL)
    {
        fprintf(stderr, "require: out of memory\n");
        return;
    }
    strcpy (m->content, module);
    strcpy (m->content+lm, version ? version : "");
    strcpy (m->content+lm+lv, abslocation ? abslocation : "");
    m->next = loadedModules;
    loadedModules = m;
    putenvprintf("MODULE=%s", module);
    putenvprintf("%s_VERSION=%s", module, version ? version : "");
    if (abslocation) putenvprintf("%s_DIR=%s", module, abslocation);
    if (abslocation != location) free(abslocation);
    
    /* only do registration register stuff at init */
    if (interruptAccept) return;
    
    if (runLoadScript("postModuleLoad.cmd", module, version) < 0)
    {
        fprintf(stderr, "require: out of memory\n");
        return;
    }
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
    module = strdup(name+1);
    module[lm-1]=0;
    if (getLibVersion(module) == NULL)
    {
        registerModule(module, (char*)val, NULL);
    }
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
    char* name;
    char* location = NULL;
    char* p;
    char* version;
    char* symname;

    /* find a symbol with a name like "_<module>LibRelease"
       where <module> is from the library name "<location>/lib<module>.so" */
    if (info->dlpi_name == NULL || info->dlpi_name[0] == 0) return 0;  /* no library name */
    name = malloc(strlen(info->dlpi_name)+11);              /* get a modifiable copy + space for "LibRelease" */
    if (name == NULL)
    {
        perror("require");
        return 0;
    }
    strcpy(name, info->dlpi_name);
    handle = dlopen(info->dlpi_name, RTLD_LAZY);            /* re-open already loaded library */
    p = strrchr(name, '/');                                 /* find file name part in "<location>/lib<module>.so" */
    if (p) {location = name; *++p=0;}                       /* terminate "<location>/" */
    else p=name;
    symname = p+2;                                          /* replace "lib" with "_" */
    symname[0] = '_';
    p = strchr(symname, '.');                               /* replace ".so" with "LibRelease" */
    if (p == NULL) p = symname + strlen(symname);
    strcpy(p, "LibRelease");
    version = dlsym(handle, symname);                       /* find symbol "_<module>LibRelease" */
    if (version)
    {
        *p=0; symname++;                                    /* build module name "<module>" */
        if ((p = strstr(name, OSI_PATH_SEPARATOR LIBDIR)) != NULL)
            p[1]=0;                                         /* cut "<location>" before libdir */
        registerModule(symname, version, location);
    }
    dlclose(handle);
    free(name);
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
    fprintf (stderr, "How to find symbols on Windows?\n");
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

int libversionShow(int showLocation, const char* outfile)
{
    moduleitem* m;
    size_t lm, lv;
    
    FILE* out = epicsGetStdout();

    if (outfile)
    {
        out = fopen(outfile, "w");
        if (out < 0)
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
        fprintf(out, "%20s %-20s %s\n",
            m->content,
            m->content+lm,
            showLocation ? m->content+lm+lv : "");
    }
    fflush(out);
    if (outfile)
        fclose(out);
    return 0;
}

#define MISMATCH -1
#define EXACT 0
#define MATCH 1
#define TESTVERS 3
#define HIGHER 3

static int compareVersions(const char* found, const char* request)
{
    int found_major, found_minor=0, found_patch=0, found_parts = 0;
    int req_major, req_minor, req_patch, req_parts;
    
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
    
    found_parts = sscanf(found, "%d.%d.%d", &found_major, &found_minor, &found_patch);
    if (request == NULL || request[0] == 0)            /* no particular version request: match anything */
    {
        if (found_parts == 0)
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
    req_parts = sscanf(request, "%d.%d.%d", &req_major, &req_minor, &req_patch);
    if (req_parts == 0)
    {
        if (requireDebug)
            printf("require: compareVersions: MISMATCH test version requested, different version found\n");
        return MISMATCH;
    }
    if (found_parts == 0)
    {
        if (requireDebug)
            printf("require: compareVersions: TESTVERS numeric requested, test version found\n");
        if(request[strlen(request)-1] == '+')
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
        if(request[strlen(request)-1] == '+')
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
            printf("require: compareVersions: MATCH patch level matches exactly requested with +\n");
        return MATCH;
    }
    if(request[strlen(request)-1] == '+')
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

    if (getenv("T_A") == NULL)
        putenvprintf("T_A=%s", targetArch);

    if (getenv("EPICS_RELEASE") == NULL)
        putenvprintf("EPICS_RELEASE=%s", epicsRelease);

    if (module == NULL)
    {
        printf("Usage: require \"<module>\" [, \"<version>\" | \"ifexists\"] [, \"<args>\"]\n");
        printf("Loads " PREFIX "<module>" INFIX EXT " and <libname>.dbd\n");
#ifndef EPICS_3_13
        printf("And calls <module>_registerRecordDeviceDriver\n");
#endif
        printf("If available, runs startup script snippet or loads substitution file or templates with args\n");
        return -1;
    }
    
    if (version && version[0] == 0) version = NULL;

    /* either order for version and args, either may be empty or NULL */
    if (version && strchr(version, '='))
    {
        const char *v = version;
        version = args;
        args = v;
        if (requireDebug)
            printf("require: swap version and args\n");
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
                printf("require: file %s exists, size %ld bytes\n",
                    filename, filestat.st_size);
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
                    printf("Warning: Module %s version %s already loaded where %s was requested\n",
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
                TRY_NONEMPTY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "%s%s.dbd", module, versionstr))
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
    if (TRY_FILE(releasediroffs, "%s.cmd", targetArch) ||
        TRY_FILE(releasediroffs, "%s.cmd", osClass) ||
        TRY_FILE(releasediroffs, "startup.cmd") ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "%s.cmd", targetArch) ||
        TRY_FILE(releasediroffs, ".." OSI_PATH_SEPARATOR "%s.cmd", osClass) ||
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
static const iocshArg requireArg0 = { "module", iocshArgString };
static const iocshArg requireArg1 = { "[version]", iocshArgString };
static const iocshArg requireArg2 = { "[substitutions]", iocshArgString };
static const iocshArg * const requireArgs[3] = { &requireArg0, &requireArg1, &requireArg2 };
static const iocshFuncDef requireDef = { "require", 3, requireArgs };
static void requireFunc (const iocshArgBuf *args)
{
    require(args[0].sval, args[1].sval, args[2].sval);
}

static const iocshArg libversionShowArg0 = { "showLocation", iocshArgInt };
static const iocshArg libversionShowArg1 = { "outputfile", iocshArgString };
static const iocshArg * const libversionArgs[2] = { &libversionShowArg0, &libversionShowArg1 };
static const iocshFuncDef libversionShowDef = { "libversionShow", 2, libversionArgs };
static void libversionShowFunc (const iocshArgBuf *args)
{
    libversionShow(args[0].ival, args[1].sval);
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
    static int firstTime = 1;
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


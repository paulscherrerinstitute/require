#include <symLib.h>
#include <sysSymTbl.h>
#include <sysLib.h>
#include <symLib.h>
#include <shellLib.h>
#include <usrLib.h>
#include <taskLib.h>
#include <stat.h>
#include <stdio.h>
#include <string.h>
#include <require.h>
#include <epicsVersion.h>
#ifndef BASE_VERSION
/* This is R3.14.* */
extern int iocshCmd (const char *cmd);
#endif

int dbLoadDatabase(char *filename, char *path, char *substitutions);

int require(char* lib, char* version)
{
    char** path;
    char* loaded;
    SYM_TYPE type;
    struct stat filestat;
    
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
    loaded = getLibVersion(lib);
    if (!loaded)
    {
        char libname[256];
        char dbdname[256];
        char depname[256];

        /* first try local library */
        if (version)
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
            if (version)
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
            if (stat(libname, &filestat) == ERROR)
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
            char *v;
            
            depfile = fopen(depname, "r");
            while (fgets(buffer, sizeof(buffer), depfile))
            {
                if (buffer[0] == '#' || buffer[0] == 0) continue;
                buffer[strlen(buffer)-1] = 0;
                v = strchr(buffer, ' ');
                if (v) *v++ = 0;
                printf ("%s depends on %s %s\n", lib, buffer, v);
                if (require(buffer,v) != OK)
                {
                    fclose(depfile);
                    return ERROR;
                }
            }
            fclose(depfile);
        }
        
        /* load library */
        printf("Loading %s\n", libname);
        if (ld(0, 0, libname) == NULL)
        {
            printf("Aborting startup stript.\n");
            shellScriptAbort();
            return ERROR;
        }
        if (errno == S_symLib_SYMBOL_NOT_FOUND)
        {
            printf("Library requires some other functions.\n");
            printf("Aborting startup stript.\n");
            shellScriptAbort();
            return ERROR;
        }
        loaded = getLibVersion(lib);
        
        /* load dbd file */
        if (stat(dbdname, &filestat) != ERROR)
        {
            /* If file exists */
            printf("Loading %s\n", dbdname);
            if (dbLoadDatabase(dbdname, NULL, NULL) != OK)
            {
                taskDelay(sysClkRateGet());
                printf ("Aborting startup stript.\n");
                shellScriptAbort();
                return ERROR;
            }
#ifndef BASE_VERSION
            /* call register function for R3.14 */
            {
                char initfunc[256];

                sprintf (initfunc, "%s_registerRecordDeviceDriver", lib);
                printf("calling %s\n", initfunc);
                iocshCmd (initfunc);
            }
#endif        
        }
        if (loaded) printf("%s version is %s\n", lib, loaded);
        return OK;
    }
    else
    {
        /* Library already loaded. Check Version. */
        if (version && strcmp(loaded, version) != 0)
        {
            int lmajor, lminor, lpatch, lmatches;
            int major, minor, patch, matches;
            
            lmatches = sscanf(loaded, "%d.%d.%d", &lmajor, &lminor, &lpatch);
            matches = sscanf(version, "%d.%d.%d", &major, &minor, &patch);
            if (matches == 0 || lmatches == 0 /* non-numerical version*/
                || major != lmajor
                || (matches >= 2 && minor != lminor)
                || (matches > 2 && patch != lpatch))
            {
                printf("Version conflict between requested %s\n"
                    "and already loaded version %s.\n"
                    "Aborting startup stript.\n",
                    lib, loaded);
                shellScriptAbort();
                return ERROR;
            }
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

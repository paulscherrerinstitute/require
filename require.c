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

int dbLoadDatabase(char *filename, char *path, char *substitutions);

int require(char* lib, char* version)
{
    char libname[256];
    char dbdname[256];
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
    if (version)
    {
        sprintf(libname, "%s/%sLib-%s", *path, lib, version);
        sprintf(dbdname, "%s/dbd/%s-%s.dbd", *path, lib, version);
    }
    else
    {
        sprintf(libname, "%s/%sLib", *path, lib);
        sprintf(dbdname, "%s/dbd/%s.dbd", *path, lib);
    }

    loaded = getLibVersion(lib);
    if (!loaded)
    {
        /* Load library and dbd file of requested version */
        errno = 0;
        if (ld(0, 0, libname) == NULL)
        {
            if (errno == S_symLib_SYMBOL_NOT_FOUND)
            {
                printf ("Library requires some other library\n");
            }
            printf ("Aborting startup stript.\n");
            shellScriptAbort();
            return ERROR;
        }
        loaded = getLibVersion(lib);
        printf("%sLib-%s loaded\n", lib, loaded);
        if (stat(dbdname, &filestat) != ERROR)
        {
            /* If file exists */
            if (dbLoadDatabase(dbdname, NULL, NULL) != OK)
            {
                taskDelay(sysClkRateGet());
                printf ("Aborting startup stript.\n");
                shellScriptAbort();
                return ERROR;
            }
            printf("dbd/%s-%s.dbd loaded\n", lib, loaded);
        }
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
                printf("Conflict between requested %s\n"
                    "and already loaded version %s.\n"
                    "Aborting startup stript.\n",
                    libname, loaded);
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



#include <symLib.h>
#include <sysSymTbl.h>
#include <stdio.h>
#include <string.h>
#include <shellLib.h>
#include <usrLib.h>

int dbLoadDatabase(char *filename, char *path, char *substitutions);

int require(char* lib, char* version)
{
    char libname[256];
    char dbdname[256];
    char symbol[256];
    char** path;
    char* loaded;
    SYM_TYPE type;
    
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
    sprintf(symbol, "_%sLibRelease", lib);
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

    if (symFindByName(sysSymTbl, symbol, &loaded, &type) != OK)
    {
        /* Load library and dbd file of requested version */
        if (ld(0, 0, libname) == NULL)
        {
            printf ("%s not loaded\n", libname);
            return ERROR;
        }
        printf("%s loaded\n", libname);
        if (dbLoadDatabase(dbdname, NULL, NULL) != OK)
        {
            printf ("%s not loaded\n", dbdname);
            return ERROR;
        }
        printf("%s loaded\n", dbdname);
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
                printf("Conflict between requested %s\nand already loaded version %s\n",
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

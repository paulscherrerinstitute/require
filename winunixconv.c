/* winunixconv.c
*
*  convert backslashes into slashes
*
*  $Author: brands $
*
*  $Source: /cvs/G/DRV/misc/winunixconv.c,v $
*
*/

#include <string.h>
#include <stdlib.h>
#include <dbScan.h>
#include <dbStaticLib.h>
#include <dbAccess.h>
#include <epicsVersion.h>
#ifdef BASE_VERSION
#define EPICS_3_13
extern DBBASE *pdbbase;
#else
#define EPICS_3_14
#include <iocsh.h>
#include <epicsExport.h>
#endif


int winunixconv (char* envvar)
{
   char *cp;
   char *envstr = getenv(envvar);
   int j = 0;


    while (envstr[j] != '\0'){
        if (envstr[j] == '\\') {
            envstr[j] = '/';
        }
        j++;
    }
    cp = mallocMustSucceed (strlen (envvar) + strlen (envstr) + 2, "winunixconv");

    strcpy (cp, envvar);
	strcat (cp, "=");
	strcat (cp, envstr);
	if (putenv (cp) < 0) {
		errPrintf(
                -1L,
                __FILE__,
                __LINE__,
                "Failed to set environment parameter \"%s\" to new value \"%s\": %s\n",
                envvar,
                envstr,
                strerror (errno));
        free (cp);
	}

    return 0;
}

#ifdef EPICS_3_14
static const iocshArg winunixconvArg0 = { "envVar", iocshArgString };
static const iocshArg * const winunixconvArgs[1] = { &winunixconvArg0 };
static const iocshFuncDef winunixconvDef = { "winunixconv", 1, winunixconvArgs };
static void winunixconvFunc (const iocshArgBuf *args)
{
    winunixconv(args[0].sval);
}
static void winunixconvRegister(void)
{
    static int winunixconvfirstTime = 1;
    if (winunixconvfirstTime) {
        iocshRegister (&winunixconvDef, winunixconvFunc);
        winunixconvfirstTime = 0;
    }
}
epicsExportRegistrar(winunixconvRegister);
#endif


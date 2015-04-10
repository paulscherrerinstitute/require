/*
* exec - execute shell commands.
*
* $Author: zimoch $
* $ID$
* $Date: 2015/04/10 13:30:29 $
*
* DISCLAIMER: Use at your own risc and so on. No warranty, no refund.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <iocsh.h>
#include <stdio.h>
#ifdef UNIX
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#endif
#include <epicsExport.h>

#ifdef UNIX
static const iocshArg execArg0 = { "command", iocshArgString };
static const iocshArg execArg1 = { "arguments", iocshArgArgv };
static const iocshArg * const execArgs[2] = { &execArg0, &execArg1 };
static const iocshFuncDef execDef = { "exec", 2, execArgs };
static const iocshFuncDef exclDef = { "!", 2, execArgs }; /* alias */

static void execFunc (const iocshArgBuf *args)
{
    char commandline [256];
    int i;
    int status;
    int len;
    char *p = commandline;
    char *arg;
    char special;

    if (args[0].sval == NULL)
    {
        system(getenv("SHELL"));
        return;
    }
    else
    {
        for (i = 0; i < args[1].aval.ac; i++)
        {
            arg = args[1].aval.av[i];
            len = strlen(arg);
            special = 0;
            if (len) {
                special = arg[len-1];
                if (special == '|' || special == ';' || special == '&') len--;
                else special = 0;
            }

            if (p - commandline + len + 4 >= sizeof(commandline))
            {
                fprintf (stderr, "command line too long\n");
                return;
            }

            /* quote words to protect special chars (e.g. spaces) */
            p += sprintf(p, " \"%.*s\"", len, arg);

            /* add unquoted special chars | ; & */
            if (special) p += sprintf(p, "%c", special);
        }
        status = system(commandline);
    }
    if (WIFSIGNALED(status))
    {
#ifdef __USE_GNU
        fprintf (stderr, "%s killed by signal %d: %s\n",
            args[0].sval, WTERMSIG(status), strsignal(WTERMSIG(status)));
#else
        fprintf (stderr, "%s killed by signal %d\n",
            args[0].sval, WTERMSIG(status));
#endif
    }
    if (WEXITSTATUS(status))
    {
        fprintf (stderr, "exit status is %d\n", WEXITSTATUS(status));
    }
}

/* sleep */
static const iocshArg sleepArg0 = { "seconds", iocshArgDouble };
static const iocshArg * const sleepArgs[1] = { &sleepArg0 };
static const iocshFuncDef sleepDef = { "sleep", 1, sleepArgs };

static void sleepFunc (const iocshArgBuf *args)
{
    struct timespec sleeptime;

    sleeptime.tv_sec = (long) args[0].dval;
    sleeptime.tv_nsec = (long) ((args[0].dval - (long) args[0].dval) * 1000000000);
    while (nanosleep (&sleeptime, &sleeptime) == -1)
    {
        if (errno != EINTR)
        {
            perror("sleep");
            break;
        }
    }
}
#endif

static void
execRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
#ifdef UNIX
        iocshRegister (&execDef, execFunc);
        iocshRegister (&exclDef, execFunc);
        iocshRegister (&sleepDef, sleepFunc);
#endif
        firstTime = 0;
    }
}

epicsExportRegistrar(execRegister);

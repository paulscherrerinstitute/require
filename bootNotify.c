/* bootNotify.c
*
*  call a script on the boot PC and give it a lot of boot infos
*
*  $Author: zimoch $
*
*  $Log: bootNotify.c,v $
*  Revision 1.4  2013/08/15 09:50:07  zimoch
*  strip off leading strings from vxWorksVersion
*
*  Revision 1.3  2006/03/03 13:30:33  zimoch
*  made epicsver compatible to 3.14
*
*  Revision 1.2  2004/05/24 15:18:58  zimoch
*  use ifName()
*
*  Revision 1.1  2004/04/16 08:07:12  zimoch
*  put here from utilities
*  these are EPICS dependend, utilities not
*
*  Revision 1.4  2004/04/02 14:52:15  zimoch
*  generic ethernet address support added
*
*  Revision 1.3  2003/03/20 14:57:50  zimoch
*  upload of ethernet address added
*
*  Revision 1.2  2003/03/18 08:03:28  zimoch
*  debugged
*
*  Revision 1.1  2003/02/18 17:26:59  zimoch
*  split bootUntil.c into 3 parts
*
*
*/
#include <stdio.h>
#include <bootInfo.h>
#include <rsh.h>
#include <version.h>
#include <epicsVersion.h>

int bootNotify (char* script, char* script2)
{
    char command[256];
    char epicsver[15];
    char* vxver = vxWorksVersion;
    
    if (script == NULL)
    {
        printErr ("usage: bootNotify [\"<path>\",] \"<script>\"\n");
        return ERROR;
    }
    if (script2)
    {
        sprintf (command, "%s/%s", script, script2);
    }
    else
    {
        sprintf (command, "%s", script);
    }
    /* strip off any leading non-numbers */
    while (vxver && (*vxver < '0' || *vxver > '9')) vxver++;
    
    sprintf (epicsver, "%d.%d.%d",
        EPICS_VERSION, EPICS_REVISION, EPICS_MODIFICATION);
    return rsh (bootHost(), command, bootInfo("%T %e %n %d %F %s"),
        vxver, epicsver, etherAddr(ifName()), 0);
}

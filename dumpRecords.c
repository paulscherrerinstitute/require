/* dumpRecords is a wrapper function for dbl 
   it is required because of the changed syntax of dbl in R3.14.
 */

#include <epicsVersion.h>
#include <stddef.h>

#ifdef BASE_VERSION
/* This is R3.13 */
long dbl(char *precordTypename, char *filename, char *fields);

int dumpRecords(char* file, char* fields)
{
    return dbl(0L, file, fields);
} 

#else
/* This is R3.14 */
#include <string.h>
#include <dbTest.h>
#include <epicsStdio.h>

int dumpRecords(char* file, char* fields)
{
    FILE* oldStdout;
    FILE* newStdout;
    
    newStdout = fopen(file, "w");
    if (!newStdout)
    {
        fprintf (stderr, "Can't open %s for writing: %s\n",
            file, strerror(errno));
        return errno;
    }
    oldStdout = epicsGetThreadStdout();
    epicsSetThreadStdout(newStdout);
    dbl(0L, fields);
    fclose(newStdout);
    epicsSetThreadStdout(oldStdout);
    return OK;
} 

#endif

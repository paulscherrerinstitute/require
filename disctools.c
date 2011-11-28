/*
* disctools - dir etc.
*
* $Author: zimoch $
* $ID$
* $Date: 2011/11/28 14:18:37 $
*
* DISCLAIMER: Use at your own risc and so on. No warranty, no refund.
*/

#include <iocsh.h>
#include <stdio.h>
#ifdef UNIX
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <malloc.h>
#include <pwd.h>
#include <grp.h>
#endif
#include <epicsExport.h>

#ifdef UNIX

/* dir, ll, ls */
static const iocshArg dirArg0 = { "directrory", iocshArgString };
static const iocshArg * const dirArgs[1] = { &dirArg0 };
static const iocshFuncDef dirDef = { "dir", 1, dirArgs };
static const iocshFuncDef llDef = { "ll", 1, dirArgs };
static const iocshFuncDef lsDef = { "ls", 1, dirArgs };

static int nohidden(const struct dirent *entry)
{
    return entry->d_name[0] != '.';
}

static void llFunc(const iocshArgBuf *args)
{
    char* dirname = ".";
    struct dirent** namelist;
    struct stat filestat;
    char type;
    char filename[256];
    char target[256];
    struct group* group;
    struct passwd* user;
    struct tm time;
    char timestr[20];
    int n, i, len;

    if (args[0].sval) dirname = args[0].sval;
    n = scandir(dirname, &namelist, nohidden, alphasort);
    if (n < 0)
    {
        perror(dirname);
        return;
    }
    for (i = 0; i < n; i++)
    {
        sprintf(filename, "%s/%s", dirname, namelist[i]->d_name);
        if (lstat(filename, &filestat))
        {
            perror(namelist[i]->d_name);
            continue;
        }
        if (S_ISREG(filestat.st_mode)) type='-';
        else if (S_ISDIR(filestat.st_mode)) type='d';
        else if (S_ISCHR(filestat.st_mode)) type='c';
        else if (S_ISBLK(filestat.st_mode)) type='b';
        else if (S_ISFIFO(filestat.st_mode)) type='p';
        else if (S_ISLNK(filestat.st_mode)) type='l';
        else if (S_ISSOCK(filestat.st_mode)) type='s';
        else type='?';
        localtime_r(&filestat.st_mtime, &time);
        strftime(timestr, 20, "%b %e %Y %H:%M", &time);
        printf("%c%c%c%c%c%c%c%c%c%c %4d",
            type,
            filestat.st_mode & S_IRUSR ? 'r' : '-',
            filestat.st_mode & S_IWUSR ? 'w' : '-',
            filestat.st_mode & S_ISUID ? 's' :
            filestat.st_mode & S_IXUSR ? 'x' : '-',
            filestat.st_mode & S_IRGRP ? 'r' : '-',
            filestat.st_mode & S_IWGRP ? 'w' : '-',
            filestat.st_mode & S_ISGID ? 's' :
            filestat.st_mode & S_IXGRP ? 'x' : '-',
            filestat.st_mode & S_IROTH ? 'r' : '-',
            filestat.st_mode & S_IWOTH ? 'w' : '-',
            filestat.st_mode & S_ISVTX ? 't' :
            filestat.st_mode & S_IXOTH ? 'x' : '-',
            filestat.st_nlink);
        user=getpwuid(filestat.st_uid);
        if (user) printf(" %-8s", user->pw_name);
        else printf(" %-8d", filestat.st_uid);
        group=getgrgid(filestat.st_gid);
        if (group) printf(" %-8s", group->gr_name);
        else printf(" %-8d", filestat.st_gid);
        printf (" %8ld %s %s",
            filestat.st_size, timestr,
            namelist[i]->d_name);
        if (S_ISLNK(filestat.st_mode))
        {
            len = readlink(filename, target, 255);
            if (len == -1) perror(filename);
            else
            {
                target[len] = 0;
                printf(" -> %s\n", target);
            }
        }
        else
        {
            printf("\n");
        }
        free(namelist[i]);
    }
    free(namelist);
}

static void lsFunc(const iocshArgBuf *args)
{
    char* dirname = ".";
    struct dirent** namelist;
    int n, i, cols, rows, r, c, len, maxlen=0;

    if (args[0].sval) dirname = args[0].sval;
    n = scandir(dirname, &namelist, nohidden, alphasort);
    if (n < 0)
    {
        perror(dirname);
        return;
    }
    for (i = 0; i < n; i++)
    {
        len = strlen(namelist[i]->d_name);
        if (len > maxlen) maxlen = len;
    }
    cols=80/(maxlen+=2);
    rows=(n-1)/cols+1;
    for (r = 0; r < rows; r++)
    {
        for (c = 0; c < cols; c++)
        {
            i = r + c*rows;
            if (i >= n) continue;
            printf("%-*s",
                maxlen, namelist[i]->d_name);
            free(namelist[i]);
        }
        printf("\n");
    }
    free(namelist);
}
/* mkdir */
static const iocshArg mkdirArg0 = { "directrory", iocshArgString };
static const iocshArg * const mkdirArgs[1] = { &mkdirArg0 };
static const iocshFuncDef mkdirDef = { "mkdir", 1, mkdirArgs };

static void mkdirFunc(const iocshArgBuf *args)
{
    char* dirname;

    dirname = args[0].sval;
    if (!dirname)
    {
        fprintf(stderr, "missing directory name\n");
        return;
    }
    if (mkdir(dirname, 0777))
    {
        perror(dirname);
    }
}

/* rmdir */
static const iocshArg rmdirArg0 = { "directrory", iocshArgString };
static const iocshArg * const rmdirArgs[1] = { &rmdirArg0 };
static const iocshFuncDef rmdirDef = { "rmdir", 1, rmdirArgs };

static void rmdirFunc(const iocshArgBuf *args)
{
    char* dirname;

    dirname = args[0].sval;
    if (!dirname)
    {
        fprintf(stderr, "missing directory name\n");
        return;
    }
    if (rmdir(dirname))
    {
        perror(dirname);
    }
}

/* rm */
static const iocshArg rmArg0 = { "file", iocshArgString };
static const iocshArg * const rmArgs[1] = { &rmArg0 };
static const iocshFuncDef rmDef = { "rm", 1, rmArgs };

static void rmFunc(const iocshArgBuf *args)
{
    char* filename;

    filename = args[0].sval;
    if (!filename)
    {
        fprintf(stderr, "missing file name\n");
        return;
    }
    if (unlink(filename))
    {
        perror(filename);
    }
}

/* mv */
static const iocshArg mvArg0 = { "oldname", iocshArgString };
static const iocshArg mvArg1 = { "newname", iocshArgString };
static const iocshArg * const mvArgs[2] = { &mvArg0, &mvArg1 };
static const iocshFuncDef mvDef = { "mv", 2, mvArgs };

static void mvFunc(const iocshArgBuf *args)
{
    char* oldname;
    char* newname;
    struct stat filestat;
    char filename[256];

    oldname = args[0].sval;
    newname = args[1].sval;
    if (!oldname || !newname)
    {
        fprintf(stderr, "need 2 file names\n");
        return;
    }
    if (!stat(newname, &filestat) && S_ISDIR(filestat.st_mode))
    {
        sprintf(filename, "%s/%s", newname, oldname);
        newname = filename;
    }
    if (rename(oldname, newname))
    {
        perror("mv");
    }
}

/* cp, copy */
static const iocshArg cpArg0 = { "source", iocshArgString };
static const iocshArg cpArg1 = { "target", iocshArgString };
static const iocshArg * const cpArgs[2] = { &cpArg0, &cpArg1 };
static const iocshFuncDef cpDef = { "cp", 2, cpArgs };
static const iocshFuncDef copyDef = { "copy", 2, cpArgs };

static void cpFunc(const iocshArgBuf *args)
{
    char buffer [256];
    char* sourcename;
    char* targetname;
    FILE* sourcefile;
    FILE* targetfile;
    size_t len;

    sourcename = args[0].sval;
    targetname = args[1].sval;
    if (sourcename == NULL || sourcename[0] == '\0')
        sourcefile = stdin;
    else if (!(sourcefile = fopen(sourcename,"r")))
    {
        perror(sourcename);
        return;
    }
    if (targetname == NULL || targetname[0] == '\0')
        targetfile = stdout;
    else if (!(targetfile = fopen(targetname,"w")))
    {
        perror(targetname);
        return;
    }
    while (!feof(sourcefile))
    {
        len = fread(buffer, 1, 256, sourcefile);
        if (ferror(sourcefile))
        {
            perror(sourcename);
            break;
        }
        fwrite(buffer, 1, len, targetfile);
        if (ferror(targetfile))
        {
            perror(targetname);
            break;
        }
    }
    if (sourcefile != stdin)
        fclose(sourcefile);
    if (targetfile != stdout)
        fclose(targetfile);
    else
        fflush(stdout);
}

/* umask */
static const iocshArg umaskArg0 = { "mask", iocshArgString };
static const iocshArg * const umaskArgs[1] = { &umaskArg0 };
static const iocshFuncDef umaskDef = { "umask", 1, umaskArgs };

static void umaskFunc(const iocshArgBuf *args)
{
    mode_t mask;
    if (args[0].sval == NULL)
    {
        mask = umask(0);
        umask(mask);
        printf("%03o\n", (int)mask);
        return;
    }
    if (sscanf(args[0].sval, "%o", &mask) == 1)
    {
        umask(mask);
        return;
    }
    fprintf(stderr, "mode %s not recognized\n", args[0].sval);
}

/* chmod */
static const iocshArg chmodArg0 = { "mode", iocshArgInt };
static const iocshArg chmodArg1 = { "file", iocshArgString };
static const iocshArg * const chmodArgs[2] = { &chmodArg0, &chmodArg1 };
static const iocshFuncDef chmodDef = { "chmod", 2, chmodArgs };

static void chmodFunc(const iocshArgBuf *args)
{
    mode_t mode = args[0].ival;
    char* path = args[1].sval;
    if (chmod(path, mode) != 0)
    {
        perror(path);
    }
}
#endif

static void
disctoolsRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
#ifdef UNIX
        iocshRegister(&dirDef, llFunc);
        iocshRegister(&llDef, llFunc);
        iocshRegister(&lsDef, lsFunc);
        iocshRegister(&mkdirDef, mkdirFunc);
        iocshRegister(&rmdirDef, rmdirFunc);
        iocshRegister(&rmDef, rmFunc);
        iocshRegister(&mvDef, mvFunc);
        iocshRegister(&cpDef, cpFunc);
        iocshRegister(&copyDef, cpFunc);
        iocshRegister(&umaskDef, umaskFunc);
        iocshRegister(&chmodDef, chmodFunc);
#endif
        firstTime = 0;
    }
}

epicsExportRegistrar(disctoolsRegister);

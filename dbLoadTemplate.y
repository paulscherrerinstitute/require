%{

/*************************************************************************\
* Copyright (c) 2006 UChicago, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* for vasprintf */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#if defined (_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

#if defined(vxWorks)
#include <ioLib.h>
#endif

#include "macLib.h"
#include "dbmf.h"

#include "dbAccess.h"
#include "dbLoadTemplate.h"
#include "osiFileName.h"
#include "epicsVersion.h"
#include "macLib.h"

#if defined(vxWorks) || defined (_WIN32)
#include "asprintf.h"
#endif

#ifdef BASE_VERSION
#define EPICS_3_13
extern void dbLoadRecords(const char*, const char*);
#else
#include "iocsh.h"
#include "epicsExport.h"
#endif

#define EPICSVER EPICS_VERSION*10000+EPICS_REVISION*100+EPICS_MODIFICATION

#if (EPICSVER<31412)
#define dbmfStrdup(s) strcpy(dbmfMalloc(strlen((char*)(s))+1),(char*)(s))
#endif

#if (EPICSVER>=31600)
#define dbmfStrdup(s) dbmfStrdup((char*)s) 
#endif

/* from runScript.c */
extern int isAbsPath(const char* filename);

static int line_num;
static int yyerror(char* str);

static char *sub_collect = NULL;
static char *sub_locals;
static char **vars = NULL;
static char *db_file_name = NULL;
static int var_count, sub_count;
static MAC_HANDLE *macHandle = NULL;

/* We allocate MAX_VAR_FACTOR chars in the sub_collect string for each
 * "variable=value," segment, and will accept at most dbTemplateMaxVars
 * template variables.  The user can adjust that variable to increase
 * the number of variables or the length allocated for the buffer.
 */
#define MAX_VAR_FACTOR 50

int dbTemplateMaxVars = 100;

%}

%start substitution_file

%token <Str> WORD QUOTE
%token DBFILE
%token PATTERN
%token GLOBAL
%token EQUALS COMMA
%left O_PAREN C_PAREN
%left O_BRACE C_BRACE

%union
{
    int Int;
    char Char;
    char *Str;
    double Real;
}

%%

substitution_file: global_or_template
    | substitution_file global_or_template
    ;

global_or_template: global_definitions
    | template_substitutions
    ;

global_definitions: GLOBAL O_BRACE C_BRACE
    | GLOBAL O_BRACE variable_definitions C_BRACE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "global_definitions: %s\n", sub_collect+1);
    #endif
        sub_locals += strlen(sub_locals);
    }
    ;

template_substitutions: template_filename O_BRACE C_BRACE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "template_substitutions: %s unused\n", db_file_name);
    #endif
        dbmfFree(db_file_name);
        db_file_name = NULL;
    }
    | template_filename O_BRACE substitutions C_BRACE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "template_substitutions: %s finished\n", db_file_name);
    #endif
        dbmfFree(db_file_name);
        db_file_name = NULL;
    }
    ;

template_filename: DBFILE WORD
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "template_filename: %s\n", $2);
    #endif
        var_count = 0;
        db_file_name = dbmfMalloc(strlen($2)+1);
        strcpy(db_file_name, $2);
        dbmfFree($2);
    }
    | DBFILE QUOTE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "template_filename: \"%s\"\n", $2);
    #endif
        var_count = 0;
        db_file_name = dbmfMalloc(strlen($2)+1);
        strcpy(db_file_name, $2);
        dbmfFree($2);
    }
    ;

substitutions: pattern_substitutions
    | variable_substitutions
    ;

pattern_substitutions: PATTERN O_BRACE C_BRACE
    | PATTERN O_BRACE C_BRACE pattern_definitions
    | PATTERN O_BRACE pattern_names C_BRACE
    | PATTERN O_BRACE pattern_names C_BRACE pattern_definitions
    ;

pattern_names: pattern_name
    | pattern_names COMMA
    | pattern_names pattern_name
    ;

pattern_name: WORD
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "pattern_name: [%d] = %s\n", var_count, $1);
    #endif
        if (var_count >= dbTemplateMaxVars) {
            fprintf(stderr,
                "More than dbTemplateMaxVars = %d macro variables used\n",
                dbTemplateMaxVars);
            yyerror(NULL);
        }
        else {
            vars[var_count] = dbmfMalloc(strlen($1)+1);
            strcpy(vars[var_count], $1);
            var_count++;
            dbmfFree($1);
        }
    }
    ;

pattern_definitions: pattern_definition
    | pattern_definitions pattern_definition
    ;

pattern_definition: global_definitions
    | O_BRACE C_BRACE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "pattern_definition: pattern_values empty\n");
        fprintf(stderr, "    dbLoadRecords(%s)\n", sub_collect+1);
    #endif
        dbLoadRecords(db_file_name, sub_collect+1);
    }
    | O_BRACE pattern_values C_BRACE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "pattern_definition:\n");
        fprintf(stderr, "    dbLoadRecords(%s)\n", sub_collect+1);
    #endif
        dbLoadRecords(db_file_name, sub_collect+1);
        *sub_locals = '\0';
        sub_count = 0;
    }
    | WORD O_BRACE pattern_values C_BRACE
    {   /* DEPRECATED SYNTAX */
        fprintf(stderr,
            "dbLoadTemplate: Substitution file uses deprecated syntax.\n"
            "    the string '%s' on line %d that comes just before the\n"
            "    '{' character is extraneous and should be removed.\n",
            $1, line_num);
    #ifdef ERROR_STUFF
        fprintf(stderr, "pattern_definition:\n");
        fprintf(stderr, "    dbLoadRecords(%s)\n", sub_collect+1);
    #endif
        dbLoadRecords(db_file_name, sub_collect+1);
        dbmfFree($1);
        *sub_locals = '\0';
        sub_count = 0;
    }
    ;

pattern_values: pattern_value
    | pattern_values COMMA
    | pattern_values pattern_value
    ;

pattern_value: QUOTE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "pattern_value: [%d] = \"%s\"\n", sub_count, $1);
    #endif
        if (sub_count < var_count) {
            strcat(sub_locals, ",");
            strcat(sub_locals, vars[sub_count]);
            strcat(sub_locals, "=\"");
            macExpandString(macHandle, $1, sub_locals+strlen(sub_locals), (long)(dbTemplateMaxVars * MAX_VAR_FACTOR - (sub_locals+strlen(sub_locals)-sub_collect) - 1));
            strcat(sub_locals, "\"");
            sub_count++;
        } else {
            fprintf(stderr, "dbLoadTemplate: Too many values given, line %d.\n",
                line_num);
        }
        dbmfFree($1);
    }
    | WORD
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "pattern_value: [%d] = %s\n", sub_count, $1);
    #endif
        if (sub_count < var_count) {
            strcat(sub_locals, ",");
            strcat(sub_locals, vars[sub_count]);
            strcat(sub_locals, "=");
            macExpandString(macHandle, $1, sub_locals+strlen(sub_locals), (long)(dbTemplateMaxVars * MAX_VAR_FACTOR - (sub_locals+strlen(sub_locals)-sub_collect)));
            sub_count++;
        } else {
            fprintf(stderr, "dbLoadTemplate: Too many values given, line %d.\n",
                line_num);
        }
        dbmfFree($1);
    }
    ;

variable_substitutions: variable_substitution
    | variable_substitutions variable_substitution
    ;

variable_substitution: global_definitions
    | O_BRACE C_BRACE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "variable_substitution: variable_definitions empty\n");
        fprintf(stderr, "    dbLoadRecords(%s)\n", sub_collect+1);
    #endif
        dbLoadRecords(db_file_name, sub_collect+1);
    }
    | O_BRACE variable_definitions C_BRACE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "variable_substitution:\n");
        fprintf(stderr, "    dbLoadRecords(%s)\n", sub_collect+1);
    #endif
        dbLoadRecords(db_file_name, sub_collect+1);
        *sub_locals = '\0';
    }
    | WORD O_BRACE variable_definitions C_BRACE
    {   /* DEPRECATED SYNTAX */
        fprintf(stderr,
            "dbLoadTemplate: Substitution file uses deprecated syntax.\n"
            "    the string '%s' on line %d that comes just before the\n"
            "    '{' character is extraneous and should be removed.\n",
            $1, line_num);
    #ifdef ERROR_STUFF
        fprintf(stderr, "variable_substitution:\n");
        fprintf(stderr, "    dbLoadRecords(%s)\n", sub_collect+1);
    #endif
        dbLoadRecords(db_file_name, sub_collect+1);
        dbmfFree($1);
        *sub_locals = '\0';
    }
    ;

variable_definitions: variable_definition
    | variable_definitions COMMA
    | variable_definitions variable_definition
    ;

variable_definition: WORD EQUALS WORD
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "variable_definition: %s = %s\n", $1, $3);
    #endif
        strcat(sub_locals, ",");
        strcat(sub_locals, $1);
        strcat(sub_locals, "=");
        macExpandString(macHandle, $3, sub_locals+strlen(sub_locals), (long)(dbTemplateMaxVars * MAX_VAR_FACTOR - (sub_locals+strlen(sub_locals)-sub_collect)));
        dbmfFree($1); dbmfFree($3);
    }
    | WORD EQUALS QUOTE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "variable_definition: %s = \"%s\"\n", $1, $3);
    #endif
        strcat(sub_locals, ",");
        strcat(sub_locals, $1);
        strcat(sub_locals, "=\"");
        macExpandString(macHandle, $3, sub_locals+strlen(sub_locals), (long)(dbTemplateMaxVars * MAX_VAR_FACTOR - (sub_locals+strlen(sub_locals)-sub_collect) - 1));
        strcat(sub_locals, "\"");
        dbmfFree($1); dbmfFree($3);
    }
    | QUOTE EQUALS QUOTE
    {
    #ifdef ERROR_STUFF
        fprintf(stderr, "variable_definition: \"%s\" = \"%s\"\n", $1, $3);
    #endif
        strcat(sub_locals, ",\"");
        strcat(sub_locals, $1);
        strcat(sub_locals, "\"=\"");
        macExpandString(macHandle, $3, sub_locals+strlen(sub_locals), (long)(dbTemplateMaxVars * MAX_VAR_FACTOR - (sub_locals+strlen(sub_locals)-sub_collect) - 1));
        strcat(sub_locals, "\"");
        dbmfFree($1); dbmfFree($3);
    }
    ;

%%
 
#include "dbLoadTemplate_lex.c"
 
static int yyerror(char* str)
{
    if (str)
        fprintf(stderr, "Substitution file error: %s\n", str);
    else
        fprintf(stderr, "Substitution file error.\n");
    fprintf(stderr, "line %d: '%s'\n", line_num, yytext);
    return 0;
}

static int is_not_inited = 1;

#ifndef vxWorks
#define dbLoadTemplate __dbLoadTemplate
#endif

int dbLoadTemplate(const char *sub_file, const char *cmd_collect, const char *path)
{
    FILE *fp;
    int i;
    char** pairs;

    line_num = 1;
    if (!sub_file || !*sub_file) {
        fprintf(stderr, "must specify variable substitution file\n");
        return -1;
    }

    if (dbTemplateMaxVars < 1) {
        fprintf(stderr,"Error: dbTemplateMaxVars = %d, must be positive\n",
                dbTemplateMaxVars);
        return -1;
    }

    fp = fopen(sub_file, "r");
    if (!fp && !isAbsPath(sub_file)) {
        const char *dirname, *end;
        int dirlen;
        char* filename;

        if (!path || !*path) {
        path = getenv("EPICS_DB_INCLUDE_PATH");
        }
        for(dirname = path; dirname != NULL; dirname = end) {
            end = strchr(dirname, OSI_PATH_LIST_SEPARATOR[0]);
            if (end) dirlen = (int)(end++ - dirname);
            else dirlen = (int)strlen(dirname);
            if (dirlen == 0) continue; /* ignore empty path elements */
            if (dirlen == 1 && dirname[0] == '.') continue; /* we had . already */
            filename = NULL;
            if (asprintf(&filename, "%.*s/%s", dirlen, dirname, sub_file) < 0)
            {
                fprintf(stderr,"dbLoadTemplate: out of memory\n");
                break;
            }
            fp = fopen(filename, "r");
            free(filename);
            if (fp) break;
        }
    }
    if (!fp) {
        fprintf(stderr, "dbLoadTemplate: error opening sub file %s: %s\n", sub_file, strerror(errno));
        return -1;
    }

    macHandle = NULL;
    if (macCreateHandle(&macHandle,(
#if (EPICSVER>=31501)
        const
#endif
        char*[]){ "", "environ", NULL, NULL }) != 0) return -1;
    macSuppressWarning(macHandle, 1);

#if (0 && EPICSVER<31403)
    /* Have no environment macro substitution, thus load envionment explicitly */
#ifndef vxWorks
    /* In 3.14 before 3.14.3 we may have non-vxWorks without environment macro substitution */
    /* non-vxWorks systems have environ instead of ppGlobalEnviron */
    #define ppGlobalEnviron environ
#endif
#ifdef _WRS_VXWORKS_MAJOR
    /* VxWorks 6 bug: environment is not NULL terminated ! */
    /* There is a non-public counter 8 bytes after ppGlobalEnviron */
    char** endEnviron = ppGlobalEnviron+((unsigned int*)&ppGlobalEnviron)[2];
    if (runScriptDebug)
            printf("runScript: %u environment variables\n", ((unsigned int*)&ppGlobalEnviron)[2]);
    for (pairs = ppGlobalEnviron; pairs < endEnviron; pairs++)
#else
    for (pairs = ppGlobalEnviron; *pairs; pairs++)
#endif
    {
        char* var, *eq;
        if (runScriptDebug)
            printf("runScript: environ %s\n", *pairs);

        /* take a copy to replace '=' with null byte */
        if ((var = strdup(*pairs)) == NULL) continue;
        eq = strchr(var, '=');
        if (eq)
        {
            *eq = 0;
            macPutValue(macHandle, var, eq+1);
        }
        free(var);
    }
#endif

    vars = malloc(dbTemplateMaxVars * sizeof(char*));
    sub_collect = malloc(dbTemplateMaxVars * MAX_VAR_FACTOR);
    if (!vars || !sub_collect) {
        if (macHandle) macDeleteHandle(macHandle);
        free(vars);
        free(sub_collect);
        fclose(fp);
        fprintf(stderr, "dbLoadTemplate: Out of memory!\n");
        return -1;
    }
    strcpy(sub_collect, ",");

    if (cmd_collect && *cmd_collect) {
        macParseDefns(macHandle, (char*)cmd_collect, &pairs);
        macInstallMacros(macHandle, pairs);
        free(pairs);
   
        strcat(sub_collect, cmd_collect);
        sub_locals = sub_collect + strlen(sub_collect);
    } else {
        sub_locals = sub_collect;
        *sub_locals = '\0';
    }
    var_count = 0;
    sub_count = 0;

    if (is_not_inited) {
        yyin = fp;
        is_not_inited = 0;
    } else {
        yyrestart(fp);
    }

    yyparse();

    for (i = 0; i < var_count; i++) {
        dbmfFree(vars[i]);
    }
    if (macHandle) macDeleteHandle(macHandle);
    free(vars);
    free(sub_collect);
    vars = NULL;
    fclose(fp);
    if (db_file_name) {
        dbmfFree(db_file_name);
        db_file_name = NULL;
    }
    return 0;
}

#ifndef EPICS_3_13
#include "registry.h"
epicsExportAddress(int, dbTemplateMaxVars);

static const iocshFuncDef dbLoadTemplateDef = {
    "dbLoadTemplate", 3, (const iocshArg *[]) {
        &(iocshArg) { "filename", iocshArgString },
        &(iocshArg) { "\"macro=value,...\"", iocshArgString },
        &(iocshArg) { "searchpath", iocshArgString },
}};

static void dbLoadTemplateFunc(const iocshArgBuf *args)
{
    dbLoadTemplate(args[0].sval, args[1].sval, args[2].sval);
}

typedef struct iocshCommand {
    iocshFuncDef const    *pFuncDef;
    iocshCallFunc          func;
    struct iocshCommand   *next;
}iocshCommand;

static void dbLoadTemplateRegister(void)
{
    static int firstTime = 1;
    if (firstTime) {
        iocshRegister(&dbLoadTemplateDef, dbLoadTemplateFunc);
        firstTime = 0;
    }
}

epicsExportRegistrar(dbLoadTemplateRegister);
#endif

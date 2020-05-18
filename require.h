#ifndef require_h
#define require_h

#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __GNUC__
#define __attribute__(dummy)
#endif

epicsShareFunc int require(const char* libname, const char* version, const char* args);
epicsShareFunc size_t foreachLoadedLib(size_t (*func)(const char* name, const char* version, const char* path, void* arg), void* arg);
epicsShareFunc const char* getLibVersion(const char* libname);
epicsShareFunc const char* getLibLocation(const char* libname);
epicsShareFunc int libversionShow(const char* outfile);
epicsShareFunc int runScript(const char* filename, const char* args);
epicsShareFunc int putenvprintf(const char* format, ...) __attribute__((__format__(__printf__,1,2)));
epicsShareFunc void pathAdd(const char* varname, const char* dirname);

#ifdef __cplusplus
}
#endif

#endif

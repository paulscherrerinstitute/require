#ifndef require_h
#define require_h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __GNUC__
#define __attribute__(dummy)
#endif

int require(const char* libname, const char* version, const char* args);
const char* getLibVersion(const char* libname);
const char* getLibLocation(const char* libname);
int libversionShow(int showLocation, const char* outfile);
int runScript(const char* filename, const char* args);
int putenvprintf(const char* format, ...) __attribute__((format(printf,1,2)));

#ifdef __cplusplus
}
#endif

#endif

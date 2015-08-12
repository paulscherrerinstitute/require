#ifndef require_h
#define require_h

#ifdef __cplusplus
extern "C" {
#endif

int require(const char* libname, const char* version, const char* args);
const char* getLibVersion(const char* libname);
int libversionShow(const char* pattern, int showLocation);

#ifdef __cplusplus
}
#endif

#endif

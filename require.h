#ifndef require_h
#define require_h

int require(const char* libname, const char* version);
const char* getLibVersion(const char* libname);
int libversionShow(const char* pattern);

#endif

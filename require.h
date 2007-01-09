#ifndef require_h
#define require_h

int require(char* lib, char* version);
char* getLibVersion(char* lib);
int libversionShow(char* pattern);

#endif

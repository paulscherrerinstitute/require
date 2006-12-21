#ifndef require_h
#define require_h

int require(char* lib, char* version);
char* getLibversion(char* lib);
int libversionShow(char* pattern);

#endif

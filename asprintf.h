#ifndef asprintf_h
#define asprintf_h
#ifdef __cplusplus
extern "C" {
#endif
#include <stdarg.h>
int asprintf(char** pbuffer, const char* format, ...) __attribute__((format(printf,2,3)));
int vasprintf(char** pbuffer, const char* format, va_list ap) __attribute__((format(printf,2,0)));
#ifdef __cplusplus
}
#endif
#endif

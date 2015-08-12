#ifndef strdup_h
#define strdup_h
#ifdef __cplusplus
extern "C" {
#endif
char *strdup(const char *s);
char *strndup(const char *s, size_t n);
#ifdef __cplusplus
}
#endif
#endif

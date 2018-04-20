#ifndef expr_h
#define expr_h

#ifdef __cplusplus
extern {
#endif

extern int exprDebug;

size_t replaceExpressions(const char* source, char* buffer, size_t buffersize);
/* Resolve integer expressions that are either free standing
 * or in parentheses () embedded in an unquoted word.
 * Do not resolve expressions in single or double quoted strings.
 * An expression optionally starts with a integer format such as %x.
 * It consists of integer numbers, operators and parentheses ().
 */

#ifdef __cplusplus
}
#endif
#endif

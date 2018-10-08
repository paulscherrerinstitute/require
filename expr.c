#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "expr.h"

int exprDebug;

static int parseSubExpr(const char **pp, long *v, int pr, int op);
static long parseString(const char **pp, const char **pstart);
#define parseExpr(pp,v) parseSubExpr(pp, v, 0, 0)
#define skipSpace(p) while (isspace((unsigned char)*p)) p++

static int parseValue(const char **pp, long *v)
{
    long val;
    const char *p = *pp;
    char o;

    /* A value is optionally prefixed with an unary operator + - ! ~.
     * It is either a number (decimal, octal or hex)
     * or an expression in ().
     * Allowed chars after a number: operators, closing parenthesis, whitespace, quotes, end of string
     */

    /* first look for value */
    skipSpace(p);
    o = *p;
    if (memchr("+-~!", o, 4))
    {
        /* unary operators */
        p++;
        if (!parseValue(&p, &val)) return 0; /* no valid value */
        if (exprDebug) printf("parseValue: %c %ld\n", o, val);
        if (o == '-') val=-val;
        else if (o == '~') val=~val;
        else if (o == '!') val=!val;
    }
    else if (o == '(')
    {
        /* sub-expression */
        p++;
        if (parseExpr(&p, &val) < 0) return 0; /* no valid expression */
        skipSpace(p);
        if (*p++ != ')') return 0; /* missing ) */
    }
    else if (o == '#')
    {
        /* string length operator */
        p++;
        if (exprDebug) printf("parseValue: string length of %s\n", p);
        if (*p == '"' || *p == '\'')
            val = parseString(&p, NULL);
        else return 0;
    }
    else
    {
        /* number */
        char *e;
        val = strtol(p, &e, 0);
        if (e == p) return 0; /* no number */

        if (isalpha((unsigned char)*e)||*e=='.')
        {
            /* part of plain word or floating point number */
            return 0;
        }
        p = e;
    }
    *pp = p;
    *v = val;
    return 1;
}

static long ipow(long base, long exp)
{
    long val;
    if (exp == 2) return base*base;
    if (exp == 3) return base*base*base;   
    if (exp == 1) return base;
    if (exp == 0) return 1;
    if (exp < 0) return 0;
    val = ipow(base,exp>>1);
    val *= val;
    if (exp&1) val *= base;
    return val;
}

static char parseSep(const char **pp, const char *seps)
{
    const char *p = *pp;

    skipSpace(p);
    if (!*p || !strchr(seps, *p)) return 0;
    *pp = p+1;
    return *p;
}

struct {char str[4]; int pr;} ops[] = {
    {":=",0},
    {"**",14},
    {"*", 13},{"/",13},{"%",13},
    {"+",12},{"-",12},
    {"<<",11},{">>>",11},{">>",11},
    {"<?",10},{">?",10},
    {"<=>",9},
    {"<=",8},{">=",8},{"<",8},{">",8},
    {"==",7},{"!=",7},
    {"&&",3},{"||",2},
    {"&",6},{"^",5},{"|",4},
    {"?:",1},{"?",1}
};

enum op {
    op_none,
    op_pow, 
    op_mul,op_div,op_mod,
    op_plus,op_minus,
    op_lshift,op_urshift,op_rshift,
    op_min,op_max,
    op_cmp,
    op_lteq,op_gteq,op_lt,op_gt,
    op_eq,op_neq,
    op_logicand,op_logicor,
    op_bitand,op_bitxor,op_bitor,
    op_alt,op_if
};

static int startsWith(const char *p, const char *s)
{
    int i = 0;
    while (*s) { i++; if (*p++ != *s++) return 0; }
    return i;
}

static int parseOp(const char **pp)
{
    const char *p = *pp;
    int o, l;

    skipSpace(p);
    if (ispunct((unsigned char)*p))
    {
        for (o = 1; o < (int)(sizeof(ops)/sizeof(ops[0])); o++)
        {
            if ((l = startsWith(p, ops[o].str)))
            {
                /* operator found */
                *pp = p+l;
                return o;
            }
        }
    }
    return 0;
}

static int parseSubExpr(const char **pp, long *v, int pr, int o)
{
    const char *p = *pp;
    long val = o ? *v : 0;
    long val2;
    int o2 = o;

    if (exprDebug) printf("parseExpr(%d): start %ld %s \"%s\"\n", pr, val, ops[o].str, p);
    do {
        if (!parseValue(&p, &val2))
        {
            if (exprDebug) printf("parseExpr(%d): no value after %ld %s\n", pr, val, ops[o].str);
            return -1;
        }
nextop:
        if ((o2 = parseOp(&p)))
        {
            if (exprDebug) printf("parseExpr(%d): %ld %s %ld %s \"%s\"\n", pr, val, ops[o].str, val2, ops[o2].str, p);
            if (o && ops[o2].pr > ops[o].pr)
            {
                if ((o2 = parseSubExpr(&p, &val2, ops[o].pr, o2)) < 0)
                {
                    if (exprDebug) printf("parseExpr(%d): parse failed after %ld %s %ld\n", pr, val, ops[o].str, val2);
                    return -1;
                }
            }
        }
        if (exprDebug) printf("parseExpr(%d): calc %ld %s %ld\n", pr, val, ops[o].str, val2);
        switch (o)
        {
            case op_none:     val = val2; break;
            case op_pow:      val = ipow(val, val2); break;
            case op_mul:      val *= val2; break;
            case op_div:      val /= val2; break;
            case op_mod:      val %= val2; break;
            case op_plus:     val += val2; break;
            case op_minus:    val -= val2; break;
            case op_lshift:   val <<= val2; break;
            case op_urshift:  val = (unsigned long)val >> val2; break;
            case op_rshift:   val >>= val2; break;
            case op_min:      if (val2 < val) val = val2; break;
            case op_max:      if (val2 > val) val = val2; break;
            case op_cmp:      val = val < val2 ? -1 : val == val2 ? 0 : 1; break;
            case op_lteq:     val = val <= val2; break;
            case op_gteq:     val = val >= val2; break;
            case op_lt:       val = val < val2; break;
            case op_gt:       val = val > val2; break;
            case op_eq:       val = val == val2; break;
            case op_neq:      val = val != val2; break;
            case op_logicand: val = val && val2; break;
            case op_logicor:  val = val || val2; break;
            case op_bitand:   val &= val2; break;
            case op_bitxor:   val ^= val2; break;
            case op_bitor:    val |= val2; break;
            case op_alt:      if (!val) val = val2; break;
        }
        if (exprDebug) printf("parseExpr(%d): result %ld\n", pr, val);
        if (o2 == op_if)
        {
            long val3 = 0;
            val2 = 1;
            if (exprDebug) printf("parseExpr(%d) if %ld\n", pr, val);
            if ((o2 = parseExpr(&p, &val2)) >= 0)
            {
                if (exprDebug) printf("parseExpr(%d) then %ld\n", pr, val2);
                if (parseSep(&p, ":"))
                {
                    parseExpr(&p, &val3);
                    if (exprDebug) printf("parseExpr(%d) else %ld\n", pr, val3);
                }
            }
            if (exprDebug) printf("parseExpr(%d) if %ld then %ld else %ld\n", pr, val, val2, val3);
            val = val ? val2 : val3;
            if (exprDebug) printf("parseExpr(%d): result %ld, o2=%d, rest \"%s\" \n", pr, val, o2, p);
            if (o2 == -1) goto nextop;
        }
        o = o2;
    } while (ops[o].pr && pr <= ops[o].pr);
    if (exprDebug) printf("parseExpr(%d): value = %ld return %d %s\n", pr, val, o, ops[o].str);
    *pp = p;
    *v = val;
    return o;
}

static const char *getFormat(const char **pp)
{
    static char format [20];
    const char *p = *pp;
    unsigned int i = 1;
    if (exprDebug) printf("getFormat %s\n", p);
    if ((format[0] = *p++) == '%')
    {
        while (i < sizeof(format) && memchr(" #-+0", *p, 5))
            format[i++] = *p++;
        while (i < sizeof(format) && *p >= '0' && *p <= '9')
            format[i++] = *p++;
        if (i < sizeof(format))
            format[i++] = 'l';
        if (i < sizeof(format) && memchr("diouxXc", *p, 7))
        {
            format[i++] = *p++;
            format[i] = 0;
            *pp = p;
            if (exprDebug) printf("format = '%s'\n", format);
            return format;
        }
    }
    if (exprDebug) printf("no format\n");
    return NULL;
}

static int parseSlice(const char **pp, long* pstart, long* plength)
{
    const char *p = *pp;
    long slice_start = 0;
    long slice_length = 0;
    long string_length = *plength;
    char o;

    if (*p++ != '[') return 0;
    parseExpr(&p, &slice_start);
    if (slice_start < 0) slice_start += string_length;
    if ((o = parseSep(&p, ":,"))) /* [start,length] or [start:end] */
    {
        parseExpr(&p, &slice_length);
        if (o == ':')
        {
            if (slice_length < 0) slice_length += string_length;
            slice_length -= slice_start;
        }
    }
    else slice_length = 1;
    if (slice_start < 0)
    {
        slice_length += slice_start;
        slice_start = 0;
    }
    if (slice_start > string_length)
        slice_length = 0;
    if (slice_length > string_length - slice_start)
        slice_length = string_length - slice_start;
    if (slice_length < 0)
        slice_length = 0;
    skipSpace(p);
    if (*p++ != ']') return 0;
    *pstart += slice_start;
    *plength = slice_length;
    *pp = p;
    return 1;
}

static long parseString(const char **pp, const char **pstart)
{
    const char *p = *pp;
    const char *string_start = p;
    long slice_start = 0;
    long length = 0;
    char q;

    q = *p++;
    while (*p) /* string length with escapes */
    {
        if (*p == '\\')
            if (*++p == 0) break;
        if (*p++ == q) break;
        length++;
    }
    while (parseSlice(&p, &slice_start, &length));
    if (exprDebug) printf("parseString %.*s[%ld,%ld]\n", (int)(p-string_start), string_start, slice_start, length);
    if (length && pstart)
    {
        while (slice_start-- > 0)
        {
            if (*string_start++ == '\\') string_start++;
        }
        *pstart = ++string_start;
    }
    *pp = p;
    return length;
}

static void writeString(char** pw, const char* r, long length, char q)
{
    char* w = *pw;
    *w++ = q;
    while (length-- > 0)
    {
        if (*r == '\\') *w++ = *r++;
        *w++ = *r++;
    }
    *w++ = q;
    *w++ = 0;
    *pw = w;
}

size_t replaceExpressions(const char *r, char *buffer, size_t buffersize)
{
    long val, string_length;
    char *w = buffer;
    char *s;
    const char *string_start = r;
    char q;

    *w = 0;
    while (*r)
    {
        s = w;
        if (*r == '"' || *r == '\'')
        {
            /* quoted strings */
            q = *r;
            string_length = parseString(&r, &string_start);
            writeString(&w, string_start, string_length, q);
            if (exprDebug) printf("quoted string %s\n", s);
        }
        else if (*r == '%')
        {
            /* formatted expression */
            const char *r2 = r;
            const char *f;
            if (exprDebug) printf("formatted expression after '%s'\n", s);
            if ((f = getFormat(&r2)) && parseExpr(&r2, &val) >= 0)
            {
                r = r2;
                if (w > buffer && w[-1] == '(' && *r2++ == ')')
                {
                    w--;
                    r = r2;
                }
                w += sprintf(w, f , val);
                if (exprDebug) printf("formatted expression %s\n", s);
            }
        }
        else if (parseExpr(&r, &val) >= 0)
        {
            /* unformatted expression */
            if (r[-1] == '?' && (q = parseSep(&r, "\"'")))
            {
                /* handle expression ? "string1" : "string2" */
                w = s;
                r--;
                string_length = parseString(&r, &string_start);
                if (val) writeString(&w, string_start, string_length, q);
                if (parseSep(&r, ":"))
                {
                    string_length = parseString(&r, &string_start);
                    if (!val) writeString(&w, string_start, string_length, q);
                }
            }
            else
            {
                w += sprintf(w, "%ld", val);
            }
            if (exprDebug) printf("expression %s\n", s);
        }
        else {
            /* unquoted string (i.e plain word) */
            do {
                if ((*w++ = *r++) == '\\') if (*r) *w++ = *r++;
            } while (*r && !strchr("%(\"', \t\n", *r));
            *w = 0;
            if (exprDebug) printf("plain word '%s'\n", s);
        }
        /* copy space */
        while (isspace((unsigned char)*r) || *r == ',') *w++ = *r++;
        /* terminate */
        *w = 0;
    }
    return w - buffer;
}

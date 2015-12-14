#include "strm.h"
#include "khash.h"
#include <pthread.h>

#if defined(NO_READONLY_DATA_CHECK) || defined(_WIN32) || defined(__CYGWIN__)

static inline int readonly_data_p(const char *s)
{
    return 0;
}

#elif defined(__APPLE__) && defined(__MACH__)

#include <mach-o/getsect.h>

static inline int readonly_data_p(const char *p)
{
    return (void *) get_etext() < (void *) p
        && (void *) p < (void *) get_edata();
}

#else

extern char _etext[];
extern char __init_array_start[];

static inline int readonly_data_p(const char *p)
{
    return _etext < p && p < (char *) &__init_array_start;
}
#endif

struct sym_key {
    const char *ptr;
    size_t len;
};

static khint_t sym_hash(struct sym_key key)
{
    const char *s = key.ptr;
    khint_t h;
    size_t len = key.len;

    h = *s++;
    while (len--) {
        h = (h << 5) - h + (khint_t) * s++;
    }
    return h;
}

static khint_t sym_eq(struct sym_key a, struct sym_key b)
{
    if (a.len != b.len)
        return FALSE;
    if (memcmp(a.ptr, b.ptr, a.len) == 0)
        return TRUE;
    return FALSE;
}

KHASH_INIT(sym, struct sym_key, strm_string, 1, sym_hash, sym_eq);

static pthread_mutex_t sym_mutex = PTHREAD_MUTEX_INITIALIZER;
static khash_t(sym) * sym_table;


#if __BYTE_ORDER == __LITTLE_ENDIAN
# define VALP_PTR(p) ((char*)p)
#else
/* big endian */
# define VALP_PTR(p) (((char*)p)+4)
#endif
#define VAL_PTR(v) VALP_PTR(&v)

static strm_string str_new(const char *p, size_t len, int foreign)
{
    strm_value tag;
    strm_value val;
    char *s;

    if (!p)
        goto mkbuf;
    if (len < 6) {
        tag = STRM_TAG_STRING_I;
        val = 0;
        s = VAL_PTR(val) + 1;
        memcpy(s, p, len);
        s[-1] = len;
    } else if (len == 6) {
        tag = STRM_TAG_STRING_6;
        val = 0;
        s = VAL_PTR(val);
        memcpy(s, p, len);
    } else {
        struct strm_string *str;

        if (p && (foreign || readonly_data_p(p))) {
            tag = STRM_TAG_STRING_F;
            str = malloc(sizeof(struct strm_string));
            str->ptr = p;
        } else {
            char *buf;

          mkbuf:
            tag = STRM_TAG_STRING_O;
            str = malloc(sizeof(struct strm_string) + len + 1);
            buf = (char *) &str[1];
            if (p) {
                memcpy(buf, p, len);
            } else {
                memset(buf, 0, len);
            }
            buf[len] = '\0';
            str->ptr = buf;
        }
        str->len = len;
        val = (strm_value) str;
    }
    return tag | (val & STRM_VAL_MASK);
}

static strm_string str_intern(const char *p, size_t len)
{
    khiter_t k;
    struct sym_key key;
    int ret;
    strm_string str;

    if (len <= 6) {
        return str_new(p, len, 0);
    }
    if (!sym_table) {
        sym_table = kh_init(sym);
    }
    key.ptr = p;
    key.len = len;
    k = kh_put(sym, sym_table, key, &ret);

    if (ret == 0) {             /* found */
        return kh_value(sym_table, k);
    }
    str = str_new(p, len, 1);
    kh_key(sym_table, k).ptr = strm_str_ptr(str);
    kh_value(sym_table, k) = str;

    return str;
}

#ifndef STRM_STR_INTERN_LIMIT
#define STRM_STR_INTERN_LIMIT 64
#endif

strm_string strm_str_new(const char *p, size_t len)
{
    if (!strm_event_loop_started) {
        /* single thread mode */
        if (p && (len < STRM_STR_INTERN_LIMIT || readonly_data_p(p))) {
            return str_intern(p, len);
        }
    }
    return str_new(p, len, 0);
}

strm_string strm_str_intern(const char *p, size_t len)
{
    strm_string str;

    assert(p != NULL);
    if (!strm_event_loop_started) {
        return str_intern(p, len);
    }
    pthread_mutex_lock(&sym_mutex);
    str = str_intern(p, len);
    pthread_mutex_unlock(&sym_mutex);

    return str;
}

strm_string strm_str_intern_str(strm_string str)
{
    if (strm_str_intern_p(str)) {
        return str;
    }
    if (!strm_event_loop_started) {
        return str_intern(strm_str_ptr(str), strm_str_len(str));
    }
    pthread_mutex_lock(&sym_mutex);
    str = str_intern(strm_str_ptr(str), strm_str_len(str));
    pthread_mutex_unlock(&sym_mutex);

    return str;
}

int strm_str_intern_p(strm_string s)
{
    switch (strm_value_tag(s)) {
    case STRM_TAG_STRING_I:
    case STRM_TAG_STRING_6:
    case STRM_TAG_STRING_F:
        return TRUE;
    case STRM_TAG_STRING_O:
    default:
        return FALSE;
    }
}

int strm_str_eq(strm_string a, strm_string b)
{
    if (a == b)
        return TRUE;
    if (strm_value_tag(a) == STRM_TAG_STRING_F &&
        strm_value_tag(b) == STRM_TAG_STRING_F) {
        /* pointer comparison is OK if strings are interned */
        return FALSE;
    }
    if (strm_str_len(a) != strm_str_len(b))
        return FALSE;
    if (memcmp(strm_str_ptr(a), strm_str_ptr(b), strm_str_len(a)) == 0)
        return TRUE;
    return FALSE;
}

int strm_str_p(strm_value v)
{
    switch (strm_value_tag(v)) {
    case STRM_TAG_STRING_I:
    case STRM_TAG_STRING_6:
    case STRM_TAG_STRING_F:
    case STRM_TAG_STRING_O:
        return TRUE;
    default:
        return FALSE;
    }
}

const char *strm_strp_ptr(strm_string * s)
{
    switch (strm_value_tag(*s)) {
    case STRM_TAG_STRING_I:
        return VALP_PTR(s) + 1;
    case STRM_TAG_STRING_6:
        return VALP_PTR(s);
    case STRM_TAG_STRING_O:
    case STRM_TAG_STRING_F:
        {
            struct strm_string *str =
                (struct strm_string *) strm_value_val(*s);
            return str->ptr;
        }
    default:
        return NULL;
    }
}

const char *strm_strp_cstr(strm_string * s, char buf[])
{
    size_t len;

    switch (strm_value_tag(*s)) {
    case STRM_TAG_STRING_I:
        len = VALP_PTR(s)[0];
        if (len == 5) {
            memcpy(buf, VALP_PTR(s), len);
            return buf;
        }
        return VALP_PTR(s) + 1;
    case STRM_TAG_STRING_6:
        memcpy(buf, VALP_PTR(s), 6);
        return buf;
    case STRM_TAG_STRING_O:
    case STRM_TAG_STRING_F:
        {
            struct strm_string *str =
                (struct strm_string *) strm_value_val(*s);
            return str->ptr;
        }
    default:
        return NULL;
    }
}

size_t strm_str_len(strm_string s)
{
    switch (strm_value_tag(s)) {
    case STRM_TAG_STRING_I:
        return (size_t) VAL_PTR(s)[0];
    case STRM_TAG_STRING_6:
        return 6;
    case STRM_TAG_STRING_O:
    case STRM_TAG_STRING_F:
        {
            struct strm_string *str =
                (struct strm_string *) strm_value_val(s);

            return str->len;
        }
    default:
        return 0;
    }
}

int strm_string_p(strm_string s)
{
    switch (strm_value_tag(s)) {
    case STRM_TAG_STRING_I:
    case STRM_TAG_STRING_6:
    case STRM_TAG_STRING_O:
    case STRM_TAG_STRING_F:
        return TRUE;
    default:
        return FALSE;
    }
}

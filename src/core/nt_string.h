#ifndef _NT_STRING_H_
#define _NT_STRING_H_


#include <nt_def.h>
#include <nt_core.h>

typedef struct {
    size_t      len;
    u_char     *data;
} nt_str_t;



typedef struct {
    nt_str_t   key;
    nt_str_t   value;
} nt_keyval_t;

typedef struct {
    unsigned    len: 28;

    unsigned    valid: 1;
    unsigned    no_cacheable: 1;
    unsigned    not_found: 1;
    unsigned    escape: 1;

    u_char     *data;

} nt_variable_value_t;


#define nt_string(str)     { sizeof(str) - 1, (u_char *) str  }
#define nt_null_string     { 0, NULL  }
#define nt_str_set(str, text)                                               \
    (str)->len = sizeof(text) - 1; (str)->data = (u_char *) text
#define nt_str_null(str)   (str)->len = 0; (str)->data = NULL


#define nt_tolower(c)      (u_char) ((c >= 'A' && c <= 'Z') ? (c | 0x20) : c)
#define nt_toupper(c)      (u_char) ((c >= 'a' && c <= 'z') ? (c & ~0x20) : c)

void nt_strlow( u_char *dst, u_char *src, size_t n );


#define nt_strncmp(s1, s2, n)  strncmp((const char *) s1, (const char *) s2, n)


/* msvc and icc7 compile strcmp() to inline loop */
#define nt_strcmp(s1, s2)  strcmp((const char *) s1, (const char *) s2)

#define nt_strstr(s1, s2)  strstr((const char *) s1, (const char *) s2)
#define nt_strlen(s)       strlen((const char *) s)

size_t nt_strnlen( u_char *p, size_t n );

#define nt_strchr(s1, c)   strchr((const char *) s1, (int) c)


static nt_inline u_char *
nt_strlchr( u_char *p, u_char *last, u_char c )
{
    while( p < last ) {

        if( *p == c ) {
            return p;

        }

        p++;

    }

    return NULL;

}


/*
 *    * msvc and icc7 compile memset() to the inline "rep stos"
 *       * while ZeroMemory() and bzero() are the calls.
 *          * icc7 may also inline several mov's of a zeroed register for small blocks.
 *             */
#define nt_memzero(buf, n)       (void) memset(buf, 0, n)
#define nt_memset(buf, c, n)     (void) memset(buf, c, n)

void nt_explicit_memzero( void *buf, size_t n );


#if (NT_MEMCPY_LIMIT)

    void *nt_memcpy( void *dst, const void *src, size_t n );
    #define nt_cpymem(dst, src, n)   (((u_char *) nt_memcpy(dst, src, n)) + (n))

#else

    /*
    *    * gcc3, msvc, and icc7 compile memcpy() to the inline "rep movs".
    *       * gcc3 compiles memcpy(d, s, 4) to the inline "mov"es.
    *          * icc8 compile memcpy(d, s, 4) to the inline "mov"es or XMM moves.
    *             */
    #define nt_memcpy(dst, src, n)   (void) memcpy(dst, src, n)
    #define nt_cpymem(dst, src, n)   (((u_char *) memcpy(dst, src, n)) + (n))

#endif


#if ( __INTEL_COMPILER >= 800  )

/*
 *    * the simple inline cycle copies the variable length strings up to 16
 *       * bytes faster than icc8 autodetecting _intel_fast_memcpy()
 *          */

static nt_inline u_char *
nt_copy( u_char *dst, u_char *src, size_t len )
{
    if( len < 17 ) {

        while( len ) {
            *dst++ = *src++;
            len--;

        }

        return dst;


    } else {
        return nt_cpymem( dst, src, len );

    }

}

#else

#define nt_copy                  nt_cpymem

#endif


#define nt_memmove(dst, src, n)   (void) memmove(dst, src, n)
#define nt_movemem(dst, src, n)   (((u_char *) memmove(dst, src, n)) + (n))


/* msvc and icc7 compile memcmp() to the inline loop */
#define nt_memcmp(s1, s2, n)  memcmp((const char *) s1, (const char *) s2, n)


u_char *nt_cpystrn( u_char *dst, u_char *src, size_t n );
//u_char *nt_pstrdup( nt_pool_t *pool, nt_str_t *src );
u_char * nt_cdecl nt_sprintf( u_char *buf, const char *fmt, ... );
u_char * nt_cdecl nt_snprintf( u_char *buf, size_t max, const char *fmt, ... );
u_char * nt_cdecl nt_slprintf( u_char *buf, u_char *last, const char *fmt,
                               ... );
u_char *nt_vslprintf( u_char *buf, u_char *last, const char *fmt, va_list args );
#define nt_vsnprintf(buf, max, fmt, args)                                   \
    nt_vslprintf(buf, buf + (max), fmt, args)

nt_int_t nt_strcasecmp( u_char *s1, u_char *s2 );
nt_int_t nt_strncasecmp( u_char *s1, u_char *s2, size_t n );

u_char *nt_strnstr( u_char *s1, char *s2, size_t n );

u_char *nt_strstrn( u_char *s1, char *s2, size_t n );
u_char *nt_strcasestrn( u_char *s1, char *s2, size_t n );
u_char *nt_strlcasestrn( u_char *s1, u_char *last, u_char *s2, size_t n );

nt_int_t nt_rstrncmp( u_char *s1, u_char *s2, size_t n );
nt_int_t nt_rstrncasecmp( u_char *s1, u_char *s2, size_t n );
nt_int_t nt_memn2cmp( u_char *s1, u_char *s2, size_t n1, size_t n2 );
nt_int_t nt_dns_strcmp( u_char *s1, u_char *s2 );
nt_int_t nt_filename_cmp( u_char *s1, u_char *s2, size_t n );


nt_int_t nt_atoi( u_char *line, size_t n );
#if (T_NT_HTTP_IMPROVED_IF)
    long long nt_atoll( u_char *line, size_t n );
#endif
nt_int_t nt_atofp( u_char *line, size_t n, size_t point );
ssize_t nt_atosz( u_char *line, size_t n );
off_t nt_atoof( u_char *line, size_t n );
time_t nt_atotm( u_char *line, size_t n );
nt_int_t nt_hextoi( u_char *line, size_t n );

u_char *nt_hex_dump( u_char *dst, u_char *src, size_t len );


#define nt_base64_encoded_length(len)  (((len + 2) / 3) * 4)
#define nt_base64_decoded_length(len)  (((len + 3) / 4) * 3)

void nt_encode_base64( nt_str_t *dst, nt_str_t *src );
void nt_encode_base64url( nt_str_t *dst, nt_str_t *src );
nt_int_t nt_decode_base64( nt_str_t *dst, nt_str_t *src );
nt_int_t nt_decode_base64url( nt_str_t *dst, nt_str_t *src );

uint32_t nt_utf8_decode( u_char **p, size_t n );
size_t nt_utf8_length( u_char *p, size_t n );
u_char *nt_utf8_cpystrn( u_char *dst, u_char *src, size_t n, size_t len );

#define NT_ESCAPE_URI            0
#define NT_ESCAPE_ARGS           1
#define NT_ESCAPE_URI_COMPONENT  2
#define NT_ESCAPE_HTML           3
#define NT_ESCAPE_REFRESH        4
#define NT_ESCAPE_MEMCACHED      5
#define NT_ESCAPE_MAIL_AUTH      6

#define NT_UNESCAPE_URI       1
#define NT_UNESCAPE_REDIRECT  2

uintptr_t nt_escape_uri( u_char *dst, u_char *src, size_t size,
                         nt_uint_t type );
void nt_unescape_uri( u_char **dst, u_char **src, size_t size, nt_uint_t type );
uintptr_t nt_escape_html( u_char *dst, u_char *src, size_t size );
uintptr_t nt_escape_json( u_char *dst, u_char *src, size_t size );


typedef struct {
    nt_rbtree_node_t         node;
    nt_str_t                 str;

} nt_str_node_t;


void nt_str_rbtree_insert_value( nt_rbtree_node_t *temp,
                                 nt_rbtree_node_t *node, nt_rbtree_node_t *sentinel );
nt_str_node_t *nt_str_rbtree_lookup( nt_rbtree_t *rbtree, nt_str_t *name,
                                     uint32_t hash );


void nt_sort( void *base, size_t n, size_t size,
              nt_int_t ( *cmp )( const void *, const void * ) );
#define nt_qsort             qsort


#define nt_value_helper(n)   #n
#define nt_value(n)          nt_value_helper(n)

#endif


#ifndef _NT_ARRAY_H_INCLUDED_
#define _NT_ARRAY_H_INCLUDED_


#include <nt_core.h>


typedef struct {
    void        *elts;
    nt_uint_t   nelts;
    size_t       size;
    nt_uint_t   nalloc;
    nt_pool_t  *pool;
} nt_array_t;


nt_array_t *nt_array_create(nt_pool_t *p, nt_uint_t n, size_t size);
void nt_array_destroy(nt_array_t *a);
void *nt_array_push(nt_array_t *a);
void *nt_array_push_n(nt_array_t *a, nt_uint_t n);


static nt_inline nt_int_t
nt_array_init(nt_array_t *array, nt_pool_t *pool, nt_uint_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    array->elts = nt_palloc(pool, n * size);
    if (array->elts == NULL) {
        return NT_ERROR;
    }

    return NT_OK;
}


#endif /* _NT_ARRAY_H_INCLUDED_ */

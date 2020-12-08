


#ifndef _NT_LIST_H_INCLUDED_
#define _NT_LIST_H_INCLUDED_


#include <nt_core.h>


typedef struct nt_list_part_s  nt_list_part_t;

struct nt_list_part_s {
    void             *elts;
    nt_uint_t        nelts;
    nt_list_part_t  *next;
};


typedef struct {
    nt_list_part_t  *last;
    nt_list_part_t   part;
    size_t            size;
    nt_uint_t        nalloc;
    nt_pool_t       *pool;
} nt_list_t;


nt_list_t *nt_list_create(nt_pool_t *pool, nt_uint_t n, size_t size);

static nt_inline nt_int_t
nt_list_init(nt_list_t *list, nt_pool_t *pool, nt_uint_t n, size_t size)
{
    list->part.elts = nt_palloc(pool, n * size);
    if (list->part.elts == NULL) {
        return NT_ERROR;
    }

    list->part.nelts = 0;
    list->part.next = NULL;
    list->last = &list->part;
    list->size = size;
    list->nalloc = n;
    list->pool = pool;

    return NT_OK;
}


/*
 *
 *  the iteration through the list:
 *
 *  part = &list.part;
 *  data = part->elts;
 *
 *  for (i = 0 ;; i++) {
 *
 *      if (i >= part->nelts) {
 *          if (part->next == NULL) {
 *              break;
 *          }
 *
 *          part = part->next;
 *          data = part->elts;
 *          i = 0;
 *      }
 *
 *      ...  data[i] ...
 *
 *  }
 */


void *nt_list_push(nt_list_t *list);
#if (T_NT_IMPROVED_LIST)
nt_int_t nt_list_delete(nt_list_t *list, void *elt);
#endif


#endif /* _NT_LIST_H_INCLUDED_ */

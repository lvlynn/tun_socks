

#include <nt_core.h>


nt_list_t *
nt_list_create(nt_pool_t *pool, nt_uint_t n, size_t size)
{
    nt_list_t  *list;

    list = nt_palloc(pool, sizeof(nt_list_t));
    if (list == NULL) {
        return NULL;
    }

    if (nt_list_init(list, pool, n, size) != NT_OK) {
        return NULL;
    }

    return list;
}


void *
nt_list_push(nt_list_t *l)
{
    void             *elt;
    nt_list_part_t  *last;

    last = l->last;

    if (last->nelts == l->nalloc) {

        /* the last part is full, allocate a new list part */

        last = nt_palloc(l->pool, sizeof(nt_list_part_t));
        if (last == NULL) {
            return NULL;
        }

        last->elts = nt_palloc(l->pool, l->nalloc * l->size);
        if (last->elts == NULL) {
            return NULL;
        }

        last->nelts = 0;
        last->next = NULL;

        l->last->next = last;
        l->last = last;
    }

    elt = (char *) last->elts + l->size * last->nelts;
    last->nelts++;

    return elt;
}


#if (T_NT_IMPROVED_LIST)
static nt_int_t
nt_list_delete_elt(nt_list_t *list, nt_list_part_t *cur, nt_uint_t i)
{
    u_char *s, *d, *last;

    s = (u_char *) cur->elts + i * list->size;
    d = s + list->size;
    last = (u_char *) cur->elts + cur->nelts * list->size;

    while (d < last) {
        *s++ = *d++;
    }

    cur->nelts--;

    return NT_OK;
}


nt_int_t
nt_list_delete(nt_list_t *list, void *elt)
{
    u_char          *data;
    nt_uint_t       i;
    nt_list_part_t *part, *pre;

    part = &list->part;
    pre = part;
    data = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            i = 0;
            pre = part;
            part = part->next;
            data = part->elts;
        }

        if ((data + i * list->size)  == (u_char *) elt) {
            if (&list->part != part && part->nelts == 1) {
                pre->next = part->next;
                if (part == list->last) {
                    list->last = pre;
                }

                return NT_OK;
            }

            return nt_list_delete_elt(list, part, i);
        }
    }

    return NT_ERROR;
}
#endif

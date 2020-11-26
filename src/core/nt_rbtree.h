
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NT_RBTREE_H_INCLUDED_
#define _NT_RBTREE_H_INCLUDED_


#include <nt_def.h>
#include <nt_core.h>


typedef nt_uint_t  nt_rbtree_key_t;
typedef nt_int_t   nt_rbtree_key_int_t;


typedef struct nt_rbtree_node_s  nt_rbtree_node_t;

struct nt_rbtree_node_s {
    nt_rbtree_key_t       key;
    nt_rbtree_node_t     *left;
    nt_rbtree_node_t     *right;
    nt_rbtree_node_t     *parent;
    u_char                 color;
    u_char                 data;
};


typedef struct nt_rbtree_s  nt_rbtree_t;

typedef void (*nt_rbtree_insert_pt) (nt_rbtree_node_t *root,
    nt_rbtree_node_t *node, nt_rbtree_node_t *sentinel);

struct nt_rbtree_s {
    nt_rbtree_node_t     *root;
    nt_rbtree_node_t     *sentinel;
    nt_rbtree_insert_pt   insert;
};


#define nt_rbtree_init(tree, s, i)                                           \
    nt_rbtree_sentinel_init(s);                                              \
    (tree)->root = s;                                                         \
    (tree)->sentinel = s;                                                     \
    (tree)->insert = i


void nt_rbtree_insert(nt_rbtree_t *tree, nt_rbtree_node_t *node);
void nt_rbtree_delete(nt_rbtree_t *tree, nt_rbtree_node_t *node);
void nt_rbtree_insert_value(nt_rbtree_node_t *root, nt_rbtree_node_t *node,
    nt_rbtree_node_t *sentinel);
void nt_rbtree_insert_timer_value(nt_rbtree_node_t *root,
    nt_rbtree_node_t *node, nt_rbtree_node_t *sentinel);
nt_rbtree_node_t *nt_rbtree_next(nt_rbtree_t *tree,
    nt_rbtree_node_t *node);


#define nt_rbt_red(node)               ((node)->color = 1)
#define nt_rbt_black(node)             ((node)->color = 0)
#define nt_rbt_is_red(node)            ((node)->color)
#define nt_rbt_is_black(node)          (!nt_rbt_is_red(node))
#define nt_rbt_copy_color(n1, n2)      (n1->color = n2->color)


/* a sentinel must be black */

#define nt_rbtree_sentinel_init(node)  nt_rbt_black(node)


static nt_inline nt_rbtree_node_t *
nt_rbtree_min(nt_rbtree_node_t *node, nt_rbtree_node_t *sentinel)
{
    while (node->left != sentinel) {
        node = node->left;
    }

    return node;
}


#endif /* _NT_RBTREE_H_INCLUDED_ */

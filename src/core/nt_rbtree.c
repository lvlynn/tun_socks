
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <nt_def.h>
#include <nt_core.h>


/*
 * The red-black tree code is based on the algorithm described in
 * the "Introduction to Algorithms" by Cormen, Leiserson and Rivest.
 */


static nt_inline void nt_rbtree_left_rotate(nt_rbtree_node_t **root,
    nt_rbtree_node_t *sentinel, nt_rbtree_node_t *node);
static nt_inline void nt_rbtree_right_rotate(nt_rbtree_node_t **root,
    nt_rbtree_node_t *sentinel, nt_rbtree_node_t *node);


void
nt_rbtree_insert(nt_rbtree_t *tree, nt_rbtree_node_t *node)
{
    nt_rbtree_node_t  **root, *temp, *sentinel;

    /* a binary tree insert */

    root = &tree->root;
    sentinel = tree->sentinel;

    if (*root == sentinel) {
        node->parent = NULL;
        node->left = sentinel;
        node->right = sentinel;
        nt_rbt_black(node);
        *root = node;

        return;
    }

    tree->insert(*root, node, sentinel);

    /* re-balance tree */

    while (node != *root && nt_rbt_is_red(node->parent)) {

        if (node->parent == node->parent->parent->left) {
            temp = node->parent->parent->right;

            if (nt_rbt_is_red(temp)) {
                nt_rbt_black(node->parent);
                nt_rbt_black(temp);
                nt_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->right) {
                    node = node->parent;
                    nt_rbtree_left_rotate(root, sentinel, node);
                }

                nt_rbt_black(node->parent);
                nt_rbt_red(node->parent->parent);
                nt_rbtree_right_rotate(root, sentinel, node->parent->parent);
            }

        } else {
            temp = node->parent->parent->left;

            if (nt_rbt_is_red(temp)) {
                nt_rbt_black(node->parent);
                nt_rbt_black(temp);
                nt_rbt_red(node->parent->parent);
                node = node->parent->parent;

            } else {
                if (node == node->parent->left) {
                    node = node->parent;
                    nt_rbtree_right_rotate(root, sentinel, node);
                }

                nt_rbt_black(node->parent);
                nt_rbt_red(node->parent->parent);
                nt_rbtree_left_rotate(root, sentinel, node->parent->parent);
            }
        }
    }

    nt_rbt_black(*root);
}


void
nt_rbtree_insert_value(nt_rbtree_node_t *temp, nt_rbtree_node_t *node,
    nt_rbtree_node_t *sentinel)
{
    nt_rbtree_node_t  **p;

    for ( ;; ) {

        p = (node->key < temp->key) ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    nt_rbt_red(node);
}


void
nt_rbtree_insert_timer_value(nt_rbtree_node_t *temp, nt_rbtree_node_t *node,
    nt_rbtree_node_t *sentinel)
{
    nt_rbtree_node_t  **p;

    for ( ;; ) {

        /*
         * Timer values
         * 1) are spread in small range, usually several minutes,
         * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
         * The comparison takes into account that overflow.
         */

        /*  node->key < temp->key */

        p = ((nt_rbtree_key_int_t) (node->key - temp->key) < 0)
            ? &temp->left : &temp->right;

        if (*p == sentinel) {
            break;
        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    nt_rbt_red(node);
}


void
nt_rbtree_delete(nt_rbtree_t *tree, nt_rbtree_node_t *node)
{
    nt_uint_t           red;
    nt_rbtree_node_t  **root, *sentinel, *subst, *temp, *w;

    /* a binary tree delete */

    root = &tree->root;
    sentinel = tree->sentinel;

    if (node->left == sentinel) {
        temp = node->right;
        subst = node;

    } else if (node->right == sentinel) {
        temp = node->left;
        subst = node;

    } else {
        subst = nt_rbtree_min(node->right, sentinel);

        if (subst->left != sentinel) {
            temp = subst->left;
        } else {
            temp = subst->right;
        }
    }

    if (subst == *root) {
        *root = temp;
        nt_rbt_black(temp);

        /* DEBUG stuff */
        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;

        return;
    }

    red = nt_rbt_is_red(subst);

    if (subst == subst->parent->left) {
        subst->parent->left = temp;

    } else {
        subst->parent->right = temp;
    }

    if (subst == node) {

        temp->parent = subst->parent;

    } else {

        if (subst->parent == node) {
            temp->parent = subst;

        } else {
            temp->parent = subst->parent;
        }

        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        nt_rbt_copy_color(subst, node);

        if (node == *root) {
            *root = subst;

        } else {
            if (node == node->parent->left) {
                node->parent->left = subst;
            } else {
                node->parent->right = subst;
            }
        }

        if (subst->left != sentinel) {
            subst->left->parent = subst;
        }

        if (subst->right != sentinel) {
            subst->right->parent = subst;
        }
    }

    /* DEBUG stuff */
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;

    if (red) {
        return;
    }

    /* a delete fixup */

    while (temp != *root && nt_rbt_is_black(temp)) {

        if (temp == temp->parent->left) {
            w = temp->parent->right;

            if (nt_rbt_is_red(w)) {
                nt_rbt_black(w);
                nt_rbt_red(temp->parent);
                nt_rbtree_left_rotate(root, sentinel, temp->parent);
                w = temp->parent->right;
            }

            if (nt_rbt_is_black(w->left) && nt_rbt_is_black(w->right)) {
                nt_rbt_red(w);
                temp = temp->parent;

            } else {
                if (nt_rbt_is_black(w->right)) {
                    nt_rbt_black(w->left);
                    nt_rbt_red(w);
                    nt_rbtree_right_rotate(root, sentinel, w);
                    w = temp->parent->right;
                }

                nt_rbt_copy_color(w, temp->parent);
                nt_rbt_black(temp->parent);
                nt_rbt_black(w->right);
                nt_rbtree_left_rotate(root, sentinel, temp->parent);
                temp = *root;
            }

        } else {
            w = temp->parent->left;

            if (nt_rbt_is_red(w)) {
                nt_rbt_black(w);
                nt_rbt_red(temp->parent);
                nt_rbtree_right_rotate(root, sentinel, temp->parent);
                w = temp->parent->left;
            }

            if (nt_rbt_is_black(w->left) && nt_rbt_is_black(w->right)) {
                nt_rbt_red(w);
                temp = temp->parent;

            } else {
                if (nt_rbt_is_black(w->left)) {
                    nt_rbt_black(w->right);
                    nt_rbt_red(w);
                    nt_rbtree_left_rotate(root, sentinel, w);
                    w = temp->parent->left;
                }

                nt_rbt_copy_color(w, temp->parent);
                nt_rbt_black(temp->parent);
                nt_rbt_black(w->left);
                nt_rbtree_right_rotate(root, sentinel, temp->parent);
                temp = *root;
            }
        }
    }

    nt_rbt_black(temp);
}


static nt_inline void
nt_rbtree_left_rotate(nt_rbtree_node_t **root, nt_rbtree_node_t *sentinel,
    nt_rbtree_node_t *node)
{
    nt_rbtree_node_t  *temp;

    temp = node->right;
    node->right = temp->left;

    if (temp->left != sentinel) {
        temp->left->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->left) {
        node->parent->left = temp;

    } else {
        node->parent->right = temp;
    }

    temp->left = node;
    node->parent = temp;
}


static nt_inline void
nt_rbtree_right_rotate(nt_rbtree_node_t **root, nt_rbtree_node_t *sentinel,
    nt_rbtree_node_t *node)
{
    nt_rbtree_node_t  *temp;

    temp = node->left;
    node->left = temp->right;

    if (temp->right != sentinel) {
        temp->right->parent = node;
    }

    temp->parent = node->parent;

    if (node == *root) {
        *root = temp;

    } else if (node == node->parent->right) {
        node->parent->right = temp;

    } else {
        node->parent->left = temp;
    }

    temp->right = node;
    node->parent = temp;
}


nt_rbtree_node_t *
nt_rbtree_next(nt_rbtree_t *tree, nt_rbtree_node_t *node)
{
    nt_rbtree_node_t  *root, *sentinel, *parent;

    sentinel = tree->sentinel;

    if (node->right != sentinel) {
        return nt_rbtree_min(node->right, sentinel);
    }

    root = tree->root;

    for ( ;; ) {
        parent = node->parent;

        if (node == root) {
            return NULL;
        }

        if (node == parent->left) {
            return parent;
        }

        node = parent;
    }
}

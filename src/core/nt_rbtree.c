
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

//https://www.cnblogs.com/doop-ymc/p/3440316.html
/* 红黑树需要遵从下面的5条性质：
 *
 * （1）节点要么是红色要么是黑色；
 *
 * （2）根节点为黑色；
 *
 * （3）叶子节点即NIL节点必定为黑色；
 *
 * （4）红色节点的孩子节点必定为黑色；
 *
 * （5）从任一节点到叶子节点，所包含的黑色节点数目相同，即黑高度相同；
 *  */

static nt_inline void nt_rbtree_left_rotate( nt_rbtree_node_t **root,
        nt_rbtree_node_t *sentinel, nt_rbtree_node_t *node );
static nt_inline void nt_rbtree_right_rotate( nt_rbtree_node_t **root,
        nt_rbtree_node_t *sentinel, nt_rbtree_node_t *node );


int nt_rbtree_insert_value( nt_flag_t flag, nt_rbtree_key_t tree_key, nt_rbtree_key_t  cur_key )
{
    if( flag == RBTREE_INSERT ) {
        if( cur_key < tree_key )
            return 1;
        else
            return -1;
    }

    if( flag == RBTREE_SEARCH ) {
        if( cur_key < tree_key )
            return 1;
        else if( cur_key > tree_key )
            return -1;
        else
            return 0;
    }

    return 1;
}


int nt_rbtree_insert_timer_value( nt_flag_t flag, nt_rbtree_key_t tree_key, nt_rbtree_key_t cur_key )
{
    /*
     * Timer values
     * 1) are spread in small range, usually several minutes,
     * 2) and overflow each 49 days, if milliseconds are stored in 32 bits.
     * The comparison takes into account that overflow.
     */
    if( flag == RBTREE_INSERT ) {
        if( ( nt_rbtree_key_int_t )( cur_key - tree_key ) < 0 )
            return 1;
        else
            return -1;
    }

    if( flag == RBTREE_SEARCH ) {
        if( ( nt_rbtree_key_int_t )( cur_key - tree_key ) < 0 )
            return 1;
        else if( ( nt_rbtree_key_int_t )( cur_key - tree_key ) > 0 )
            return -1;
        else
            return 0;
    }
}


//查找
nt_rbtree_node_t *nt_rbtree_search( nt_rbtree_t *tree, nt_rbtree_key_t key )
{
    nt_rbtree_node_t   *root,  *sentinel;

    root = tree->root;
    sentinel = tree->sentinel;
    if( tree->root == sentinel ){
        printf( "nt_rbtree_search return root sentinel\n" );
        return sentinel;
    }

    int ret = 0;
    while( ( ret = tree->insert( RBTREE_SEARCH, root->key, key ) ) != 0 ) {
    //    printf( "ret=%d", ret );
        if( ret < 0 )
            root = root->right;
        else
            root = root->left;

        if( root == sentinel )
           break; 
        /* if( key < root->key )
            root = root->left;
        else
            root = root->right; */
    }
    return root;
}





/*
 *    * 前序遍历"红黑树"
 **/
static void preorder( nt_rbtree_t *tree, nt_uint_t handle )
{
    typedef struct {
        int port;
        int test;
        char ip[18];
    } nt_test_t;
    nt_rbtree_t rt = *tree;
    nt_rbtree_node_t *node = tree->root;
    nt_rbtree_node_t *sentinel = tree->sentinel;

    if( node != sentinel ) {

        /*
            typedef void (*c) ( nt_rbtree_key_t key  );
            c = ( *(void (**)(nt_rbtree_key_t))&handle );
            c( node->key );

            c  test = ( c  )&handle ;
            test( node->key );

           (*(c *)&handle)(node->key);
        */

        ( *( void ( ** )( nt_rbtree_key_t ) )&handle )( node->key );

        rt.root = node->left;
        preorder( &rt, handle );
        rt.root = node->right;
        preorder( &rt, handle );
    }


}

//void ( nt_rbtree_dump_handle *)( nt_rbtree_key_t key );
void  nt_rbtree_dump( nt_rbtree_t *tree, nt_uint_t handle )
{
    //  nt_rbtree_dump_handle = void *(nt_rbtree_key_t ) handle;
    if( tree )
        preorder( tree, handle );

}



int nt_rbtree_call_insert( nt_rbtree_t *tree, nt_rbtree_node_t *node )
{

//   ()((void *)(temp) - sizeof(nt_rbtree_node_t))
//    nt_rbtree_t *tree = ( nt_rbtree_t * )ptr;
    nt_rbtree_node_t *sentinel = tree->sentinel ;
    nt_rbtree_node_t *temp = tree->root ;
    nt_rbtree_node_t  **p;

    for( ;; ) {
        int ret = tree->insert( RBTREE_INSERT, temp->key, node->key );
        if( ret > 0 )
            p = &temp->left;
        else if( ret < 0 )
            p = &temp->right;
        else {
//            printf( "相同不添加\n" );
            node->parent = sentinel;
            return  NT_ERROR;
        }

        //       p = (tnode->port < ttemp->port) ? &temp->left : &temp->right;
        if( *p == sentinel ) {
            break;

        }

        temp = *p;
    }

    *p = node;
    node->parent = temp;
    node->left = sentinel;
    node->right = sentinel;
    nt_rbt_red( node );

    return NT_OK;
}

void
nt_rbtree_insert( nt_rbtree_t *tree, nt_rbtree_node_t *node )
{
    nt_rbtree_node_t  **root, *temp, *sentinel;

    /* a binary tree insert */
    //获取树的根结点
    root = &tree->root;
    sentinel = tree->sentinel;

    if( *root == sentinel ) {
        node->parent = NULL;
        // node->parent = sentinel;
        node->left = sentinel;
        node->right = sentinel;
        nt_rbt_black( node );
        *root = node;
        
        tree->count += 1;
        return;
    }

    //进行插入操作
//    tree->insert(root, node, sentinel);
    if( nt_rbtree_call_insert( tree, node ) == NT_ERROR )
        return ;

    tree->count += 1;

    /* re-balance tree */
    //对树进行平衡处理,仅需要处理父结点为红色的情况
    while( node != *root && nt_rbt_is_red( node->parent ) ) {

        //插入节点的父结点为左结点
        if( node->parent == node->parent->parent->left ) {
            temp = node->parent->parent->right;

            //插入结点的叔父结点为红，此时仅需要将叔父结点与父结点改为黑，祖父结点改为红，然后
            //再以祖父结点为开始，进行平衡
            if( nt_rbt_is_red( temp ) ) {
                nt_rbt_black( node->parent );
                nt_rbt_black( temp );
                nt_rbt_red( node->parent->parent );
                node = node->parent->parent;

            } else {
                //若插入到父结点的右边进行LR旋转
                if( node == node->parent->right ) {
                    node = node->parent;
                    nt_rbtree_left_rotate( root, sentinel, node );
                }

                //若插入到左边进行R旋转
                nt_rbt_black( node->parent );
                nt_rbt_red( node->parent->parent );
                nt_rbtree_right_rotate( root, sentinel, node->parent->parent );
            }

        } else {
            temp = node->parent->parent->left;
            //同上，操作对称
            if( nt_rbt_is_red( temp ) ) {
                nt_rbt_black( node->parent );
                nt_rbt_black( temp );
                nt_rbt_red( node->parent->parent );
                node = node->parent->parent;

            } else {
                if( node == node->parent->left ) {
                    node = node->parent;
                    nt_rbtree_right_rotate( root, sentinel, node );
                }

                nt_rbt_black( node->parent );
                nt_rbt_red( node->parent->parent );
                nt_rbtree_left_rotate( root, sentinel, node->parent->parent );
            }
        }
    }

    nt_rbt_black( *root );
}

int
nt_rbtree_delete_key( nt_rbtree_t *tree,  nt_rbtree_key_t key )
{
    nt_rbtree_node_t *d;

    d = nt_rbtree_search( tree, key );

    if( d == tree->sentinel )
        return -1;

    nt_rbtree_delete( tree, d );

    return 0;
}


void
nt_rbtree_delete( nt_rbtree_t *tree, nt_rbtree_node_t *node )
{
    nt_uint_t           red;
    nt_rbtree_node_t  **root, *sentinel, *subst, *temp, *w;

    /* a binary tree delete */
    //取得树的根结点及NIL结点
    root = &tree->root;
    sentinel = tree->sentinel;

    /* 下面是获取node结点的后继结点，temp指向替换结点，subst指向后继结点，即待删除结点
     * 当待删除结点有左右子树为空时，temp指向非空子结点，subst指向node结点
     * 否则，temp指向替换结点，subst指向后继结点
     */
    if( node->left == sentinel ) {
        temp = node->right;
        subst = node;

    } else if( node->right == sentinel ) {
        temp = node->left;
        subst = node;

    } else {
        //查找后继结点
        subst = nt_rbtree_min( node->right, sentinel );
        //得到替换结点的指针

        if( subst->left != sentinel ) {
            temp = subst->left;
        } else {
            temp = subst->right;
        }
    }


    tree->count -= 1;
    //如果待删除结点为根结点，些时的情况是要么树中就一个根结点，要么存在一个红色的孩子
    //调整树的根结点指针，更改替换结点的颜色
    //如果要删除的是根节点，应该把跟节点变成哨兵节点
    if( subst == *root ) {
        *root = temp;
        nt_rbt_black( temp );

        // DEBUG stuff
        node->left = NULL;
        node->right = NULL;
        node->parent = NULL;
        node->key = 0;

        return;
    }

    //保存后继结点的红黑性
    red = nt_rbt_is_red( subst );

    //先直接删除后继结点，将替换结点连接至后继结点的父结点
    //先将subst从树中删除出来
    if( subst == subst->parent->left ) {
        subst->parent->left = temp;

    } else {
        subst->parent->right = temp;
    }

    //然后是根据node的情况处理
    //首先当node与subst相同，即node存在左子树或者右子树为空时的情况时，直接删除，连接子结
    //点的parent指针到node的父结点
    if( subst == node ) {

        temp->parent = subst->parent;
        //若不同，则直接用subst结点替换node结点，而不是进行值的复制
    } else {
        //修改替换结点的父指针
        if( subst->parent == node ) {
            temp->parent = subst;

        } else {
            temp->parent = subst->parent;
        }

        //完成后继结点替换node结点，下面是复制node结点的指针值及红黑性
        subst->left = node->left;
        subst->right = node->right;
        subst->parent = node->parent;
        nt_rbt_copy_color( subst, node );

        //若node为根结点，修改树的根结点指针
        if( node == *root ) {
            *root = subst;

        } else {
            //或者修改node的父结点对subst的指向
            if( node == node->parent->left ) {
                node->parent->left = subst;
            } else {
                node->parent->right = subst;
            }
        }

        //修改以前的node结点的子结点的指向
        if( subst->left != sentinel ) {
            subst->left->parent = subst;
        }

        if( subst->right != sentinel ) {
            subst->right->parent = subst;
        }
    }

    /* DEBUG stuff */
    node->left = NULL;
    node->right = NULL;
    node->parent = NULL;
    node->key = 0;

    //case 1：后继结点为红色，则必定满足红黑树的性质不需要做调整
    if( red ) {
        return;
    }

    /* a delete fixup */
    //case 2.1：后继结点为黑色，且替换结点为黑色，实际上此时的替换结点为NIL，
    //但因为删除了一个黑色结点导致黑高度减一，红黑树性质破坏，调整红黑树
    while( temp != *root && nt_rbt_is_black( temp ) ) {

        //case 2.1.1：替换结点为左结点
        if( temp == temp->parent->left ) {

            //w指向右兄弟结点
            w = temp->parent->right;

            //case 2.1.1.1：右兄弟结点为红色，此时将右兄弟结点变黑，父结点变红（父结点以前的颜色不影响最后结果）
            //再进行一次左旋转，实际上将些种情形转变为下面的三种情况。
            if( nt_rbt_is_red( w ) ) {
                nt_rbt_black( w );
                nt_rbt_red( temp->parent );
                nt_rbtree_left_rotate( root, sentinel, temp->parent );
                w = temp->parent->right;
            }

            //case 2.1.1.2：右兄弟结点拥有两，这种情况要分两种处理
            //case 2.1.1.2.1：父结点为黑色，则将右兄弟结点变红后，还需要将指针移到父结点，进行继续处理
            // //case 2.1.1.2.2：父结点为红色，则将右兄弟结点变红后，将父结点变黑，处理就ok了，不过这个父结点
            //变黑是在最后进行处理的，以防现在变黑继续处理。
            //还需要注意一个问题：这里的右兄弟结点必定为黑色的，为红色情况已经优先处理了。
            if( nt_rbt_is_black( w->left ) && nt_rbt_is_black( w->right ) ) {
                nt_rbt_red( w );
                temp = temp->parent;
                ////case 2.1.1.3：右兄弟结点的了结点颜色不一致，些时需要分情况讨论
            } else {
                //case 2.1.1.3.1：右子侄结点为黑色，进行RL旋转
                if( nt_rbt_is_black( w->right ) ) {
                    nt_rbt_black( w->left );
                    nt_rbt_red( w );
                    nt_rbtree_right_rotate( root, sentinel, w );
                    w = temp->parent->right;
                }

                //case 2.1.1.3.2：右子侄结点为红色，进行L旋转
                nt_rbt_copy_color( w, temp->parent );
                nt_rbt_black( temp->parent );
                nt_rbt_black( w->right );
                nt_rbtree_left_rotate( root, sentinel, temp->parent );
                temp = *root;
            }

        } else {
            ////case 2.1.2：替换结点为右结点，原理同上，操作对称，不详解（话说这情况时是不是左子树为空啊！）
            w = temp->parent->left;

            if( nt_rbt_is_red( w ) ) {
                nt_rbt_black( w );
                nt_rbt_red( temp->parent );
                nt_rbtree_right_rotate( root, sentinel, temp->parent );
                w = temp->parent->left;
            }

            if( nt_rbt_is_black( w->left ) && nt_rbt_is_black( w->right ) ) {
                nt_rbt_red( w );
                temp = temp->parent;

            } else {
                if( nt_rbt_is_black( w->left ) ) {
                    nt_rbt_black( w->right );
                    nt_rbt_red( w );
                    nt_rbtree_left_rotate( root, sentinel, w );
                    w = temp->parent->left;
                }

                nt_rbt_copy_color( w, temp->parent );
                nt_rbt_black( temp->parent );
                nt_rbt_black( w->left );
                nt_rbtree_right_rotate( root, sentinel, temp->parent );
                temp = *root;
            }
        }
    }

    //case 2.2 后继结点为黑色，替换结点为红色，直接修改替换结点为黑色，
    //红黑树的性质得到恢复
    nt_rbt_black( temp );
}


static nt_inline void
nt_rbtree_left_rotate( nt_rbtree_node_t **root, nt_rbtree_node_t *sentinel,
                       nt_rbtree_node_t *node )
{
    nt_rbtree_node_t  *temp;

    temp = node->right;
    node->right = temp->left;

    if( temp->left != sentinel ) {
        temp->left->parent = node;
    }

    temp->parent = node->parent;

    if( node == *root ) {
        *root = temp;

    } else if( node == node->parent->left ) {
        node->parent->left = temp;

    } else {
        node->parent->right = temp;
    }

    temp->left = node;
    node->parent = temp;
}


static nt_inline void
nt_rbtree_right_rotate( nt_rbtree_node_t **root, nt_rbtree_node_t *sentinel,
                        nt_rbtree_node_t *node )
{
    nt_rbtree_node_t  *temp;

    temp = node->left;
    node->left = temp->right;

    if( temp->right != sentinel ) {
        temp->right->parent = node;
    }

    temp->parent = node->parent;

    if( node == *root ) {
        *root = temp;

    } else if( node == node->parent->right ) {
        node->parent->right = temp;

    } else {
        node->parent->left = temp;
    }

    temp->right = node;
    node->parent = temp;
}


nt_rbtree_node_t *
nt_rbtree_next( nt_rbtree_t *tree, nt_rbtree_node_t *node )
{
    nt_rbtree_node_t  *root, *sentinel, *parent;

    sentinel = tree->sentinel;

    if( node->right != sentinel ) {
        return nt_rbtree_min( node->right, sentinel );
    }

    root = tree->root;

    for( ;; ) {
        parent = node->parent;

        if( node == root ) {
            return NULL;
        }

        if( node == parent->left ) {
            return parent;
        }

        node = parent;
    }
}

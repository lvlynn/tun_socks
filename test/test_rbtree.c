#include <nt_core.h>

#define random(x) (rand()%x)

int main()
{
    nt_rbtree_t tree;
    nt_rbtree_node_t sentinel;

    nt_rbtree_init( &tree, &sentinel, nt_rbtree_insert_value );

    nt_rbtree_node_t rbnode[100] ;
    int i ;
    for( i = 99; i >= 0 ; i-- ) {
      //  rbnode[i].key = i;
        rbnode[i].key = random(1000);
        rbnode[i].parent = NULL;
        rbnode[i].left = NULL;
        rbnode[i].right = NULL;

        nt_rbtree_insert( &tree, &rbnode[i] );
    }


    for(  i = 0; i < 100; i++ ) {
        nt_rbtree_node_t *p = nt_rbtree_min( tree.root, &sentinel );
		printf("key=%d\n",  p->key);
        nt_rbtree_delete( &tree, p );
    }

    return 0;
}

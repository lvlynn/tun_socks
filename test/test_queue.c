#include <nt_def.h>
#include <nt_core.h>

typedef struct {
    nt_int_t num;
    nt_str_t str;
    nt_queue_t queue;

} TestNode;

nt_int_t compare_node( const nt_queue_t *left, const nt_queue_t *right )
{
    TestNode* left_node  = nt_queue_data( left, TestNode, queue );
    TestNode* right_node = nt_queue_data( right, TestNode, queue );

    return left_node->num > right_node->num;

}


int main()
{
    nt_queue_t QueHead;
    nt_queue_init( &QueHead );

    TestNode Node[10];
    nt_int_t i;
    for( i = 0; i < 10; ++i ) {
        Node[i].num = rand() % 100;

    }

    nt_queue_insert_head( &QueHead, &Node[0].queue );
    nt_queue_insert_tail( &QueHead, &Node[1].queue );
    nt_queue_insert_after( &QueHead, &Node[2].queue );
    nt_queue_insert_head( &QueHead, &Node[4].queue );
    nt_queue_insert_tail( &QueHead, &Node[3].queue );
    nt_queue_insert_head( &QueHead, &Node[5].queue );
    nt_queue_insert_tail( &QueHead, &Node[6].queue );
    nt_queue_insert_after( &QueHead, &Node[7].queue );
    nt_queue_insert_head( &QueHead, &Node[8].queue );
    nt_queue_insert_tail( &QueHead, &Node[9].queue );

    nt_queue_t *q;
    for( q = nt_queue_head( &QueHead ); q != nt_queue_sentinel( &QueHead ); q = nt_queue_next( q ) ) {
        TestNode* Node = nt_queue_data( q, TestNode, queue );
        printf( "Num=%d\n", Node->num );

    }

    nt_queue_sort( &QueHead, compare_node );

    printf( "\n" );
    for( q = nt_queue_head( &QueHead ); q != nt_queue_sentinel( &QueHead ); q = nt_queue_next( q ) ) {
        TestNode* Node = nt_queue_data( q, TestNode, queue );
        printf( "Num=%d\n", Node->num );

    }

    return 0;

}

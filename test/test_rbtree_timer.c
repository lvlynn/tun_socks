#include <nt_core.h>

int main()
{
    printf("Hello world\n");
    nt_rbtree_t tree ; //树
    nt_rbtree_node_t sentnel; //哨兵节点
    
    //定时器节点
    nt_rbtree_node_t timer;
//    nt_msec_t timer; //key

    //第三个参数为插入的回调函数
    nt_rbtree_init( &tree, &sentnel, nt_rbtree_insert_timer_value  );

    //插入定时器1
    timer.key = 1111;
    nt_rbtree_insert(&tree, &timer);

    //删除定时器
    //    nt_rbtree_delete(&tree, &timer);

    //插入定时器1
    nt_rbtree_node_t timer1;
    timer1.key = 1110;
    nt_rbtree_insert(&tree, &timer1);

    nt_rbtree_node_t *root, *sen , *min;


    root = tree.root ;
    sen = tree.sentinel;

    min = nt_rbtree_min(root, sen);

    printf( "%d\n", min->key );
        

    return 0;
}


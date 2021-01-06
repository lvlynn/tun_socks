

#ifndef _NT_HASH_H_INCLUDED_
#define _NT_HASH_H_INCLUDED_


#include <nt_core.h>

//散列方法的函数指针定义
//传入关键字的首地址，len是关键字的长度
typedef nt_uint_t ( *nt_hash_key_pt )( u_char *data, size_t len );


//------------------------------------------------------------
// 动态hash表
//------------------------------------------------------------

//该结构表示散列表中的桶
typedef struct hash_elt_s{

    struct hash_elt_s *next;

    void    *value; //关键字值

    u_short     len;   //关键字长度

    /*元素关键字首地址*/
    u_char        name[1]; //url，也就是<key,value>中的value

} hash_elt_t;



//主要用于提供创建一个hash所需要的一些信息
typedef struct {

    hash_elt_t  **buckets; //每个桶的起始地址

    //散列方法
    nt_hash_key_pt   key;  //hash函数指针

    //散列方法
    nt_hash_key_pt   search;  //hash元素查找指针


    //最大槽位
    nt_uint_t        size; //hash表中桶的最大值,实际桶的个数存放在前面ngx_hash_t中的size字段中
    //每个槽位的大小，这就限制了关键字的最大长度

    //散列表名字
    char             *name;  //hash表的名字，其实没啥用（在log中用）

    //给散列表分配空间的内存池
    nt_pool_t       *pool;   //构建hash所用的内存池
    nt_pool_t       *temp_pool;   //构建hash所用的内存池
} hash_t;

nt_int_t hash_init( hash_t *hash, nt_uint_t size);
nt_int_t hash_add( hash_t *hash, u_char *name, u_char *value);
nt_int_t hash_del( hash_t *hash, u_char *name);
void * hash_query( hash_t *hash, u_char *name );
nt_int_t hash_free( hash_t *hash );
void  hash_dump( hash_t *hash );
 

//------------------------------------------------------------

//该结构表示散列表中的桶
typedef struct nt_hash_elt_s{
    // struct nt_hash_elt_s  *next;
    /*指向用户自定义元素数据*/
    void             *value; //ip，也就是<key,value>中的key
    /*元素关键字的长度*/
    u_short           len;   //url长度


    /*元素关键字首地址*/
    u_char            name[1]; //url，也就是<key,value>中的value


} nt_hash_elt_t;


//散列表结构体
typedef struct {
    //散列表中槽的个数
    nt_hash_elt_t  **buckets; //每个桶的起始地址
    nt_uint_t        size;  //桶的个数
//    nt_uint_t        type; // 0 静态  // 1 可动态添加删除
} nt_hash_t;

//这个结构主要用于包含通配符的hash的，具体如下：
//表示前置或者后置通配符
typedef struct {
    //基本散列表
    nt_hash_t        hash;
    //用来存放某个已经达到末尾的通配符url对应的value值，
    //如果通配符url没有达到末尾，这个字段为NULL。
    //value指针可以指向用户数据
    void             *value;
} nt_hash_wildcard_t;


typedef struct {
    //元素关键字
    nt_str_t         key;
    //计算出来的hash
    nt_uint_t        key_hash;
    //实际用户数据
    void             *value;
} nt_hash_key_t;


typedef struct {
    //精准匹配的散列表
    nt_hash_t            hash;
    //查询前置通配符的散列表
    nt_hash_wildcard_t  *wc_head;
    //查询后置通配符的散列表
    nt_hash_wildcard_t  *wc_tail;
} nt_hash_combined_t;

//主要用于提供创建一个hash所需要的一些信息
typedef struct {
    //指向普通的完全匹配散列表
    nt_hash_t       *hash; //指向待新建的hash表
    //散列方法
    nt_hash_key_pt   key;  //hash函数指针

    //最大槽位
    nt_uint_t        max_size; //hash表中桶的最大值,实际桶的个数存放在前面ngx_hash_t中的size字段中
    //每个槽位的大小，这就限制了关键字的最大长度
    nt_uint_t        bucket_size;  //每个桶的最大尺寸


    //散列表名字
    char             *name;  //hash表的名字，其实没啥用（在log中用）

    //给散列表分配空间的内存池
    nt_pool_t       *pool;   //构建hash所用的内存池
    nt_pool_t       *temp_pool;  //构建hash所用的临时内存池
} nt_hash_init_t;


#define NT_HASH_SMALL            1
#define NT_HASH_LARGE            2

#define NT_HASH_LARGE_ASIZE      16384
#define NT_HASH_LARGE_HSIZE      10007

#define NT_HASH_WILDCARD_KEY     1
#define NT_HASH_READONLY_KEY     2

//这个结构存放了初始化hash需要的所有键值对
typedef struct {
    nt_uint_t        hsize;

    nt_pool_t       *pool;
    nt_pool_t       *temp_pool;

    nt_array_t       keys;  //存放不包含通配符的<key,value>键值对
    nt_array_t      *keys_hash;  //用来检测冲突的

    nt_array_t       dns_wc_head;
    nt_array_t      *dns_wc_head_hash;

    nt_array_t       dns_wc_tail;
    nt_array_t      *dns_wc_tail_hash;
} nt_hash_keys_arrays_t;


typedef struct {
    nt_uint_t        hash;
    nt_str_t         key;
    nt_str_t         value;
    u_char           *lowcase_key;
} nt_table_elt_t;


void *nt_hash_find( nt_hash_t *hash, nt_uint_t key, u_char *name, size_t len );
void *nt_hash_find_wc_head( nt_hash_wildcard_t *hwc, u_char *name, size_t len );
void *nt_hash_find_wc_tail( nt_hash_wildcard_t *hwc, u_char *name, size_t len );
void *nt_hash_find_combined( nt_hash_combined_t *hash, nt_uint_t key,
                             u_char *name, size_t len );

nt_int_t nt_hash_init( nt_hash_init_t *hinit, nt_hash_key_t *names,
                       nt_uint_t nelts );
nt_int_t nt_hash_wildcard_init( nt_hash_init_t *hinit, nt_hash_key_t *names,
                                nt_uint_t nelts );

//hash函数
#define nt_hash(key, c)   ((nt_uint_t) key * 31 + c)
//下述方法都是使用BKDR算法进行hash，区别在于第二个对key的内容进行小写化处理
nt_uint_t nt_hash_key( u_char *data, size_t len );
nt_uint_t nt_hash_key_lc( u_char *data, size_t len );
nt_uint_t nt_hash_strlow( u_char *dst, u_char *src, size_t n );


nt_int_t nt_hash_keys_array_init( nt_hash_keys_arrays_t *ha, nt_uint_t type );
nt_int_t nt_hash_add_key( nt_hash_keys_arrays_t *ha, nt_str_t *key,
                          void *value, nt_uint_t flags );



#endif /* _NT_HASH_H_INCLUDED_ */

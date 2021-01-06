

#include <nt_core.h>
//https://blog.csdn.net/cai2016/article/details/52728761
//========================动态删除增加=============================
//

//取出包括q的type类型的地址。这样我们就能够訪问type内的成员
#define nt_hash_data(q, type, link)                                         \
    (type *) ((u_char *) q - offsetof(type, link))

#if 1


nt_int_t
hash_add( hash_t *hash, u_char *name, u_char *value){
    nt_uint_t key_hash;
    hash_elt_t  *elt;
    hash_elt_t  *pre;
    hash_elt_t  *add;
    size_t len;
    nt_uint_t pos;

    len = strlen( name  );

//    debug( "name=%s, len=%d", name, len );
    key_hash = hash->key( name, len);
    // key_hash = nt_hash_key( name, len);
    pos = key_hash  % hash->size ; 
    elt = hash->buckets[ pos ];
//    debug( "pos=%ld", pos );

//    debug( "elt= %p", elt );
    if( elt == NULL ){
        elt = ( hash_elt_t *)  malloc(  sizeof( hash_elt_t ) + len );
//        debug( "elt= %p", elt );

        elt->next = NULL;
        elt->len = len;
        nt_memcpy( elt->name, name, len);
        elt->value = value;
        hash->buckets[pos] = elt; 
        return NT_OK;
    }

    while( elt ){
        if (len != (size_t) elt->len) {
            pre = elt;
            elt = elt->next;
            continue;
        }

        //已存在，无需添加
        if( nt_strncmp( elt->name , name , len  ) == 0)
            return NT_BUSY;

        pre = elt;
        elt = elt->next;
    }

    add = ( hash_elt_t *)  malloc(  sizeof( hash_elt_t ) + len );
    if( add == NULL )
      return  NT_ERROR;

//    debug( "pre =%p", pre );
//    debug( "elt =%p", &elt );
    //elt = nt_hash_data( &elt, nt_hash_elt_t, next );

 //   debug( "elt =%p", elt );
   
    add->next=NULL;
    add->len = len;
    nt_memcpy( add->name, name, len);
    add->value = value;
    pre->next = add;

    return NT_OK;
}

void 
hash_dump( hash_t *hash ){
    
    debug( "=========dump============"  );
    hash_elt_t  *elt;
    hash_elt_t  *next;
    nt_uint_t pos;
    char name[64] = {0};
    while( pos < hash->size  ){
        elt = hash->buckets[ pos  ];
        while( elt ){
            snprintf( name,  elt->len + 1, "%s", elt->name  );
            debug( "%s", name  );
            next = elt->next;
            nt_free( elt );
            elt = next;
        }
        pos ++;
    }
}



nt_int_t
hash_free( hash_t *hash ){
    
    hash_elt_t  *elt;
    hash_elt_t  *next;
    nt_uint_t pos;

    while( pos < hash->size  ){
        elt = hash->buckets[ pos  ];
        while( elt ){
            next = elt->next;
            nt_free( elt );
            elt = next;
        }
        pos ++;
    }
}


nt_int_t
hash_del( hash_t *hash, u_char *name){

    nt_uint_t key_hash;
    hash_elt_t  *elt;
    hash_elt_t *pre = NULL;
    size_t len;
    nt_uint_t pos;

    len = strlen( name  );

    key_hash = hash->key( name, len);
    pos = key_hash  % hash->size ; 
    elt = hash->buckets[ pos ];

    if( elt == NULL )
        return NT_ERROR;

    //当前桶未存 键值对，无需删除
    if( elt->len == 0 ){
        return NT_OK;
    } 

    while( elt ){
        if (len != (size_t) elt->len) {
            pre = elt;
            elt = elt->next;
            continue;
        }

        //对比
        if( nt_strncmp(  elt->name , name , len  ) == 0){
            debug( "找到 name=%s", name );
            break;
        }

        pre = elt;
        elt = elt->next;
    }

    if( elt == NULL ){
        debug( "未找到要删除key" );
        return NT_ERROR;
    }

    //删除的是只存一个的情况
    if( pre == NULL ){
        hash->buckets[pos] = NULL;
    } else{
        pre->next = elt->next;
    }


    nt_free( elt );
    elt = NULL;
    return NT_OK;
}

void *
hash_query( hash_t *hash, u_char *name ){
    nt_uint_t key_hash;
    hash_elt_t  *elt;
    size_t len;

    len = strlen( name  );
    
    debug( "len=%d", len );

    key_hash = hash->key( name, len);
    elt = hash->buckets[key_hash % hash->size];

    debug( "elt=%p", elt );
    if( elt == NULL )
        return NULL;

    //当前桶未存 键值对
    if( elt->len == 0 ){
        return NULL;
    } 

    while( elt ){
        if (len != (size_t) elt->len) {
            elt = elt->next;
            continue;
        }

        debug( "en=%s, n=%s", elt->name , name );
        //对比
        if( nt_strncmp(  elt->name , name , len  ) == 0){
            break;
        }

        elt = elt->next;
    }

    //未找到
    if( elt == NULL )
        return NULL;

    return elt->value;
}

#endif
//初始化成可动态添加删除的链地址法哈希表 
    nt_int_t
hash_init( hash_t *hash, nt_uint_t size)
{
    nt_uint_t i;
    hash_elt_t **buckets;
    hash->size = size;
    hash->key = nt_hash_key ;

    buckets = nt_pcalloc(hash->pool, size * sizeof( hash_elt_t *));
    if (buckets == NULL) {
        return NT_ERROR;
    }

    for( i = 0; i< size; i++ ){
        buckets[i] = NULL;
    //    debug( "buckets[%d] = %p", i, &buckets[i] );
    }
    
    hash->buckets = buckets;

    for( i = 0; i< size; i++ )
    //    debug( "buckets[%d] = %p", i, buckets[i] );
    return NT_OK;

}
//=================================================================
/*
 * nginx 哈希表采用开放地址法，而不是链地址法
 * 关于开放地址法通常需要有三种方法：线性探测、二次探测、再哈希法。
 * key生成采用BKDR算法
 *
 * */

/* @hash 表示哈希表的结构体
 * @key  表示根据哈希方法计算出来的hash值
 * @name 表示实际关键字地址
 * @len  表示实际关键字长度
 */
	void *
nt_hash_find(nt_hash_t *hash, nt_uint_t key, u_char *name, size_t len)
{
    nt_uint_t       i = 0;
    nt_hash_elt_t  *elt = NULL;

#if 0
    nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0, "hf:\"%*s\"", len, name);
#endif

	//根据hash值找到桶索引
	//桶结束的标志为 value 为 NULL
    elt = hash->buckets[key % hash->size];

    if (elt == NULL) {
        return NULL;
    }

    while (elt->value) {
        //判断元素集合中的key长度是否等于要查找的关键字长度
		//关键字长度不匹配，下一个
        if (len != (size_t) elt->len) {
            goto next;
        }

       // debug( "ni=%s, ei=%s", name, elt->name );
		//遍历关键字每一个字符，若全部对得上，则找到，否则有一个不同下一个
        //等同于strncmp , 比较name 和elt->name 是否相等
        for (i = 0; i < len; i++) {
            //相等直接返回，不相同就继续next
            if (name[i] != elt->name[i]) {
                goto next;
            }
        }

        return elt->value;

    next:

        //通过对齐的方式得到下一个元素
        //从elt关键字开始向后移动关键字长度个，并行对齐，即为下一个ngx_hash_elt_t
        elt = (nt_hash_elt_t *) nt_align_ptr(&elt->name[0] + elt->len,
                                             sizeof(void *));

        continue;
    }

    return NULL;
}

/*
 * 这个函数根据name在前缀通配符hash中查找对应的value值。
 * 有了hash表后，查找是件相对更容易的事，从后往前的获取name的每个字段（根据.分割），
 * 用每个字段查hash表，如果获取的value值的标记存在下一级hash，则用同样的方法查下一个字段对应的hash表，
 * 就这样直到查到的value为真实的值为止。另
 * 一个通配符查找函数ngx_hash_find_wc_tail这是同样的原理，不同的是对name从前往后处理每个字段而已。
 *
 * */

/* @hwc  表示支持通配符的哈希表的结构体
 * @name 表示实际关键字地址
 * @len  表示实际关键字长度
 */
	void *
nt_hash_find_wc_head(nt_hash_wildcard_t *hwc, u_char *name, size_t len)
{
    void        *value;
    nt_uint_t   i, n, key;

#if 0
    nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0, "wch:\"%*s\"", len, name);
#endif

    n = len;
	//从后往前搜索第一个dot，则n 到 len-1 即为关键字中最后一个 子关键字
    while (n) {
        if (name[n - 1] == '.') {
            break;
        }

        n--;
    }

    key = 0;

	//n 到 len-1 即为关键字中最后一个 子关键字，计算其hash值
    for (i = n; i < len; i++) {
        key = nt_hash(key, name[i]);
    }

#if 0
    nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0, "key:\"%ui\"", key);
#endif

	//调用普通查找找到关键字的value（用户自定义数据指针）
    value = nt_hash_find(&hwc->hash, key, &name[n], len - n);

#if 0
    nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0, "value:\"%p\"", value);
#endif
	/* 	用value指针低2位
	 *	00 - value 是 "example.com" 和 "*.example.com"的数据指针
	 *  01 - value 仅仅是 "*.example.com"的数据指针
	 *  10 - value 是 支持通配符哈希表是 "example.com" 和 "*.example.com" 指针
	 *  11 - value 仅仅是 "*.example.com"的指针 
	 */

	if (value) {

		// value 为 10 或 11
		if ((uintptr_t) value & 2) {

			//搜索到了最后一个子关键字且没有通配符，如"example.com"的example
			if (n == 0) {

                /* "example.com" */
				//value低两位为11，仅为"*.example.com"的指针，这里没有通配符，没招到，返回NULL
                if ((uintptr_t) value & 1) {
                    return NULL;
                }

				//value低两位为10，为"example.com"的指针，value就在下一级的ngx_hash_wildcard_t 的value中，去掉携带的低2位11
                hwc = (nt_hash_wildcard_t *)
                                          ((uintptr_t) value & (uintptr_t) ~3);
                return hwc->value;
            }

			//还未搜索完，低两位为11或10，继续去下级ngx_hash_wildcard_t中搜索
            hwc = (nt_hash_wildcard_t *) ((uintptr_t) value & (uintptr_t) ~3);

			//继续搜索 关键字中剩余部分，如"example.com"，搜索 0 到 n -1 即为 example
            value = nt_hash_find_wc_head(hwc, name, n - 1);

			//若找到，则返回
            if (value) {
                return value;
            }

			//低两位为00 找到，即为wc->value
            return hwc->value;
        }

		//低两位为01
        if ((uintptr_t) value & 1) {

			//关键字没有通配符，错误返回空
            if (n == 0) {

                /* "example.com" */

                return NULL;
            }

			//有通配符，直接返回
            return (void *) ((uintptr_t) value & (uintptr_t) ~3);
        }

		//低两位为00，直接返回
        return value;
    }

    return hwc->value;
}


void *
nt_hash_find_wc_tail(nt_hash_wildcard_t *hwc, u_char *name, size_t len)
{
    void        *value;
    nt_uint_t   i, key;

#if 0
    nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0, "wct:\"%*s\"", len, name);
#endif

    key = 0;

	//从前往后搜索第一个dot，则0 到 i 即为关键字中第一个 子关键字
    for (i = 0; i < len; i++) {
        if (name[i] == '.') {
            break;
        }

		//计算哈希值
        key = nt_hash(key, name[i]);
    }

	//没有通配符，返回NULL
    if (i == len) {
        return NULL;
    }

#if 0
    nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0, "key:\"%ui\"", key);
#endif

	//调用普通查找找到关键字的value（用户自定义数据指针）
    value = nt_hash_find(&hwc->hash, key, name, i);

#if 0
    nt_log_error(NT_LOG_ALERT, nt_cycle->log, 0, "value:\"%p\"", value);
#endif


  		/**还记得上节在ngx_hash_wildcard_init中，用value指针低2位来携带信息吗？其是有特殊意义的，如下：
         * 00 - value 是数据指针
		 * 11 - value的指向下一个哈希表, "example.*"
		 */
    if (value) {

		//低2位为11，value的指向下一个哈希表，递归搜索
        if ((uintptr_t) value & 2) {
		
            i++;

            hwc = (nt_hash_wildcard_t *) ((uintptr_t) value & (uintptr_t) ~3);

            value = nt_hash_find_wc_tail(hwc, &name[i], len - i);

			//找到低两位11，返回
            if (value) {
                return value;
            }

			//找到低两位00，返回hwc->value
            return hwc->value;
        }

        return value;
    }

    return hwc->value;
}

/*
	组合哈希表的查找思路非常简单，
	先在普通哈希表中查找，
	没找到再去前置通配符哈希表中查找，
	最后去后置通配符哈希表中查找
*/
void *
nt_hash_find_combined(nt_hash_combined_t *hash, nt_uint_t key, u_char *name,
    size_t len)
{
    void  *value = NULL;

    if (hash->hash.buckets) {
//        debug( "hask-key=%ld , name=%s", key,  name );
        value = nt_hash_find(&hash->hash, key, name, len);

        if (value) {
            return value;
        }
    }

    if (len == 0) {
        return NULL;
    }

    if (hash->wc_head && hash->wc_head->hash.buckets) {
        value = nt_hash_find_wc_head(hash->wc_head, name, len);

        if (value) {
            return value;
        }
    }

    if (hash->wc_tail && hash->wc_tail->hash.buckets) {
        value = nt_hash_find_wc_tail(hash->wc_tail, name, len);

        if (value) {
            return value;
        }
    }

    debug( "retrun NULL" );
    return NULL;
}


#define NT_HASH_ELT_SIZE(name)                                               \
    (sizeof(void *) + nt_align((name)->key.len + 2, sizeof(void *)))

/*
 * hinit参数是初始化hash的相关信息，
 * names存放了所有需要插入到hash中的<key,value>对，
 * nelts是<key,value>对的个数。
 * */
nt_int_t
nt_hash_init(nt_hash_init_t *hinit, nt_hash_key_t *names, nt_uint_t nelts)
{
    u_char          *elts;
    size_t           len;
    u_short         *test;
	nt_uint_t       i, n, key, size, start, bucket_size;
	nt_hash_elt_t  *elt, **buckets;

   
    //-------------------------------------------------------------
    //以下初始化成静态哈希表



	/* nelts是关键字的数量，bucket_size为一个bucket的大小，这里注意的就是一个bucket至少可以容得下一个关键字，
	 * 而下面的NGX_HASH_ELT_SIZE(&name[n]) + sizeof(void *)正好就是一个关键字所占的空间。
	 * 通过判断条件来看，如果我们设定的bucket大小，必须保证能容得下任何一个关键字，否则，就报错，提示bucket指定的太小。
	 * 关于NGX_HASH_ELT_SIZE这个宏，这里提一下，nginx把所有定位到某个bucket的关键字，即冲突的，封装成ngx_hash_elt_t结构
	 * 挨在一起放置，这样组成了一个ngx_hash_elt_t数组，这个数组空间的地址，由ngx_hash_t中的buckets保存。对于某个关键字来说，
	 * 它有一个ngx_hash_elt_t的头结构和紧跟在后面的内容组成，从这个角度看一个关键字所占用的空间正好等于NGX_HASH_ELT_SIZE宏的值
	 * 只是里面多了一个对齐的动作。
	 */
    //检查哈希表中的桶个数 不能为0
    if (hinit->max_size == 0) {
        nt_log_error(NT_LOG_EMERG, hinit->pool->log, 0,
                      "could not build %s, you should "
                      "increase %s_max_size: %i",
                      hinit->name, hinit->name, hinit->max_size);
        return NT_ERROR;
    }


    //检查哈希表中的桶的大小不能超过 ( 65536 - nt_cacheline_size )
    if (hinit->bucket_size > 65536 - nt_cacheline_size) {
        nt_log_error(NT_LOG_EMERG, hinit->pool->log, 0,
                      "could not build %s, too large "
                      "%s_bucket_size: %i",
                      hinit->name, hinit->name, hinit->bucket_size);
        return NT_ERROR;
    }

    // （1）检查bucket_size是否合法，也就是它的值必须保证一个桶至少能存放一个<key,value>键值对，具体如下：
    for (n = 0; n < nelts; n++) {
        /*
         * 宏NGX_HASH_ELT_SIZE用来计算一个ngx_hash_key_t表示一个实际的<key,value>键值对占用内存的大小，
         * 之所以NGX_HASH_ELT_SIZE(&names[n]) 后面需要加上sizeof(void *)，
         * 主要是每个桶都用一个值位NULL的void*指针来标记结束。
         * */
		/* 
         * 这里考虑放置每个bucket最后的null指针所需要的空间，即代码中的sizeof(void *)，这个NULL在find过程中作为一个bucket
         * 的结束标记来使用。
         */
        if (hinit->bucket_size < NT_HASH_ELT_SIZE(&names[n]) + sizeof(void *))
        {
            nt_log_error(NT_LOG_EMERG, hinit->pool->log, 0,
                          "could not build %s, you should "
                          "increase %s_bucket_size: %i",
                          hinit->name, hinit->name, hinit->bucket_size);
            return NT_ERROR;
        }
    }

 /* max_size是bucket的最大数量, 这里的test是用来做探测用的，探测的目标是在当前bucket的数量下，冲突发生的是否频繁。
     * 过于频繁则说明当前的bucket数量过少，需要调整。那么如何判定冲突过于频繁呢？就是利用这个test数组，它总共有max_size个
     * 元素，即最大的bucket。每个元素会累计落到该位置关键字长度，当大于256个字节，即u_short所表示的最大大小时，则判定
     * bucket过少，引起了严重的冲突。后面会看到具体的处理。
     */
    test = nt_alloc(hinit->max_size * sizeof(u_short), hinit->pool->log);
    if (test == NULL) {
        return NT_ERROR;
    }

    ////除去桶标记后桶的大小
	 /* 每个bucket的末尾一个null指针作为bucket的结束标志， 这里bucket_size是容纳实际数据大小，故减去一个指针大小 */
    bucket_size = hinit->bucket_size - sizeof(void *);

    //桶的最小个数
	/* 
     * 这里考虑NGX_HASH_ELT_SIZE中，由于对齐的缘故，一个关键字最少需要占用两个指针的大小。
     * 在这个前提下，来估计所需要的bucket最小数量，即考虑元素越小，从而一个bucket容纳的数量就越多，
     * 自然使用的bucket的数量就越少，但最少也得有一个。
     */
    start = nelts / (bucket_size / (2 * sizeof(void *)));
    debug( "start=%d", start  );

    start = start ? start : 1;

/* 
     * 调整max_size，即bucket数量的最大值，依据是：bucket超过10000，且总的bucket数量与元素个数比值小于100
     * 那么bucket最大值减少1000，至于这几个判断值的由来，尚不清楚，经验值或者理论值。
     */
    if (hinit->max_size > 10000 && nelts && hinit->max_size / nelts < 100) {
        start = hinit->max_size - 1000;
    }

    //更新size来满足bucket_size
/* 在之前确定的最小bucket个数的基础上，开始探测(通过test数组)并根据需要适当扩充，前面有分析其原理 */
    debug( "start=%d", start );
    for (size = start; size <= hinit->max_size; size++) {
//        debug( "size=%d", size );

        nt_memzero(test, size * sizeof(u_short));

        for (n = 0; n < nelts; n++) {
            if (names[n].key.data == NULL) {
                continue;
            }

            key = names[n].key_hash % size;  //计算当前<key,value>在哪个桶
  //          debug( "key=%d", key );
//            debug( "name=%s", names[n].key.data );
  //          debug( "NT_HASH_ELT_SIZE=%d", NT_HASH_ELT_SIZE(&names[n]) );
            len = test[key] + NT_HASH_ELT_SIZE(&names[n]);  //计算当前<key,value>插入到桶之后桶的大小
  //          debug( "len=%d", len );

#if 0
            nt_log_error(NT_LOG_ALERT, hinit->pool->log, 0,
                          "%ui: %ui %uz \"%V\"",
                          size, key, len, &names[n].key);
#endif

            //检查桶是否溢出了
            if (len > bucket_size) {
                goto next;
            }

            test[key] = (u_short) len;
        }

        goto found;

/* 到next这里，就是实际处理bucket扩充的情况了，即递增表示bucket数量的size变量 */
    next:

        continue;
    }

    size = hinit->max_size;
    debug( "size=%d", size );

    nt_log_error(NT_LOG_WARN, hinit->pool->log, 0,
                  "could not build optimal %s, you should increase "
                  "either %s_max_size: %i or %s_bucket_size: %i; "
                  "ignoring %s_bucket_size",
                  hinit->name, hinit->name, hinit->max_size,
                  hinit->name, hinit->bucket_size, hinit->name);

found:

	/* 确定了合适的bucket数量，即size。 重新初始化test数组，初始值为一个指针大小。*/
    for (i = 0; i < size; i++) {
        test[i] = sizeof(void *);
    }

	/* 统计各个bucket中的关键字所占的空间，这里要提示一点，test[i]中除了基本的数据大小外，还有一个指针的大小
     * 如上面的那个for循环所示。
     */
    for (n = 0; n < nelts; n++) {
        if (names[n].key.data == NULL) {
            continue;
        }

        key = names[n].key_hash % size;
        len = test[key] + NT_HASH_ELT_SIZE(&names[n]);

        if (len > 65536 - nt_cacheline_size) {
            nt_log_error(NT_LOG_EMERG, hinit->pool->log, 0,
                          "could not build %s, you should "
                          "increase %s_max_size: %i",
                          hinit->name, hinit->name, hinit->max_size);
            nt_free(test);
            return NT_ERROR;
        }

        test[key] = (u_short) len;
    }

    //计算所有桶的大小
    len = 0;

	/* 调整成对齐到cacheline的大小，并记录所有元素的总长度 */
    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {
            continue;
        }

        //每个桶大小满足cache行对齐
        test[i] = (u_short) (nt_align(test[i], nt_cacheline_size));

        len += test[i];
    }

    //hash为NULL，则动态生成管理hash的结构
/* 
     * 申请bucket元素所占的空间，这里注意的一点就是，如果之前hash表头结构没有申请，
     * 那么在申请时将ngx_hash_wildcard_t结构也一起申请了。
     */
    if (hinit->hash == NULL) {
        //calloc会把获取的内存初始化为0
        hinit->hash = nt_pcalloc(hinit->pool, sizeof(nt_hash_wildcard_t)
                                             + size * sizeof(nt_hash_elt_t *));
        if (hinit->hash == NULL) {
            nt_free(test);
            return NT_ERROR;
        }

        buckets = (nt_hash_elt_t **)
                      ((u_char *) hinit->hash + sizeof(nt_hash_wildcard_t));

    } else {
        buckets = nt_pcalloc(hinit->pool, size * sizeof(nt_hash_elt_t *));
        if (buckets == NULL) {
            nt_free(test);
            return NT_ERROR;
        }
    }

    debug( "len=%d", len );
    //将所有桶占用的空间分配在连续的内存空间中
    elts = nt_palloc(hinit->pool, len + nt_cacheline_size);
    if (elts == NULL) {
        nt_free(test);
        return NT_ERROR;
    }

    elts = nt_align_ptr(elts, nt_cacheline_size);

/* 设置各个bucket中包含实际数据的空间的地址(或者说位置) */
    for (i = 0; i < size; i++) {
        if (test[i] == sizeof(void *)) {
            continue;
        }
        //初始化每个桶的起始位置
        buckets[i] = (nt_hash_elt_t *) elts;
        elts += test[i];
    }

/* 用来累计真实数据的长度，不计结尾指针的长度 */
    for (i = 0; i < size; i++) {
        test[i] = 0;
    }

/* 依次向各个bucket中填充实际的内容，代码没什么好分析的。*/
    for (n = 0; n < nelts; n++) {
        if (names[n].key.data == NULL) {
            continue;
        }

        //计算当前<key,value>所在的桶
        key = names[n].key_hash % size;
        //计算当前<key,value>所在桶中的位置
        elt = (nt_hash_elt_t *) ((u_char *) buckets[key] + test[key]);

        //将当前<key,value>的值复制到桶中
//        debug( "names[n].value=%d", names[n].value );
        elt->value = names[n].value;
        elt->len = (u_short) names[n].key.len;

        nt_strlow(elt->name, names[n].key.data, names[n].key.len);

        //更新当前桶的大小
/* test[key]记录当前bucket内容的填充位置，即下次填充的开始位置 */
        test[key] = (u_short) (test[key] + NT_HASH_ELT_SIZE(&names[n]));
    }

    //为了标记每个桶的结束
 /* 设置bucket结束位置的null指针，*/
    for (i = 0; i < size; i++) {
        if (buckets[i] == NULL) {
            continue;
        }

/* 
         * 由于前面bucket的处理中多留出了一个指针的空间，而此时的test[i]是bucket中实际数据的共长度，
         * 所以bucket[i] + test[i]正好指向了末尾null指针所在的位置。处理的时候，把它当成一个ngx_hash_elt_t结构看，
         * 在该结构中的第一个元素，正好是一个void指针，我们只处理它，别的都不去碰，所以没有越界的问题。
         */
        elt = (nt_hash_elt_t *) ((u_char *) buckets[i] + test[i]);

        //前面每个test加上sizeof(void *)就是为了这个value指针
        elt->value = NULL;
    }

    nt_free(test);

    hinit->hash->buckets = buckets;
    hinit->hash->size = size;

#if 0

    for (i = 0; i < size; i++) {
        nt_str_t   val;
        nt_uint_t  key;

        elt = buckets[i];

        if (elt == NULL) {
            nt_log_error(NT_LOG_ALERT, hinit->pool->log, 0,
                          "%ui: NULL", i);
            continue;
        }

        while (elt->value) {
            val.len = elt->len;
            val.data = &elt->name[0];

            key = hinit->key(val.data, val.len);

            nt_log_error(NT_LOG_ALERT, hinit->pool->log, 0,
                          "%ui: %p \"%V\" %ui", i, elt, &val, key);

            elt = (nt_hash_elt_t *) nt_align_ptr(&elt->name[0] + elt->len,
                                                   sizeof(void *));
        }
    }

#endif

    return NT_OK;
}


nt_int_t
nt_hash_wildcard_init(nt_hash_init_t *hinit, nt_hash_key_t *names,
    nt_uint_t nelts)
{
    size_t                len, dot_len;
    nt_uint_t            i, n, dot;
    nt_array_t           curr_names, next_names;
    nt_hash_key_t       *name, *next_name;
    nt_hash_init_t       h;
    nt_hash_wildcard_t  *wdc;

    if (nt_array_init(&curr_names, hinit->temp_pool, nelts,
                       sizeof(nt_hash_key_t))
        != NT_OK)
    {
        return NT_ERROR;
    }

    if (nt_array_init(&next_names, hinit->temp_pool, nelts,
                       sizeof(nt_hash_key_t))
        != NT_OK)
    {
        return NT_ERROR;
    }

    for (n = 0; n < nelts; n = i) {

#if 0
        nt_log_error(NT_LOG_ALERT, hinit->pool->log, 0,
                      "wc0: \"%V\"", &names[n].key);
#endif

        dot = 0;

        for (len = 0; len < names[n].key.len; len++) {
            if (names[n].key.data[len] == '.') {
                dot = 1;
                break;
            }
        }

        name = nt_array_push(&curr_names);
        if (name == NULL) {
            return NT_ERROR;
        }

        name->key.len = len;
        name->key.data = names[n].key.data;
        name->key_hash = hinit->key(name->key.data, name->key.len);
        name->value = names[n].value;

#if 0
        nt_log_error(NT_LOG_ALERT, hinit->pool->log, 0,
                      "wc1: \"%V\" %ui", &name->key, dot);
#endif

        dot_len = len + 1;

        if (dot) {
            len++;
        }

        next_names.nelts = 0;

        if (names[n].key.len != len) {
            next_name = nt_array_push(&next_names);
            if (next_name == NULL) {
                return NT_ERROR;
            }

            next_name->key.len = names[n].key.len - len;
            next_name->key.data = names[n].key.data + len;
            next_name->key_hash = 0;
            next_name->value = names[n].value;

#if 0
            nt_log_error(NT_LOG_ALERT, hinit->pool->log, 0,
                          "wc2: \"%V\"", &next_name->key);
#endif
        }

        for (i = n + 1; i < nelts; i++) {
            if (nt_strncmp(names[n].key.data, names[i].key.data, len) != 0) {
                break;
            }

            if (!dot
                && names[i].key.len > len
                && names[i].key.data[len] != '.')
            {
                break;
            }

            next_name = nt_array_push(&next_names);
            if (next_name == NULL) {
                return NT_ERROR;
            }

            next_name->key.len = names[i].key.len - dot_len;
            next_name->key.data = names[i].key.data + dot_len;
            next_name->key_hash = 0;
            next_name->value = names[i].value;

#if 0
            nt_log_error(NT_LOG_ALERT, hinit->pool->log, 0,
                          "wc3: \"%V\"", &next_name->key);
#endif
        }

        if (next_names.nelts) {

            h = *hinit;
            h.hash = NULL;

            if (nt_hash_wildcard_init(&h, (nt_hash_key_t *) next_names.elts,
                                       next_names.nelts)
                != NT_OK)
            {
                return NT_ERROR;
            }

            wdc = (nt_hash_wildcard_t *) h.hash;

            if (names[n].key.len == len) {
                wdc->value = names[n].value;
            }

            name->value = (void *) ((uintptr_t) wdc | (dot ? 3 : 2));

        } else if (dot) {
            name->value = (void *) ((uintptr_t) name->value | 1);
        }
    }

    if (nt_hash_init(hinit, (nt_hash_key_t *) curr_names.elts,
                      curr_names.nelts)
        != NT_OK)
    {
        return NT_ERROR;
    }

    return NT_OK;
}


nt_uint_t
nt_hash_key(u_char *data, size_t len)
{
//    debug( "data=%s", data );
    nt_uint_t  i, key;

    key = 0;

    for (i = 0; i < len; i++) {
        key = nt_hash(key, data[i]);
    }

    return key;
}


nt_uint_t
nt_hash_key_lc(u_char *data, size_t len)
{
    nt_uint_t  i, key;

    key = 0;

    for (i = 0; i < len; i++) {
        key = nt_hash(key, nt_tolower(data[i]));
    }

    return key;
}


nt_uint_t
nt_hash_strlow(u_char *dst, u_char *src, size_t n)
{
    nt_uint_t  key;

    key = 0;

    while (n--) {
        *dst = nt_tolower(*src);
        key = nt_hash(key, *dst);
        dst++;
        src++;
    }

    return key;
}


nt_int_t
nt_hash_keys_array_init(nt_hash_keys_arrays_t *ha, nt_uint_t type)
{
    nt_uint_t  asize;

    if (type == NT_HASH_SMALL) {
        asize = 4;
        ha->hsize = 107;

    } else {
        asize = NT_HASH_LARGE_ASIZE;
        ha->hsize = NT_HASH_LARGE_HSIZE;
    }

    if (nt_array_init(&ha->keys, ha->temp_pool, asize, sizeof(nt_hash_key_t))
        != NT_OK)
    {
        return NT_ERROR;
    }

    if (nt_array_init(&ha->dns_wc_head, ha->temp_pool, asize,
                       sizeof(nt_hash_key_t))
        != NT_OK)
    {
        return NT_ERROR;
    }

    if (nt_array_init(&ha->dns_wc_tail, ha->temp_pool, asize,
                       sizeof(nt_hash_key_t))
        != NT_OK)
    {
        return NT_ERROR;
    }

    ha->keys_hash = nt_pcalloc(ha->temp_pool, sizeof(nt_array_t) * ha->hsize);
    if (ha->keys_hash == NULL) {
        return NT_ERROR;
    }

    ha->dns_wc_head_hash = nt_pcalloc(ha->temp_pool,
                                       sizeof(nt_array_t) * ha->hsize);
    if (ha->dns_wc_head_hash == NULL) {
        return NT_ERROR;
    }

    ha->dns_wc_tail_hash = nt_pcalloc(ha->temp_pool,
                                       sizeof(nt_array_t) * ha->hsize);
    if (ha->dns_wc_tail_hash == NULL) {
        return NT_ERROR;
    }

    return NT_OK;
}

/*
 * ha中包含了三类ngx_hash_key_t类型的数组，分别用来初始化三类hash（介绍ngx_hash_keys_arrays_t时已说明），那么新的<key,value>应该加入到哪个数组中就是ngx_hash_add_key这个函数的主要任务。参数中的flags用来标记是否key中可能包含通配符，一般这个参数设置为NGX_HASH_WILDCARD_KEY，即可能包含通配符。需要注意的是这个函数会改变包含通配符的key，将通配符去掉，如*.baidu.com会改变为com.baidu.，.baidu.com会改变为com.baidu，www.baidu.*会改变为www.baidu，www.baidu.这种通配是不允许出现的。
 *
 * */
/*
 *  向哈希键数组中添加键并去重
 *
 * */
nt_int_t
nt_hash_add_key(nt_hash_keys_arrays_t *ha, nt_str_t *key, void *value,
    nt_uint_t flags)
{
    size_t           len;
    u_char          *p;
    nt_str_t       *name;
    nt_uint_t       i, k, n, skip, last;
    nt_array_t     *keys, *hwc;
    nt_hash_key_t  *hk;

    last = key->len;

    //传入flags 为NT_HASH_WILDCARD_KEY ，表名 key 可能使用通配符
    if (flags & NT_HASH_WILDCARD_KEY) {

        /*
         * supported wildcards:
         *     "*.example.com", ".example.com", and "www.example.*"
         */

        n = 0;

        for (i = 0; i < key->len; i++) {

            //一个关键字 不能有两个*
            if (key->data[i] == '*') {
                if (++n > 1) {
                    return NT_DECLINED;
                }
            }

            //关键字不能有两个.. 挨着
            if (key->data[i] == '.' && key->data[i + 1] == '.') {
                return NT_DECLINED;
            }

            //关键字结尾不能为\0
            if (key->data[i] == '\0') {
                return NT_DECLINED;
            }
        }

        //.example.com skip = 1
        if (key->len > 1 && key->data[0] == '.') {
            skip = 1;
            goto wildcard;
        }

        if (key->len > 2) {

            //www.example.* skip = 2
            if (key->data[0] == '*' && key->data[1] == '.') {
                skip = 2;
                goto wildcard;
            }

            //*.example.com skip = 0
            if (key->data[i - 2] == '.' && key->data[i - 1] == '*') {
                skip = 0;
                last -= 2;  //去除*. 两位后的长度
                goto wildcard;
            }
        }

        //如果n大于0，返回错误
        if (n) {
            return NT_DECLINED;
        }
    }

    /* exact hash */
    /* 以下为精确匹配哈希表*/
    k = 0;

    //对关键字，并且对没有要求不能小写处理的 进行小写转换，并计算hash值
    for (i = 0; i < last; i++) {
        if (!(flags & NT_HASH_READONLY_KEY)) {
            key->data[i] = nt_tolower(key->data[i]);
        }
        k = nt_hash(k, key->data[i]);
    }

    //取余后的pos
    k %= ha->hsize;
    

    /* check conflicts in exact hash */
    /* 检查精确表中的冲突*/
    name = ha->keys_hash[k].elts;

    if (name) {
        for (i = 0; i < ha->keys_hash[k].nelts; i++) {

            //判断关键字长度是否相等
            if (last != name[i].len) {
                continue;
            }

            //判断关键字是否存在，存在就不插入
            if (nt_strncmp(key->data, name[i].data, last) == 0) {
                return NT_BUSY;
            }
        }

    } else {
        //桶中无存关键字，申请一个数组来存
        if (nt_array_init(&ha->keys_hash[k], ha->temp_pool, 4,
                           sizeof(nt_str_t))
            != NT_OK)
        {
            return NT_ERROR;
        }
    }


    name = nt_array_push(&ha->keys_hash[k]);
    if (name == NULL) {
        return NT_ERROR;
    }

    *name = *key;

    hk = nt_array_push(&ha->keys);
    if (hk == NULL) {
        return NT_ERROR;
    }

    hk->key = *key;
    hk->key_hash = nt_hash_key(key->data, last);
    hk->value = value;

    return NT_OK;


wildcard:

    /* wildcard hash */

    k = nt_hash_strlow(&key->data[skip], &key->data[skip], last - skip);

    k %= ha->hsize;

    if (skip == 1) {

        /* check conflicts in exact hash for ".example.com" */

        name = ha->keys_hash[k].elts;

        if (name) {
            len = last - skip;

            for (i = 0; i < ha->keys_hash[k].nelts; i++) {
                if (len != name[i].len) {
                    continue;
                }

                if (nt_strncmp(&key->data[1], name[i].data, len) == 0) {
                    return NT_BUSY;
                }
            }

        } else {
            if (nt_array_init(&ha->keys_hash[k], ha->temp_pool, 4,
                               sizeof(nt_str_t))
                != NT_OK)
            {
                return NT_ERROR;
            }
        }

        name = nt_array_push(&ha->keys_hash[k]);
        if (name == NULL) {
            return NT_ERROR;
        }

        name->len = last - 1;
        name->data = nt_pnalloc(ha->temp_pool, name->len);
        if (name->data == NULL) {
            return NT_ERROR;
        }

        nt_memcpy(name->data, &key->data[1], name->len);
    }


    if (skip) {

        /*
         * convert "*.example.com" to "com.example.\0"
         *      and ".example.com" to "com.example\0"
         */

        p = nt_pnalloc(ha->temp_pool, last);
        if (p == NULL) {
            return NT_ERROR;
        }

        len = 0;
        n = 0;

        for (i = last - 1; i; i--) {
            if (key->data[i] == '.') {
                nt_memcpy(&p[n], &key->data[i + 1], len);
                n += len;
                p[n++] = '.';
                len = 0;
                continue;
            }

            len++;
        }

        if (len) {
            nt_memcpy(&p[n], &key->data[1], len);
            n += len;
        }

        p[n] = '\0';

        hwc = &ha->dns_wc_head;
        keys = &ha->dns_wc_head_hash[k];

    } else {

        /* convert "www.example.*" to "www.example\0" */

        last++;

        p = nt_pnalloc(ha->temp_pool, last);
        if (p == NULL) {
            return NT_ERROR;
        }

        nt_cpystrn(p, key->data, last);

        hwc = &ha->dns_wc_tail;
        keys = &ha->dns_wc_tail_hash[k];
    }


    /* check conflicts in wildcard hash */

    name = keys->elts;

    if (name) {
        len = last - skip;

        for (i = 0; i < keys->nelts; i++) {
            if (len != name[i].len) {
                continue;
            }

            if (nt_strncmp(key->data + skip, name[i].data, len) == 0) {
                return NT_BUSY;
            }
        }

    } else {
        if (nt_array_init(keys, ha->temp_pool, 4, sizeof(nt_str_t)) != NT_OK)
        {
            return NT_ERROR;
        }
    }

    name = nt_array_push(keys);
    if (name == NULL) {
        return NT_ERROR;
    }

    name->len = last - skip;
    name->data = nt_pnalloc(ha->temp_pool, name->len);
    if (name->data == NULL) {
        return NT_ERROR;
    }

    nt_memcpy(name->data, key->data + skip, name->len);


    /* add to wildcard hash */

    hk = nt_array_push(hwc);
    if (hk == NULL) {
        return NT_ERROR;
    }

    hk->key.len = last - 1;
    hk->key.data = p;
    hk->key_hash = 0;
    hk->value = value;

    return NT_OK;
}


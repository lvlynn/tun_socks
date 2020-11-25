

include objs/include/nt.mk
CFLAGS =  -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -g

#do_test:
#	 $(CC)  $(CFLAGS) $(ALL_INCS)    test/test_select.c  -o bin/test_select  -g -L./bin -lnt  


SOURCE = $(wildcard test/*.c)
OBJS = $(patsubst %.c,%.o,$(SOURCE))
BIN = $(patsubst %.c,%,$(SOURCE))


#$@   --代表目标文件(target)
#$^   --代表所有的依赖文件(components)
#$<   --代表第一个依赖文件(components中最左边的那个)。
#$?   --代表当前目标所依赖的文件列表中比当前目标文件还要新的文件。
#$*   --不包括后缀名的当前依赖文件的名字

app_srcs := $(shell find test -name \*.c |  cut -d '/' -f 2)

null:$(app_srcs:%.c=%)

%:
	$(CC)  $(CFLAGS) $(ALL_INCS)    test/$@.c -o bin/$@  -g -L./bin -lnt

clean:	
		rm -f $(OBJS)
		rm -f $(TARGET)

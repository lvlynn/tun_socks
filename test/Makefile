#得到当前Makefile的路径
mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
#Makefile 绝对目录
PWD_OBJS=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs,%,$(PWD_OBJS))
PWD=$(subst /test,,$(PWD_OBJS))



include $(PWD)/objs/include/nt.mk
CFLAGS =  -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -g

#do_test:
#	 $(CC)  $(CFLAGS) $(ALL_INCS)    test/test_select.c  -o bin/test_select  -g -L./bin -lnt  



SOURCE = $(wildcard $(PWD)/test/*.c)
OBJS = $(patsubst %.c,%.o,$(SOURCE))
BIN = $(patsubst $(PWD)/test/%.c,%,$(SOURCE))


#$@   --代表目标文件(target)
#$^   --代表所有的依赖文件(components)
#$<   --代表第一个依赖文件(components中最左边的那个)。
#$?   --代表当前目标所依赖的文件列表中比当前目标文件还要新的文件。
#$*   --不包括后缀名的当前依赖文件的名字

app_srcs := $(shell find $(PWD)/test/ -name \*.c |  cut -d '/' -f 2)

#all:$(app_srcs:%.c=%)
all:$(BIN)
	@echo $(BIN)	
%:
	@echo $@
#	@echo $(OBJS)
#	@echo $(PWD)
#	@echo $(app_srcs)
	$(CC)  $(CFLAGS) $(ALL_INCS)   $(PWD)/test/$@.c -o $(PWD)/bin/$@  -g -L$(PWD)/bin -lnt

clean:	
		rm -f $(OBJS)
		rm -f $(TARGET)

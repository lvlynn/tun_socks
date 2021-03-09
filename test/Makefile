mkfile_path=$(abspath $(firstword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(dir $(mkfile_path))
#获得Makefile当前目录
MAKEFILE_DIR_PATSUBST := $(patsubst %/,%,$(MAKEFILE_DIR))
LAST_MENU=$(notdir $(MAKEFILE_DIR_PATSUBST))
PWD=$(subst /$(LAST_MENU),,$(MAKEFILE_DIR_PATSUBST))


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
	$(CC)  $(CFLAGS) $(NT_INCLUDE)   $(PWD)/test/$@.c -o $(PWD)/bin/$@  -g -L$(PWD)/bin -lnt

clean:	
		rm -f $(OBJS)
		rm -f $(TARGET)

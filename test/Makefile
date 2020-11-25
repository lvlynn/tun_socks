

include objs/include/nt.mk
CFLAGS =  -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -g

#do_test:
#	 $(CC)  $(CFLAGS) $(ALL_INCS)    test/test_select.c  -o bin/test_select  -g -L./bin -lnt  


SOURCE = $(wildcard test/*.c)
OBJS = $(patsubst %.c,%.o,$(SOURCE))
BIN = $(patsubst %.c,%,$(SOURCE))

do_test:$(BIN)
	echo "ttttttttttttttttttttt"
	echo $(BIN)

%:$(SOURCE)
	$(CC)  $(CFLAGS) $(ALL_INCS)   $< -o $@ 

#$@--目标文件，$^--所有的依赖文件，$<--第一个依赖文件
#test/%.o:test/%.c
#	echo $^
#		$(CC) -c $(CFLAGS_WARN) $(DEFINE) $(INCLUDE) $< -o $@	
clean:	
		rm -f $(OBJS)
		rm -f $(TARGET)

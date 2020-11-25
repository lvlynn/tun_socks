

include objs/include/nt.mk
CFLAGS =  -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -g

do_test:
	 $(CC)  $(CFLAGS) $(ALL_INCS)    test/test_select.c  -o bin/test_select  -g -L./bin -lnt  

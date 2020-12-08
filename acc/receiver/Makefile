

include objs/include/nt.mk
CFLAGS =  -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -g

PWD=$(shell pwd)


menu=$(PWD)/acc/receiver

obj=$(menu)/tun.o \
	$(menu)/rbtree.o \
	$(menu)/receiver.o \

ALL_INCS +=-I$(menu)

do_test: $(obj)


#	 $(CC)  $(CFLAGS) $(ALL_INCS)    $(PWD)/receiver.c  -o bin/rcv  -g -L./bin -lnt  

$(menu)/tun.o:
	$(CC)  $(CFLAGS) $(ALL_INCS)  -c  $(menu)/tun.c  -o $(menu)/build/tun.o  -g -L./bin -lnt
$(menu)/rbtree.o:
	$(CC)  $(CFLAGS) $(ALL_INCS)  -c  $(menu)/rbtree.c  -o $(menu)/build/rbtree.o  -g -L./bin -lnt

$(menu)/receiver.o:
	$(CC)  $(CFLAGS) $(ALL_INCS)  -c    $(menu)/receiver.c  -o $(menu)/build/receiver.o  -g -L./bin -lnt



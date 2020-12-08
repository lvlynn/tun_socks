

include objs/include/nt.mk
CFLAGS =  -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -g

PWD=$(shell pwd)


menu=$(PWD)/acc/receiver
out=$(PWD)/acc/receiver/build

obj=$(out)/tun.o \
	$(out)/rbtree.o \
	$(out)/receiver.o \

ALL_INCS +=-I$(menu)

do_test: clean $(obj) 
	$(CC)  $(CFLAGS) $(ALL_INCS) $(obj) -o  $(out)/receiver -g -L./bin  -lnt


#	 $(CC)  $(CFLAGS) $(ALL_INCS)    $(PWD)/receiver.c  -o bin/rcv  -g -L./bin -lnt  

clean:
	rm -rf $(out)/*.o $(out)/receiver

$(out)/tun.o:
	$(CC)  $(CFLAGS) $(ALL_INCS)  -c  $(menu)/tun.c  -o $(out)/tun.o  -g -L./bin -lnt
$(out)/rbtree.o:
	$(CC)  $(CFLAGS) $(ALL_INCS)  -c  $(menu)/rbtree.c  -o $(out)/rbtree.o  -g -L./bin -lnt

$(out)/receiver.o:
	$(CC)  $(CFLAGS) $(ALL_INCS)  -c    $(menu)/receiver.c  -o $(out)/receiver.o  -g -L./bin -lnt



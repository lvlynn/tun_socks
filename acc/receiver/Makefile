

CFLAGS =  -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -g

#得到当前Makefile的路径
mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
#Makefile 绝对目录
PWD_OBJS=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs,%,$(PWD_OBJS))
PWD=$(subst /acc/receiver,,$(PWD_OBJS))

#PWD=$(shell pwd)

include $(PWD)/objs/include/nt.mk

menu=$(PWD)/acc/receiver
out=$(PWD)/acc/receiver/build

obj=$(out)/tun.o \
	$(out)/rbtree.o \
	$(out)/receiver.o \

ALL_INCS +=-I$(menu)

do_test: clean $(obj) 
	$(CC)  $(CFLAGS) $(ALL_INCS) $(obj) -o  $(out)/receiver -g -L$(PWD)/bin  -lnt


#	 $(CC)  $(CFLAGS) $(ALL_INCS)    $(PWD)/receiver.c  -o bin/rcv  -g -L./bin -lnt  

clean:
	rm -rf $(out)/*.o $(out)/receiver

$(out)/tun.o:
	$(CC)  $(CFLAGS) $(ALL_INCS)  -c  $(menu)/tun.c  -o $(out)/tun.o  -g -L$(PWD)/bin -lnt
$(out)/rbtree.o:
	$(CC)  $(CFLAGS) $(ALL_INCS)  -c  $(menu)/rbtree.c  -o $(out)/rbtree.o  -g -L$(PWD)/bin -lnt

$(out)/receiver.o:
	$(CC)  $(CFLAGS) $(ALL_INCS)  -c    $(menu)/receiver.c  -o $(out)/receiver.o  -g -L$(PWD)/bin -lnt







default:    build do_test 

#all:
#	$(CC) libnet/ip.c libnet/tcp.c tun.c -o bin/tun -g -I./libnet

clean:
	rm -rf bin/nt 
	rm -rf objs/config/*.o
	rm -rf objs/src/core/*.o
	rm -rf objs/src/event/*.o
	rm -rf objs/src/event/modules/*.o
	rm -rf objs/src/os/unix/*.o
	rm -rf objs/src/stream/*.o


build:
	$(MAKE) -f  objs/Makefile

do_test:
	$(MAKE) -f test/Makefile

do_receive:
	$(MAKE) -f acc/receiver/Makefile

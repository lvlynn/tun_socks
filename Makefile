

#CC=arm-openwrt-linux-gcc
#AR=arm-openwrt-linux-ar
#STRIP=arm-openwrt-linux-strip
#OPENVPN_SSL_INC=/data/project/funjsq/openssl_project/netgear/r7800/openssl-1.0.2h/include
#OPENVPN_SSL_LIB=/data/project/funjsq/openssl_project/netgear/r7800/openssl-1.0.2h


ifeq ($(Host),gcc)
	echo "test"
	echo $(CC)
	Host=linux
endif
ifeq ($(CC),gcc)
	Host=
else
	Host1=$(subst -gcc,, $(CC))
	Host=$(strip $(Host1))
endif

PWD=$(shell pwd)



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

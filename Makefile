


all:
	$(CC) libnet/ip.c libnet/tcp.c tun.c -o bin/tun -g -I./libnet

clean:
	rm -rf bin/tun *.o

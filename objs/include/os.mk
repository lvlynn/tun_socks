

objs/src/os/unix/nt_posix_init.o: $(CORE_DEPS) \
	src/os/unix/nt_posix_init.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/nt_posix_init.o \
		src/os/unix/nt_posix_init.c

objs/src/os/unix/nt_socket.o: $(CORE_DEPS) \
	src/os/unix/nt_socket.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/nt_socket.o \
		src/os/unix/nt_socket.c



objs/src/os/unix/nt_recv.o: $(CORE_DEPS) \
	src/os/unix/nt_recv.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/nt_recv.o \
		src/os/unix/nt_recv.c


objs/src/os/unix/nt_send.o: $(CORE_DEPS) \
	src/os/unix/nt_send.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/nt_send.o \
		src/os/unix/nt_send.c

objs/src/os/unix/nt_udp_recv.o: $(CORE_DEPS) \
	src/os/unix/nt_udp_recv.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/nt_udp_recv.o \
		src/os/unix/nt_udp_recv.c


objs/src/os/unix/nt_udp_send.o: $(CORE_DEPS) \
	src/os/unix/nt_udp_send.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/os/unix/nt_udp_send.o \
		src/os/unix/nt_udp_send.c



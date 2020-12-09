mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
PWD_INCLUDE=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs/include,%,$(PWD_INCLUDE))
PWD=$(subst /objs/include,,$(PWD_INCLUDE))


$(PWD)/objs/src/os/unix/nt_posix_init.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_posix_init.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_posix_init.o \
		$(PWD)/src/os/unix/nt_posix_init.c

$(PWD)/objs/src/os/unix/nt_errno.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_errno.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_errno.o \
		$(PWD)/src/os/unix/nt_errno.c

$(PWD)/objs/src/os/unix/nt_times.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_times.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_times.o \
		$(PWD)/src/os/unix/nt_times.c


$(PWD)/objs/src/os/unix/nt_files.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_files.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_files.o \
		$(PWD)/src/os/unix/nt_files.c


$(PWD)/objs/src/os/unix/nt_alloc.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_alloc.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_alloc.o \
		$(PWD)/src/os/unix/nt_alloc.c


$(PWD)/objs/src/os/unix/nt_socket.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_socket.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_socket.o \
		$(PWD)/src/os/unix/nt_socket.c

$(PWD)/objs/src/os/unix/nt_read.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_read.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_read.o \
		$(PWD)/src/os/unix/nt_read.c

$(PWD)/objs/src/os/unix/nt_write.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_write.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_write.o \
		$(PWD)/src/os/unix/nt_write.c

$(PWD)/objs/src/os/unix/nt_recv.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_recv.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_recv.o \
		$(PWD)/src/os/unix/nt_recv.c


$(PWD)/objs/src/os/unix/nt_send.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_send.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_send.o \
		$(PWD)/src/os/unix/nt_send.c

$(PWD)/objs/src/os/unix/nt_udp_recv.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_udp_recv.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_udp_recv.o \
		$(PWD)/src/os/unix/nt_udp_recv.c


$(PWD)/objs/src/os/unix/nt_udp_send.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_udp_send.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/os/unix/nt_udp_send.o \
		$(PWD)/src/os/unix/nt_udp_send.c



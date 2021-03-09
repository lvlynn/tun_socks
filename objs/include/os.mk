mkfile_path=$(abspath $(firstword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(dir $(mkfile_path))
#获得Makefile当前目录
MAKEFILE_DIR_PATSUBST := $(patsubst %/,%,$(MAKEFILE_DIR))
LAST_MENU=$(notdir $(MAKEFILE_DIR_PATSUBST))
PWD=$(subst /$(LAST_MENU),,$(MAKEFILE_DIR_PATSUBST))


$(PWD)/objs/src/os/unix/nt_posix_init.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_posix_init.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_posix_init.o \
		$(PWD)/src/os/unix/nt_posix_init.c

$(PWD)/objs/src/os/unix/nt_linux_init.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_linux_init.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_linux_init.o \
		$(PWD)/src/os/unix/nt_linux_init.c

$(PWD)/objs/src/os/unix/nt_errno.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_errno.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_errno.o \
		$(PWD)/src/os/unix/nt_errno.c

$(PWD)/objs/src/os/unix/nt_times.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_times.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_times.o \
		$(PWD)/src/os/unix/nt_times.c


$(PWD)/objs/src/os/unix/nt_files.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_files.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_files.o \
		$(PWD)/src/os/unix/nt_files.c


$(PWD)/objs/src/os/unix/nt_alloc.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_alloc.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_alloc.o \
		$(PWD)/src/os/unix/nt_alloc.c

######################## [socket] ###############################
$(PWD)/objs/src/os/unix/nt_socket.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_socket.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_socket.o \
		$(PWD)/src/os/unix/nt_socket.c

$(PWD)/objs/src/os/unix/nt_read.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_read.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_read.o \
		$(PWD)/src/os/unix/nt_read.c

$(PWD)/objs/src/os/unix/nt_write.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_write.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_write.o \
		$(PWD)/src/os/unix/nt_write.c

$(PWD)/objs/src/os/unix/nt_recv.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_recv.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_recv.o \
		$(PWD)/src/os/unix/nt_recv.c


$(PWD)/objs/src/os/unix/nt_send.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_send.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_send.o \
		$(PWD)/src/os/unix/nt_send.c

$(PWD)/objs/src/os/unix/nt_udp_recv.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_udp_recv.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_udp_recv.o \
		$(PWD)/src/os/unix/nt_udp_recv.c


$(PWD)/objs/src/os/unix/nt_udp_send.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_udp_send.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_udp_send.o \
		$(PWD)/src/os/unix/nt_udp_send.c

######################## [end] ###############################
######################## [process] ###########################
######################## 进程间通信 ##########################
$(PWD)/objs/src/os/unix/nt_channel.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_channel.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_channel.o \
		$(PWD)/src/os/unix/nt_channel.c

$(PWD)/objs/src/os/unix/nt_pipe.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_pipe.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_pipe.o \
		$(PWD)/src/os/unix/nt_pipe.c

$(PWD)/objs/src/os/unix/nt_shmem.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_shmem.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_shmem.o \
		$(PWD)/src/os/unix/nt_shmem.c

$(PWD)/objs/src/os/unix/nt_process.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_process.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_process.o \
		$(PWD)/src/os/unix/nt_process.c

$(PWD)/objs/src/os/unix/nt_process_cycle.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_process_cycle.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_process_cycle.o \
		$(PWD)/src/os/unix/nt_process_cycle.c

$(PWD)/objs/src/os/unix/nt_daemon.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_daemon.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_daemon.o \
		$(PWD)/src/os/unix/nt_daemon.c

$(PWD)/objs/src/os/unix/nt_setproctitle.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_setproctitle.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_setproctitle.o \
		$(PWD)/src/os/unix/nt_setproctitle.c

$(PWD)/objs/src/os/unix/nt_setaffinity.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_setaffinity.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_setaffinity.o \
		$(PWD)/src/os/unix/nt_setaffinity.c

$(PWD)/objs/src/os/unix/nt_sysinfo.o: $(CORE_DEPS) \
	$(PWD)/src/os/unix/nt_sysinfo.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/os/unix/nt_sysinfo.o \
		$(PWD)/src/os/unix/nt_sysinfo.c



######################## [end] ###############################


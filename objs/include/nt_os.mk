mkfile_path=$(abspath $(firstword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(dir $(mkfile_path))
#获得Makefile当前目录
MAKEFILE_DIR_PATSUBST := $(patsubst %/,%,$(MAKEFILE_DIR))
LAST_MENU=$(notdir $(MAKEFILE_DIR_PATSUBST))
PWD=$(subst /$(LAST_MENU),,$(MAKEFILE_DIR_PATSUBST))


NT_OS_INC= -I $(PWD)/src/os/unix

NT_OS_OBJ=$(PWD)/objs/src/os/unix/nt_posix_init.o \
		 $(PWD)/objs/src/os/unix/nt_linux_init.o \
		  $(PWD)/objs/src/os/unix/nt_errno.o \
		  $(PWD)/objs/src/os/unix/nt_alloc.o \
		  $(PWD)/objs/src/os/unix/nt_times.o \
		  $(PWD)/objs/src/os/unix/nt_files.o \
		  $(PWD)/objs/src/os/unix/nt_read.o \
		  $(PWD)/objs/src/os/unix/nt_write.o \
		  $(PWD)/objs/src/os/unix/nt_recv.o \
		  $(PWD)/objs/src/os/unix/nt_send.o \
		  $(PWD)/objs/src/os/unix/nt_socket.o \
		  $(PWD)/objs/src/os/unix/nt_udp_recv.o \
		  $(PWD)/objs/src/os/unix/nt_udp_send.o \
		  $(PWD)/objs/src/os/unix/nt_channel.o \
		  $(PWD)/objs/src/os/unix/nt_pipe.o \
		  $(PWD)/objs/src/os/unix/nt_shmem.o \
		  $(PWD)/objs/src/os/unix/nt_process.o \
		  $(PWD)/objs/src/os/unix/nt_process_cycle.o \
		  $(PWD)/objs/src/os/unix/nt_daemon.o \
		  $(PWD)/objs/src/os/unix/nt_setproctitle.o \
		  $(PWD)/objs/src/os/unix/nt_setaffinity.o \
		  $(PWD)/objs/src/os/unix/nt_sysinfo.o 

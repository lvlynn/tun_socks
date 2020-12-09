mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
PWD_INCLUDE=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs/include,%,$(PWD_INCLUDE))
PWD=$(subst /objs/include,,$(PWD_INCLUDE))


NT_OS_INC= $(PWD)/src/os/unix/nt_os.h \
		   $(PWD)/src/os/unix/nt_files.h \
		   $(PWD)/src/os/unix/nt_times.h \
		   $(PWD)/src/os/unix/nt_socket.h \
		   $(PWD)/src/os/unix/nt_process.h \
		   $(PWD)/src/os/unix/nt_linux_config.h 

NT_OS_OBJ=$(PWD)/objs/src/os/unix/nt_posix_init.o \
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
		  $(PWD)/objs/src/os/unix/nt_udp_send.o 

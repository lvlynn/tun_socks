

NT_OS_INC= src/os/unix/nt_os.h \
		   src/os/unix/nt_files.h \
		   src/os/unix/nt_times.h \
		   src/os/unix/nt_socket.h \
		   src/os/unix/nt_process.h \
		   src/os/unix/nt_linux_config.h 

NT_OS_OBJ=objs/src/os/unix/nt_posix_init.o \
		  objs/src/os/unix/nt_errno.o \
		  objs/src/os/unix/nt_alloc.o \
		  objs/src/os/unix/nt_times.o \
		  objs/src/os/unix/nt_files.o \
		  objs/src/os/unix/nt_recv.o \
		  objs/src/os/unix/nt_send.o \
		  objs/src/os/unix/nt_socket.o \
		  objs/src/os/unix/nt_udp_recv.o \
		  objs/src/os/unix/nt_udp_send.o 

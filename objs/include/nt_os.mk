

NT_OS_INC= src/os/unix/nt_os.h \
		   src/os/unix/nt_socket.h \
		   src/os/unix/nt_linux_config.h 

NT_OS_OBJ=objs/src/os/unix/nt_posix_init.o \
		  objs/src/os/unix/nt_recv.o \
		  objs/src/os/unix/nt_send.o \
		  objs/src/os/unix/nt_socket.o \
		  objs/src/os/unix/nt_udp_recv.o \
		  objs/src/os/unix/nt_udp_send.o 

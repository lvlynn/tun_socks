
CFLAGS =  -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -Wno-int-conversion -g
LINK =  $(CC)

PWD=$(shell pwd)

ALL_INCS = -I src/core \
		   -I src/event \
		   -I src/event/modules \
		   -I src/os/unix \
		   -I config 

CORE_DEPS = src/core/nt.h \
			src/core/nt_def.h \
			src/core/nt_core.h \
			src/core/nt_connection.h \
			src/core/nt_log.h \
			src/os/unix/nt_os.h \
			src/os/unix/nt_socket.h \
			src/os/unix/nt_linux_config.h \
			config/nt_auto_config.h

CORE_INCS = -I src/core \
			-I src/event \
			-I src/event/modules \
			-I src/os/unix \
			-I config 

ADDON_DEPS = $(CORE_DEPS) 

BUILD_BIN = nt

CORE_OBJ = 	objs/src/core/nt.o \
	objs/src/core/nt_log.o \
	objs/src/core/nt_time.o \
	objs/src/core/nt_connection.o \
	objs/src/event/nt_event.o \
	objs/src/event/modules/nt_select.o \
	objs/src/os/unix/nt_posix_init.o \
	objs/src/os/unix/nt_recv.o \
	objs/src/os/unix/nt_send.o \
	objs/src/os/unix/nt_socket.o \
	objs/src/os/unix/nt_udp_recv.o \
	objs/src/os/unix/nt_udp_send.o \
	objs/config/nt_modules.o



build:  binary modules manpage

binary: objs/nt

objs/nt: $(CORE_OBJ)

	$(LINK) -ldl \
	$(CORE_OBJ) \
	-o bin/$(BUILD_BIN) \
	-Wl,-E

modules: 

objs/config/nt_modules.o: $(CORE_DEPS) \
	config/nt_modules.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/config/nt_modules.o \
		config/nt_modules.c

include objs/core.mk

include objs/event.mk

include objs/os.mk

manpage:
	echo "end"

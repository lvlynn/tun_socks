
CFLAGS =  -g -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -Wno-int-conversion -g

LINK =  $(CC)

PWD=$(shell pwd)


include $(PWD)/objs/include/nt.mk

CORE_INCS = -I $(PWD)/src/core \
			-I $(PWD)/src/event \
			-I $(PWD)/src/event/modules \
			-I $(PWD)/src/os/unix \
			-I $(PWD)/config 

ADDON_DEPS = $(CORE_DEPS) 

BUILD_BIN = nt


CORE_OBJ = 	objs/src/core/nt.o \
		$(LIB_CORE_OBJ)

build:  binary modules manpage

binary: objs/nt  libs

objs/nt: $(CORE_OBJ)
	$(LINK)  \
	$(CORE_OBJ) \
	-o bin/$(BUILD_BIN) \
	-Wl,-E

libs:
	$(AR) cr bin/libnt.a $(LIB_CORE_OBJ)

modules: 

objs/config/nt_modules.o: $(CORE_DEPS) \
	config/nt_modules.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/config/nt_modules.o \
		config/nt_modules.c

include objs/include/core.mk

include objs/include/event.mk

include objs/include/os.mk

manpage:
	echo "end"

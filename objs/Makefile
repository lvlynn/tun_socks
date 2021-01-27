
CFLAGS =  -g -pipe  -O -W -Wall -Wpointer-arith -Wno-unused-parameter -Wno-int-conversion -g

LINK =  $(CC)

#PWD=$(shell pwd)
#$(abspath $(lastword $(MAKEFILE_LIST)))返回当前正在被执行的Makefile的绝对路径。
#mkfile_path := $(abspath $(lastword $(MAKEFILE_LIST)))  #获取当前正在执行的makefile的绝对路径
#cur_makefile_path := patsubst$(%/, %, dir $(mkfile_patch))  #获取当前正在执行的makefile的绝对目录

#得到当前Makefile的路径
mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
#Makefile 绝对目录
PWD_OBJS=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs,%,$(PWD_OBJS))
PWD=$(subst /objs,,$(PWD_OBJS))


include $(PWD)/objs/include/nt.mk

CORE_INCS = -I $(PWD)/src/core \
			-I $(PWD)/src/event \
			-I $(PWD)/src/event/modules \
			-I $(PWD)/src/os/unix \
			-I $(PWD)/src/stream \
			-I $(PWD)/config 

ADDON_DEPS = $(CORE_DEPS) 

BUILD_BIN = nt


CORE_OBJ = 	$(PWD)/objs/src/core/nt.o \
		$(LIB_CORE_OBJ)

build:  binary modules manpage

binary: objs/nt  libs

objs/nt: $(CORE_OBJ)
	$(LINK)  \
	$(CORE_OBJ) \
	-o $(PWD)/bin/$(BUILD_BIN) \
	-Wl,-E

libs:
	$(AR) cr $(PWD)/bin/libnt.a $(LIB_CORE_OBJ)

modules: 

$(PWD)/objs/config/nt_modules.o: $(CORE_DEPS) \
	$(PWD)/config/nt_modules.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/config/nt_modules.o \
		$(PWD)/config/nt_modules.c

include $(PWD)/objs/include/core.mk

include $(PWD)/objs/include/event.mk

include $(PWD)/objs/include/os.mk

include $(PWD)/objs/include/stream.mk

manpage:
	echo "end"

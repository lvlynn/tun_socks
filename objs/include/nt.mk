
mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
PWD_INCLUDE=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs/include,%,$(PWD_INCLUDE))
PWD=$(subst /objs/include,,$(PWD_INCLUDE))



test1=$(test)/$(file_name)
ALL_INCS = -I $(PWD)/src/core \
    -I $(PWD)/src/event \
    -I $(PWD)/src/event/modules \
    -I $(PWD)/src/os/unix \
	-I $(PWD)/src/stream \
    -I $(PWD)/config

#include $(test1)/objs/include/nt_core.mk
include $(PWD)/objs/include/nt_core.mk
include $(PWD)/objs/include/nt_event.mk
include $(PWD)/objs/include/nt_os.mk
include $(PWD)/objs/include/nt_stream.mk

CORE_DEPS = $(NT_CORE_INC) \
	$(NT_EVENT_INC) \
	$(NT_OS_INC) \
	$(NT_STREAM_INC) \
	$(PWD)/config/nt_auto_config.h


LIB_CORE_OBJ = 	$(NT_CORE_OBJ) \
	$(NT_EVENT_OBJ) \
	$(NT_OS_OBJ) \
	$(NT_STREAM_OBJ) \
    $(PWD)/objs/config/nt_modules.o

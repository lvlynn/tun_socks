
mkfile_path=$(abspath $(firstword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(dir $(mkfile_path))
#获得Makefile当前目录
MAKEFILE_DIR_PATSUBST := $(patsubst %/,%,$(MAKEFILE_DIR))
LAST_MENU=$(notdir $(MAKEFILE_DIR_PATSUBST))
PWD=$(subst /$(LAST_MENU),,$(MAKEFILE_DIR_PATSUBST))



 # include $(PWD)/test.mk
# ALL_INCS = -I $(PWD)/src/core \
#     -I $(PWD)/src/event \
#     -I $(PWD)/src/event/modules \
#     -I $(PWD)/src/os/unix \
#     -I $(PWD)/src/stream \
#     -I $(PWD)/config

include $(PWD)/objs/include/nt_core.mk
include $(PWD)/objs/include/nt_event.mk
include $(PWD)/objs/include/nt_os.mk
include $(PWD)/objs/include/nt_stream.mk

#引入编译模块的 o列表
include $(PWD)/objs/include/nt_module_o.mk

NT_INCLUDE = $(NT_CORE_INC) \
	$(NT_EVENT_INC) \
	$(NT_OS_INC) \
	$(NT_STREAM_INC) \
	$(NT_MODULES_INC) \
	-I $(PWD)/config


LIB_CORE_OBJ = 	$(NT_CORE_OBJ) \
	$(NT_EVENT_OBJ) \
	$(NT_OS_OBJ) \
	$(NT_STREAM_OBJ) \
	$(NT_MODULES_OBJ) \
    $(PWD)/objs/config/nt_modules.o

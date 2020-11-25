
ALL_INCS = -I src/core \
    -I src/event \
    -I src/event/modules \
    -I src/os/unix \
    -I config


include objs/include/nt_core.mk
include objs/include/nt_event.mk
include objs/include/nt_os.mk

CORE_DEPS = $(NT_CORE_INC) \
	$(NT_EVENT_INC) \
	$(NT_OS_INC) \
	config/nt_auto_config.h


LIB_CORE_OBJ = 	$(NT_CORE_OBJ) \
	$(NT_EVENT_OBJ) \
	$(NT_OS_OBJ) \
    objs/config/nt_modules.o

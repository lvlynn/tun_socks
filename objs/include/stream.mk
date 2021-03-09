mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
PWD_INCLUDE=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs/include,%,$(PWD_INCLUDE))
PWD=$(subst /objs/include,,$(PWD_INCLUDE))
mkfile_path=$(abspath $(firstword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(dir $(mkfile_path))
#获得Makefile当前目录
MAKEFILE_DIR_PATSUBST := $(patsubst %/,%,$(MAKEFILE_DIR))
# CUR_MENU=$(notdir $(MAKEFILE_DIR_PATSUBST))
PWD=$(subst /objs,,$(MAKEFILE_DIR_PATSUBST))


$(PWD)/objs/src/stream/nt_stream.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/stream/nt_stream.o \
		$(PWD)/src/stream/nt_stream.c

$(PWD)/objs/src/stream/nt_stream_core_module.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_core_module.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/stream/nt_stream_core_module.o \
		$(PWD)/src/stream/nt_stream_core_module.c


$(PWD)/objs/src/stream/nt_stream_handler.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_handler.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/stream/nt_stream_handler.o \
		$(PWD)/src/stream/nt_stream_handler.c


$(PWD)/objs/src/stream/nt_stream_log_module.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_log_module.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/stream/nt_stream_log_module.o \
		$(PWD)/src/stream/nt_stream_log_module.c


$(PWD)/objs/src/stream/nt_stream_upstream.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_upstream.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/stream/nt_stream_upstream.o \
		$(PWD)/src/stream/nt_stream_upstream.c

$(PWD)/objs/src/stream/nt_stream_proxy_module.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_proxy_module.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/stream/nt_stream_proxy_module.o \
		$(PWD)/src/stream/nt_stream_proxy_module.c

$(PWD)/objs/src/stream/nt_stream_upstream_round_robin.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_upstream_round_robin.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/stream/nt_stream_upstream_round_robin.o \
		$(PWD)/src/stream/nt_stream_upstream_round_robin.c


$(PWD)/objs/src/stream/nt_stream_script.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_script.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/stream/nt_stream_script.o \
		$(PWD)/src/stream/nt_stream_script.c

$(PWD)/objs/src/stream/nt_stream_variables.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_variables.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/src/stream/nt_stream_variables.o \
		$(PWD)/src/stream/nt_stream_variables.c


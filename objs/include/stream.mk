mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
PWD_INCLUDE=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs/include,%,$(PWD_INCLUDE))
PWD=$(subst /objs/include,,$(PWD_INCLUDE))


$(PWD)/objs/src/stream/nt_stream.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/stream/nt_stream.o \
		$(PWD)/src/stream/nt_stream.c

$(PWD)/objs/src/stream/nt_stream_core_module.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_core_module.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/stream/nt_stream_core_module.o \
		$(PWD)/src/stream/nt_stream_core_module.c


$(PWD)/objs/src/stream/nt_stream_handler.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_handler.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/stream/nt_stream_handler.o \
		$(PWD)/src/stream/nt_stream_handler.c


$(PWD)/objs/src/stream/nt_stream_upstream.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_upstream.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/stream/nt_stream_upstream.o \
		$(PWD)/src/stream/nt_stream_upstream.c

$(PWD)/objs/src/stream/nt_stream_upstream_round_robin.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_upstream_round_robin.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/stream/nt_stream_upstream_round_robin.o \
		$(PWD)/src/stream/nt_stream_upstream_round_robin.c


$(PWD)/objs/src/stream/nt_stream_script.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_script.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/stream/nt_stream_script.o \
		$(PWD)/src/stream/nt_stream_script.c

$(PWD)/objs/src/stream/nt_stream_variables.o: $(CORE_DEPS) \
	$(PWD)/src/stream/nt_stream_variables.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/stream/nt_stream_variables.o \
		$(PWD)/src/stream/nt_stream_variables.c


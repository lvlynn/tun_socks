mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
PWD_INCLUDE=$(shell dirname $(mkfile_path))
PWD=$(subst /objs/include,,$(PWD_INCLUDE))
#PWD=$(patsubst %/objs/include,%,$(PWD_INCLUDE))


$(PWD)/objs/src/core/nt.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt.c 
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt.o \
		$(PWD)/src/core/nt.c

$(PWD)/objs/src/core/nt_module.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_module.c 
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_module.o \
		$(PWD)/src/core/nt_module.c

$(PWD)/objs/src/core/nt_time.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_time.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_time.o \
		$(PWD)/src/core/nt_time.c

$(PWD)/objs/src/core/nt_log.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_log.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_log.o \
		$(PWD)/src/core/nt_log.c

$(PWD)/objs/src/core/nt_syslog.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_syslog.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_syslog.o \
		$(PWD)/src/core/nt_syslog.c

$(PWD)/objs/src/core/nt_crc32.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_crc32.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_crc32.o \
		$(PWD)/src/core/nt_crc32.c

$(PWD)/objs/src/core/nt_inet.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_inet.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_inet.o \
		$(PWD)/src/core/nt_inet.c

$(PWD)/objs/src/core/nt_parse.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_parse.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_parse.o \
		$(PWD)/src/core/nt_parse.c

$(PWD)/objs/src/core/nt_parse_time.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_parse_time.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_parse_time.o \
		$(PWD)/src/core/nt_parse_time.c

$(PWD)/objs/src/core/nt_connection.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_connection.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_connection.o \
		$(PWD)/src/core/nt_connection.c

$(PWD)/objs/src/core/nt_cycle.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_cycle.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_cycle.o \
		$(PWD)/src/core/nt_cycle.c

$(PWD)/objs/src/core/nt_queue.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_queue.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_queue.o \
		$(PWD)/src/core/nt_queue.c

$(PWD)/objs/src/core/nt_palloc.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_palloc.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_palloc.o \
		$(PWD)/src/core/nt_palloc.c

$(PWD)/objs/src/core/nt_file.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_file.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_file.o \
		$(PWD)/src/core/nt_file.c


$(PWD)/objs/src/core/nt_array.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_array.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_array.o \
		$(PWD)/src/core/nt_array.c

$(PWD)/objs/src/core/nt_list.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_list.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_list.o \
		$(PWD)/src/core/nt_list.c


$(PWD)/objs/src/core/nt_buf.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_buf.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_buf.o \
		$(PWD)/src/core/nt_buf.c

$(PWD)/objs/src/core/nt_conf.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_conf.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_conf.o \
		$(PWD)/src/core/nt_conf.c


$(PWD)/objs/src/core/nt_string.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_string.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_string.o \
		$(PWD)/src/core/nt_string.c

$(PWD)/objs/src/core/nt_rbtree.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_rbtree.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_rbtree.o \
		$(PWD)/src/core/nt_rbtree.c

$(PWD)/objs/src/core/nt_static_hash.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_static_hash.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_static_hash.o \
		$(PWD)/src/core/nt_static_hash.c

$(PWD)/objs/src/core/nt_resolver.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_resolver.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_resolver.o \
		$(PWD)/src/core/nt_resolver.c

$(PWD)/objs/src/core/nt_proxy_protocol.o:  $(CORE_DEPS) \
	$(PWD)/src/core/nt_proxy_protocol.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/core/nt_proxy_protocol.o \
		$(PWD)/src/core/nt_proxy_protocol.c


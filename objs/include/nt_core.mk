
mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
PWD_INCLUDE=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs/include,%,$(PWD_INCLUDE))
PWD=$(subst /objs/include,,$(PWD_INCLUDE))


NT_CORE_INC= $(PWD)/src/core/nt.h \
		  $(PWD)/src/core/nt_def.h \
		  $(PWD)/src/core/nt_core.h \
		  $(PWD)/src/core/nt_connection.h \
		  $(PWD)/src/core/nt_cycle.h \
		  $(PWD)/src/core/nt_file.h \
		  $(PWD)/src/core/nt_time.h \
		  $(PWD)/src/core/nt_string.h \
		  $(PWD)/src/core/nt_rbtree.h \
		  $(PWD)/src/core/nt_log.h 


NT_CORE_OBJ=$(PWD)/objs/src/core/nt_log.o \
			$(PWD)/objs/src/core/nt_syslog.o \
			$(PWD)/objs/src/core/nt_time.o \
			$(PWD)/objs/src/core/nt_inet.o \
			$(PWD)/objs/src/core/nt_crc32.o \
			$(PWD)/objs/src/core/nt_parse.o \
			$(PWD)/objs/src/core/nt_parse_time.o \
			$(PWD)/objs/src/core/nt_connection.o \
			$(PWD)/objs/src/core/nt_cycle.o \
			$(PWD)/objs/src/core/nt_queue.o \
			$(PWD)/objs/src/core/nt_palloc.o \
			$(PWD)/objs/src/core/nt_file.o \
			$(PWD)/objs/src/core/nt_array.o \
			$(PWD)/objs/src/core/nt_buf.o \
			$(PWD)/objs/src/core/nt_list.o \
			$(PWD)/objs/src/core/nt_conf.o \
			$(PWD)/objs/src/core/nt_rbtree.o \
			$(PWD)/objs/src/core/nt_string.o 



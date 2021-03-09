mkfile_path=$(abspath $(firstword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(dir $(mkfile_path))
#获得Makefile当前目录
MAKEFILE_DIR_PATSUBST := $(patsubst %/,%,$(MAKEFILE_DIR))
LAST_MENU=$(notdir $(MAKEFILE_DIR_PATSUBST))
TEST_PWD=$(subst /$(LAST_MENU),,$(MAKEFILE_DIR_PATSUBST))

NT_STREAM_INC=-I $(PWD)/src/stream \
			  -I $(PWD)/src/stream/modules

NT_STREAM_OBJ=$(PWD)/objs/src/stream/nt_stream.o \
			 $(PWD)/objs/src/stream/nt_stream_core_module.o \
			 $(PWD)/objs/src/stream/nt_stream_handler.o \
			 $(PWD)/objs/src/stream/nt_stream_upstream.o \
			 $(PWD)/objs/src/stream/nt_stream_upstream_round_robin.o \
			 $(PWD)/objs/src/stream/nt_stream_script.o \
			 $(PWD)/objs/src/stream/nt_stream_variables.o \
			 $(PWD)/objs/src/stream/nt_stream_log_module.o \
			 $(PWD)/objs/src/stream/nt_stream_proxy_module.o 

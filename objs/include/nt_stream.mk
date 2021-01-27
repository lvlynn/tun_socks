mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
PWD_INCLUDE=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs/include,%,$(PWD_INCLUDE))
PWD=$(subst /objs/include,,$(PWD_INCLUDE))

NT_STREAM_INC=$(PWD)/src/stream/nt_stream.h 

NT_STREAM_OBJ=$(PWD)/objs/src/stream/nt_stream.o \
			 $(PWD)/objs/src/stream/nt_stream_core_module.o \
			 $(PWD)/objs/src/stream/nt_stream_handler.o \
			 $(PWD)/objs/src/stream/nt_stream_upstream.o \
			 $(PWD)/objs/src/stream/nt_stream_upstream_round_robin.o \
			 $(PWD)/objs/src/stream/nt_stream_script.o \
			 $(PWD)/objs/src/stream/nt_stream_variables.o 

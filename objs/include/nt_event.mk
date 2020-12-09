mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
PWD_INCLUDE=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs/include,%,$(PWD_INCLUDE))
PWD=$(subst /objs/include,,$(PWD_INCLUDE))

NT_EVENT_INC=$(PWD)/src/event/nt_event.h 

NT_EVENT_OBJ=$(PWD)/objs/src/event/nt_event.o \
	     $(PWD)/objs/src/event/nt_event_posted.o \
	     $(PWD)/objs/src/event/nt_event_accept.o \
	     $(PWD)/objs/src/event/nt_event_timer.o \
	     $(PWD)/objs/src/event/modules/nt_select.o 

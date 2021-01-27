mkfile_path=$(abspath $(lastword $(MAKEFILE_LIST)))
PWD_INCLUDE=$(shell dirname $(mkfile_path))
#PWD=$(patsubst %/objs/include,%,$(PWD_INCLUDE))
PWD=$(subst /objs/include,,$(PWD_INCLUDE))

$(PWD)/objs/src/event/nt_event.o: $(CORE_DEPS) \
	$(PWD)/src/event/nt_event.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/event/nt_event.o \
		$(PWD)/src/event/nt_event.c

$(PWD)/objs/src/event/nt_event_posted.o: $(CORE_DEPS) \
	$(PWD)/src/event/nt_event_posted.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/event/nt_event_posted.o \
		$(PWD)/src/event/nt_event_posted.c

$(PWD)/objs/src/event/nt_event_accept.o: $(CORE_DEPS) \
	$(PWD)/src/event/nt_event_accept.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/event/nt_event_accept.o \
		$(PWD)/src/event/nt_event_accept.c

$(PWD)/objs/src/event/nt_event_connect.o: $(CORE_DEPS) \
	$(PWD)/src/event/nt_event_connect.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/event/nt_event_connect.o \
		$(PWD)/src/event/nt_event_connect.c

$(PWD)/objs/src/event/nt_event_timer.o: $(CORE_DEPS) \
	$(PWD)/src/event/nt_event_timer.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/event/nt_event_timer.o \
		$(PWD)/src/event/nt_event_timer.c


$(PWD)/objs/src/event/modules/nt_select.o: $(CORE_DEPS) \
	$(PWD)/src/event/modules/nt_select.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o $(PWD)/objs/src/event/modules/nt_select.o \
		$(PWD)/src/event/modules/nt_select.c


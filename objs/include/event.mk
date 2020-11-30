
objs/src/event/nt_event.o: $(CORE_DEPS) \
	src/event/nt_event.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/nt_event.o \
		src/event/nt_event.c

objs/src/event/nt_event_posted.o: $(CORE_DEPS) \
	src/event/nt_event_posted.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/nt_event_posted.o \
		src/event/nt_event_posted.c

objs/src/event/nt_event_timer.o: $(CORE_DEPS) \
	src/event/nt_event_timer.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/nt_event_timer.o \
		src/event/nt_event_timer.c


objs/src/event/modules/nt_select.o: $(CORE_DEPS) \
	src/event/modules/nt_select.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/modules/nt_select.o \
		src/event/modules/nt_select.c


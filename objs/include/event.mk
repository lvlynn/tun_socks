
objs/src/event/nt_event.o: $(CORE_DEPS) \
	src/event/nt_event.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/nt_event.o \
		src/event/nt_event.c

objs/src/event/modules/nt_select.o: $(CORE_DEPS) \
	src/event/modules/nt_select.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/event/modules/nt_select.o \
		src/event/modules/nt_select.c


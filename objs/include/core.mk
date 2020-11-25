


objs/src/core/nt.o:  $(CORE_DEPS) \
	src/core/nt.c 
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/nt.o \
		src/core/nt.c\

objs/src/core/nt_time.o:  $(CORE_DEPS) \
	src/core/nt_time.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/nt_time.o \
		src/core/nt_time.c\

objs/src/core/nt_log.o:  $(CORE_DEPS) \
	src/core/nt_log.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/nt_log.o \
		src/core/nt_log.c\

objs/src/core/nt_connection.o:  $(CORE_DEPS) \
	src/core/nt_connection.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/nt_connection.o \
		src/core/nt_connection.c

objs/src/core/nt_cycle.o:  $(CORE_DEPS) \
	src/core/nt_cycle.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/nt_cycle.o \
		src/core/nt_cycle.c

objs/src/core/nt_queue.o:  $(CORE_DEPS) \
	src/core/nt_queue.c
	$(CC) -c $(CFLAGS) $(CORE_INCS) \
		-o objs/src/core/nt_queue.o \
		src/core/nt_queue.c


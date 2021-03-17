mkfile_path=$(abspath $(firstword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(dir $(mkfile_path))
#获得Makefile当前目录
MAKEFILE_DIR_PATSUBST := $(patsubst %/,%,$(MAKEFILE_DIR))
LAST_MENU=$(notdir $(MAKEFILE_DIR_PATSUBST))
PWD=$(subst /$(LAST_MENU),,$(MAKEFILE_DIR_PATSUBST))


include $(PWD)/config/nt_feature.mk
ifeq ($(NT_HAVE_MODULE_ACC_RCV),y)

$(PWD)/objs/modules/nt_stream_acc_rcv_module/nt_stream_acc_rcv_module.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_acc_rcv_module/nt_stream_acc_rcv_module.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_acc_rcv_module/nt_stream_acc_rcv_module.o \
		$(PWD)/modules/nt_stream_acc_rcv_module/nt_stream_acc_rcv_module.c

$(PWD)/objs/modules/nt_stream_acc_rcv_module/nt_tun_acc_handler.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_acc_rcv_module/nt_tun_acc_handler.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_acc_rcv_module/nt_tun_acc_handler.o \
		$(PWD)/modules/nt_stream_acc_rcv_module/nt_tun_acc_handler.c


$(PWD)/objs/modules/nt_stream_acc_rcv_module/tun.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_acc_rcv_module/tun.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_acc_rcv_module/tun.o \
		$(PWD)/modules/nt_stream_acc_rcv_module/tun.c
	
$(PWD)/objs/modules/nt_stream_acc_rcv_module/rbtree.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_acc_rcv_module/rbtree.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_acc_rcv_module/rbtree.o \
		$(PWD)/modules/nt_stream_acc_rcv_module/rbtree.c

$(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/ip.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_acc_rcv_module/protocol/ip.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/ip.o \
		$(PWD)/modules/nt_stream_acc_rcv_module/protocol/ip.c

$(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/tcp.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_acc_rcv_module/protocol/tcp.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/tcp.o \
		$(PWD)/modules/nt_stream_acc_rcv_module/protocol/tcp.c

$(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/socks5.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_acc_rcv_module/protocol/socks5.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/socks5.o \
		$(PWD)/modules/nt_stream_acc_rcv_module/protocol/socks5.c

$(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/forward.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_acc_rcv_module/protocol/forward.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/forward.o \
		$(PWD)/modules/nt_stream_acc_rcv_module/protocol/forward.c



endif



ifeq ($(NT_HAVE_MODULE_SOCKS5),y)

$(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_module.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_socks5_module/nt_stream_socks5_module.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_module.o \
		$(PWD)/modules/nt_stream_socks5_module/nt_stream_socks5_module.c

$(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_auth.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_socks5_module/nt_stream_socks5_auth.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_auth.o \
		$(PWD)/modules/nt_stream_socks5_module/nt_stream_socks5_auth.c

$(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_forward.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_socks5_module/nt_stream_socks5_forward.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_forward.o \
		$(PWD)/modules/nt_stream_socks5_module/nt_stream_socks5_forward.c

$(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_raw.o:  $(CORE_DEPS) \
	$(PWD)/modules/nt_stream_socks5_module/nt_stream_socks5_raw.c
	$(CC) -c $(CFLAGS) $(NT_INCLUDE) \
		-o $(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_raw.o \
		$(PWD)/modules/nt_stream_socks5_module/nt_stream_socks5_raw.c

endif	


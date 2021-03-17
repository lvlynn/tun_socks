mkfile_path=$(abspath $(firstword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(dir $(mkfile_path))
#获得Makefile当前目录
MAKEFILE_DIR_PATSUBST := $(patsubst %/,%,$(MAKEFILE_DIR))
LAST_MENU=$(notdir $(MAKEFILE_DIR_PATSUBST))
PWD=$(subst /$(LAST_MENU),,$(MAKEFILE_DIR_PATSUBST))


NT_MODULES_TEST := 4444444444/$(NT_HAVE_MODULE_ACC_RCV)
NT_MODULES_INC := 

NT_MODULES_OBJ :=
 include $(PWD)/config/nt_feature.mk

ifeq ($(NT_HAVE_MODULE_ACC_RCV),y)

NT_MODULES_INC += -I $(PWD)/modules/nt_stream_acc_rcv_module 
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_acc_rcv_module/nt_stream_acc_rcv_module.o
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_acc_rcv_module/nt_tun_acc_handler.o 
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_acc_rcv_module/tun.o 
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_acc_rcv_module/rbtree.o 
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/ip.o 
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/tcp.o 
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/socks5.o 
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_acc_rcv_module/protocol/forward.o 

endif


ifeq ($(NT_HAVE_MODULE_SOCKS5),y)
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_module.o 
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_auth.o 
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_forward.o 
NT_MODULES_OBJ += $(PWD)/objs/modules/nt_stream_socks5_module/nt_stream_socks5_raw.o 
endif



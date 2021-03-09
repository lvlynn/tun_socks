#ifndef _TUN_H_
#define _TUN_H_

#include <errno.h>
#include <net/route.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>

int interface_up( char *interface_name );
int tun_init( char *interface_name );

#endif


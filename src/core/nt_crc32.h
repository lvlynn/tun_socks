
#ifndef _NT_CRC32_H_INCLUDED_
#define _NT_CRC32_H_INCLUDED_


#include <nt_core.h>


extern uint32_t  *nt_crc32_table_short;
extern uint32_t   nt_crc32_table256[];


static nt_inline uint32_t
nt_crc32_short(u_char *p, size_t len)
{
    u_char    c;
    uint32_t  crc;

    crc = 0xffffffff;

    while (len--) {
        c = *p++;
        crc = nt_crc32_table_short[(crc ^ (c & 0xf)) & 0xf] ^ (crc >> 4);
        crc = nt_crc32_table_short[(crc ^ (c >> 4)) & 0xf] ^ (crc >> 4);
    }

    return crc ^ 0xffffffff;
}


static nt_inline uint32_t
nt_crc32_long(u_char *p, size_t len)
{
    uint32_t  crc;

    crc = 0xffffffff;

    while (len--) {
        crc = nt_crc32_table256[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    }

    return crc ^ 0xffffffff;
}


#define nt_crc32_init(crc)                                                   \
    crc = 0xffffffff


static nt_inline void
nt_crc32_update(uint32_t *crc, u_char *p, size_t len)
{
    uint32_t  c;

    c = *crc;

    while (len--) {
        c = nt_crc32_table256[(c ^ *p++) & 0xff] ^ (c >> 8);
    }

    *crc = c;
}


#define nt_crc32_final(crc)                                                  \
    crc ^= 0xffffffff


nt_int_t nt_crc32_table_init(void);


#endif /* _NT_CRC32_H_INCLUDED_ */

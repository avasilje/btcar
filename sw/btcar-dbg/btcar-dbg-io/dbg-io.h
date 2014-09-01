#ifndef __DBG_IO_H__
#define __DBG_IO_H__

void dev_tx (DWORD dw_byte_to_write, BYTE *pc_cmd, const WCHAR *pc_cmd_name);

typedef struct T_DEV_RSP_tag
{
    BYTE b_cmd;
    BYTE b_len;
    BYTE *pb_data;
} T_DEV_RSP;


#endif // !__DBG_IO_H__

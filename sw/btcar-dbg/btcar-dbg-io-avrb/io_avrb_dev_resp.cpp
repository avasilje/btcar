/***C*********************************************************************************************
**
** SRC-FILE     :   io_devresp.cpp
**                                        
** PROJECT      :   BTCAR
**                                                                
** SRC-VERSION  :   
**              
** DATE         :   
**
** AUTHOR       :   
**
** DESCRIPTION  :   Device's responses process routines
**                  Typicall function parses incoming data packet from the device, convert it to 
**                  user friendly format and sends to user interface (IO pipe) as a text string.
**                  
** COPYRIGHT    :   
**
****C*E******************************************************************************************/
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include "cmd_lib.h"
#include "dbg-io.h"

void proceed_loopback_resp (T_DEV_RSP *pt_resp, WCHAR *pc_cmd_resp_out, size_t t_max_resp_len)
{
    swprintf(pc_cmd_resp_out, t_max_resp_len, L"Response: %s\n", (WCHAR*)pt_resp->pb_data);
}

void proceed_dev_sign_resp (T_DEV_RSP *pt_resp, WCHAR *pc_cmd_resp_out, size_t t_max_resp_len)
{
    BYTE *pb_data;
    WCHAR   ca_sign[256] = L"";

    pb_data = pt_resp->pb_data;

    // Convert signature to Multibyte string
    size_t n_chars;
    mbstowcs_s(
            &n_chars,
            ca_sign,
            sizeof(ca_sign)>>1,     // sign len
            (char*)(&pb_data[2]),   // sign text
            _TRUNCATE);

    ca_sign[n_chars-1] = 0;   // assure null termination

    swprintf(pc_cmd_resp_out, t_max_resp_len, L"Device signature:\n%s. Hardware version %d.%d\n", ca_sign, pb_data[0], pb_data[1]);

}

int dev_response_processing (T_DEV_RSP *pt_resp, WCHAR *pc_cmd_resp_out, size_t t_max_resp_len)
{

    pc_cmd_resp_out[0] = L'\0';

    switch (pt_resp->b_cmd)
    {
    case 0x11: // Device signature
        proceed_dev_sign_resp(pt_resp, pc_cmd_resp_out, t_max_resp_len);
        break;
    case 0x17: // Loopback. Just return input as is in WCHAR format
        proceed_loopback_resp(pt_resp, pc_cmd_resp_out, t_max_resp_len);
        break;
    default:
        wprintf(L"Shit happens");
        while (1);
    }

    wprintf(L"UI INIT <-- IO : CMD Resp: %s\n", pc_cmd_resp_out);

    return TRUE;
}

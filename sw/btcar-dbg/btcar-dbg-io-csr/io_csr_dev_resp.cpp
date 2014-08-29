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
#include "FTD2XX.H"
#include "cmd_lib.h"
#include "io_csr.h"

void proceed_dev_sign_resp();

int dev_resp_read_payload()
{
   // Read out payload 
    DWORD dw_bytes_rcv;
    int n_rc;

    int n_payload_len;
    unsigned short *pus_payload;

    n_payload_len = guca_dev_resp[1];

    if (n_payload_len == 0)
        return TRUE;

    pus_payload = (unsigned short*)&guca_dev_resp[2];

    n_rc = FT_W32_ReadFile(
        gh_dev,
        pus_payload, 
        n_payload_len,
        &dw_bytes_rcv, 
        &gt_dev_rx_overlapped);

    if (n_rc == 0)
    {
        n_rc = FT_W32_GetLastError(gh_dev);
        if ( n_rc != ERROR_IO_PENDING) 
        {
            // log an error
            // sent something to UI
            while(1);
        }

        dw_bytes_rcv = 0;
        n_rc = FT_W32_GetOverlappedResult(
            gh_dev,         
            &gt_dev_rx_overlapped,
            &dw_bytes_rcv,
            TRUE);
                       
    }

    if (dw_bytes_rcv != n_payload_len)
    {  
        wprintf(L"Error in command response ");
        swprintf(gca_io_cmd_resp, RESP_STR_LEN, L"%s\nError in command response\n", gca_io_cmd_resp);
        return FALSE;
    }

    return TRUE;
}

void proceed_loopback_resp (void)
{

    swprintf(gca_io_cmd_resp, RESP_STR_LEN, L"Response: %s\n", (WCHAR*)&guca_dev_resp[2]);
}

void proceed_dev_sign_resp (void)
{

    WCHAR   ca_sign[256] = L"";

    {   // Convert signature to Multibyte string
        size_t n_chars;
        mbstowcs_s(
                &n_chars,
                ca_sign,
                sizeof(ca_sign)>>1,         // sign len
                (char*)&guca_dev_resp[4],   // sign text
                _TRUNCATE);

        ca_sign[n_chars-1] = 0;   // assure null termination
    }

    swprintf(gca_io_cmd_resp, RESP_STR_LEN, L"Device signature:\n%s. Hardware version %d.%d\n", ca_sign, guca_dev_resp[2], guca_dev_resp[3]);

}

int dev_response_processing (void)
{

    // TODO: Rework global str gca_io_cmd_resp to param

    // parse system command from global buffer
    if (gdw_dev_bytes_rcv == 0)
    {
        wprintf(L"No response\n");
    }
    else if (gdw_dev_bytes_rcv >= 2)
    {

        // read out response payload
        if (!dev_resp_read_payload())
        {
            dev_clear_fifos();
            return TRUE;
        }
        gca_io_cmd_resp[0] = L'\0';

        switch (guca_dev_resp[0])
        {
        case 0x11: // Device signature
            proceed_dev_sign_resp();        // Result in gca_io_cmd_resp
            break;
        case 0x17: // Loopback. Just return input as is in WCHAR format
            proceed_loopback_resp();        // Result in gca_io_cmd_resp
            break;
        default:
            wprintf(L"Shit happens");
            while (1);
        }

        // Send processed device response to IO
        // AV TODO: rework to TLV message
        {
            size_t          t_msg_len;
            BYTE            *pb_msg_buff = &gba_io_ui_tx_msg[0];

            pb_msg_buff = add_tlv_str(pb_msg_buff, UI_IO_TLV_TYPE_CMD_RSP, gca_io_cmd_resp);

            wprintf(L"UI INIT <-- IO : CMD Resp: %s\n", gca_io_cmd_resp);

            t_msg_len = terminate_tlv_list(pb_msg_buff);
            io_pipe_tx_byte(gba_io_ui_tx_msg, t_msg_len);
        }

    }
    else
    { // gdw_dev_bytes_rcv < 2
        wprintf(L"Garbage received\n");
    }

    return TRUE;
}

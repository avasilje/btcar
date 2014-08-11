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
#include "io_proc.h"
#include "cmd_lib.h"

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
        gh_btcar_dev,
        pus_payload, 
        n_payload_len,
        &dw_bytes_rcv, 
        &gt_rx_dev_overlapped);

    if (n_rc == 0)
    {
        n_rc = FT_W32_GetLastError(gh_btcar_dev);
        if ( n_rc != ERROR_IO_PENDING) 
        {
            // log an error
            // sent something to UI
            while(1);
        }

        dw_bytes_rcv = 0;
        n_rc = FT_W32_GetOverlappedResult(
            gh_btcar_dev,         
            &gt_rx_dev_overlapped,
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

int btcar_dev_response_processing(){

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
            btcar_dev_clear_fifos(); 
            return TRUE;
        }
        gca_io_cmd_resp[0] = L'\0';

        switch(guca_dev_resp[0])
        {
            case 0x11 : // Device signature
                proceed_dev_sign_resp();        // Result in gca_io_cmd_resp
                break;
            default:
                wprintf(L"Shit happens");
                while(1);
        }

        // Send processed device response to IO
        io_pipe_tx(); // Get from gca_io_cmd_resp
    }
    else
    { // gdw_dev_bytes_rcv < 2
        wprintf(L"Garbage received\n");
    }

    return TRUE;
}

void proceed_dev_sign_resp(){

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


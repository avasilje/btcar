/***C*********************************************************************************************
**
** SRC-FILE     :   io_devcmd.cpp
**                                        
** PROJECT      :   DLE
**                                                                
** SRC-VERSION  :   
**              
** DATE         :   22/01/2011
**
** AUTHOR       :   AV
**
** DESCRIPTION  :   The functions set to convert user commands from CMD_PROC_CMD_LIB to device commands
**                  Typicall function extract data from incoming user's command, repack them into
**                  binary packet and sends to DLE device
**                  
** COPYRIGHT    :   (c) 2011 Andrejs Vasiljevs. All rights reserved.
**
****C*E******************************************************************************************/
#include <windows.h>
#include <wchar.h>
#include "FTD2XX.H"
#include "io_proc.h"
#include "io_proc_devcmd.h"
#include "cmd_lib.h"

#pragma pack(1)
struct t_cmd_hdr {
    BYTE    uc_cmd;
    BYTE    uc_len;
};

OVERLAPPED gt_tx_io_overlapped = { 0 };

void send_devcmd(DWORD dw_byte_to_write, unsigned char *pc_cmd, const WCHAR *pc_cmd_name){

    int n_rc, n_gle;
    DWORD dw_bytes_written;

    if (gt_flags.dev_conn != FL_SET) return;

    { // print out raw command
        DWORD i;
        wprintf(L"\n--------------------------------- %s : %0X", pc_cmd_name, pc_cmd[0]);

        for (i = 1; i <  dw_byte_to_write; i++)
        {
            wprintf(L"-%02X", pc_cmd[i]);
        }
        wprintf(L"\n");
    }

    n_rc = FT_W32_WriteFile(
        gh_btcar_dev,
        pc_cmd, 
        dw_byte_to_write,
        &dw_bytes_written, 
        &gt_tx_io_overlapped);

    if (!n_rc)
    {
        n_gle = FT_W32_GetLastError(gh_btcar_dev);
        if (n_gle != ERROR_IO_PENDING)
        {
            n_rc = FALSE;
            goto cleanup_send_devcmd;
        }
    }

    n_rc = FT_W32_GetOverlappedResult(gh_btcar_dev, &gt_tx_io_overlapped, &dw_bytes_written, TRUE);
    if (!n_rc || (dw_byte_to_write != dw_bytes_written))
    {
        n_rc = FALSE;
        goto cleanup_send_devcmd;
    }

    n_rc = TRUE;

    // lock further command processing until response received
    // ...

cleanup_send_devcmd:

    if (n_rc)
    {
        wprintf(L"io --> mcu : %s \n", pc_cmd_name);
    }
    else
    {
        wprintf(L"Error writing command %s\n", pc_cmd_name);
        gt_flags.dev_conn = FL_FALL;
        SetEvent(gha_events[HANDLE_NOT_IDLE]);
    }

    return;

}

void cmd_io_dev_sign()
{
    int n_rc;
    DWORD dw_byte_to_write;

    #pragma pack(1)
    struct t_cmd_dev_sign {
        struct t_cmd_hdr hdr;
        // No payload
    } t_cmd = { {0x11, 0x00} };

    // -----------------------------------------------
    // --- write data from UI command to MCU command
    // ------------------------------------------------
    n_rc = TRUE;

    // Verify command arguments
    // ...

    if (!n_rc)
        goto cmd_dev_sign_error;
    
    // ----------------------------------------
    // --- Initiate data transfer to MCU
    // ----------------------------------------
    dw_byte_to_write = sizeof(t_cmd.hdr) + t_cmd.hdr.uc_len;

    send_devcmd(dw_byte_to_write, (unsigned char *)&t_cmd, L"DEV_SIGN");
    return;

cmd_dev_sign_error:

    wprintf(L"command error : DEV_SIGN\n");
    return;

}

void cmd_io_mled()
{
    int n_rc;
    DWORD dw_byte_to_write;

    #pragma pack(1)
    struct t_cmd_mled {
        struct t_cmd_hdr hdr;
        BYTE    uc_led_mask;
    } t_cmd = { {0x16, 0x01}, 0x00 };

    // -----------------------------------------------
    // --- write data from UI command to DEV command
    // ------------------------------------------------
    n_rc = TRUE;

    if (gt_cmd_mcu_led.green.dw_val)
        t_cmd.uc_led_mask |= 0x01;

    if (gt_cmd_mcu_led.red.dw_val)
        t_cmd.uc_led_mask |= 0x10;

    if (!n_rc)
        goto cmd_io_mled_error;
    
    // ----------------------------------------
    // --- Initiate data transfer to MCU
    // ----------------------------------------
    dw_byte_to_write = sizeof(t_cmd.hdr) + t_cmd.hdr.uc_len;

    send_devcmd(dw_byte_to_write, (unsigned char *)&t_cmd, L"MLED");
    return;

cmd_io_mled_error:

    wprintf(L"command error : MLED\n");
    return;

}


#if  0   // Command template                    
void cmd_io_???()
{

    int n_rc;
    DWORD dw_byte_to_write;

    #pragma pack(1)
    struct t_cmd_???{
        struct t_cmd_hdr hdr;
        BYTE/WORD    uc/us_???;
    } t_cmd = { {0x??, 0x??}, 0x?? ... };

    // -----------------------------------------------
    // --- write data from UI command to DEV command
    // ------------------------------------------------
    n_rc = TRUE;

    // Verify command arguments
    // ...

    t_cmd.??? = (WORD/BYTE)gt_cmd_???.???.dw_val;

    if (!n_rc)
        goto cmd_io_???_error;
    
    // ----------------------------------------
    // --- Initiate data transfer to MCU
    // ----------------------------------------
    dw_byte_to_write = sizeof(t_cmd.hdr) + t_cmd.hdr.uc_len;

    send_devcmd(dw_byte_to_write, (unsigned char *)&t_cmd, L"???");
    return;

cmd_io_???_error:

    wprintf(L"command error : ???\n");
    return;

}

#endif // #if 0
                       

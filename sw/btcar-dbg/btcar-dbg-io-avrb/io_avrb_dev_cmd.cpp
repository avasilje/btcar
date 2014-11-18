/***C*********************************************************************************************
**
** SRC-FILE     :   io_devcmd.cpp
**                                        
** PROJECT      :   BTCAR
**                                                                
** SRC-VERSION  :   
**              
** DATE         :   22/01/2011
**
** AUTHOR       :   AV
**
** DESCRIPTION  :   The file define the set of functions to convert user 
**                  commands from UI to device commands.
**                  Device - AVR Back door
**                  Typicall function extract data from incoming command,
**                  repack them into binary packet and sends to the device.
**                  
** COPYRIGHT    :   (c) 2011 Andrejs Vasiljevs. All rights reserved.
**
****C*E************************************************************************/
#include <windows.h>
#include <wchar.h>
#include "cmd_lib.h"
#include "dbg-io.h"
#include "io_avrb_ui_cmd.h"
#include "io_avrb_dev_cmd.h"

#pragma pack(1)
struct t_cmd_hdr {
    BYTE    uc_cmd;
    BYTE    uc_len;
};


void cmd_io_sign (void)
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
        goto cmd_sign_error;
    
    // ----------------------------------------
    // --- Initiate data transfer to MCU
    // ----------------------------------------
    dw_byte_to_write = sizeof(t_cmd.hdr) + t_cmd.hdr.uc_len;

    dev_tx(dw_byte_to_write, (BYTE*)&t_cmd, L"SIGN");
    return;

cmd_sign_error:

    wprintf(L"command error : DEV_SIGN\n");
    return;

}

void cmd_io_mled (void)
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

    dev_tx(dw_byte_to_write, (BYTE*)&t_cmd, L"MLED");
    return;

cmd_io_mled_error:

    wprintf(L"command error : MLED\n");
    return;

}

void cmd_io_loopback (void)
{
    int n_rc;
    DWORD dw_byte_to_write;

    #pragma pack(1)
    struct t_cmd_loopback {
        struct t_cmd_hdr hdr;
        WCHAR  ca_lbs[LOOPBACK_STRING_DATA_LEN];
    } t_cmd = {{0x17, 0x00}, {0}};

    // -----------------------------------------------
    // --- write data from UI command to DEV command
    // ------------------------------------------------
    n_rc = TRUE;

    size_t t_str_len;
    t_str_len = wcslen(gt_cmd_loopback.lbs.pc_str);
    t_cmd.hdr.uc_len = t_str_len*2 + 2;

    memcpy(t_cmd.ca_lbs, gt_cmd_loopback.lbs.pc_str, t_cmd.hdr.uc_len);
    // ----------------------------------------
    // --- Initiate data transfer to MCU
    // ----------------------------------------
    dw_byte_to_write = sizeof(t_cmd.hdr) + t_cmd.hdr.uc_len;

    dev_tx(dw_byte_to_write, (BYTE*)&t_cmd, L"LOOPBACK");
    return;

}

void cmd_io_servo()
{
    int n_rc;
    DWORD dw_byte_to_write;

    #pragma pack(1)
    struct t_cmd_servo{
        struct t_cmd_hdr hdr;
        BYTE   uc_ch;
        BYTE   uc_val;
    } t_cmd = { {0x20, 0x02}, 0x00, 0x00 };

    // -----------------------------------------------
    // --- write data from UI command to DEV command
    // ------------------------------------------------
    n_rc = TRUE;

    // Verify command arguments
    // ...

    t_cmd.uc_ch  = (BYTE)gt_cmd_servo.ch.dw_val;
    t_cmd.uc_val = (BYTE)gt_cmd_servo.val.dw_val;

    if (!n_rc)
        goto cmd_io_servo_error;
    
    // ----------------------------------------
    // --- Initiate data transfer to MCU
    // ----------------------------------------
    dw_byte_to_write = sizeof(t_cmd.hdr) + t_cmd.hdr.uc_len;

    dev_tx(dw_byte_to_write, (BYTE*)&t_cmd, L"SERVO");
    return;

cmd_io_servo_error:

    wprintf(L"command error : SERVO\n");
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

    dev_tx(dw_byte_to_write, (BYTE*)&t_cmd, L"???");
    return;

cmd_io_???_error:

    wprintf(L"command error : ???\n");
    return;

}

#endif // #if 0
                       

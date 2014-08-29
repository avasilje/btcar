/***C*********************************************************************************************
**
** SRC-FILE     :   io_csr.cpp
**
** PROJECT      :   BTCAR
**
** AUTHOR       :   AV
**
** DESCRIPTION  :   Set of UI cmd for CSR UART
**
** COPYRIGHT    :
**
****C*E******************************************************************************************/
#include <windows.h>
#include <wchar.h>
#include "cmd_lib.h"
#include "io_csr_ui_cmd.h"
#include "io_csr_dev_cmd.h"

//---------------------------------------------------------------------------
t_cmd_sign_tag gt_cmd_sign = {
    {NULL, CFT_LAST, 0, 0}
};

//---------------------------------------------------------------------------
t_cmd_mcu_led_tag gt_cmd_mcu_led = {
    {L"GREEN"  ,  CFT_NUM,      0,           0},
    {L"RED"    ,  CFT_NUM,      0,           0},
    {NULL, CFT_LAST, 0, 0}
};


//---------------------------------------------------------------------------

WCHAR ca_lbs_str[LOOPBACK_STRING_DATA_LEN] = L"--LOOPBACK_TEST_STRING--";
t_cmd_loopback_tag gt_cmd_loopback = {
    {L"STR"    ,  CFT_TXT,      LOOPBACK_STRING_DATA_LEN,  (DWORD)ca_lbs_str},
    {NULL, CFT_LAST, 0, 0}
};


#if 0
//---------------------------------------------------------------------------
t_cmd_???_tag gt_cmd_??? = {
    {L"???"      ,  CFT_NUM,      0,           0},
    {NULL, CFT_LAST, 0, 0}
};


//---------------------------------------------------------------------------
#define xxx_WR_DATA_LEN 128
WCHAR ca_xxx_wr_data[xxx_WR_DATA_LEN];
t_cmd_xxx_wr_tag gt_cmd_xxx_wr = {
    {L"DATA"    ,  CFT_TXT,      xxx_WR_DATA_LEN,  (DWORD)ca_xxx_wr_data},
    {NULL, CFT_LAST, 0, 0}
};

#endif // #if 0


T_UI_CMD gta_io_csr_ui_cmd[] = {
        { L"SIGN",     (T_UI_CMD_FIELD*)&gt_cmd_sign,     (void*)cmd_io_sign     },
        { L"MLED",     (T_UI_CMD_FIELD*)&gt_cmd_mcu_led,  (void*)cmd_io_mled     },
        { L"LOOPBACK", (T_UI_CMD_FIELD*)&gt_cmd_loopback, (void*)cmd_io_loopback },
        { 0, 0 }
};


//---------------------------------------------------------------------------



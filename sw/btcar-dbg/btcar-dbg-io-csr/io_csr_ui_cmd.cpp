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

//----------------------------------------------------------
struct t_cmd_init_tag
{
    T_UI_CMD   eomsg;
};                          

extern struct t_cmd_init_tag gt_cmd_init;

//----------------------------------------------------------
struct t_cmd_sign_tag
{
    T_UI_CMD   eomsg;
};

extern struct t_cmd_sign_tag gt_cmd_sign;

//----------------------------------------------------------
struct t_cmd_mcu_led_tag
{
    T_UI_CMD   green;
    T_UI_CMD   red;
    T_UI_CMD   eomsg;
};

extern struct t_cmd_mcu_led_tag gt_cmd_mcu_led;

#if 0
//----------------------------------------------------------
struct t_cmd_???_tag
{
    T_UI_CMD   ???;
    T_UI_CMD   eomsg;
};

extern struct t_cmd_???_tag gt_cmd_???;

#endif // #if 0

//T_IO_UI_CMD gta_ui_cmd[] = {
//        { L"SIGN", (T_UI_CMD*)&gt_cmd_sign },
//        { L"INIT", (T_UI_CMD*)&gt_cmd_init },
//        { L"MLED", (T_UI_CMD*)&gt_cmd_mcu_led },
//        { 0, 0 }
//};

T_UI_CMD gta_io_ui_cmd[] = {
        { { L"SIGN", (T_UI_CMD*)&gt_cmd_sign    }, NULL },
        { { L"INIT", (T_UI_CMD*)&gt_cmd_init    }, NULL },
        { { L"MLED", (T_UI_CMD*)&gt_cmd_mcu_led }, NULL },
        { { 0, 0 }
};


//---------------------------------------------------------------------------



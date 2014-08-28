#ifndef __IO_CSR_UI_CMD_H__
#define __IO_CSR_UI_CMD_H__

extern T_CP_CMD gta_io_ui_cmd[];


//----------------------------------------------------------
struct t_cmd_init_tag{
    T_CP_CMD_FIELD   eomsg;
};                          

extern struct t_cmd_init_tag gt_cmd_init;

//----------------------------------------------------------
struct t_cmd_dev_sign_tag{
    T_CP_CMD_FIELD   eomsg;
};

extern struct t_cmd_dev_sign_tag gt_cmd_dev_sign;

//----------------------------------------------------------
struct t_cmd_mcu_led_tag{
    T_CP_CMD_FIELD   green;
    T_CP_CMD_FIELD   red;
    T_CP_CMD_FIELD   eomsg;
};

extern struct t_cmd_mcu_led_tag gt_cmd_mcu_led;

#if 0
//----------------------------------------------------------
struct t_cmd_???_tag{
    T_CP_CMD_FIELD   ???;
    T_CP_CMD_FIELD   eomsg;
};

extern struct t_cmd_???_tag gt_cmd_???;
#endif // #if 0

typedef struct t_cmd_tag
{
    T_CP_CMD    *pt_ui_cmd;
    void(*pf_ui_cmd_func)();
} T_IO_UI_CMD;



#endif // !__IO_CSR_UI_CMD_H__

#ifndef __IO_AVRB_UI_CMD_H__
#define __IO_AVRB_UI_CMD_H__

//----------------------------------------------------------
struct t_cmd_sign_tag{
    T_UI_CMD_FIELD   eomsg;
};

extern struct t_cmd_sign_tag gt_cmd_dev_sign;

//----------------------------------------------------------
struct t_cmd_mcu_led_tag{
    T_UI_CMD_FIELD   green;
    T_UI_CMD_FIELD   red;
    T_UI_CMD_FIELD   eomsg;
};

extern struct t_cmd_mcu_led_tag gt_cmd_mcu_led;

//----------------------------------------------------------

#define LOOPBACK_STRING_DATA_LEN 128

struct t_cmd_loopback_tag{
    T_UI_CMD_FIELD   lbs;
    T_UI_CMD_FIELD   eomsg;
};

extern struct t_cmd_loopback_tag gt_cmd_loopback;


#if 0
//----------------------------------------------------------
struct t_cmd_???_tag{
    T_UI_CMD_FIELD   ???;
    T_UI_CMD_FIELD   eomsg;
};

extern struct t_cmd_???_tag gt_cmd_???;
#endif // #if 0


#endif // !__IO_AVRB_UI_CMD_H__


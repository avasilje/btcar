/***C*********************************************************************************************
**
** SRC-FILE     :   
**                                        
** PROJECT      :   CMD line 
**                                                                
** DATE         :   
**
** AUTHOR       :   AV
**
** DESCRIPTION  :   
**                  
** COPYRIGHT    :   
**
****C*E******************************************************************************************/
#ifndef __CMD_LIB_H__
#define __CMD_LIB_H__
typedef enum e_cmd_field_type_tag {
    CFT_NUM,  
    CFT_TXT, 
    CFT_RESERVED = 4, 
    CFT_LAST = -1
}E_CMD_FIELD_TYPE;

typedef struct cmd_field_tag{

    WCHAR   *pc_name;
    E_CMD_FIELD_TYPE e_type;

    int     n_len;
    union {
        DWORD   dw_val;
        WCHAR   *pc_str;
    };
} T_CP_CMD_FIELD;

typedef struct t_cmd_tag {
    WCHAR   *pc_name;
    T_CP_CMD_FIELD  *pt_fields;
}T_CP_CMD;

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



// --- Functions -----------

T_CP_CMD* lookup_cp_cmd(WCHAR *pc_cmd_arg, T_CP_CMD *pt_cmd_lib);
int check_cp_cmd(WCHAR *pc_cmd_arg, T_CP_CMD_FIELD *pt_fields);
T_CP_CMD* decomposite_cp_cmd(WCHAR *pc_cmd_arg, T_CP_CMD *pt_cmd, int n_update);

  //  \
  //  |
  //  +--IO2UI
  //      |
  //      +---UI CMD [START, END, CMD]
  //      |      |
  //      |      |
  //      |      +-- START DATA <not impl> 
  //      |      |
  //      |      +-- END DATA <not impl> 
  //      |      |
  //      |      +-- CMD NAME [STR]
  //      |             |
  //      |             +-- FLD NAME[STR]
  //      |             +-- FLD TYPE
  //      |             +-- FLD LEN
  //      |             +-- CMD FLD CNT
  //      |
  //      +-- CMD_RESP [MSG STR]
    

// --- UI initialization related stuff ---
typedef enum E_UI_IO_TLV_TYPE_tag
{ 
    UI_IO_TLV_TYPE_NONE = 0,

    UI_IO_TLV_TYPE_UI_CMD       = 0x20,      // UI initialization message. Val = [START, CMD, END]
    UI_IO_TLV_TYPE_CMD_RSP,
    
    UI_IO_TLV_TYPE_CMD_NAME     = 0x30,
    UI_IO_TLV_TYPE_CMD_FLD_CNT,

    UI_IO_TLV_TYPE_FLD_NAME     = 0x40,
    UI_IO_TLV_TYPE_FLD_TYPE,
    UI_IO_TLV_TYPE_FLD_LEN,
    UI_IO_TLV_TYPE_FLD_VAL
} E_UI_IO_TLV_TYPE;

typedef enum E_IO_UI_UI_CMD_tag
{ 
    IO_UI_UI_CMD_START  = 0x10,     // Start UI initialization
    IO_UI_UI_CMD_CDEF   = 0x20,     // UI CMD definition
    IO_UI_UI_CMD_END    = 0x30,     // End UI initializatio
} E_IO_UI_UI_CMD;


typedef struct T_UI_IO_TLV_tag
{
	E_UI_IO_TLV_TYPE    type;
	int                 len;
	WCHAR               *val_str;
	DWORD               val_dword;
} T_UI_IO_TLV;



#endif // __CMD_LIB_H__
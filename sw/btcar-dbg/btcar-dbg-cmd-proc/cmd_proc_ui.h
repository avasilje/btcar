/***C*********************************************************************************************
**
** SRC-FILE     :   cmd_proc_ui.h
**                                        
** PROJECT      :   user interface API definition
**                                                                
** SRC-VERSION  :   0
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
#ifndef __CMD_PROC_UI_H__
#define __CMD_PROC_UI_H__

// UI initialization related stuff
typedef enum E_UI_INIT_TLV_TYPE_tag
{
	UI_INIT_TLV_TYPE_NONE = 0,
	UI_INIT_TLV_TYPE_UI_INIT_START = 1,
	UI_INIT_TLV_TYPE_UI_INIT_END,
	UI_INIT_TLV_TYPE_CMD_NAME,
	UI_INIT_TLV_TYPE_CMD_FLD_CNT,
	UI_INIT_TLV_TYPE_FLD_NAME,
	UI_INIT_TLV_TYPE_FLD_TYPE,
	UI_INIT_TLV_TYPE_FLD_LEN,
	UI_INIT_TLV_TYPE_FLD_VAL
} E_UI_INIT_TLV_TYPE;

typedef struct T_UI_INIT_TLV_tag
{
	E_UI_INIT_TLV_TYPE  type;
	int                 len;
	WCHAR               *val_str;
	DWORD               val_dword;
} T_UI_INIT_TLV;

#endif // __CMD_PROC_UI_H__
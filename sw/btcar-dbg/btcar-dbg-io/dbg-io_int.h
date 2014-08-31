#ifndef __DBG_IO_INT_H__
#define __DBG_IO_INT_H__

#define HANDLE_NOT_IDLE         0
#define HANDLE_IO_PIPE_RX       1    // Handle for UI related events (incomming UI commands)
#define HANDLE_IO_PIPE_CONN     2   // Handle for IO pipe related events
#define HANDLE_DEV_RX           3    // incoming response from device
#define HANDLE_LAST_HANDLE      4

#define HANDLES_NUM HANDLE_LAST_HANDLE

extern HANDLE gha_events[HANDLES_NUM];

typedef enum e_flag_tag {
    FL_CLR      = 0,
    FL_RISE     = 1,
    FL_FALL     = 2,
    FL_SET      = 3,
    FL_UNDEF    = 4
} E_FLAG;

typedef struct t_io_flags_tag{
    E_FLAG   dev_conn;
    E_FLAG   io_conn;
    E_FLAG   io_ui;
    E_FLAG   exit;
}T_IO_FLAGS;


int  io_pipe_tx_byte(BYTE *pc_io_msg, size_t t_msg_len);

typedef struct t_io_ui_tag{
    // Add to here
    //   pipe name
    //   pipe handle
    //   IO <-> UI TX buffer
    //   IO <-> UI RX buffer

    T_UI_CMD        *pt_curr_cmd;

}T_IO_UI;

// Interaface to device specific modules
// AV TODO: Remove the declaration by calling API function
extern T_UI_CMD gta_io_ui_cmd[];
extern WCHAR gca_pipe_name[];
extern WCHAR gca_ui_init_str[];
extern int dev_response_processing(T_DEV_RSP *pt_rsp, WCHAR *pc_cmd_resp_out, size_t t_max_resp_len);

#endif // !__DBG_IO_INT_H__

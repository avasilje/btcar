#ifndef __IO_CSR_H__
#define __IO_CSR_H__


#define IO_RX_MSG_LEN 1024
#define IO_TX_MSG_LEN 1024

#define HANDLE_NOT_IDLE         0
#define HANDLE_IO_PIPE_RX       1    // Handle for UI related events (incomming UI commands)
#define HANDLE_IO_PIPE_CONN     2   // Handle for IO pipe related events
#define HANDLE_DEV_RX           3    // incoming response from device
#define HANDLE_LAST_HANDLE      4

#define HANDLES_NUM HANDLE_LAST_HANDLE

extern HANDLE gha_events[HANDLES_NUM];
extern FT_HANDLE gh_dev;

typedef enum e_flag_tag {
    FL_CLR      = 0,
    FL_RISE     = 1,
    FL_FALL     = 2,
    FL_SET      = 3,
    FL_UNDEF    = 4
} E_FLAG;

#define RESP_STR_LEN 1024

typedef struct t_io_flags_tag{
    E_FLAG   dev_conn;
    E_FLAG   io_conn;
    E_FLAG   io_ui;
    E_FLAG   exit;
}T_IO_FLAGS;

extern T_IO_FLAGS gt_flags;

extern DWORD gdw_dev_bytes_rcv;
extern BYTE guca_dev_resp[1024];
extern int gn_dev_resp_timeout;

extern OVERLAPPED gt_dev_rx_overlapped;

extern WCHAR gca_io_cmd_resp[RESP_STR_LEN];

extern HANDLE gh_dump_file;

//extern T_FREQ_TABLE  *gpt_active_table;

void io_pipe_close();
int  io_ui_cmd_proc();
int  io_pipe_check();
int  io_ui_check();

int  io_pipe_tx_str(WCHAR *pc_io_msg);
int  io_pipe_tx_byte(BYTE *pc_io_msg, size_t t_msg_len);
int  io_pipe_rx_init();

int dev_clear_fifos();

// AV TODO: Initernal utility. Should reworked somehow
size_t terminate_tlv_list(BYTE *pb_msg_buff);
extern BYTE  gba_io_ui_tx_msg[];

typedef struct t_io_ui_tag{
    // Add to here
    //   pipe name
    //   pipe handle
    //   IO <-> UI TX buffer
    //   IO <-> UI RX buffer

    T_UI_CMD        *pt_curr_cmd;

}T_IO_UI;

#endif // !__IO_CSR_H__

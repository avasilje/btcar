
#define IO_RX_MSG_LEN 1024
#define IO_TX_MSG_LEN 1024

#define HANDLE_NOT_IDLE         0
#define HADLE_IO_CMD            1       // incoming command from IO pipe
#define HADLE_DEV_RESP          2       // incoming response from device
#define HANDLE_IO_PIPE          3
#define HANDLE_LAST_HANDLE      4

#define HANDLES_NUM HANDLE_LAST_HANDLE

extern HANDLE gha_events[HANDLES_NUM];
extern FT_HANDLE gh_btcar_dev;

typedef enum e_flag_tag {
    FL_CLR      = 0,
    FL_RISE     = 1,
    FL_FALL     = 2,
    FL_SET      = 3,
    FL_UNDEF    = 4
} E_FLAG;

#define RESP_STR_LEN 1024
#define IO_UI_INIT_STR_LEN 1024

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

extern OVERLAPPED gt_rx_dev_overlapped;

extern WCHAR gca_io_cmd_resp[RESP_STR_LEN];

extern HANDLE gh_dump_file;

//extern T_FREQ_TABLE  *gpt_active_table;

void io_close();
int  io_command_processing();
int  io_connection_check();
int  io_ui_status_check();

int  io_pipe_tx(WCHAR *pc_io_msg);
int  io_pipe_rx_init();

void btcar_dev_close();
int  btcar_dev_connect();
int  btcar_dev_response_processing();
int  btcar_dev_connection_check();
int  btcar_dev_resp_rx_init(DWORD dw_dev_resp_req_len);
int  btcar_dev_clear_fifos();




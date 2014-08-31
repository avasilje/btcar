// AV TODO:
//  1. beatify console behavior if IO disconnected during typing

/***C*********************************************************************************************
**
** SRC-FILE     :   cmd_proc.cpp
**                                        
** PROJECT      :   BTCAR debug command user interface
**                                                                
** SRC-VERSION  :   
**              
** DATE         :   
**
** AUTHOR       :   AV
**
** DESCRIPTION  :   Command line tool for BTCAR IO commands with autocomplete & history support.
**                  BTCAR IO commands are defines in BTCRA-DBG-CMD-LIB project.
**                  
** COPYRIGHT    :   
**
****C*E******************************************************************************************/
#include <windows.h>
#include <wchar.h>
#include <stdio.h>
#include "cmd_line_wc.h"
#include "cmd_lib.h"
#include "dbg-ui.h"
#include "dbg-ui_inp.h"

// --- Internal function ------------------------------------------------------


// --- Globals ----------------------------------------------------------------
CMD_PROC_FLAGS gt_flags;
int gn_action;

#define CONSOLE_TITLE_LEN 128
WCHAR  gca_console_title[CONSOLE_TITLE_LEN];

// ----------------------------------------------------------------------------
HANDLE gha_events[HANDLES_NUM];

HANDLE gh_io_pipe;
OVERLAPPED gt_io_pipe_overlap = { 0 };
OVERLAPPED gt_io_pipe_rx_overlap = { 0 };
OVERLAPPED gt_io_pipe_tx_overlap = { 0 };

BYTE gba_io_pipe_rx_buff[IO_PIPE_RX_BUFF_LEN];

// ----------------------------------------------------------------------------

#define IO_PIPE_NAME_NUM 3
const WCHAR *gcaa_pipe_name_list[IO_PIPE_NAME_NUM] =
             {  L"\\\\.\\pipe\\io_csr", 
                L"\\\\.\\pipe\\io_avrb", 
                L"\\\\.\\pipe\\io_avrf" };
const WCHAR *gpc_pipe_name = NULL;
        
typedef struct t_io_ui_tag{
	T_UI_CMD   *pt_curr_cmd;
}T_IO_UI;

T_IO_UI gt_io_ui;

#define MAX_IO_UI_CMD   100
T_UI_CMD    gta_io_ui_cmd[MAX_IO_UI_CMD];       // AV TODO: Rework to dynamic allocation
T_UI_CMD    *gpt_io_ui_cmd = NULL;


/***C*F******************************************************************************************
**
** FUNCTION       : get_command_line_args
** DATE           : 01/20/2011
** AUTHOR         : AV
**
** DESCRIPTION    : Parses command line argument
**
** PARAMETERS     : 
**
** RETURN-VALUE   : 
**
** NOTES          : Not tested
**
***C*F*E***************************************************************************************************/
int get_command_line_args(int argc, WCHAR *argv[], int n_arg_num){

    while(n_arg_num < argc){

        if ( wcscmp(argv[n_arg_num], L"-h") == 0 ||
             wcscmp(argv[n_arg_num], L"--help") == 0 ){
             gn_action  = SHOW_OPTIONS_HELP;
             break;
        }


        if ( wcscmp(argv[n_arg_num], L"-v") == 0 ||
             wcscmp(argv[n_arg_num], L"--verbose") == 0 ){
            n_arg_num++;
            gt_flags.verbose = FL_SET;
            continue;
        }

        if ( wcscmp(argv[n_arg_num], L"-c") == 0 ||
             wcscmp(argv[n_arg_num], L"--command") == 0 ){
            n_arg_num++;
            gn_action = SEND_COMMAND;
            continue;
        }

        if ( wcscmp(argv[n_arg_num], L"-no_io_start") == 0){
            n_arg_num++;
            gt_flags.io_auto_start = FL_CLR;
            continue;
        }


        // If argument not regonized, then single command supposed
        break;
    }

    return n_arg_num;
}

void ui_cmd_field_dump (T_UI_CMD_FIELD* pt_cmd_fields)
{
	T_UI_CMD_FIELD	*pt_field = pt_cmd_fields;

	while (pt_field->pc_name)
	{
		if (pt_field->e_type == CFT_TXT)
		{
			wprintf(L"        %-10s %d %d %s\n",
				pt_field->pc_name,
				pt_field->e_type,
				pt_field->n_len,
				pt_field->pc_str);
		}
		else if (pt_field->e_type == CFT_NUM)
		{
			wprintf(L"        %-10s %d %d %d\n",
				pt_field->pc_name,
				pt_field->e_type,
				pt_field->n_len,
				pt_field->dw_val);
		}
		pt_field++;
	}

	return;
}

void ui_cmd_fields_free (T_UI_CMD_FIELD* pt_cmd_fields)
{
	T_UI_CMD_FIELD	*pt_field = pt_cmd_fields;

	while (pt_field->pc_name)
	{
		free(pt_field->pc_name);

		if (pt_field->e_type == CFT_TXT && pt_field->pc_str)
		{
			free(pt_field->pc_str);
		}
		pt_field++;
	}

	free(pt_cmd_fields);

    // AV TODO: rework to memfree()
    memset(gta_io_ui_cmd, 0, sizeof(gta_io_ui_cmd));

	return;
}

void ui_cmd_dump (void)
{
	T_UI_CMD	*pt_cmd = &gta_io_ui_cmd[0];

	while (pt_cmd->pc_name)
	{
		wprintf(L"    %s\n", pt_cmd->pc_name);
		ui_cmd_field_dump(pt_cmd->pt_fields);
		pt_cmd++;
	}
}

void ui_cmds_free (void)
{
	T_UI_CMD	*pt_cmd = &gta_io_ui_cmd[0];

	while (pt_cmd->pc_name)
	{
		ui_cmd_fields_free(pt_cmd->pt_fields);
		pt_cmd++;
	}
}

/***C*F******************************************************************************************
**
** FUNCTION       : ui_cmd_proc
** DATE           : 
** AUTHOR         : AV
**
** DESCRIPTION    : Check user input (parse command), write  to DLE IO pipe if command OK
**
** PARAMETERS     : 
**
** RETURN-VALUE   : returns TRUE if OK, FALSE otherwise
**
** NOTES          : 
**
***C*F*E***************************************************************************************************/
int ui_cmd_proc(WCHAR *pc_cmd_arg)
{
    T_UI_CMD   *pt_curr_cmd;

    WCHAR   ca_cmd[CMD_LINE_LENGTH];
    WCHAR   *pc_cmd_token;

    // Make command local copy to use STRTOK
    wcscpy(ca_cmd, pc_cmd_arg);

    // Read command
    pc_cmd_token = wcstok(ca_cmd, L" ");
    if (!pc_cmd_token) return TRUE;

    pc_cmd_token = _wcsupr(pc_cmd_token);

    if (wcscmp(pc_cmd_token, L"QUIT") == 0 || 
        wcscmp(pc_cmd_token, L"EXIT") == 0 ||
        wcscmp(pc_cmd_token, L"Q")    == 0 )
    {
        gt_flags.exit = FL_SET;
        return FALSE;
    }

    if (wcscmp(pc_cmd_token, L"HELP") == 0)
    {
        show_cmd_help(gpt_io_ui_cmd);
        return TRUE;
    }

    // command processor starts here
    pt_curr_cmd = NULL;
    if (gpt_io_ui_cmd)
    { 
        pt_curr_cmd = decomposite_cp_cmd(pc_cmd_arg, gpt_io_ui_cmd, FALSE);
    }
    
    if (pt_curr_cmd == NULL)
    { // Command not found or error in parameters
        return TRUE;
    }
    
    io_pipe_tx_str(pc_cmd_arg);

    return TRUE;
}

void ui_check (int *pn_prompt_restore)
{
	// Try to initialize UI over IO pipe if not initialized yet. IO pipe must be connected 
	if ((gt_flags.io_ui == FL_CLR || gt_flags.io_ui == FL_UNDEF) && gt_flags.io_conn != FL_SET)
	{ // Do nothing if IO not connected 

	}

	// UI init completed
	if (gt_flags.io_ui == FL_RISE)
	{
		wprintf(L"UI commands initialized\n");
        SetConsoleTitle(gca_console_title);

        *pn_prompt_restore = TRUE;

		ui_cmd_dump();
		gt_flags.io_ui = FL_SET;
        gpt_io_ui_cmd = gta_io_ui_cmd;

	}

	// Something goes wrong during UI init
	if (gt_flags.io_ui == FL_FALL)
	{
		wprintf(L"UI commands freed\n");
        *pn_prompt_restore = TRUE;

		ui_cmds_free();
		gt_flags.io_ui = FL_CLR;
        gpt_io_ui_cmd = NULL;
	}

	// Nothing to do. Everything is OK
	if (gt_flags.io_ui == FL_SET)
	{

	}

}

/***C*F******************************************************************************************
**
** FUNCTION       : io_pipe_rx_init
**
** AUTHOR         : AV
**
** DESCRIPTION    : Initiate IO asynchronous read
**
** PARAMETERS     : 
**
** RETURN-VALUE   : returns TRUE if OK, FALSE otherwise
**
** NOTES          : 
**
***C*F*E***************************************************************************************************/
int io_pipe_rx_init (void){

    int n_rc, n_gle;
    DWORD dw_n;

    n_rc = ReadFile(
        gh_io_pipe,                 //__in         HANDLE hFile,
        gba_io_pipe_rx_buff,        //__out        LPVOID lpBuffer,
        IO_PIPE_RX_BUFF_LEN,        //__in         DWORD nNumberOfBytesToRead,
        &dw_n,                      //__out_opt    LPDWORD lpNumberOfBytesRead,
        &gt_io_pipe_rx_overlap      //__inout_opt  LPOVERLAPPED lpOverlapped
        );

    if(n_rc != 0)
    {   
        return FALSE;
    }
    n_gle = GetLastError();

    if (n_gle == ERROR_IO_PENDING)
    {
        gt_flags.io_conn = FL_SET;
    }
    else
    {
        gt_flags.io_conn = FL_FALL;
    }

    return TRUE;
}

int io_pipe_tx_str (WCHAR *p_io_msg)
{
    int n_rc, n_gle;
    DWORD dw_n;
    
    // Send command 
    n_rc = WriteFile(
        gh_io_pipe,                     //__in         HANDLE hFile,
        p_io_msg,                       //__in         LPCVOID lpBuffer,
        wcslen(p_io_msg)*2 + 2,         //__in         DWORD nNumberOfBytesToWrite, (+2 null termination)
        &dw_n,                          //__out_opt    LPDWORD lpNumberOfBytesWritten,
        &gt_io_pipe_tx_overlap          //__inout_opt  LPOVERLAPPED lpOverlapped
        );

    if (!n_rc){
        n_gle = GetLastError();
        if (n_gle != ERROR_IO_PENDING)
        {
            wprintf(L"Something wrong GLE=%d @ %d\n", n_rc, __LINE__);
            // AV TODO: restore prompt
            gt_flags.io_conn = FL_FALL;
            SetEvent(gha_events[HANDLE_IO_PIPE]);

            return FALSE;
        }
    }

    return TRUE;

}

T_UI_CMD_FIELD* io_pipe_rx_proc_ui_cmd_cdef (BYTE *pb_msg_buff_inp)
{
    T_UI_IO_TLV   t_tlv;
    DWORD           dw_fld_cnt, dw_fld_cnt_ref;
	BYTE            *pb_msg_buff = pb_msg_buff_inp;
	T_UI_CMD_FIELD	*pt_cmd_fields, *pt_field;

    // Input buffer points to the begining of first FIELD TLV

    // Just count number of fields in TLV list to create fields array 
	// Number of FLD_NAME TLVs is compared with reference in dedicated TLV.
    dw_fld_cnt = 0;
	dw_fld_cnt_ref = ~(0);
	pb_msg_buff = pb_msg_buff_inp;
	while (pb_msg_buff = get_tlv_tl(pb_msg_buff, &t_tlv))
    {
		if (t_tlv.type == UI_IO_TLV_TYPE_CMD_FLD_CNT)
		{
			pb_msg_buff = get_tlv_v_dword(pb_msg_buff, &t_tlv);
			dw_fld_cnt_ref = t_tlv.val_dword;
			continue;
		}

		if (t_tlv.type == UI_IO_TLV_TYPE_FLD_NAME)
        {
			dw_fld_cnt++;
        }
		pb_msg_buff += t_tlv.len;
    }

	if (dw_fld_cnt != dw_fld_cnt_ref)
	{
		wprintf(L"Att: Something wrong @ $d - 0x%08X != 0x%08X", dw_fld_cnt, dw_fld_cnt_ref);
		return NULL;
	}

    // Allocate Fields array 
	pt_cmd_fields = (T_UI_CMD_FIELD*)malloc((dw_fld_cnt + 1) * sizeof(T_UI_CMD_FIELD));	// +1 for NULL terminated array
	memset(pt_cmd_fields, 0, (dw_fld_cnt + 1) * sizeof(T_UI_CMD_FIELD));

	// Fill up fields' data
	pt_field = NULL;
	pb_msg_buff = pb_msg_buff_inp;
	while (pb_msg_buff = get_tlv_tl(pb_msg_buff, &t_tlv))
	{
		switch (t_tlv.type)
		{
		case UI_IO_TLV_TYPE_FLD_NAME:

			// Init or advance field pointer 
			if (pt_field) pt_field++;
			else pt_field = &pt_cmd_fields[0];

			pb_msg_buff = get_tlv_v_str(pb_msg_buff, &t_tlv);
			pt_field->pc_name = t_tlv.val_str;
            pt_field->n_len = -1;
			break;

		case UI_IO_TLV_TYPE_FLD_TYPE:
			pb_msg_buff = get_tlv_v_dword(pb_msg_buff, &t_tlv);
			pt_field->e_type = (E_CMD_FIELD_TYPE)t_tlv.val_dword;
			break;

		case UI_IO_TLV_TYPE_FLD_LEN:
			pb_msg_buff = get_tlv_v_dword(pb_msg_buff, &t_tlv);
			pt_field->n_len = t_tlv.val_dword;
			break;

		case UI_IO_TLV_TYPE_FLD_VAL:
			if (pt_field->e_type == CFT_NUM)
			{
				pb_msg_buff = get_tlv_v_dword(pb_msg_buff, &t_tlv);
				pt_field->dw_val = t_tlv.val_dword;
			}
			else // Assume: if (pt_field->e_type == CFT_TXT)
			{
				pb_msg_buff = get_tlv_v_str_n(pb_msg_buff, &t_tlv, pt_field->n_len);
				pt_field->pc_str = t_tlv.val_str;
			}
			break;

		default:
			// Not interesting TLV - just skip
			pb_msg_buff += t_tlv.len;
		}

		// Sanity checks
		if (!pb_msg_buff)
		{ // Something bad happens
			ui_cmd_fields_free(pt_cmd_fields);
			return NULL;
		}

	} // End of FLD TLV loop
    
	// ui_cmd_field_dump(pt_cmd_fields);	// AV TODO: temporary printout 

	return pt_cmd_fields;
}

int io_pipe_rx_proc_ui_cmd (BYTE *pc_ui_init_msg, E_IO_UI_UI_CMD e_ui_cmd)
{
    T_UI_IO_TLV   t_tlv;
	BYTE *pb_msg_buff = pc_ui_init_msg;

    pb_msg_buff = get_tlv_tl(pb_msg_buff, &t_tlv);

    if (e_ui_cmd == IO_UI_UI_CMD_START)
    { // UI init START
        // Get & Parse welcome message

        wcscpy_s(gca_console_title, CONSOLE_TITLE_LEN, (WCHAR*)pb_msg_buff);

        // Init pointer for UI command definitions
        gt_io_ui.pt_curr_cmd = &gta_io_ui_cmd[0];
        io_pipe_tx_str(L"ACK");
    }
    else if (e_ui_cmd == IO_UI_UI_CMD_CDEF)
    { // UI init - Add command definition to cmd list

		pb_msg_buff = get_tlv_v_str(pb_msg_buff, &t_tlv);
        gt_io_ui.pt_curr_cmd->pc_name = t_tlv.val_str;
		gt_io_ui.pt_curr_cmd->pt_fields = io_pipe_rx_proc_ui_cmd_cdef(pb_msg_buff);

		gt_io_ui.pt_curr_cmd++;

        io_pipe_tx_str(L"ACK");
    }
    else if (e_ui_cmd == IO_UI_UI_CMD_END)
    { // UI init END

        io_pipe_tx_str(L"READY");
		gt_flags.io_ui = FL_RISE;
    }

    return TRUE;
}

/***C*F******************************************************************************************
**
** FUNCTION       : io_pipe_rx_proc
** DATE           : 
** AUTHOR         : AV
**
** DESCRIPTION    : Receives IO incoming messages and proceed depends on current mode
**
** PARAMETERS     : 
**
** RETURN-VALUE   : returns TRUE if OK, FALSE otherwise
**
** NOTES          : 
**
***C*F*E***************************************************************************************************/
int io_pipe_rx_proc (int *pn_prompt_restore)
{

    int n_rc, n_gle;
    DWORD   dw_bytes_received;

    n_rc = GetOverlappedResult(
        gh_io_pipe,             //__in  HANDLE hFile,
        &gt_io_pipe_rx_overlap, //__in   LPOVERLAPPED lpOverlapped,
        &dw_bytes_received,     //__out  LPDWORD lpNumberOfBytesTransferred,
        FALSE                   //__in   BOOL bWait
    );

    if (n_rc)
    {
        n_rc = io_pipe_rx_init();
        if(!n_rc) return n_rc;
    }
    else
    {
        n_gle = GetLastError();
        if (n_gle != ERROR_IO_INCOMPLETE){
            gt_flags.io_conn = FL_FALL;
            SetEvent(gha_events[HANDLE_IO_PIPE]);
            wprintf(L"\nError reading IO pipe\n");
            *pn_prompt_restore = TRUE;
            return TRUE;
        }
    }


    { // Determine msg type - UI_INIT, CMD_RESP, ...
        T_UI_IO_TLV     t_tlv;
	    BYTE *pb_msg_buff = (BYTE*)gba_io_pipe_rx_buff;

        pb_msg_buff = get_tlv_tl(pb_msg_buff, &t_tlv);

        if (t_tlv.type == UI_IO_TLV_TYPE_UI_CMD)
        {
            pb_msg_buff = get_tlv_v_dword(pb_msg_buff, &t_tlv);
            io_pipe_rx_proc_ui_cmd(pb_msg_buff, (E_IO_UI_UI_CMD)t_tlv.val_dword);
        }
        else if (t_tlv.type == UI_IO_TLV_TYPE_CMD_RSP)
        {
		    // io_pipe_rx_proc_cmd_rsp()
            wprintf(L"\n%s\n", (WCHAR*)pb_msg_buff);
            pb_msg_buff += t_tlv.len;
            *pn_prompt_restore = TRUE;
        }
        else
        {
            // Something received while not expected!!
            wprintf(L"\nUnexpected message from IO - %s \n", gba_io_pipe_rx_buff);
            *pn_prompt_restore = TRUE;
        }
    
    }

    return TRUE;
}

/***C*F******************************************************************************************
**
** FUNCTION       : io_pipe_connect
** DATE           : 
** AUTHOR         : AV
**
** DESCRIPTION    : IO pipe state machine. Automatically connect/disconects from IO pipe
**
** PARAMETERS     : 
**
** RETURN-VALUE   : returns TRUE if OK, FALSE otherwise
**
** NOTES          : 
**
***C*F*E***************************************************************************************************/

int io_pipe_connect (void)
{
    
    int n_rc, n_gle;
    DWORD   dwMode;

    // If pipe not selected yet, choose one from the list
    // Trying to connect to known named Pipes
    
    for (int n_i = 0; n_i < IO_PIPE_NAME_NUM; n_i ++)
    {
        const WCHAR *pc_try_pipe_name;

        pc_try_pipe_name = (gpc_pipe_name)? gpc_pipe_name : gcaa_pipe_name_list[n_i];
        
        // Try to connect to existing IO pipe
        gh_io_pipe = CreateFile( 
            pc_try_pipe_name,                                       // pipe name 
            GENERIC_READ | GENERIC_WRITE | FILE_WRITE_ATTRIBUTES,   // read and write access 
            0,                                                      // no sharing 
            NULL,                                                   // default security attributes
            OPEN_EXISTING,                                          // opens existing pipe 
            FILE_FLAG_OVERLAPPED,                                   //attributes 
            NULL);                                                  // no template file 

        if (gh_io_pipe != INVALID_HANDLE_VALUE)
        {
            gpc_pipe_name = pc_try_pipe_name;
        }

        // Once Pipe name selected - stick on it
        if (gpc_pipe_name)
        {
            break;
        }

    }


    if (gh_io_pipe == INVALID_HANDLE_VALUE)
    {
        n_gle = GetLastError();
        n_rc = 0;
        goto io_pipe_connect_cleanup;
    }
        

    // The pipe connected; change to message-read mode. 
    dwMode = PIPE_READMODE_MESSAGE; 
    n_rc = SetNamedPipeHandleState( 
        gh_io_pipe,       // pipe handle 
        &dwMode,          // new pipe mode 
        NULL,             // don't set maximum bytes 
        NULL);            // don't set maximum time 

    if (!n_rc)
    {
        // Critical error! terminate application
        wprintf(L"SetNamedPipeHandleState failed. GLE=%d\n", GetLastError());
        gt_flags.exit = FL_SET; // Critical error. Terminate application
        return FALSE;
    }

io_pipe_connect_cleanup:

    // Log activity only if IO pipe connection status changed
    if (n_rc)
    {
        if (gt_flags.io_conn != FL_SET)
        {
            gt_flags.io_conn = FL_RISE;
            SetEvent(gha_events[HANDLE_IO_PIPE]);
        }
    }
    else
    {
        if (gt_flags.io_conn != FL_CLR)
        {
            gt_flags.io_conn = FL_FALL;
            SetEvent(gha_events[HANDLE_IO_PIPE]);
        }
    }
    
    return TRUE;

}

int io_pipe_check (int *pn_prompt_restore)
{

    int n_rc, n_gle;

    // Try to connect to IO pipe periodically if not connected yet 
    if (gt_flags.io_conn == FL_CLR)
    {
        n_rc = io_pipe_connect();
        if (!n_rc)
        {
            n_gle = GetLastError();
            wprintf(L"Can't open IO pipe %s. Error %d @ %d", gpc_pipe_name, n_gle, __LINE__);
            *pn_prompt_restore = TRUE;
            return FALSE;   // Critical error
        }
    }

    // IO pipe just connected
    if (gt_flags.io_conn == FL_RISE)
    {
        // IO pipe connected. Try to initiate very first read transaction
        n_rc = io_pipe_rx_init();
        if (n_rc)
        {
            wprintf(L"IO pipe %s connected. Sending INIT request...\n", gpc_pipe_name);
            *pn_prompt_restore = TRUE;
            gt_flags.io_conn = FL_SET;

            swprintf(gca_console_title, CONSOLE_TITLE_LEN, L"%s - Connected.", gpc_pipe_name);
            SetConsoleTitle(gca_console_title);

            // Send request for UI initialization
            io_pipe_tx_str(L"INIT");
        }
        else
        {
            n_gle = GetLastError();
            wprintf(L"Can't read from IO pipe. Error %d @ %d", gpc_pipe_name, n_gle, __LINE__);
            gt_flags.io_conn = FL_FALL;
        }
    }

    // Somethign wrong with IO pipe
    if (gt_flags.io_conn == FL_FALL)
    {
        wprintf(L"IO pipe disconnected\n");
        *pn_prompt_restore = TRUE;
        gt_flags.io_conn = FL_CLR;
        CloseHandle(gh_io_pipe);

        swprintf(gca_console_title, CONSOLE_TITLE_LEN, L"%s - Disconnected.", gpc_pipe_name);
        SetConsoleTitle(gca_console_title);

        // Reset IO_UI 
        gt_flags.io_ui = FL_FALL; 
    }

    if (gt_flags.io_conn == FL_SET)
    {
        HANDLE h_temp;

        // Try to connect to IO pipe again. Just ot check it is alive.
        // If everything is OK, CreateFiles have to return ERROR_PIPE_BUSY
        h_temp = CreateFile( 
            gpc_pipe_name,                                          // pipe name 
            GENERIC_READ | GENERIC_WRITE | FILE_WRITE_ATTRIBUTES,   // read and write access 
            0,                                                      // no sharing 
            NULL,                                                   // default security attributes
            OPEN_EXISTING,                                          // opens existing pipe 
            FILE_FLAG_OVERLAPPED,                                   // attributes 
            NULL);                                                  // no template file 

        if (h_temp == INVALID_HANDLE_VALUE) 
        {
            n_gle = GetLastError();
            if (n_gle == ERROR_PIPE_BUSY)
            {
                return TRUE;
            }
        }

        gt_flags.io_conn = FL_FALL;
    }

    return TRUE;

}

/***C*F******************************************************************************************
**
** FUNCTION       : CtrlHandler
** DATE           : 01/20/2011
** AUTHOR         : AV
**
** DESCRIPTION    : Program termination hook
**
** PARAMETERS     : 
**
** RETURN-VALUE   : 
**
** NOTES          : 
**
***C*F*E***************************************************************************************************/
BOOL CtrlHandler( DWORD fdwCtrlType ){ 

    wprintf(L"\nUse 'quit' or just 'q' to save command history");
    wprintf(L"\nHave a nice day!\n");

    return FALSE;
} 

/***C*F******************************************************************************************
**
** FUNCTION       : wmain
** DATE           : 
** AUTHOR         : AV
**
** DESCRIPTION    : 
**
** PARAMETERS     : 
**
** RETURN-VALUE   : 
**
** NOTES          : 
**
***C*F*E***************************************************************************************************/
void wmain(int argc, WCHAR *argv[]){

    int n_arg_num;

    WCHAR   *pc_cmd;
    int     n_rc, n_evt;
    int     n_prompt_restore;
    void    *pv_cmd_proc_info;

    // Default options
    gn_action = CMD_LINE_MODE;
    gt_flags.io_auto_start = FL_CLR;
    gt_flags.io_conn = FL_CLR;
    gt_flags.io_ui   = FL_CLR;
    gt_flags.exit    = FL_CLR;

    n_arg_num = get_command_line_args(argc, argv, 1);
 
    if (gn_action == SHOW_OPTIONS_HELP){
        show_options_help();
        return;
    }

    // Creat pipe IO connection event
    gha_events[HANDLE_IO_PIPE] = CreateEvent( 
         NULL,    // default security attribute 
         FALSE,   // manual-reset event 
         FALSE,   // initial state
         NULL);   // unnamed event object 

    // Creat command RX event
    gha_events[HANDLE_IO_PIPE_RX] = CreateEvent( 
         NULL,    // default security attribute 
         FALSE,   // manual-reset event 
         FALSE,   // initial state
         NULL);   // unnamed event object 

    gha_events[HANDLE_KEYBOARD] = GetStdHandle(STD_INPUT_HANDLE);

    gt_io_pipe_overlap.hEvent   = gha_events[HANDLE_IO_PIPE];
    gt_io_pipe_rx_overlap.hEvent     = gha_events[HANDLE_IO_PIPE_RX];

    swprintf(gca_console_title, CONSOLE_TITLE_LEN, L"Not connected");
    SetConsoleTitle(gca_console_title);

    wprintf(L"                       \n");
    wprintf(L" ----------------------\n");
    wprintf(L" --- DBG UI console ---\n");
    wprintf(L" ----------------------\n");
    wprintf(L"                       \n");

    // Catch console control events
    SetConsoleCtrlHandler( (PHANDLER_ROUTINE) CtrlHandler, TRUE );

    // Init command input processor
    // AV TODO: create cmd_history by pipe server name
    pv_cmd_proc_info = init_cmd_proc(L"cmd_hist.dat");
    if (pv_cmd_proc_info == NULL)
        return;

    // Show initial prompt
    n_prompt_restore = TRUE;

    // Initial "magic punch" for IO connection
    SetEvent(gha_events[HANDLE_IO_PIPE]);

    // --------------------------
    // --- Main wait loop
    // --------------------------
    while(gt_flags.exit != FL_SET)
    {

        n_evt = WaitForMultipleObjects(
            HANDLES_NUM,                 // __in  DWORD nCount,
            gha_events,                  // __in  const HANDLE *lpHandles,
            FALSE,                       // __in  BOOL bWaitAll,
            100                          // __in  DWORD dwMilliseconds
        );

        if (n_evt == WAIT_TIMEOUT)
        { 
            // If nothing to do and IO is disconnected, then try to connect again
            if (gt_flags.io_conn == FL_CLR)
            {
                n_rc = io_pipe_check(&n_prompt_restore);
                if (!n_rc) return;
            }

			// Check other background task
			// ...
			ui_check(&n_prompt_restore);

        }
        else if (n_evt == WAIT_OBJECT_0 + HANDLE_IO_PIPE)
        {
            n_rc = io_pipe_check(&n_prompt_restore);
            if (!n_rc) return;
        }
        else if (n_evt == WAIT_OBJECT_0 + HANDLE_IO_PIPE_RX)
        {
            n_rc = io_pipe_rx_proc(&n_prompt_restore);
            if (!n_rc) return;
        }
        else if (n_evt == WAIT_OBJECT_0 + HANDLE_KEYBOARD)
        {

            pc_cmd = cmd_keys_pressed(pv_cmd_proc_info, &n_prompt_restore, gpt_io_ui_cmd);

            if (pc_cmd)
            {
                n_rc = ui_cmd_proc(pc_cmd);
                if (!n_rc) break;
            }
        }
        else
        {
            wprintf(L"?\n");
        }

        if (n_prompt_restore)
        {
            cmd_proc_prompt(pv_cmd_proc_info);
            n_prompt_restore = FALSE;
        }
    } // End of While forever

    close_cmd_proc(pv_cmd_proc_info);

    return;
}


#if 0
int start_io(){

    if (gt_flags.io_auto_start == FL_SET)
    { // Start IO process and pass outgoing pipe as STDIN
        WCHAR ca_cmd_line[1024];
        PROCESS_INFORMATION piProcInfo; 
        STARTUPINFO siStartInfo;
        BOOL bSuccess = FALSE; 
 
        // Set up members of the PROCESS_INFORMATION structure. 
        ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );
 
        // Set up members of the STARTUPINFO structure. 
        // This structure specifies the STDIN and STDOUT handles for redirection.
 
        ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
        siStartInfo.cb = sizeof(STARTUPINFO); 

        wsprintf(ca_cmd_line, L"btcar-dbg-io-proc.exe %s", gpc_pipe_name);
        // Create the child process. 
        bSuccess = CreateProcess(
            NULL, 
            ca_cmd_line,            // command line 
            NULL,                   // process security attributes 
            NULL,                   // primary thread security attributes 
            FALSE,                  // handles are inherited 
            CREATE_NEW_CONSOLE,     // creation flags 
            NULL,                   // use parent's environment 
            NULL,                   // use parent's current directory 
            &siStartInfo,           // STARTUPINFO pointer 
            &piProcInfo);           // receives PROCESS_INFORMATION 
   
        // If an error occurs, exit the application. 
        if ( !bSuccess ){
            wprintf(L"Can't create IO process. Error @ %d", __LINE__);
            return FALSE;
        }
    }


    return TRUE;
}
#endif 


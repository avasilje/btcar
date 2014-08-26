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
#include "cmd_proc.h"
#include "cmd_proc_inp.h"

// --- Internal function ---------------------------------------------
int start_io();
int send_single_command(int argc, WCHAR *argv[], int n_arg_num);
int proceed_cmd(WCHAR *pc_cmd);
int get_command_line_args(int argc, WCHAR *argv[], int n_arg_num);
int check_io_pipe_connection(int *pn_prompt_restore);
int proceed_rx_msg(int *pn_prompt_restore);
BOOL CtrlHandler( DWORD fdwCtrlType );


// --- Synchronization vars  -----------------------------------------
HANDLE gh_io_pipe;
HANDLE gha_events[HANDLES_NUM];
OVERLAPPED gt_io_pipe_overlap;
OVERLAPPED gt_io_rx_overlap;
OVERLAPPED gt_io_tx_overlap;

// --- Other globals -------------------------------------------------
CMD_PROC_FLAGS gt_flags;
int gn_action;
WCHAR gca_cmd_io_resp[IO_RX_MSG_LEN];

const WCHAR gca_pipe_name[] = L"\\\\.\\pipe\\btcar_io_ui";     // AV TODO: rework to input argument

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
    gha_events[HANDLE_RX_IO_MSG] = CreateEvent( 
         NULL,    // default security attribute 
         FALSE,   // manual-reset event 
         FALSE,   // initial state
         NULL);   // unnamed event object 

    // Creat command TX event
    gha_events[HANDLE_TX_IO_MSG] = CreateEvent( 
         NULL,    // default security attribute 
         FALSE,   // manual-reset event 
         FALSE,   // initial state
         NULL);   // unnamed event object 

    gha_events[HANDLE_KEYBOARD] = GetStdHandle(STD_INPUT_HANDLE);

    gt_io_pipe_overlap.hEvent   = gha_events[HANDLE_IO_PIPE];
    gt_io_rx_overlap.hEvent     = gha_events[HANDLE_RX_IO_MSG];
    gt_io_tx_overlap.hEvent     = gha_events[HANDLE_TX_IO_MSG];

    wprintf(L"                       \n");
    wprintf(L" ----------------------\n");
    wprintf(L" --- BTCAR DBG tool ---\n");
    wprintf(L" ----------------------\n");
    wprintf(L"                       \n");

    // --------------------------
    // --- Start BTCAR IO module 
    // --------------------------
    if (!start_io()) return;
    
    if (gn_action == SEND_COMMAND){
        send_single_command(argc, argv, n_arg_num);
        return;
    }

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
                n_rc = check_io_pipe_connection(&n_prompt_restore);
                if (!n_rc) return;
            }

            continue;
        }
        else if (n_evt == WAIT_OBJECT_0 + HANDLE_IO_PIPE)
        {
            n_rc = check_io_pipe_connection(&n_prompt_restore);
            if (!n_rc) return;
        }
        else if (n_evt == WAIT_OBJECT_0 + HANDLE_RX_IO_MSG)
        {
            n_rc = proceed_rx_msg(&n_prompt_restore);
            if (!n_rc) return;
        }
        else if (n_evt == WAIT_OBJECT_0 + HANDLE_TX_IO_MSG)
        {
            // Check TX message complete
            // ...
        }
        else if (n_evt == WAIT_OBJECT_0 + HANDLE_KEYBOARD)
        {
            pc_cmd = cmd_keys_pressed(pv_cmd_proc_info, &n_prompt_restore);

            if (pc_cmd)
            {
                n_rc = proceed_cmd(pc_cmd);
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
/***C*F******************************************************************************************
**
** FUNCTION       : send_single_command
** DATE           : 01/20/2011
** AUTHOR         : AV
**
** DESCRIPTION    : Send single command from command line and exit
**
** PARAMETERS     : 
**
** RETURN-VALUE   : 
**
** NOTES          : Not tested. Some problems with IO pipe expected
**
***C*F*E*************************************************************************************************/
int send_single_command(int argc, WCHAR *argv[], int n_arg_num){

    WCHAR    ca_cmd_line[CMD_LINE_LENGTH];
    WCHAR    *pc_cmd;

    pc_cmd = ca_cmd_line;
    pc_cmd[0] = 0;

    // combine all remaining command line tokens to one string
    while(n_arg_num < argc ){
        wcsncat_s(pc_cmd, wcs_sizeof(ca_cmd_line), argv[n_arg_num], wcs_sizeof(ca_cmd_line));
        n_arg_num ++;    
    }

    return proceed_cmd(pc_cmd);
}
/***C*F******************************************************************************************
**
** FUNCTION       : init_rx_msg_read
** DATE           : 01/20/2011
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
int init_rx_msg_read(){

    int n_rc, n_gle;
    DWORD dw_n;

    n_rc = ReadFile(
        gh_io_pipe,                 //__in         HANDLE hFile,
        gca_cmd_io_resp,            //__out        LPVOID lpBuffer,
        IO_RX_MSG_LEN,              //__in         DWORD nNumberOfBytesToRead,
        &dw_n,                      //__out_opt    LPDWORD lpNumberOfBytesRead,
        &gt_io_rx_overlap           //__inout_opt  LPOVERLAPPED lpOverlapped
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

int send_to_io_pipe(WCHAR *p_io_msg)
{
    int n_rc, n_gle;
    DWORD dw_n;
    
    // Send command 
    n_rc = WriteFile(
        gh_io_pipe,                     //__in         HANDLE hFile,
        p_io_msg,                       //__in         LPCVOID lpBuffer,
        (int)wcslen(p_io_msg) << 1,     //__in         DWORD nNumberOfBytesToWrite,
        &dw_n,                          //__out_opt    LPDWORD lpNumberOfBytesWritten,
        &gt_io_tx_overlap               //__inout_opt  LPOVERLAPPED lpOverlapped
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

} // End of TX MSG block

int proceed_ui_msg(WCHAR *pc_ui_cmd)
{
    
    if (pc_ui_cmd[0] == 'S')
    { // UI init START
        send_to_io_pipe(L"ACK");
    }
    else if (pc_ui_cmd[0] == 'C')
    { // UI init CMD. Add command to cmd list
        send_to_io_pipe(L"ACK");
    }
    else if (pc_ui_cmd[0] == 'E')
    { // UI init END
        send_to_io_pipe(L"READY");
    }

    return TRUE;
}

/***C*F******************************************************************************************
**
** FUNCTION       : proceed_rx_msg
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
int proceed_rx_msg(int *pn_prompt_restore){

    int n_rc, n_gle;
    DWORD   dw_bytes_received;

    n_rc = GetOverlappedResult(
        gh_io_pipe,             // __in  HANDLE hFile,
        &gt_io_rx_overlap,      //__in   LPOVERLAPPED lpOverlapped,
        &dw_bytes_received,     //__out  LPDWORD lpNumberOfBytesTransferred,
        FALSE                   //__in   BOOL bWait
    );

    if (n_rc)
    {
        n_rc = init_rx_msg_read();
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

    // Handle incoming message depend on IO UI state
    if (gt_flags.io_ui == FL_CLR)
    {
        wprintf(L"\n UI init msg: %s\n", gca_cmd_io_resp);
        *pn_prompt_restore = TRUE;

        proceed_ui_msg(gca_cmd_io_resp);

    }
    else if (gt_flags.io_ui == FL_SET)
    {
        wprintf(L"\n%s\n", gca_cmd_io_resp);
        *pn_prompt_restore = TRUE;
    }
    else{
        // Something received while not expected!!
        wprintf(L"\nUnexpected message from IO - %s \n", gca_cmd_io_resp);
        *pn_prompt_restore = TRUE;
    }

    return TRUE;
}

/***C*F******************************************************************************************
**
** FUNCTION       : io_connect
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

int io_connect(){
    
    int n_rc, n_gle;
    DWORD   dwMode;

    // Try to connect to existing IO pipe
    gh_io_pipe = CreateFile( 
        gca_pipe_name,                                          // pipe name 
        GENERIC_READ | GENERIC_WRITE | FILE_WRITE_ATTRIBUTES,   // read and write access 
        0,                                                      // no sharing 
        NULL,                                                   // default security attributes
        OPEN_EXISTING,                                          // opens existing pipe 
        FILE_FLAG_OVERLAPPED,                                   //attributes 
        NULL);                                                  // no template file 

        
    if (gh_io_pipe == INVALID_HANDLE_VALUE){
        n_gle = GetLastError();
        n_rc = 0;
        goto io_connect_cleanup;
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

io_connect_cleanup:

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

int check_io_pipe_connection(int *pn_prompt_restore){

    int n_rc, n_gle;

    // Try to connect to IO pipe periodically if not connected yet 
    if (gt_flags.io_conn == FL_CLR)
    {
        n_rc = io_connect();
        if (!n_rc)
        {
            n_gle = GetLastError();
            wprintf(L"Can't open IO pipe %s. Error %d @ %d", gca_pipe_name, n_gle, __LINE__);
            *pn_prompt_restore = TRUE;
            return FALSE;   // Critical error
        }
    }

    // IO pipe just connected
    if (gt_flags.io_conn == FL_RISE)
    {
        // IO pipe connected. Try to initiate very first read transaction
        n_rc = init_rx_msg_read();
        if (n_rc)
        {
            wprintf(L"IO pipe connected. Sending INIT request...\n");
            *pn_prompt_restore = TRUE;
            gt_flags.io_conn = FL_SET;

            // Send request for UI initialization
            send_to_io_pipe(L"INIT");
        }
        else
        {
            n_gle = GetLastError();
            wprintf(L"Can't read from IO pipe. Error %d @ %d", gca_pipe_name, n_gle, __LINE__);
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

        // Reset IO_UI 
        gt_flags.io_ui = FL_FALL; 
    }

    if (gt_flags.io_conn == FL_SET)
    {
        HANDLE h_temp;

        // Try to connect to IO pipe again. Just ot check it is alive.
        // If everything is OK, CreateFiles have to return ERROR_PIPE_BUSY
        h_temp = CreateFile( 
            gca_pipe_name,                                          // pipe name 
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
** FUNCTION       : proceed_cmd
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
int proceed_cmd(WCHAR *pc_cmd_arg)
{

    T_CP_CMD   *pt_curr_cmd;

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
        show_cmd_help();
        return TRUE;
    }

    // command processor starts here
    pt_curr_cmd = decomposite_cp_cmd(pc_cmd_arg, gta_cmd_lib, FALSE);
    
    if (pt_curr_cmd == NULL)
    { // Command not found or error in parameters
        return TRUE;
    }
    
    send_to_io_pipe(pc_cmd_arg);

    return TRUE;
}
/***C*F******************************************************************************************
**
** FUNCTION       : start_io
** DATE           : 01/20/2011
** AUTHOR         : AV
**
** DESCRIPTION    : Create DLE IO socket and start up DLE IO process
**
** PARAMETERS     : 
**
** RETURN-VALUE   : returns TRUE if OK, FALSE otherwise
**
** NOTES          : Pipe connction performed in pipe's FSM. see check_io_pipe_connection()
**
***C*F*E***************************************************************************************************/
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

        wsprintf(ca_cmd_line, L"btcar-dbg-io-proc.exe %s", gca_pipe_name);
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

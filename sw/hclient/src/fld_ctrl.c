#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <wtypes.h>
#include <math.h>
#include <assert.h>
#include <dbt.h>
#include <Shellapi.h>
#include <fcntl.h>
#include <io.h>
#include "hidsdi.h"
#include "hid.h"
#include "resource.h"
#include "hclient.h"
#include "buffers.h"
#include "ecdisp.h"
#include "list.h"
#include <strsafe.h>

#include "FTD2XX.H"

#define SMALL_BUFF 128
static CHAR szTempBuff[SMALL_BUFF];

#define HANDLE_TIMEOUT_TMR  0
#define HANDLE_REPEAT_TMR   1
#define HANDLE_LAST         2

HANDLE gha_events[HANDLE_LAST];
CHAR    x_coord_str[256];
CHAR    x_servo_str[256];

HANDLE hTimer;
HWND hFldCtrl;
HWND hCoord;
HWND hDlg_main;
DWORD last_servo_send;

UINT_PTR my_tmr;

POINT curs;
POINT curs_norm;
RECT fld_ctrl_r;

HID_DEVICE BT_writeDevice;

int     timer_tck;

FT_HANDLE gh_dev;
OVERLAPPED gt_dev_rx_overlapped = { 0 };
OVERLAPPED gt_dev_tx_overlapped = { 0 };

char    devDescriptor[64];
char    devSerial[64];

DWORD gdw_dev_bytes_rcv;
BYTE guca_dev_resp[1024];

int gn_dev_index = 0;
int gn_dev_resp_timeout;

#define RESP_STR_LEN 1024
WCHAR gca_dev_resp_str[RESP_STR_LEN];



int dev_open_uart (int n_dev_indx, FT_HANDLE *ph_device)
{

    FT_STATUS ft_status;
    DWORD   dw_num_devs;
    LONG    devLocation;


    ft_status = FT_ListDevices(&dw_num_devs, NULL, FT_LIST_NUMBER_ONLY);
    if (ft_status != FT_OK) return FALSE;

    if (dw_num_devs == 0){
        // No devices were found
        return FALSE;
    }

    ft_status = FT_ListDevices((void*)n_dev_indx, &devLocation, FT_LIST_BY_INDEX | FT_OPEN_BY_LOCATION);
    if (ft_status != FT_OK) {
        return FALSE;
    }

    ft_status |= FT_ListDevices((void*)n_dev_indx, &devDescriptor, FT_LIST_BY_INDEX | FT_OPEN_BY_DESCRIPTION);
    ft_status |= FT_ListDevices((void*)n_dev_indx, &devSerial, FT_LIST_BY_INDEX | FT_OPEN_BY_SERIAL_NUMBER);

    if (ft_status != FT_OK){
        return FALSE;
    }
#define FT_Classic  0

#if FT_Classic
    ft_status |= FT_OpenEx((void*)devLocation, FT_OPEN_BY_LOCATION, ph_device);

    ft_status |= FT_SetTimeouts(*ph_device, 500, 500);
    ft_status |= FT_SetLatencyTimer(*ph_device, 2);

    // Divisor selection
    // BAUD = 3000000 / Divisor
    // Divisor = (N + 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875)
    // Divisor = 24 ==> Baud 125000 
    ft_status |= FT_SetDivisor(*ph_device, 3000000 / 125000);

    // Set UART format 8N1
    ft_status |= FT_SetDataCharacteristics(*ph_device, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE);

    if (ft_status != FT_OK){
        return FALSE;
    }


    // Just in case
    FT_Purge(*ph_device, FT_PURGE_TX | FT_PURGE_RX);
#else


    // Open a device for overlapped I/O using its serial number
    *ph_device = FT_W32_CreateFile(
        (LPCTSTR)devLocation,
        GENERIC_READ | GENERIC_WRITE,
        0,
        0,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED | FT_OPEN_BY_LOCATION,
        0);

    if (*ph_device == INVALID_HANDLE_VALUE)
    {
        // FT_W32_CreateDevice failed
        return FALSE;
    }

    // ----------------------------------------
    // --- Set comm parameters
    // ----------------------------------------

    FTDCB ftDCB;
    FTTIMEOUTS    ftTimeouts;
    FTCOMSTAT    ftPortStatus;
    DWORD   dw_port_error;

    if (!FT_W32_GetCommState(*ph_device, &ftDCB))
    {
        return FALSE;
    }

    // Divisor selection
    // BAUD = 3000000 / Divisor
    // Divisor = (N + 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875)
    // Divisor = 24 ==> Baud 125000 

    ftDCB.BaudRate = 38400;
    ftDCB.fBinary = TRUE;                       /* Binary Mode (skip EOF check)    */
    ftDCB.fParity = FALSE;                      /* Enable parity checking          */
    ftDCB.fOutxCtsFlow = FALSE;                 /* CTS handshaking on output       */
    ftDCB.fOutxDsrFlow = FALSE;                 /* DSR handshaking on output       */
    ftDCB.fDtrControl = DTR_CONTROL_DISABLE;    /* DTR Flow control                */
    ftDCB.fTXContinueOnXoff = FALSE;

    ftDCB.fErrorChar = FALSE;            // enable error replacement 
    ftDCB.fNull = FALSE;                // enable null stripping 
    ftDCB.fRtsControl = RTS_CONTROL_DISABLE;       // RTS flow control 
    ftDCB.fAbortOnError = TRUE;            // abort reads/writes on error 

    ftDCB.fOutX = FALSE;                        /* Enable output X-ON/X-OFF        */
    ftDCB.fInX = FALSE;                         /* Enable input X-ON/X-OFF         */
    ftDCB.fNull = FALSE;                        /* Enable Null stripping           */
    ftDCB.fRtsControl = RTS_CONTROL_DISABLE;    /* Rts Flow control                */
    ftDCB.fAbortOnError = TRUE;                 /* Abort all reads and writes on Error */

    // 8N1
    ftDCB.ByteSize = 8;                 /* Number of bits/byte, 4-8        */
    ftDCB.Parity = NOPARITY;            /* 0-4=None,Odd,Even,Mark,Space    */
    ftDCB.StopBits = ONESTOPBIT;        /* 0,1,2 = 1, 1.5, 2               */

    if (!FT_W32_SetCommState(*ph_device, &ftDCB))
    {
        return FALSE;
    }

    FT_W32_GetCommState(*ph_device, &ftDCB);

    // Set serial port Timeout values
    FT_W32_GetCommTimeouts(*ph_device, &ftTimeouts);

    ftTimeouts.ReadIntervalTimeout = 0;
    ftTimeouts.ReadTotalTimeoutMultiplier = 0;
    ftTimeouts.ReadTotalTimeoutConstant = 200;
    ftTimeouts.WriteTotalTimeoutConstant = 0;
    ftTimeouts.WriteTotalTimeoutMultiplier = 0;

    FT_W32_SetCommTimeouts(*ph_device, &ftTimeouts);

    FT_W32_ClearCommError(*ph_device, &dw_port_error, &ftPortStatus);
    FT_W32_PurgeComm(*ph_device, PURGE_TXCLEAR | PURGE_RXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

#endif  // End of W32 device init

    return TRUE;
}


void send_over_BT(BYTE servo_acc, BYTE servo_whl)
{
    BOOL WriteStatus, Status;
    BYTE data[3];
    DWORD bytesWritten;

    BT_writeDevice.OutputReportBuffer[0] = 3;   // Report ID ???
    BT_writeDevice.OutputReportBuffer[1] = 0x17; // Stream - 0x17 servo control
    BT_writeDevice.OutputReportBuffer[2] = (BYTE)servo_acc;
    BT_writeDevice.OutputReportBuffer[3] = (BYTE)servo_whl;

    Status = WriteFile (BT_writeDevice.HidDevice,
                        BT_writeDevice.OutputReportBuffer,
                        BT_writeDevice.Caps.OutputReportByteLength,
                        &bytesWritten,
                        NULL);

     WriteStatus = (bytesWritten == BT_writeDevice.Caps.OutputReportByteLength);
     Status = Status && WriteStatus;                         

     return;
}

void send_over_ftdi(BYTE servo_acc, BYTE servo_whl)
{
    int n_rc;
    BYTE data[3];
    DWORD dw_byte_to_write, dw_bytes_written;
    DWORD n_gle;

    data[0] = 0x17; // Stream - 0x17 servo control
    data[1] = (BYTE)servo_acc;
    data[2] = (BYTE)servo_whl;

    dw_byte_to_write = sizeof(data);

    n_rc = FT_W32_WriteFile(
        gh_dev,
        data, 
        dw_byte_to_write,
        &dw_bytes_written, 
        &gt_dev_tx_overlapped);


    if (!n_rc)
    {
        n_gle = FT_W32_GetLastError(gh_dev);
        if (n_gle != ERROR_IO_PENDING)
        {
            n_rc = FALSE;
            // goto cleanup_dev_tx;
        }
    }

    n_rc = FT_W32_GetOverlappedResult(gh_dev, &gt_dev_tx_overlapped, &dw_bytes_written, TRUE);
    if (!n_rc || (dw_byte_to_write != dw_bytes_written))
    {
        n_rc = FALSE;
    }

}

void send_servo()
{

    HWND hServo = GetDlgItem(hDlg_main, IDC_FLD_FOCUS);
    int servo_acc, servo_whl;

    servo_acc = (255 * curs.x + 0.5) / (fld_ctrl_r.right - fld_ctrl_r.left);
    servo_whl = 255 - (255 * curs.y + 0.5) / (fld_ctrl_r.bottom - fld_ctrl_r.top);

    if (servo_whl > 255) servo_whl = 255;
    if (servo_acc > 255) servo_acc = 255;

    if (servo_whl < 0) servo_whl = 0;
    if (servo_acc < 0) servo_acc = 0;
        
    if (timer_tck)
    {
        sprintf(x_servo_str, "servo: % 3d, % 3d; [+]", servo_acc, servo_whl);
    }
    else
    {
        sprintf(x_servo_str, "servo: % 3d, % 3d; [O]", servo_acc, servo_whl);
    }
    timer_tck = 1 - timer_tck;

    SetWindowText(hServo, x_servo_str);

    if (gh_dev)
    {
        send_over_ftdi(servo_acc, servo_whl);
     }
    else if (BT_writeDevice.HidDevice)
    {
        send_over_BT(servo_acc, servo_whl);
    }

    last_servo_send = GetTickCount();

    return;

}



void WM_INITDIALOG_hook (
    HWND hDlg, 
    UINT message, 
    WPARAM wParam, 
    LPARAM lParam)
{

    timer_tck = 0;
//    dev_open_uart (0, &gh_dev);

    hDlg_main = hDlg;

    HWND hUART_DEV = GetDlgItem(hDlg_main, IDC_UART_DEVICE);
    SetWindowText(hUART_DEV, devDescriptor);

}


void WM_COMMAND_hook (
    HWND hDlg, 
    UINT message, 
    WPARAM wParam, 
    LPARAM lParam)
{

    PHID_DEVICE                      pDevice;

    BOOL            status;

    switch (LOWORD(wParam))
    {

        case IDC_FLD_CTRL_DEV:
        {
            if (BT_writeDevice.HidDevice != 0)
            {
                HWND hUART_DEV = GetDlgItem(hDlg_main, IDC_UART_DEVICE);
                SetWindowText(hUART_DEV, "");

                HWND hBUTT = GetDlgItem(hDlg_main, IDC_FLD_CTRL_DEV);
                SetWindowText(hBUTT, "DEV OPEN");

                CloseHidDevice(&BT_writeDevice);

                EnableWindow(GetDlgItem(hDlg, IDC_DEVICES), 1);

                // Set device change message to enable other controls
                PostMessage(hDlg,
                            WM_COMMAND,
                            IDC_DEVICES + (CBN_SELCHANGE<<16),
                            (LPARAM) GetDlgItem(hDlg,IDC_DEVICES));

                ZeroMemory(&BT_writeDevice, sizeof(BT_writeDevice));
            }
            else
            {
                INT iIndex;

                pDevice = NULL;
                iIndex = (INT) SendDlgItemMessage(hDlg,
                                                  IDC_DEVICES,
                                                  CB_GETCURSEL,
                                                  0,
                                                  0);
                if (CB_ERR != iIndex)
                { 
                    pDevice = (PHID_DEVICE) SendDlgItemMessage(hDlg,
                                                               IDC_DEVICES,
                                                               CB_GETITEMDATA,
                                                               iIndex,
                                                               0);
                }

                if (NULL == pDevice) 
                    break;

                status = OpenHidDevice(pDevice -> DevicePath, 
                                        FALSE,
                                        TRUE,
                                        FALSE,
                                        FALSE,
                                        &BT_writeDevice);
                                            
                if (!status) 
                {
                    MessageBox(hDlg,
                                "Couldn't open device for write access",
                                HCLIENT_ERROR,
                                MB_ICONEXCLAMATION);
            
                    ZeroMemory(&BT_writeDevice, sizeof(BT_writeDevice));

                }

                // Disable all other controls
                EnableWindow(GetDlgItem(hDlg, IDC_EXTCALLS), 0);
                EnableWindow(GetDlgItem(hDlg, IDC_READ), 0);   
                EnableWindow(GetDlgItem(hDlg, IDC_WRITE), 0); 
                EnableWindow(GetDlgItem(hDlg, IDC_FEATURES), 0);
                EnableWindow(GetDlgItem(hDlg, IDC_DEVICES), 0);

                StringCbPrintf(szTempBuff,
                               SMALL_BUFF,
                               "VID: 0x%04X  PID: 0x%04X  UsagePage: 0x%X  Usage: 0x%X",
                                BT_writeDevice.Attributes.VendorID,
                                BT_writeDevice.Attributes.ProductID,
                                BT_writeDevice.Caps.UsagePage,
                                BT_writeDevice.Caps.Usage);

                HWND hUART_DEV = GetDlgItem(hDlg_main, IDC_UART_DEVICE);
                SetWindowText(hUART_DEV, szTempBuff);

                HWND hBUTT = GetDlgItem(hDlg_main, IDC_FLD_CTRL_DEV);
                SetWindowText(hBUTT, "DEV CLOSE");

            } // End of Device open 
                       
            break;
        } // End of FLD_CTRL_DEV case

    } // End of wParam switch

    return;
}

/*

            if (bParseData(writeDevice.OutputData, rWriteData, iCount, &iErrorLine))
            {
                Write(&writeDevice);
            }
                            CloseHidDevice(&writeDevice);
                break; //end case IDC_WRITE//
            }

*/

void WM_MOUSEMOUVE_hook (
    HWND hDlg, 
    UINT message, 
    WPARAM wParam, 
    LPARAM lParam)
{

    switch (message) 
    { 
        case WM_MOUSEMOVE: 
 
            if (0 == (wParam & MK_LBUTTON))
            { 
                break;
            }

            int xPos = GET_X_LPARAM(lParam); 
            int yPos = GET_Y_LPARAM(lParam); 

            hFldCtrl = GetDlgItem(hDlg_main, IDC_FLD_CTRL);
            hCoord = GetDlgItem(hDlg_main, IDC_COORD);

            GetWindowRect(hFldCtrl, &fld_ctrl_r); //get window rect of control relative to screen

            curs.x = xPos;
            curs.y = yPos;
             
//            ScreenToClient(hwndDlg, &pt); //convert screen co-ords to client based points
            {
                int rc;
                rc = MapWindowPoints(
                    hDlg,
                    hFldCtrl,
                    &curs,
                    1);
            }

            if ( (curs.x < 0) || (curs.y < 0) ||
                 (curs.x > (fld_ctrl_r.right - fld_ctrl_r.left)) ||
                 (curs.y > (fld_ctrl_r.bottom - fld_ctrl_r.top)) )
            { 
                SetWindowText(hCoord, "OutOfScope");
            }
            else 
            {
                DWORD curr_tick;
                curs_norm.x = curs.x - ((fld_ctrl_r.right - fld_ctrl_r.left) >> 1);
                curs_norm.y = -(curs.y - ((fld_ctrl_r.bottom - fld_ctrl_r.top) >> 1));

                sprintf(x_coord_str, "abs: % 3d, % 3d; "
                                     "rel: % 3d, % 3d",
                                     curs.x, curs.y,
                                     curs_norm.x, curs_norm.y);

                SetWindowText(hCoord, x_coord_str);
                
                curr_tick = GetTickCount();
                if ((curr_tick - last_servo_send) > 40)
                {
                    send_servo();
                }

/*
                SetTimer(hDlg_main,        // handle to main window 
                    IDT_TIMER_SEND,            // timer identifier 
                    10000,                 // 10-second interval 
                    (TIMERPROC) NULL);     // no timer callback 

                SetTimer(hDlg_main,        // handle to main window 
                    IDT_TIMER_TO,            // timer identifier 
                    1000,                 // 10-second interval 
                    (TIMERPROC) NULL);     // no timer callback 
*/

            }
            break;

        default:
            break;
    }

    return;
}

void WM_TIMER_hook(hDlg, message, wParam, lParam)
{

    if (wParam == IDT_TIMER_TO)
    {
        KillTimer(hDlg_main, IDT_TIMER_SEND);
        KillTimer(hDlg_main, IDT_TIMER_TO);

        HWND hServo = GetDlgItem(hDlg_main, IDC_FLD_FOCUS);
        sprintf(x_servo_str, "servo: Stopped ");
        SetWindowText(hServo, x_servo_str);

        sprintf(x_coord_str, "stopped");
        SetWindowText(hCoord, x_coord_str);

    }

    if (wParam == IDT_TIMER_SEND)
    {
        send_servo();
    }
}

void my_hook(
    HWND hDlg, 
    UINT message, 
    WPARAM wParam, 
    LPARAM lParam)
{
    static is_initialized = 0;


    switch (message)
    {
    case WM_INITDIALOG:
        break;

    case WM_COMMAND:
        if (!is_initialized)
        {
            is_initialized = 1;
            WM_INITDIALOG_hook(hDlg, message, wParam, lParam);
            
            ZeroMemory(&BT_writeDevice, sizeof(BT_writeDevice));
        }

        WM_COMMAND_hook(hDlg, message, wParam, lParam);

    case WM_MOUSEMOVE:

        WM_MOUSEMOUVE_hook(hDlg, message, wParam, lParam);



        break;
    case WM_TIMER:
        WM_TIMER_hook(hDlg, message, wParam, lParam);
        break;
    default:
        break;
    }
}

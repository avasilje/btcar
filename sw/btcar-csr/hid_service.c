/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *    hid_service.c
 *
 *  DESCRIPTION
 *    This file defines routines for using HID service.
 *
 ******************************************************************************/

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <gatt.h>
#include <gatt_prim.h>
#include <mem.h>
#include <buf_utils.h>
#include <pio.h>

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "app_gatt.h"
#include "hid_service.h"
#include "nvm_access.h"
#include "app_gatt_db.h"
#include "btcar_hw.h"
#include "user_config.h"
#include "uart.h"

uint8 sheet_happens = 0;

extern uint8 send_hello;
extern uint8 send_hello_cnt;

/*=============================================================================*
 *  Private Data Types
 *============================================================================*/

typedef struct
{
    /* INPUT report notification handle as configured 
     * by peer host device.
     */
    uint16                  input_report_handle;

    /* Input Report Client Configuration */
    gatt_client_config      input_client_config;

    /* Array to hold the last sent input report to the HID Host.*/
    uint8                   last_input_report[ATTR_LEN_HID_INPUT_REPORT];

    /* Array to hold output report data.*/
    uint8                   output_report[ATTR_LEN_HID_OUTPUT_REPORT];

    /* HID protocol mode is HID_REPORT_MODE mode by default (boot mode not supported) */
    hid_protocol_mode       report_mode;

    /* Set to TRUE if the HID device is suspended. By default set to FALSE (ie., 
     * Not Suspended)
     */
    bool                    suspended;

    /* This flag is set if the notifications for Report mode input 
     * reports are enabled and the corresponding input reporting mode is set
     */
    bool                    notify_enable;

    /* NVM offset at which HID data is stored */
    uint16                  nvm_offset;

} HID_DATA_T;


/*=============================================================================*
 *  Private Data
 *============================================================================*/

HID_DATA_T hid_data;

/*=============================================================================*
 *  Private Definitions
 *============================================================================*/

#define HID_SERVICE_NVM_MEMORY_WORDS                      (3)

/* The offset of data being stored in NVM for HID service. This offset is added
 * to HID service offset to NVM region (see hid_data.nvm_offset) to get the 
 * absolute offset at which this data is stored in NVM
 */
#define HID_NVM_INPUT_RPT_CLIENT_CONFIG_OFFSET            (1)

/*=============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

static void handleOutputReport(GATT_ACCESS_IND_T *ind);
static void updateInputReportConfiguration(gatt_client_config config, 
                                                                uint16 handle);
static void handleControlPointUpdate(hid_control_point_op control_op);

/*=============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleOutputReport
 *
 *  DESCRIPTION
 *      This function handles output report.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

static void handleOutputReport(GATT_ACCESS_IND_T *ind)
{
    /* The output report of one byte will have bit-fields assigned as:
     * Bit               Description 
     *  0                 NUM LOCK 
     *  1                 CAPS LOCK
     *  2                 SCROLL LOCK
     *  3                 COMPOSE
     *  4                 KANA
     *  5 to 7            CONSTANT
     */

    /* Copy the output report locally and update the LEDS. Currently only
     * CAPS LOCK and NUM LOCK LEDs are supported.
     */
    
    MemCopy(hid_data.output_report, ind->value, 
               ATTR_LEN_HID_OUTPUT_REPORT);

    set_pwm_cfg(0);


    {
#define TX_DATA_LEN        3
        bool rc;
        uint8 tx_data[TX_DATA_LEN];

        if (hid_data.output_report[0] == 0x17)
        {
            tx_data[0] = 0x17;  // Servo stream
            tx_data[1] = hid_data.output_report[1];
            tx_data[2] = hid_data.output_report[2];
        
            rc = UartWrite(&tx_data[0], TX_DATA_LEN);
            
            if (!rc)
            {
                // Preserve last servo control data to send it upon current TX finished
            }
       }
         
    }

    PioSet(AUX_LED_PIO, aux_led_toggle);
    aux_led_toggle = 1 - aux_led_toggle;
    
}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      updateInputReportConfiguration
 *
 *  DESCRIPTION
 *      This function is used to update client configurations.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

static void updateInputReportConfiguration(gatt_client_config config, 
                                                                 uint16 handle)
{
    /* If CCCD of input report is written, the
     * input report handle and notification status of input report need to be
     * updated as single variable is being maintained for both boot and report
     * protocol modes.
     */
    if(config == gatt_client_config_notification)
    {
        hid_data.input_report_handle = handle;
        hid_data.notify_enable = TRUE;
    }
    else
    {
        hid_data.notify_enable = FALSE;
    }
}    

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleControlPointUpdate
 *
 *  DESCRIPTION
 *      This function is used to handle HID Control Point updates
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

static void handleControlPointUpdate(hid_control_point_op control_op)
{

    switch(control_op)
    {
        case hid_suspend:
        {
             hid_data.suspended = TRUE;

             /* Host has suspended its operations, application may like 
              * to do a low frequency key scan. The sample application is 
              * not doing any thing special in this case other than not sending
              * a connection parameter update request if the remote host is
              * currently suspended.
              */
        }
        break;

        case hid_exit_suspend:
        {
             hid_data.suspended = FALSE;

             /* Host has exited suspended mode, application may like 
              * to do a normal frequency key scan. The sample application is 
              * not doing any thing special in this case.
              */
        }
        break;

        default:
        {
            /* Ignore invalid value */
        }
        break;
    }

}

/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HidDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialise HID service data 
 *      structure.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void HidDataInit(void)
{
    MemSet(hid_data.last_input_report, 0x00, ATTR_LEN_HID_INPUT_REPORT);

    MemSet(hid_data.output_report, 0x00, ATTR_LEN_HID_OUTPUT_REPORT);

    hid_data.input_report_handle = INVALID_ATT_HANDLE;
    hid_data.notify_enable = FALSE;

    if(!AppIsDeviceBonded())
    {
        /* Initialise Interrupt and Control Characteristic Client Configuration 
         * only if device is not bonded
         */
        hid_data.input_client_config = gatt_client_config_none;

        /* Update the NVM  with the same. */
        Nvm_Write((uint16*)&hid_data.input_client_config,
                  sizeof(gatt_client_config),
             hid_data.nvm_offset + HID_NVM_INPUT_RPT_CLIENT_CONFIG_OFFSET);
    }
    else /* Bonded */
    {
        /* Bonded and Notifications are configured for Input Reports */

        /* At every new connection, HID device will be reset to Report 
         * Mode (i.e., the default mode). So, check if client 
         * configuration for HID Input Report is already set, then move 
         * the device to NOTIFY state
         */

        if(hid_data.input_client_config == gatt_client_config_notification)
        {
            hid_data.input_report_handle = HANDLE_HID_INPUT_REPORT;
            hid_data.notify_enable = TRUE;
        }
    }

    /* Default to Report Mode */
    hid_data.report_mode = hid_report_mode;
    hid_data.suspended = FALSE;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HidInitChipReset
 *
 *  DESCRIPTION
 *      This function is used to initialise HID service data 
 *      structure at chip reset
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void HidInitChipReset(void)
{
    /* HID service specific things that need to be done upon a power reset. */
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HidHandleAccessRead
 *
 *  DESCRIPTION
 *      This function handles Read operation on HID service attributes
 *      maintained by the application and responds with the GATT_ACCESS_RSP 
 *      message.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void HidHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    uint16 length = 0;
    uint8  *p_value = NULL, val[2];
    sys_status rc = sys_status_success;


    switch(p_ind->handle)
    {

        case HANDLE_HID_INPUT_RPT_CLIENT_CONFIG:
        {
            p_value = val;

            BufWriteUint16(&p_value, hid_data.input_client_config);
            length = 2;

            /* BufWriteUint16 will have incremented p_value. Revert it back
             * to point to val.
             */
            p_value -= length;
        }
        break;

        case HANDLE_HID_INPUT_REPORT:
        {
            p_value = hid_data.last_input_report;
            
            length = ATTR_LEN_HID_INPUT_REPORT;
        }
        break;

        case HANDLE_HID_PROTOCOL_MODE:
        {
            p_value = val;

            p_value[0] = hid_data.report_mode;
            length = ATTR_LEN_HID_PROTOCOL_MODE;
        }
        break;

        case HANDLE_HID_OUTPUT_REPORT:
        {
             p_value = hid_data.output_report;

             length = ATTR_LEN_HID_OUTPUT_REPORT;
        }
        break;

        default:
        {
            /* Other characteristics in HID don't support read property */
            rc = gatt_status_read_not_permitted;
            while(1){
                rc = gatt_status_read_not_permitted;
            };
        }
        break;

    }

    GattAccessRsp(p_ind->cid, p_ind->handle, rc,
                  length, p_value);

}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HidHandleAccessWrite
 *
 *  DESCRIPTION
 *      This function handles Write operation on HID service attributes 
 *      maintained by the application.and responds with the GATT_ACCESS_RSP 
 *      message.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

extern void HidHandleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{
    uint16 client_config, input_rpt_hndl;
    uint8 *p_value = p_ind->value;
    sys_status rc = sys_status_success;
    bool update_input_report_config = FALSE;

    switch(p_ind->handle)
    {
        case HANDLE_HID_INPUT_RPT_CLIENT_CONFIG:
        {

            client_config = BufReadUint16(&p_value);

            if((client_config == gatt_client_config_notification) ||
               (client_config == gatt_client_config_none))
            {
                uint16 offset;

                if(1)
                {
                    hid_data.input_client_config = client_config;

                    /* If Report mode set, update Input Report Configuration 
                     * for Report mode
                     */
                    if(hid_data.report_mode)
                    {
                        update_input_report_config = TRUE;
                        input_rpt_hndl = HANDLE_HID_INPUT_REPORT;
                    }

                    offset =  hid_data.nvm_offset+ 
                              HID_NVM_INPUT_RPT_CLIENT_CONFIG_OFFSET;

                }
                /* Write HID Input Report Client configuration to NVM if the
                 * devices are bonded.
                 */
                 Nvm_Write(&client_config,
                          sizeof(gatt_client_config),
                          offset);

                /* Device shall only trigger Input reports if notifications are 
                 * enabled for characteristic corresponding to the current 
                 * protocol mode.
                 */
                if(update_input_report_config)
                {
                    updateInputReportConfiguration(client_config, 
                                                   input_rpt_hndl);
                }
            }
            else
            {
                /* INDICATION or RESERVED */

                /* Return Error as only Notifications are supported for application */
                rc = gatt_status_app_mask;
                sheet_happens++;
            }
        }
        break;


        case HANDLE_HID_OUTPUT_REPORT:
        {
            if(p_ind->size_value == ATTR_LEN_HID_OUTPUT_REPORT)
            {
                /* Handle received output report */
                handleOutputReport(p_ind);
            }
            else
            {
                rc = gatt_status_invalid_length;
                sheet_happens++;
            }
        }
        break;

        case HANDLE_HID_CONTROL_POINT:
        {
            uint8   control_op = BufReadUint8(&p_value);

            handleControlPointUpdate(control_op);
        }
        break;

        case HANDLE_HID_PROTOCOL_MODE:
        {
            uint8 mode = BufReadUint8(&p_value);
            hid_protocol_mode old_mode = hid_data.report_mode;

            if(((mode == hid_report_mode)) &&
                (mode != old_mode)) /* Change of Protocol Mode */
            {
                /* AV: Boot mode not supported, so shouldn't triggers 
                 * TODO: cleanup BOOT related stuff 
                 */
                if(mode == hid_report_mode)
                {
                    client_config = hid_data.input_client_config;
                    input_rpt_hndl = HANDLE_HID_INPUT_REPORT;
                }

                hid_data.report_mode = mode;

                updateInputReportConfiguration(client_config,
                                               input_rpt_hndl);

            } /* Else Ignore the value */
        }
        break;

        default:
        {
            /* Other characteristics in HID don't support write property */
            rc = gatt_status_write_not_permitted;
            sheet_happens++;

        }
        break;
    }

    /* Send ACCESS RESPONSE */
    GattAccessRsp(p_ind->cid, p_ind->handle, rc, 0, NULL);

}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HidIsNotifyEnabledOnReportId
 *
 *  DESCRIPTION
 *      This function returns whether notifications are enabled on CCCD of
 *      reports specified by 'report_id'.
 *
 *  RETURNS/MODIFIES
 *      TRUE/FALSE: Notification configured or not
 *
 *----------------------------------------------------------------------------*/

extern bool HidIsNotifyEnabledOnReportId(uint8 report_id)
{
    switch(report_id)
    {
        case HID_INPUT_REPORT_ID: 
            return hid_data.notify_enable;
        default:
            return FALSE;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HidIsNotificationEnabled
 *
 *  DESCRIPTION
 *      This function returns whether notifications are enabled on all CCCDs of
 *      reports used in a particular protocol mode.
 *
 *  RETURNS/MODIFIES
 *      TRUE if notifications are configured on all the report handles used in a
 *      particular mode.
 *
 *----------------------------------------------------------------------------*/

extern bool HidIsNotificationEnabled(void)
{
    return hid_data.notify_enable;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HidSendInputReport
 *
 *  DESCRIPTION
 *      This function is used to notify key presses to connected host.
 *
 *  RETURNS
 *      True if data is notified to connected host
 *
 *----------------------------------------------------------------------------*/

extern bool HidSendInputReport(uint16 ucid, uint8 report_id, uint8 *report)
{
    bool notification_sent = FALSE;
    
    switch(report_id)
    {
        case HID_INPUT_REPORT_ID:
        {
            GattCharValueNotification(ucid,
                              hid_data.input_report_handle,
                              ATTR_LEN_HID_INPUT_REPORT,
                              report);
            /* Update the last sent input report array with the input report
             * that has just been sent.
             */
            MemCopy(hid_data.last_input_report, report, 
                                                ATTR_LEN_HID_INPUT_REPORT);

            notification_sent = TRUE;
        }
        break;

        default:
        break;
    }

    return notification_sent;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HidReadDataFromNVM
 *
 *  DESCRIPTION
 *      This function is used to read HID service specific data store in NVM
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void HidReadDataFromNVM(bool bonded, uint16 *p_offset)
{
    hid_data.nvm_offset = *p_offset;

    /* Read NVM only if devices are bonded */
    if(bonded)
    {

        /* Read HID Input Report Client Configuration */
        Nvm_Read((uint16*)&hid_data.input_client_config,
                   sizeof(gatt_client_config),
                  hid_data.nvm_offset + HID_NVM_INPUT_RPT_CLIENT_CONFIG_OFFSET);

    }
    
    /* Increment the offset by the number of words of NVM memory required 
     * by HID service 
     */
    *p_offset += HID_SERVICE_NVM_MEMORY_WORDS;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HidCheckHandleRange
 *
 *  DESCRIPTION
 *      This function is used to check if the handle belongs to the HID 
 *      service
 *
 *  RETURNS/MODIFIES
 *      Boolean - Indicating whether handle falls in range or not.
 *
 *----------------------------------------------------------------------------*/

extern bool HidCheckHandleRange(uint16 handle)
{
    return ((handle >= HANDLE_HID_SERVICE) &&
            (handle <= HANDLE_HID_SERVICE_END))
            ? TRUE : FALSE;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HidIsStateSuspended
 *
 *  DESCRIPTION
 *      This function is used to check if the HID host has entered suspend state
 *
 *  RETURNS/MODIFIES
 *      Boolean - TRUE if the remote device is in suspended state.
 *
 *----------------------------------------------------------------------------*/
 extern bool HidIsStateSuspended(void)
{
    return hid_data.suspended;
}

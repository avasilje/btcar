/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 * FILE
 *    scan_param_service.c
 *
 *  DESCRIPTION
 *    This file defines routines for using Scan Parameter service.
 *
 ******************************************************************************/

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <gatt_prim.h>
#include <buf_utils.h>
#include <status.h>
#include <gatt.h>

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "scan_param_service.h"
#include "app_gatt.h"
#include "app_gatt_db.h"
#include "nvm_access.h"

/*=============================================================================*
 *  Private Data Types
 *============================================================================*/

typedef struct
{
    /* Scan Interval supported by the Client */
    uint16  interval;

    /* Scan Window supported by the Client */
    uint16  window;

    /* Client configurate for Scan Refresh characteristic */
    gatt_client_config refresh_client_config;

    /* Offset at which Scan Parameter data is stored in NVM */
    uint16 nvm_offset;

} SCAN_PARAM_DATA_T;


/*=============================================================================*
 *  Private Data
 *============================================================================*/

SCAN_PARAM_DATA_T scan_param_data;

/*=============================================================================*
 *  Private Definitions
 *============================================================================*/

#define SCAN_PARAM_SERVICE_NVM_MEMORY_WORDS           (1)

/* The offset of data being stored in NVM for Scan Parameter service. This 
 * offset is added to Scan Parameter service offset to NVM region (see 
 * scan_param_data.nvm_offset) to get the absolute offset at which this data 
 * is stored in NVM.
 */
#define SCAN_PARAM_NVM_REFRESH_CLIENT_CONFIG_OFFSET   (0)

/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      ScanParamDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialise GAP service data 
 *      structure.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void ScanParamDataInit(void)
{
    /* Set scan parameters to some invalid values */
    scan_param_data.interval = SCAN_PARAM_INVALID_INTERVAL;
    scan_param_data.window = SCAN_PARAM_INVALID_WINDOW;

    if(!AppIsDeviceBonded())
    {
        /* Initialise Scan Refresh Characteristic Client Configuration 
         * only if device is not bonded
         */
        scan_param_data.refresh_client_config = gatt_client_config_none;
        Nvm_Write((uint16*)&scan_param_data.refresh_client_config,
                  sizeof(gatt_client_config),
      scan_param_data.nvm_offset + SCAN_PARAM_NVM_REFRESH_CLIENT_CONFIG_OFFSET);
    }

}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      ScanParamInitChipReset
 *
 *  DESCRIPTION
 *      This function is used to initialise Scan Parameter service data 
 *      structure at chip reset
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void ScanParamInitChipReset(void)
{
    /* Scan parameters service specific things that need to be done upon a
     * power reset.
     */
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      ScanParamHandleAccessRead
 *
 *  DESCRIPTION
 *      This function handles Read operation on Scan Parameter service
 *      attributes maintained by the application and responds with the 
 *      GATT_ACCESS_RSP message.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void ScanParamHandleAccessRead(GATT_ACCESS_IND_T *p_ind)
{
    uint16 length = 0;
    uint8  val[2], *p_value = NULL;
    sys_status rc = sys_status_success;

    switch(p_ind->handle)
    {

        case HANDLE_SCAN_REFRESH_C_CFG:
        {
            p_value = val;
            length = 2; /* Two Octets */

            BufWriteUint16(&p_value, scan_param_data.refresh_client_config);

            /* BufWriteUint16 will have incremented p_value. Revert it back
             * to point to val.
             */
            p_value -= length;
        }
        break;

        default:
        {
            rc = gatt_status_read_not_permitted;
        }
        break;

    }

    GattAccessRsp(p_ind->cid, p_ind->handle, rc,
                  length, p_value);

}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      ScanParamHandleAccessWrite
 *
 *  DESCRIPTION
 *      This function handles Write operation on Scan Parameter service 
 *      attributes maintained by the application.and responds with the 
 *      GATT_ACCESS_RSP message.
 *
 *  RETURNS/MODIFIES
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

extern void ScanParamHandleAccessWrite(GATT_ACCESS_IND_T *p_ind)
{
    uint8  *p_value =  p_ind->value;
    sys_status rc = sys_status_success;

    switch(p_ind->handle)
    {

        case HANDLE_SCAN_INTERVAL_WINDOW:
        {
            scan_param_data.interval = BufReadUint16(&p_value);
            scan_param_data.window = BufReadUint16(&p_value);
        }
        break;

        case HANDLE_SCAN_REFRESH_C_CFG:
        {
            uint16 client_config = BufReadUint16(&p_value);

            if((client_config == gatt_client_config_notification) ||
               (client_config == gatt_client_config_none))
            {
                scan_param_data.refresh_client_config = client_config;

                /* Write Scan refresh Client configuration to NVM if the 
                 * device is bonded.
                 */
                 Nvm_Write(&client_config,
                          sizeof(gatt_client_config),
                          scan_param_data.nvm_offset+ 
                          SCAN_PARAM_NVM_REFRESH_CLIENT_CONFIG_OFFSET);
            }
            else
            {
                /* INDICATION or RESERVED */

                /* Return Error as only Notifications are supported */
                rc = gatt_status_app_mask;
            }
        }
        break;        


        default:
        {
            rc = gatt_status_write_not_permitted;
        }
        break;

    }

    GattAccessRsp(p_ind->cid, p_ind->handle, rc, 0, NULL);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      ScanParamReadDataFromNVM
 *
 *  DESCRIPTION
 *      This function is used to read Scan Parameter service specific data 
 *      stored in NVM
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void ScanParamReadDataFromNVM(bool bonded, uint16 *p_offset)
{
    scan_param_data.nvm_offset = *p_offset;

    /* Read NVM only if devices are bonded */
    if(bonded)
    {

        /* Read Scan Parameter Refresh Client Configuration */
        Nvm_Read((uint16*)&scan_param_data.refresh_client_config,
                   sizeof(gatt_client_config),
      scan_param_data.nvm_offset + SCAN_PARAM_NVM_REFRESH_CLIENT_CONFIG_OFFSET);

    }
    
    /* Increment the offset by the number of words of NVM memory required 
     * by Scan Parameter service 
     */
    *p_offset += SCAN_PARAM_SERVICE_NVM_MEMORY_WORDS;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      ScanParamCheckHandleRange
 *
 *  DESCRIPTION
 *      This function is used to check if the handle belongs to the scan
 *      parameters service.
 *
 *  RETURNS/MODIFIES
 *      Boolean - Indicating whether handle falls in range or not.
 *
 *----------------------------------------------------------------------------*/

extern bool ScanParamCheckHandleRange(uint16 handle)
{
    return ((handle >= HANDLE_SCAN_PARAMS_SERVICE) &&
            (handle <= HANDLE_SCAN_PARAMS_SERVICE_END))
            ? TRUE : FALSE;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      ScanParamGetIntervalWindow
 *
 *  DESCRIPTION
 *      This function is used to get the scan interval and scan window supported
 *      by the client.
 *
 *----------------------------------------------------------------------------*/

extern void ScanParamGetIntervalWindow(uint16 *p_interval, uint16 *p_window)
{
    *p_interval = scan_param_data.interval;
    *p_window = scan_param_data.window;
}

#ifndef __NO_IDLE_TIMEOUT__

/*-----------------------------------------------------------------------------*
 *  NAME
 *      ScanParamRefreshNotify
 *
 *  DESCRIPTION
 *      This function is used by application to send Scan Refresh characteristic
 *      if notications are enabled on the characteristic
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void ScanParamRefreshNotify(uint16 ucid)
{
    uint8 value = SERVER_REQUIRES_REFRESH;

    if(scan_param_data.refresh_client_config == gatt_client_config_notification)
    {
        GattCharValueNotification(ucid, 
                              HANDLE_SCAN_REFRESH, 
                              1, &value);
    }

}

#endif /* __NO_IDLE_TIMEOUT__ */
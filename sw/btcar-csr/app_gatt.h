/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *      app_gatt.h
 *
 *  DESCRIPTION
 *      Header definitions for common application attributes
 *
 ******************************************************************************/

#ifndef __APP_GATT_H__
#define __APP_GATT_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <panic.h>

/*=============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Maximum Length of Device Name. After taking into consideration other
 * elements added to advertisement, the advertising packet can receive upto a 
 * maximum of 31 octets from application
 */
#define DEVICE_NAME_MAX_LENGTH               (20)

/* Invalid UCID indicating we are not currently connected */
#define GATT_INVALID_UCID                    (0xFFFF)

/* Invalid Attribute Handle */
#define INVALID_ATT_HANDLE                   (0x0000)

/* AD Type for Appearance */
#define AD_TYPE_APPEARANCE                   (0x19)

/* Extract low order byte of 16-bit UUID */
#define LE8_L(x)                             ((x) & 0xff)

/* Extract low order byte of 16-bit UUID */
#define LE8_H(x)                             (((x) >> 8) & 0xff)

/* Timer value for remote device to re-encrypt the link using old keys */
#define BONDING_CHANCE_TIMER                 (30*SECOND)

/*=============================================================================*
 *  Public Data Types
 *============================================================================*/

/* GATT Client Charactieristic Configuration Value [Ref GATT spec, 3.3.3.3]*/
typedef enum
{
    gatt_client_config_none = 0x0000,
    gatt_client_config_notification = 0x0001,
    gatt_client_config_indication = 0x0002,
    gatt_client_config_reserved = 0xFFF4

} gatt_client_config;

/*  Application defined panic codes */
typedef enum
{
    app_panic_set_advert_params,
    app_panic_set_advert_data,
    app_panic_set_scan_rsp_data,
    app_panic_con_param_update,
    app_panic_connection_est, /* Failure in connection establishment */
    app_panic_db_registration,
    app_panic_nvm_read,
    app_panic_nvm_write,
    app_panic_read_tx_pwr_level,
    app_panic_add_whitelist,
    app_panic_invalid_state   /* An LM or GATT event received in an unexpected
                               * application state.
                               */
}app_panic_code;

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/


/* This function is used to call panics from application */
extern void ReportPanic(app_panic_code panic_code);

/* This function returns the status whether the connected device is 
 * bonded or not
 */
extern bool AppIsDeviceBonded(void);

/* This function adds or deletes entries from whitelist */
extern void AppUpdateWhiteList(void);

#endif /* __APP_GATT_H__ */

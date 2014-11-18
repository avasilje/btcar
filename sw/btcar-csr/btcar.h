/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *          btcar.h  -  user application for a BTLE btcar
 *
 *  DESCRIPTION
 *      Header file for a sample BTCAR application.
 *
 ******************************************************************************/

#ifndef __BTCAR_H__
#define __BTCAR_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <bluetooth.h>
#include <timer.h>

/*=============================================================================*
 *  Local Header File
 *============================================================================*/

#include "app_gatt.h"
#include "app_gatt_db.h"
#include "gap_conn_params.h"
#include "user_config.h"

/*=============================================================================*
 *  Public Definitions
 *============================================================================*/

/* Maximum number of words in central device IRK */
#define MAX_WORDS_IRK                   (8)

/*Number of IRKs that application can store */
#define MAX_NUMBER_IRK_STORED           (1)

/*=============================================================================*
 *  Public Data Types
 *============================================================================*/

typedef enum
{

    btcar_init = 0,             /* Initial State */
    btcar_direct_advert,        /* This state is entered while reconnecting to
                                 * a bonded host that does not support random 
                                 * resolvable address. When privacy feature is 
                                 * enabled, this state is entered when the remote 
                                 * host has written to the re-connection address 
                                 * characteristic during the last connection
                                 */
    btcar_fast_advertising,     /* Fast Undirected advertisements configured */
    btcar_slow_advertising,     /* Slow Undirected advertisements configured */

    /* AV TODO: remove pass key functionality */
    btcar_passkey_input,        /* State in which the keyboard has to send the user
                                 * entered passkey */    

    btcar_connected,            /* Keyboard has established connection to the 
                                   host */
    btcar_disconnecting,        /* Enters when disconnect is initiated by the 
                                   HID device */

    /* AV TODO: reconsider idle state usage & conditions */
    btcar_idle                  /* Enters when ??? no key presses are reported from 
                                   connected keyboard for vendor specific time 
                                   defined by CONNECTED_IDLE_TIMEOUT_VALUE macro ??? */
} btcar_state;

/* Structure defined for Central device IRK. A device can store more than one
 * IRK for the same remote device. This structure can be extended to accommodate
 * more IRKs for the same remote device.
 */
typedef struct
{
    uint16                              irk[MAX_WORDS_IRK];

} CENTRAL_DEVICE_IRK_T;

/* Structure to hold a key press. It contains a report ID and a report. The
 * length of the report should be the maximum length of all the types of
 * report used in the application as the same array is used while adding all
 * types of report to the queue.
 */
typedef struct
{
    uint8 report_id;
    uint8 report[LARGEST_REPORT_SIZE];

} KEY_STROKE_T;

/* Circular queue for storing pending key strokes */
typedef struct
{
    /* Circular queue buffer */
    KEY_STROKE_T key_stroke[MAX_PENDING_KEY_STROKES];

    /* Starting index of circular queue carrying the oldest key stroke */
    uint8 start_idx;

    /* Out-standing key strokes in the queue */
    uint8 num;

} CQUEUE_KEY_STROKE_T;

typedef struct
{
    btcar_state state;

    /* Value for which advertisement timer needs to be started. 
     *
     * For bonded devices, the timer is initially started for 30 seconds to 
     * enable fast connection by bonded device to the sensor.
     * This is then followed by reduced power advertisements for 1 minute.
     */
    uint32 advert_timer_value;
    
    /* Store timer id in 'FAST_ADVERTISING', 'SLOW_ADVERTISING' and 
     * 'CONNECTED' states.
     */
    timer_id app_tid;

    /* Timer to hold the time elapsed after the last
     * L2CAP_CONNECTION_PARAMETER_UPDATE Request failed.
     */
     timer_id conn_param_update_tid;

    /* A counter to keep track of the number of times the application has tried 
     * to send L2CAP connection parameter update request. When this reaches 
     * MAX_NUM_CONN_PARAM_UPDATE_REQS, the application stops re-attempting to
     * update the connection parameters */
    uint8 conn_param_update_cnt;

    /* TYPED_BD_ADDR_T of the host to which device is connected */
    TYPED_BD_ADDR_T con_bd_addr;

    /* Track the UCID as Clients connect and disconnect */
    uint16 st_ucid;

    /* Boolean flag to indicated whether the device is bonded */
    bool bonded;

    /* TYPED_BD_ADDR_T of the host to which device is bonded. Heart rate
     * sensor can only bond to one collector 
     */
    TYPED_BD_ADDR_T bonded_bd_addr;

    /* Diversifier associated with the LTK of the bonded device */
    uint16 diversifier;

    /* Other option is to use a independent global varibale for this in 
     *the seperate file */
    /*Central Private Address Resolution IRK  Will only be used when
     *central device used resolvable random address. */
    CENTRAL_DEVICE_IRK_T central_device_irk;

    /* Value of TK to be supplied to the Security Manager upon Passkey entry */
    uint32 pass_key;

    /* Boolean flag to indicate whether encryption is enabled with the bonded 
     * host
     */
    bool encrypt_enabled;

    /* Boolean flag set to transfer key press data post connection with the 
     * host.
     */
    bool data_pending;

    /* Circular queue for storing pending key strokes */
    CQUEUE_KEY_STROKE_T pending_key_strokes;

    /* Boolean flag set to indicate pairing button press */
    bool pairing_button_pressed;

    /* If a key press occurs in slow advertising state, the application shall 
     * move to directed or fast advertisements state. This flag is used to 
     * know whether the application needs to move to directed or fast 
     * advertisements state while handling GATT_CANCEL_CONNECT_CFM message.
     */
    bool start_adverts;

    /* Boolean to indicate whether the application has sent data to the firmware
     * and waiting for confirmation from it.
     */
    bool data_tx_in_progress;

    /* Boolean which indicates that the application is waiting for the firmware
     * to free its buffers. This flag is set when the application tries to add
     * some data(which has to be sent to the remote device) to the the firmware
     * and receives a gatt_status_busy response in GATT_CHAR_VAL_NOT_CFM.
     */
    bool waiting_for_fw_buffer;

    /* This timer will be used if the application is already bonded to the 
     * remote host address but the remote device wanted to rebond which we had 
     * declined. In this scenario, we give ample time to the remote device to 
     * encrypt the link using old keys. If remote device doesn't encrypt the 
     * link, we will disconnect the link on this timer expiry.
     */
    timer_id bonding_reattempt_tid;

    /* Varibale to store the current connection interval being used. */
    uint16   conn_interval;

    /* Variable to store the current slave latency. */
    uint16   conn_latency;

    /*Variable to store the current connection timeout value. */
    uint16   conn_timeout;
} BTCAR_DATA_T;

/*=============================================================================*
 *  Public Data
 *============================================================================*/

/* The data structure used by the BTCAR application. */
extern BTCAR_DATA_T g_btcar_data;

/*=============================================================================*
 *  Public Definitions
 *============================================================================*/

#define IS_BTCAR_ADVERTISING() ((g_btcar_data.state == btcar_fast_advertising) || \
                              (g_btcar_data.state == btcar_slow_advertising)|| \
                              (g_btcar_data.state == btcar_direct_advert))

/* Get the pointer to the input report stored in the queue at index 'i' */
#define GET_CQUEUE_INPUT_REPORT_REF(i) \
                   (g_btcar_data.pending_key_strokes.key_stroke[(i)].report)

/* Get the report ID of the report stored in the queue at index 'i' */
#define GET_CQUEUE_INPUT_REPORT_ID(i) \
                   (g_btcar_data.pending_key_strokes.key_stroke[(i)].report_id)

/* Set the report ID of the report stored in the queue at index 'i' */
#define SET_CQUEUE_INPUT_REPORT_ID(i, j) \
             (g_btcar_data.pending_key_strokes.key_stroke[(i)].report_id = (j))

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

/* This function is called when the pairing removal button is pressed down for a
 * period specified by PAIRING_REMOVAL_TIMEOUT
 */
extern void HandlePairingButtonPress(timer_id tid);

/* This function sends key strokes in circular queue maintained by application */
extern void SendKeyStrokesFromQueue(void);

/* This function checks the notification status of HID service */
extern bool AppCheckNotificationStatus(void);


#endif /* __BTCAR_H__ */

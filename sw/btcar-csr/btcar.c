/*******************************************************************************
 * FILE
 *    btcar.c
 *
 *  DESCRIPTION
 *    This file defines a simple btcar application.
 *
 ******************************************************************************/

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <main.h>
#include <gatt.h>
#include <security.h>
#include <ls_app_if.h>
#include <nvm.h>
#include <pio.h>
#include <mem.h>
#include <time.h>

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/

#include "btcar.h"
#include "btcar_hw.h"
#include "app_gatt.h"
#include "gap_service.h"
#include "hid_service.h"
#include "battery_service.h"
#include "scan_param_service.h"
#include "app_gatt_db.h"
#include "btcar_gatt.h"
#include "nvm_access.h"

/*=============================================================================*
 *  Private Definitions
 *============================================================================*/

/******** TIMERS ********/

/* Maximum number of timers */
#define MAX_APP_TIMERS                      (6)

/* Magic value to check the sanity of NVM region used by the application */
#define NVM_SANITY_MAGIC                    (0xAB06)

/* NVM offset for NVM sanity word */
#define NVM_OFFSET_SANITY_WORD              (0)

/* NVM offset for bonded flag */
#define NVM_OFFSET_BONDED_FLAG              (NVM_OFFSET_SANITY_WORD + 1)

/* NVM offset for bonded device bluetooth address */
#define NVM_OFFSET_BONDED_ADDR              (NVM_OFFSET_BONDED_FLAG + \
                                             sizeof(g_btcar_data.bonded))

/* NVM offset for diversifier */
#define NVM_OFFSET_SM_DIV                   (NVM_OFFSET_BONDED_ADDR + \
                                             sizeof(g_btcar_data.bonded_bd_addr))

/* NVM offset for IRK */
#define NVM_OFFSET_SM_IRK                   (NVM_OFFSET_SM_DIV + \
                                             sizeof(g_btcar_data.diversifier))

/* Number of words of NVM used by application. Memory used by supported 
 * services is not taken into consideration here.
 */
#define N_APP_USED_NVM_WORDS                (NVM_OFFSET_SM_IRK + \
                                             MAX_WORDS_IRK)

/* Time after which a L2CAP connection parameter update request will be
 * re-sent upon failure of an earlier sent request.
 */
#define GAP_CONN_PARAM_TIMEOUT              (30 * SECOND)

/* Size of each buffer associated with the shared memory of PIO controller. */
#define KEYWORDS                            ((ROWS >> 1) + (ROWS % 2))

/* AV TODO: remove keyboard related stuff */
/* Usage ID for the key 'z' and 'Enter' in keyboard. Usage IDs for numbers from
 * 1 to 0 start after 'z'. Refer Table 12: Keyboard/Keypad Page, 
 * HID USB usage table 1.11.
 */
#define USAGE_ID_KEY_Z                      (0x1D)
#define USAGE_ID_KEY_ENTER                  (0x28)

/*=============================================================================*
 *  Private Data
 *============================================================================*/

/* Keyboard application data structure */
BTCAR_DATA_T g_btcar_data;

/* Declare space for application timers. */
static uint16 app_timers[SIZEOF_APP_TIMER * MAX_APP_TIMERS];

/*=============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

static void resetQueueData(void);
static void readPersistentStore(void);
static void DataInit(void);

#ifndef __NO_IDLE_TIMEOUT__

static void IdleTimerHandler(timer_id tid);

#endif /* __NO_IDLE_TIMEOUT__ */

static void resetIdleTimer(void);
static void requestConnParamUpdate(timer_id tid);
static void appInitStateExit(void);
static void appSetState(btcar_state new_state);
static void appStartAdvert(void);
static void handleSignalGattAddDBCfm(GATT_ADD_DB_CFM_T *p_event_data);
static void handleSignalGattConnectCfm(GATT_CONNECT_CFM_T* event_data);
static void handleSignalLmEvConnectionComplete(
                                     LM_EV_CONNECTION_COMPLETE_T *p_event_data);
static void handleSignalGattCancelConnectCfm(void);
static void handleSignalGattAccessInd(GATT_ACCESS_IND_T *event_data);
static void handleSignalLmEvDisconnectComplete(
                               HCI_EV_DATA_DISCONNECT_COMPLETE_T *p_event_data);
static void handleSignalLMEncryptionChange(LM_EVENT_T *event_data);
static void handleSignalSmKeysInd(SM_KEYS_IND_T *event_data);
static void handleSignalSmPairingAuthInd(SM_PAIRING_AUTH_IND_T *p_event_data);
static void handleSignalSmSimplePairingCompleteInd(
                                  SM_SIMPLE_PAIRING_COMPLETE_IND_T *event_data);
static void handleSignalSMpasskeyInputInd(void);
static void handleSignalLsConnParamUpdateCfm(
                                  LS_CONNECTION_PARAM_UPDATE_CFM_T *event_data);
static void handleSignalLmConnectionUpdate(
                                       LM_EV_CONNECTION_UPDATE_T* p_event_data);

static void handleSignalLsConnParamUpdateInd(
                                LS_CONNECTION_PARAM_UPDATE_IND_T *p_event_data);
static void handleSignalGattCharValNotCfm
                                (GATT_CHAR_VAL_IND_CFM_T *p_event_data);
static void handleSignalLsRadioEventInd(void);
static void handleSignalSmDivApproveInd(SM_DIV_APPROVE_IND_T *p_event_data);
static void handleBondingChanceTimerExpiry(timer_id tid);

/*AV: LM Tracing info */
#define EVENT_ARR_IDX_MAX 30
lm_event_code g_event_arr[EVENT_ARR_IDX_MAX];
int g_event_arr_idx;

LM_EVENT_T g_event_arr_data[EVENT_ARR_IDX_MAX];

uint8 g_connection_fl = 0;

uint8 g_reports_sent[10] = {0};


/*=============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      resetQueueData
 *
 *  DESCRIPTION
 *      This function is used to discard all the data that is in the queue.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void resetQueueData(void)
{
    /* Initialise Key press pending flag */
    g_btcar_data.data_pending = FALSE;

    /* Initialise Circular Queue buffer */
    g_btcar_data.pending_key_strokes.start_idx = 0;
    g_btcar_data.pending_key_strokes.num = 0;

}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      DataInit
 *
 *  DESCRIPTION
 *      This function is used to initialise btcar application data 
 *      structure.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void DataInit(void)
{
    /* Initialize the data structure variables used by the application to their
     * default values. Each service data has also to be initialized.
     */
    g_btcar_data.advert_timer_value = TIMER_INVALID;
    
    TimerDelete(g_btcar_data.app_tid);
    g_btcar_data.app_tid = TIMER_INVALID;

    TimerDelete(g_btcar_data.conn_param_update_tid);
    g_btcar_data.conn_param_update_tid= TIMER_INVALID;

    /* Delete the bonding chance timer */
    TimerDelete(g_btcar_data.bonding_reattempt_tid);
    g_btcar_data.bonding_reattempt_tid = TIMER_INVALID;

    g_btcar_data.st_ucid = GATT_INVALID_UCID;

    g_btcar_data.encrypt_enabled = FALSE;

    g_btcar_data.pairing_button_pressed = FALSE;

    g_btcar_data.start_adverts = FALSE;

    g_btcar_data.data_tx_in_progress = FALSE;

    g_btcar_data.waiting_for_fw_buffer = FALSE;

    /* Reset the connection parameter variables. */
    g_btcar_data.conn_interval = 0;
    g_btcar_data.conn_latency = 0;
    g_btcar_data.conn_timeout = 0;

    HwDataInit();

    /* Initialise GAP Data Structure */
    GapDataInit();

    /* HID Service data initialisation */
    HidDataInit();

    /* Battery Service data initialisation */
    BatteryDataInit();

    /* Scan Parameter Service data initialisation */
    ScanParamDataInit();

    
/*AV: LM Tracing info */    
    { 
        int i;
        g_event_arr_idx = 0;
        for(i = 0; i < EVENT_ARR_IDX_MAX; i++){
            g_event_arr[i] = 0x7777;
        }
        
        MemSet(g_event_arr_data, 0x0, sizeof(g_event_arr_data));
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      readPersistentStore
 *
 *  DESCRIPTION
 *      This function is used to initialise and read NVM data
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void readPersistentStore(void)
{
    uint16 offset = N_APP_USED_NVM_WORDS;
    uint16 nvm_sanity = 0xffff;

    /* Read persistent storage to know if the device was last bonded 
     * to another device 
     */

    /* If the device was bonded, trigger advertisements for the bonded
     * host(using whitelist filter). If the device was not bonded, trigger
     * advertisements for any host to connect to the device.
     */

    Nvm_Read(&nvm_sanity, sizeof(nvm_sanity), NVM_OFFSET_SANITY_WORD);

    if(nvm_sanity == NVM_SANITY_MAGIC)
    {
        /* Read Bonded Flag from NVM */
        Nvm_Read((uint16*)&g_btcar_data.bonded, sizeof(g_btcar_data.bonded), 
                                                        NVM_OFFSET_BONDED_FLAG);

        if(g_btcar_data.bonded)
        {

            /* Bonded Host Typed BD Address will only be stored if bonded flag
             * is set to TRUE. Read last bonded device address.
             */
            Nvm_Read((uint16*)&g_btcar_data.bonded_bd_addr, 
                       sizeof(TYPED_BD_ADDR_T), NVM_OFFSET_BONDED_ADDR);

            /* If the bonded device address is resovable then read the bonded
             * device's IRK
             */
            if(IsAddressResolvableRandom(&g_btcar_data.bonded_bd_addr))
            {
                Nvm_Read(g_btcar_data.central_device_irk.irk,
                                    MAX_WORDS_IRK, NVM_OFFSET_SM_IRK);
            }
        }

        else /* Case when we have only written NVM_SANITY_MAGIC to NVM but
              * didn't get bonded to any host in the last powered session
              */
        {
            g_btcar_data.bonded = FALSE;
        }
        
        /* Read the diversifier associated with the presently bonded/last bonded
         * device.
         */
        Nvm_Read(&g_btcar_data.diversifier, sizeof(g_btcar_data.diversifier),
                 NVM_OFFSET_SM_DIV);

        /* Read device name and length from NVM */
        GapReadDataFromNVM(&offset);

    }
    else /* NVM Sanity check failed means either the device is being brought up 
          * for the first time or memory has got corrupted in which case discard 
          * the data and start fresh.
          */
    {
        nvm_sanity = NVM_SANITY_MAGIC;

        /* Write NVM Sanity word to the NVM */
        Nvm_Write(&nvm_sanity, sizeof(nvm_sanity), NVM_OFFSET_SANITY_WORD);

        /* The device will not be bonded as it is coming up for the first time*/
        g_btcar_data.bonded = FALSE;

        /* Write bonded status to NVM */
        Nvm_Write((uint16*)&g_btcar_data.bonded, sizeof(g_btcar_data.bonded),
                                                        NVM_OFFSET_BONDED_FLAG);

        /* The device is not bonded so don't have BD Address to update */

        /* When the Device is booting up for the first time after flashing the
         * image to it, it will not have bonded to any device. So, no LTK will
         * be associated with it. So, set the diversifier to 0.
         */
        g_btcar_data.diversifier = 0;

        /* Write the same to NVM. */
        Nvm_Write(&g_btcar_data.diversifier, sizeof(g_btcar_data.diversifier),
                  NVM_OFFSET_SM_DIV);

        /* Write Gap data to NVM */
        GapInitWriteDataToNVM(&offset);

    }

    /* Read HID service data from NVM if the devices are bonded and  
     * update the offset with the number of word of NVM required by 
     * this service
     */
    HidReadDataFromNVM(g_btcar_data.bonded, &offset);

    /* Read Battery service data from NVM if the devices are bonded and  
     * update the offset with the number of word of NVM required by 
     * this service
     */
    BatteryReadDataFromNVM(g_btcar_data.bonded, &offset);

    /* Read Scan Parameter service data from NVM if the devices are bonded and  
     * update the offset with the number of word of NVM required by 
     * this service
     */
    ScanParamReadDataFromNVM(g_btcar_data.bonded, &offset);
}

#ifndef __NO_IDLE_TIMEOUT__

/*-----------------------------------------------------------------------------*
 *  NAME
 *      IdleTimerHandler
 *
 *  DESCRIPTION
 *      This function is used to handle IDLE timer.expiry in connected states.
 *      At the expiry of this timer, application shall disconnect with the 
 *      host and shall move to btcar_idle' state.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
 
static void IdleTimerHandler(timer_id tid)
{
    /* Trigger Disconnect and move to CON_DISCONNECTING state */

    if(tid == g_btcar_data.app_tid)
    {
        g_btcar_data.app_tid = TIMER_INVALID;

        if(g_btcar_data.state == btcar_connected)
        {
            appSetState(btcar_disconnecting);
        
            /* Reset circular buffer queue and ignore any pending key strokes */
            resetQueueData();
        }
    } /* Else ignore the timer expiry, it may be due to a race condition */
}

#endif /* __NO_IDLE_TIMEOUT__ */

/*-----------------------------------------------------------------------------*
 *  NAME
 *      resetIdleTimer
 *
 *  DESCRIPTION
 *      This function deletes and re-creates the idle timer. The idle timer is
 *      reset after sending all the reports generated by user action.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void resetIdleTimer(void)
{
    /* Reset Idle timer */
    TimerDelete(g_btcar_data.app_tid);

    g_btcar_data.app_tid = TIMER_INVALID;

#ifndef __NO_IDLE_TIMEOUT__

    g_btcar_data.app_tid = TimerCreate(CONNECTED_IDLE_TIMEOUT_VALUE, 
                                    TRUE, IdleTimerHandler);

#endif /* __NO_IDLE_TIMEOUT__ */

}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      requestConnParamUpdate
 *
 *  DESCRIPTION
 *      This function is used to send L2CAP_CONNECTION_PARAMETER_UPDATE_REQUEST
 *      to the remote device when an earlier sent request had failed.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void requestConnParamUpdate(timer_id tid)
{
    ble_con_params btcar_pref_conn_params;
    
    if(g_btcar_data.conn_param_update_tid == tid)
    {
        g_btcar_data.conn_param_update_tid= TIMER_INVALID;
        
        btcar_pref_conn_params.con_max_interval  = PREFERRED_MAX_CON_INTERVAL;
        btcar_pref_conn_params.con_min_interval  = PREFERRED_MIN_CON_INTERVAL;
        btcar_pref_conn_params.con_slave_latency = PREFERRED_SLAVE_LATENCY;
        btcar_pref_conn_params.con_super_timeout = PREFERRED_SUPERVISION_TIMEOUT;

        /* Send a connection parameter update request only if the remote device
         * has not entered 'suspend' state.
         */
        if(!HidIsStateSuspended())
        {
            if(LsConnectionParamUpdateReq(&(g_btcar_data.con_bd_addr), 
                                                    &btcar_pref_conn_params))
            {
                ReportPanic(app_panic_con_param_update);
            }
            g_btcar_data.conn_param_update_cnt++;
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      appInitStateExit
 *
 *  DESCRIPTION
 *      This function is called upon exiting from btcar_init state. The 
 *      application starts advertising after exiting this state.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void appInitStateExit(void)
{
    StartHardware();

    /* Application will start advertising upon exiting btcar_init state. So,
     * update the whitelist.
     */
    AppUpdateWhiteList();
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      appSetState
 *
 *  DESCRIPTION
 *      This function is used to set the state of the application.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
 
static void appSetState(btcar_state new_state)
{
    /* Check if the new state to be set is not the same as the present state
     * of the application.
     */
    uint16 old_state = g_btcar_data.state;
    
    if (old_state != new_state)
    {
        /* Handle exiting old state */
        switch (old_state)
        {
            case btcar_init:
                appInitStateExit();
            break;

            case btcar_fast_advertising:
                /* Nothing to do. */
            break;

            case btcar_slow_advertising:
                /* Nothing to do. */
            break;

            case btcar_connected:
                /* Nothing to do. */
            break;

            case btcar_disconnecting:
                /* Nothing to do. */
            break;

            case btcar_passkey_input:
                /* Nothing to do. */
            break;

            case btcar_idle:
                /* Nothing to do. */
            break;

            default:
                /* Nothing to do. */    
            break;
        }

        /* Set new state */
        g_btcar_data.state = new_state;

        /* Handle entering new state */
        switch (new_state)
        {

            case btcar_direct_advert:
                /* Directed advertisement doesn't use any timer. Directed
                 * advertisements are done for 1.28 seconds always.
                 */
                g_btcar_data.advert_timer_value = TIMER_INVALID;
                GattStartAdverts(FALSE, gap_mode_connect_directed);
            break;

            case btcar_fast_advertising:
                GattTriggerFastAdverts();
                if(!g_btcar_data.bonded)
                {
                    EnablePairLED(TRUE);
                }
            break;

            case btcar_slow_advertising:
                GattStartAdverts(FALSE, gap_mode_connect_undirected);
            break;

            case btcar_connected:
                /* Common things to do upon entering btcar_connected state */
                
                /* Cancel Discoverable or Reconnection timer running in 
                 * ADVERTISING state and start IDLE timer */
                resetIdleTimer();

                /* Disable the Pair LED, if enabled. */
                EnablePairLED(FALSE);
            break;

            case btcar_passkey_input:
                /* After entering passkey_input state, the user will start
                 * typing in the passkey.
                 */
            break;

            case btcar_disconnecting:
                GattDisconnectReq(g_btcar_data.st_ucid);
            break;

            case btcar_idle:
                /* Disable the Pair LED, if enabled */
                EnablePairLED(FALSE);
            break;

            default:
            break;
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      appStartAdvert
 *
 *  DESCRIPTION
 *      This function is used to start directed advertisements if a valid
 *      reconnection address has been written by the remote device. Otherwise,
 *      it starts fast undirected advertisements.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void appStartAdvert(void)
{
    btcar_state advert_type;

    if(g_btcar_data.bonded && 
       !IsAddressResolvableRandom(&g_btcar_data.bonded_bd_addr) &&
       !IsAddressNonResolvableRandom(&g_btcar_data.bonded_bd_addr))
    {
        advert_type = btcar_direct_advert;

#ifdef __GAP_PRIVACY_SUPPORT__

        /* If re-connection address is not written, start fast undirected 
         * advertisements 
         */
        if(!GapIsReconnectionAddressValid())
        {
            advert_type = btcar_fast_advertising;
        }

#endif /* __GAP_PRIVACY_SUPPORT__ */
    }
    else /* Start with fast advertisements */
    {
        advert_type = btcar_fast_advertising;
    }

    appSetState(advert_type);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattAddDBCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_ADD_DB_CFM
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void handleSignalGattAddDBCfm(GATT_ADD_DB_CFM_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        case btcar_init:
        {
            if(p_event_data->result == sys_status_success)
            {
                appStartAdvert();
            }

            else
            {
                /* Don't expect this to happen */
                ReportPanic(app_panic_db_registration);
            }
        }
        break;

        default:
            ReportPanic(app_panic_invalid_state);
    }
}

/*---------------------------------------------------------------------------
 *
 *  NAME
 *      handleSignalLmEvConnectionComplete
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_CONNECTION_COMPLETE.
 *
 *  RETURNS
 *      Nothing.
 *
 
*----------------------------------------------------------------------------*/
static void handleSignalLmEvConnectionComplete(
                                     LM_EV_CONNECTION_COMPLETE_T *p_event_data)
{
    /* Store the connection parameters. */
     g_btcar_data.conn_interval = p_event_data->data.conn_interval;
     g_btcar_data.conn_latency = p_event_data->data.conn_latency;
     g_btcar_data.conn_timeout = p_event_data->data.supervision_timeout;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattConnectCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CONNECT_CFM
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalGattConnectCfm(GATT_CONNECT_CFM_T* event_data)
{

#ifdef __FORCE_MITM__

    gap_mode_discover discover_mode = gap_mode_discover_limited;

#endif /* __FORCE_MITM__ */

    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        case btcar_fast_advertising:
        case btcar_slow_advertising:
        case btcar_direct_advert:
        {
            g_btcar_data.advert_timer_value = TIMER_INVALID;

            if(event_data->result == sys_status_success)
            {
                /* Store received UCID */
                g_btcar_data.st_ucid = event_data->cid;

                if(g_btcar_data.bonded && 
                    IsAddressResolvableRandom(&g_btcar_data.bonded_bd_addr) &&
                    (SMPrivacyMatchAddress(&event_data->bd_addr,
                                            g_btcar_data.central_device_irk.irk,
                                            MAX_NUMBER_IRK_STORED, 
                                            MAX_WORDS_IRK) < 0))
                {
                    /* Application was bonded to a remote device with resolvable
                     * random address and application has failed to resolve the
                     * remote device address to which we just connected So disc-
                     * -onnect and start advertising again
                     */

                    /* Disconnect if we are connected */
                    appSetState(btcar_disconnecting);

                }

                else
                {
                    g_btcar_data.con_bd_addr = event_data->bd_addr;

                    
                    /* Security supported by the remote HID host */
                    
                    /* Request Security only if remote device address is not
                     *resolvable random
                     */
                    if(!IsAddressResolvableRandom(&g_btcar_data.con_bd_addr))
                    {
                    
                        SMRequestSecurityLevel(&g_btcar_data.con_bd_addr);
                        
                    }

#ifdef __FORCE_MITM__

                    else
                    {
                        if(g_btcar_data.bonded)
                        {
                            discover_mode = gap_mode_discover_no;
                        }
                        /* Change the authentication requirements to 'MITM
                         * required'.
                         */
                        GapSetMode(gap_role_peripheral, discover_mode,
                                 gap_mode_connect_undirected, gap_mode_bond_yes,
                                                gap_mode_security_authenticate);
                    }
                    
#endif /* __FORCE_MITM__ */
                    
                   
                    appSetState(btcar_connected);
                }
                
            }
            else
            {

                if((event_data)->result == 
                    HCI_ERROR_DIRECTED_ADVERTISING_TIMEOUT)
                {
                    /* Case where bonding has been removed when directed 
                     * advertisements were on-going
                     */
                    if(g_btcar_data.pairing_button_pressed)
                    {
                        /* Reset and clear the whitelist */
                        LsResetWhiteList();
                    }

                    /* Trigger undirected advertisements as directed 
                     * advertisements have timed out.
                     */
                    appSetState(btcar_fast_advertising);
                }
                else
                {
                    ReportPanic(app_panic_connection_est);
                }
            }
        }
        break;
        
        default:
            ReportPanic(app_panic_invalid_state);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattCancelConnectCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CANCEL_CONNECT_CFM
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalGattCancelConnectCfm(void)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        /* GATT_CANCEL_CONNECT_CFM is received when undirected advertisements
         * are stopped.
         */
        case btcar_fast_advertising:
        case btcar_slow_advertising:
        {
            if(g_btcar_data.pairing_button_pressed)
            {
                /* Reset and clear the whitelist */
                LsResetWhiteList();

                g_btcar_data.pairing_button_pressed = FALSE;
                
                if(g_btcar_data.state == btcar_fast_advertising)
                {
                     GattTriggerFastAdverts();
                     /* Enable LED(to blink) as pairing has been removed */
                     EnablePairLED(TRUE);
                }
                else
                {
                    appSetState(btcar_fast_advertising);
                }
            }
            else
            {
                if(g_btcar_data.state == btcar_fast_advertising)
                {
                    appSetState(btcar_slow_advertising);
                }
                else if(g_btcar_data.start_adverts)
                {
                    /* Reset the start_adverts flag and start directed/
                     * fast advertisements
                     */
                    g_btcar_data.start_adverts = FALSE;
                    
                    appStartAdvert();
                }
                else
                {
                    /* Slow undirected advertisements have been stopped. Device 
                     * shall move to IDLE state until next user activity or
                     * pending notification.                     
                     */
                    appSetState(btcar_idle);
                }
            }
        }
        break;
        
        default:
            ReportPanic(app_panic_invalid_state);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattAccessInd
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_ACCESS_IND
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalGattAccessInd(GATT_ACCESS_IND_T *event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        case btcar_connected:
        case btcar_passkey_input:
            GattHandleAccessInd(event_data);
        break;

        default:
            /* Control should never reach here. */
        break;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLmEvDisconnectComplete
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_DISCONNECT_COMPLETE.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalLmEvDisconnectComplete(
                                HCI_EV_DATA_DISCONNECT_COMPLETE_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        /* LM_EV_DISCONNECT_COMPLETE is received by the application when
         * 1. The remote side initiates a disconnection. The remote side can
         *    disconnect in any of the following states:
         *    a. btcar_connected.
         *    b. btcar_passkey_input.
         *    c. btcar_disconnecting.
         * 2. The keyboard application itself initiates a disconnection. The
         *    keyboard application will have moved to btcar_disconnecting state
         *    when it initiates a disconnection.
         * 3. There is a link loss.
         */
        case btcar_disconnecting:
        case btcar_connected:
        case btcar_passkey_input:
        {
            /* Initialize the keyboard data structure. This will expect that the
             * remote side re-enables encryption on the re-connection though it
             * was a link loss.
             */
            DataInit();

            /* The keyboard needs to advertise after disconnection in the
             * following cases.
             * 1. If there was a link loss.
             * 2. If the keyboard is not bonded to any host(A central device may
             *    have connected, read the device name and disconnected).
             * 3. If a new data is there in the queue.
             * Otherwise, it can move to btcar_idle state.
             */
            if(p_event_data->reason == HCI_ERROR_CONN_TIMEOUT ||
               !g_btcar_data.bonded || g_btcar_data.data_pending)
            {
                appStartAdvert();
            }
            else
            {
                appSetState(btcar_idle);
            }
            
        }        
        break;
        
        default:
            /* Control should never reach here. */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLMEncryptionChange
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_ENCRYPTION_CHANGE
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalLMEncryptionChange(LM_EVENT_T *event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        case btcar_connected:
        case btcar_passkey_input:
        {            
            HCI_EV_DATA_ENCRYPTION_CHANGE_T *pEvDataEncryptChange = 
                                        &event_data->enc_change.data;

            if(pEvDataEncryptChange->status == HCI_SUCCESS)
            {
                g_btcar_data.encrypt_enabled = pEvDataEncryptChange->enc_enable;

                /* Delete the bonding chance timer */
                TimerDelete(g_btcar_data.bonding_reattempt_tid);
                g_btcar_data.bonding_reattempt_tid = TIMER_INVALID;
            }

            /* If the current connection parameters being used don't comply with
             * the application's preferred connection parameters and the timer 
             * is not running and , start timer to trigger Connection Parameter 
             * Update procedure
             */
            if(( g_btcar_data.conn_param_update_tid == TIMER_INVALID) &&
               ( g_btcar_data.conn_interval < PREFERRED_MIN_CON_INTERVAL ||
                 g_btcar_data.conn_interval > PREFERRED_MAX_CON_INTERVAL
#if PREFERRED_SLAVE_LATENCY
                ||  g_btcar_data.conn_latency < PREFERRED_SLAVE_LATENCY
#endif
               )
              )
            {
                /* Set the num of conn parameter update attempts to zero */
                g_btcar_data.conn_param_update_cnt = 0;

                /* Start timer to trigger Connection Paramter Update procedure */
                g_btcar_data.conn_param_update_tid = 
                                        TimerCreate(GAP_CONN_PARAM_TIMEOUT,
                                                TRUE, requestConnParamUpdate);
            } /* Else at the expiry of timer Connection parameter 
               * update procedure will get triggered
               */

            if(g_btcar_data.encrypt_enabled)
            {
                /* Update battery status at every connection instance. It
                 * may not be worth updating timer more ofter, but again it 
                 * will primarily depend upon application requirements 
                 */
/*                BatteryUpdateLevel(g_btcar_data.st_ucid); */
                
                {
                    uint16 ucid;
                    uint8 cur_bat_level;
                
                    ucid = g_btcar_data.st_ucid;
                    
                    /* Read the battery level */
                    cur_bat_level = 88;
                
                    if((ucid != GATT_INVALID_UCID))
                    {
            
                        GattCharValueNotification(ucid, 
                                                  HANDLE_BATT_LEVEL, 
                                                  1, &cur_bat_level);
                        
                    }
                }
                

#ifndef __NO_IDLE_TIMEOUT__

                ScanParamRefreshNotify(g_btcar_data.st_ucid);

#endif /* __NO_IDLE_TIMEOUT__ */

                /* If any keys are in the queue, send them */
                if(g_btcar_data.data_pending)
                {

                    /* Send the keys from queue only if a transmission is not
                     * already in progress.
                     */
                    if(AppCheckNotificationStatus()&& 
                       !g_btcar_data.data_tx_in_progress &&
                       !g_btcar_data.waiting_for_fw_buffer)
                    {
                        SendKeyStrokesFromQueue();
                    }
                }
    
            }
        }
        break;

        default:
            ReportPanic(app_panic_invalid_state);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmKeysInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_KEYS_IND
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalSmKeysInd(SM_KEYS_IND_T *event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        case btcar_connected:
        case btcar_passkey_input:
        {
            /* Store the diversifier which will be used for accepting/rejecting
             * the encryption requests.
             */
            g_btcar_data.diversifier = (event_data->keys)->div;
            
            /* Write the new diversifier to NVM */
            Nvm_Write(&g_btcar_data.diversifier,
                      sizeof(g_btcar_data.diversifier), NVM_OFFSET_SM_DIV);

            /* Store IRK if the connected host is using random resolvable 
             * address. IRK is used afterwards to validate the identity of 
             * connected host.
             */

            if(IsAddressResolvableRandom(&g_btcar_data.con_bd_addr)) 
            {
                MemCopy(g_btcar_data.central_device_irk.irk,
                                    (event_data->keys)->irk,
                                    MAX_WORDS_IRK);
                
                /* store IRK in NVM */
                Nvm_Write(g_btcar_data.central_device_irk.irk, 
                                        MAX_WORDS_IRK, NVM_OFFSET_SM_IRK);
            }
        }
        break;
        
        default:
            ReportPanic(app_panic_invalid_state);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmPairingAuthInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_PAIRING_AUTH_IND. This message will
 *      only be received when the peer device is initiating 'Just Works' 
 8      pairing.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalSmPairingAuthInd(SM_PAIRING_AUTH_IND_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        case btcar_connected:
        {
            /* Authorise the pairing request if the Keyboard is NOT bonded */
            if(!g_btcar_data.bonded)
            {
                SMPairingAuthRsp(p_event_data->data, TRUE);
            }
            else /* Otherwise Reject the pairing request */
            {
                SMPairingAuthRsp(p_event_data->data, TRUE); /* AV: FALSE -> TRUE */
            }
        }
        break;

        default:
            ReportPanic(app_panic_invalid_state);
        break;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmSimplePairingCompleteInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_SIMPLE_PAIRING_COMPLETE_IND
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalSmSimplePairingCompleteInd(
                                 SM_SIMPLE_PAIRING_COMPLETE_IND_T *event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        case btcar_connected:
        /* Case when MITM pairing fails, encryption change indication will not
         * have come to application. So, the application will still be in
         * btcar_passkey_input state.
         */
        case btcar_passkey_input:
        {
            if(event_data->status == sys_status_success)
            {
                g_btcar_data.bonded = TRUE;
                g_btcar_data.bonded_bd_addr = event_data->bd_addr;

                /* Store bonded host typed bd address to NVM */

                /* Write one word bonded flag */
                Nvm_Write((uint16*)&g_btcar_data.bonded,
                           sizeof(g_btcar_data.bonded), NVM_OFFSET_BONDED_FLAG);

                /* Write typed bd address of bonded host */
                Nvm_Write((uint16*)&g_btcar_data.bonded_bd_addr,
                sizeof(TYPED_BD_ADDR_T), NVM_OFFSET_BONDED_ADDR);

                /* White list is configured with the Bonded host address */
                AppUpdateWhiteList();
            }
            else
            {
                /* Pairing has failed.
                 * 1. If pairing has failed due to repeated attempts, the 
                 *    application should immediately disconnect the link.
                 * 2. The application was bonded and pairing has failed.
                 *    Since the application was using whitelist, so the remote 
                 *    device has same address as our bonded device address.
                 *    The remote connected device may be a genuine one but 
                 *    instead of using old keys, wanted to use new keys. We 
                 *    don't allow bonding again if we are already bonded but we
                 *    will give some time to the connected device to encrypt the
                 *    link using the old keys. if the remote device encrypts the
                 *    link in that time, it's good. Otherwise we will disconnect
                 *    the link.
                 */
                if(event_data->status == sm_status_repeated_attempts)
                {
                    appSetState(btcar_disconnecting);
                }
                else if(g_btcar_data.bonded)
                {
                    g_btcar_data.encrypt_enabled = FALSE;
                    g_btcar_data.bonding_reattempt_tid = 
                                          TimerCreate(
                                               BONDING_CHANCE_TIMER,
                                               TRUE, 
                                               handleBondingChanceTimerExpiry);
                }
            }
        }
        break;
        
        default:
            /* SM_SIMPLE_PAIRING_COMPLETE_IND reaches the application after
             * LM_EV_DISCONNECT_COMPLETE when the remote end terminates
             * the connection without enabling encryption or completing pairing.
             * The application will be either in advertising or idle state in
             * this scenario. So, don't panic
             */
        break;
            
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSMpasskeyInputInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_PASSKEY_INPUT_IND
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalSMpasskeyInputInd(void)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        /* When the remote device is displaying a passkey for the first time,
         * application will be in 'connected' state. If an earlier passkey
         * entry failed, the remote device may re-try with a new passkey.
         */
        case btcar_connected:
        case btcar_passkey_input:
        {
            g_btcar_data.pass_key = 0 ;
            appSetState(btcar_passkey_input);
            
/*            SMPasskeyInput(&g_btcar_data.con_bd_addr, &g_btcar_data.pass_key);
            appSetState(btcar_connected);
*/            
            
        }
        break;
        
        default:
            ReportPanic(app_panic_invalid_state);
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsConnParamUpdateCfm
 *
 *  DESCRIPTION
 *      This function handles the signal LS_CONNECTION_PARAM_UPDATE_CFM.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
 
static void handleSignalLsConnParamUpdateCfm(
                            LS_CONNECTION_PARAM_UPDATE_CFM_T *event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        case btcar_connected:
        case btcar_passkey_input:
        {
            if ((event_data->status !=
                    ls_err_none) && (g_btcar_data.conn_param_update_cnt < 
                    MAX_NUM_CONN_PARAM_UPDATE_REQS))
            {
                /* Delete timer if running */
                TimerDelete(g_btcar_data.conn_param_update_tid);
                
                g_btcar_data.conn_param_update_tid = 
                TimerCreate(GAP_CONN_PARAM_TIMEOUT, TRUE, requestConnParamUpdate);
            }
        }
        break;
        
        default:
            ReportPanic(app_panic_invalid_state);
    }
}

/*----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLmConnectionUpdate
 *
 *  DESCRIPTION
 *      This function handles the signal LM_EV_CONNECTION_UPDATE.
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
static void handleSignalLmConnectionUpdate(
                                       LM_EV_CONNECTION_UPDATE_T* p_event_data)
{
    switch(g_btcar_data.state)
    {
        case btcar_connected:
        case btcar_passkey_input:
        case btcar_disconnecting:
        {
            /* Store the new connection parameters. */
            g_btcar_data.conn_interval = p_event_data->data.conn_interval;
            g_btcar_data.conn_latency = p_event_data->data.conn_latency;
            g_btcar_data.conn_timeout = p_event_data->data.supervision_timeout;
        }
        break;

        default:
            /* Connection parameter update indication received in unexpected
             * application state.
             */
            ReportPanic(app_panic_invalid_state);
        break;
    }
}



/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsConnParamUpdateInd
 *
 *  DESCRIPTION
 *      This function handles the signal LS_CONNECTION_PARAM_UPDATE_IND
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalLsConnParamUpdateInd(
                                 LS_CONNECTION_PARAM_UPDATE_IND_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        case btcar_connected:
        case btcar_passkey_input:
        {
            /* The application had already received the new connection 
             * parameters while handling event LM_EV_CONNECTION_UPDATE.
             * Check if new parameters comply with application preferred 
             * parameters. If not, application shall trigger Connection 
             * parameter update procedure 
             */

            if( g_btcar_data.conn_interval < PREFERRED_MIN_CON_INTERVAL ||
                g_btcar_data.conn_interval > PREFERRED_MAX_CON_INTERVAL
#if PREFERRED_SLAVE_LATENCY
               ||  g_btcar_data.conn_latency < PREFERRED_SLAVE_LATENCY
#endif
              )
            {
                /* Delete timer if running */
                TimerDelete(g_btcar_data.conn_param_update_tid);
                /* Set the connection parameter update attempts counter to 
                 * zero 
                 */
                g_btcar_data.conn_param_update_cnt = 0;

                /* Start timer to trigger Connection Paramter Update procedure */
                g_btcar_data.conn_param_update_tid = TimerCreate(
                          GAP_CONN_PARAM_TIMEOUT, TRUE, requestConnParamUpdate);

            }
        }
        break;

        default:
            ReportPanic(app_panic_invalid_state);

    }
 }

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalGattCharValNotCfm
 *
 *  DESCRIPTION
 *      This function handles the signal GATT_CHAR_VAL_NOT_CFM which is received
 *      as acknowledgement from the firmware that the data sent from application
 *      has been queued to be transmitted
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleSignalGattCharValNotCfm(GATT_CHAR_VAL_IND_CFM_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        case btcar_connected:
        case btcar_passkey_input:
        {
            /* Check whether the notification is for the data sent from the
             * queue. GATT_CHAR_VAL_NOT_CFM comes for all the notification
             * requests sent from the application, in this case for battery
             * level and scan refresh notifications also
             */
            g_reports_sent[8] ++;

            if(p_event_data->handle == HANDLE_HID_INPUT_REPORT)
            {
                /* Firmware has completed handling our request for queueing the
                 * data to be sent to the remote device. If the data was
                 * successfully queued, we can send the remaining data in the
                 * application queue. If not, application needs to retry sending
                 * the same data again.
                 */
                g_btcar_data.data_tx_in_progress = FALSE;
                
                g_reports_sent[0] ++;
                
                if(p_event_data->result == sys_status_success)
                {
                    g_reports_sent[1] ++;
                    /* Update the start index of the queue. */
                    g_btcar_data.pending_key_strokes.start_idx = 
                    (g_btcar_data.pending_key_strokes.start_idx + 1) %
                    MAX_PENDING_KEY_STROKES;

                    /* Decrement the number of pending key strokes. If more
                     * key strokes are in queue, send them
                     */
                    if(g_btcar_data.pending_key_strokes.num)
                    {
                        -- g_btcar_data.pending_key_strokes.num;
                        g_reports_sent[2] ++;
    
                    }

                    /* If all the data from the application queue is emptied,
                     * reset the data_pending flag.
                     */
                    if(!g_btcar_data.pending_key_strokes.num)
                    {
                        /* All the reports in the queue have been sent. Reset the
                         * idle timer and set the data pending flag to false
                         */
                        resetIdleTimer();
                        g_btcar_data.data_pending = FALSE;
                        g_reports_sent[3] ++;

                    }

                    /* If more data is there in the queue, send it. */
                    if(g_btcar_data.data_pending)
                    {
                        g_reports_sent[4] ++;

                        if(g_btcar_data.encrypt_enabled &&
                                                   AppCheckNotificationStatus())
                        {
                            g_reports_sent[6] ++;
                            SendKeyStrokesFromQueue();
                        }
                    }
                }

                else /* The firmware has not added the data in it's queue to be
                      * sent to the remote device. Mostly it'll have returned
                      * gatt_status_busy. Wait for the firmware to empty it's
                      * buffers by sending data to the remote device. This is
                      * indicated by firmware to application by radio event
                      * tx_data
                      */
                {
                    g_btcar_data.waiting_for_fw_buffer = TRUE;
                    g_reports_sent[7] ++;

                    /* Enable the application to receive confirmation that the
                     * data has been transmitted to the remote device by
                     * configuring notifications on tx_data radio events
                     */
                    LsRadioEventNotification(g_btcar_data.st_ucid,
                                                           radio_event_tx_data);
                }
            }
        }
        break;

        default:
        {
            g_reports_sent[9] ++;
            ReportPanic(app_panic_invalid_state);
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalLsRadioEventInd
 *
 *  DESCRIPTION
 *      This function handles the signal LS_RADIO_EVENT_IND
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void handleSignalLsRadioEventInd(void)
{
    /* Radio events notification would have been enabled after the firmware
     * buffers are full and hence incapable of receiving any more notification
     * requests from the application. Disable them now
     */
    LsRadioEventNotification(g_btcar_data.st_ucid, radio_event_none);    
    
    if(g_btcar_data.waiting_for_fw_buffer)
    {
        g_btcar_data.waiting_for_fw_buffer = FALSE;
        if(g_btcar_data.data_pending)
        {
            /* Not necessary to check the data_tx_in_progress flag as this
             * shouldn't be set when we are waiting for firmware to free it's
             * buffers.
             */
            if(g_btcar_data.encrypt_enabled && AppCheckNotificationStatus())
            {
                SendKeyStrokesFromQueue();
            }
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleSignalSmDivApproveInd
 *
 *  DESCRIPTION
 *      This function handles the signal SM_DIV_APPROVE_IND.
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void handleSignalSmDivApproveInd(SM_DIV_APPROVE_IND_T *p_event_data)
{
    /* Handling signal as per current state */
    switch(g_btcar_data.state)
    {
        
        /* Request for approval from application comes only when pairing is not
         * in progress. So, this event can't come in btcar_passkey_input state
         */
        case btcar_connected:
        {
            sm_div_verdict approve_div = SM_DIV_REVOKED;
            
            /* Check whether the application is still bonded(bonded flag gets
             * reset upon 'connect' button press by the user). Then check whether
             * the diversifier is the same as the one stored by the application
             */
            if(g_btcar_data.bonded)
            {
                if(g_btcar_data.diversifier == p_event_data->div)
                {
                    approve_div = SM_DIV_APPROVED;
                }
            }

            SMDivApproval(p_event_data->cid, approve_div);
        }
        break;

        default:
            ReportPanic(app_panic_invalid_state);
    }
}


/*----------------------------------------------------------------------------*
 *  NAME
 *      handleBondingChanceTimerExpiry
 *
 *  DESCRIPTION
 *      This function is handle the expiry of bonding chance timer.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
static void handleBondingChanceTimerExpiry(timer_id tid)
{
    if(g_btcar_data.bonding_reattempt_tid== tid)
    {
        g_btcar_data.bonding_reattempt_tid= TIMER_INVALID;
        /* The bonding chance timer has expired. This means the remote device
         * has not encrypted the link using old keys. Disconnect the link.
         */
        appSetState(btcar_disconnecting);
    }/* Else it may be due to some race condition. Ignore it. */
}

/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*----------------------------------------------------------------------------*
 *  NAME
 *      ReportPanic
 *
 *  DESCRIPTION
 *      This function raises the panic.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/
extern void ReportPanic(app_panic_code panic_code)
{
    /* If we want any debug prints, we can put them here */
    Panic(panic_code);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppIsDeviceBonded
 *
 *  DESCRIPTION
 *      This function returns the status wheather the connected device is 
 *      bonded or not.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern bool AppIsDeviceBonded(void)
{
    return (g_btcar_data.bonded);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppUpdateWhiteList
 *
 *  DESCRIPTION
 *      This function updates the whitelist with bonded device address if
 *      it's not private, and also reconnection address when it has been written
 *      by the remote device.
 *
 *----------------------------------------------------------------------------*/

extern void AppUpdateWhiteList(void)
{
    LsResetWhiteList();
    
    if(g_btcar_data.bonded && 
            (!IsAddressResolvableRandom(&g_btcar_data.bonded_bd_addr)) &&
            (!IsAddressNonResolvableRandom(&g_btcar_data.bonded_bd_addr)))
    {
        /* If the device is bonded and bonded device address is not private
         * (resolvable random or non-resolvable random), configure White list
         * with the Bonded host address 
         */
         
        if(LsAddWhiteListDevice(&g_btcar_data.bonded_bd_addr) != ls_err_none)
        {
            ReportPanic(app_panic_add_whitelist);
        }
    }

#ifdef __GAP_PRIVACY_SUPPORT__

    if(GapIsReconnectionAddressValid())
    {
        TYPED_BD_ADDR_T temp_addr;

        temp_addr.type = ls_addr_type_random;
        MemCopy(&temp_addr.addr, GapGetReconnectionAddress(), sizeof(BD_ADDR_T));
        if(LsAddWhiteListDevice(&temp_addr) != ls_err_none)
        {
            ReportPanic(app_panic_add_whitelist);
        }
    }

#endif /* __GAP_PRIVACY_SUPPORT__ */

}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      ProcessReport
 *
 *  DESCRIPTION
 *      This function processes the raw reports received from PIO controller
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
 
extern void ProcessReport(uint8* raw_report)
{
    if(g_btcar_data.state == btcar_passkey_input)
    {
        /* Normally while entering a passkey, the user will enter one
         * key after another. So, it is safe to assume that only one key
         * will be pressed at a time. When only one key is pressed, its
         * value will be the third byte of the report which is the first
         * data byte.
         */
        uint8 key_pressed = raw_report[2];

        if(key_pressed)
        {
            if((key_pressed > USAGE_ID_KEY_Z) && 
                                 (key_pressed < USAGE_ID_KEY_ENTER))
            {
                /* Each time a new key press is detected during passkey
                 * entry, the passkey value needs to be updated. The
                 * earlier entered digit is multiplied by 10 and the
                 * new key is added.
                 */

                g_btcar_data.pass_key = g_btcar_data.pass_key * 10 + 
                                     (key_pressed - USAGE_ID_KEY_Z)%10;
            }
            else if(key_pressed == USAGE_ID_KEY_ENTER)
            {
                /* Passkey will be non-zero if any number keys are 
                 * pressed. Send the passkey response if the passkey
                 * is non-zero. Otherwise, send a negative passkey
                 * response(When enter is pressed without pressing
                 * any key or only alphabetic keys are pressed
                 * followed by enter key press).
                 */

                /* AV: Set breakpoint here */
                if(g_btcar_data.pass_key)
                {
                    SMPasskeyInput(&g_btcar_data.con_bd_addr, 
                                                 &g_btcar_data.pass_key);
                }
                else
                {
                    SMPasskeyInputNeg(&g_btcar_data.con_bd_addr);
                }
                
                /* Now that the passkey is sent, change the application
                 * state back to 'connected'
                 */
                appSetState(btcar_connected);
            }
        }
    }

    else
    {
#if 0        
        if(FormulateReportsFromRaw(raw_report))
        {
        
            if(g_btcar_data.state == btcar_connected && 
                                             g_btcar_data.encrypt_enabled)
            {
                /* If the data transmission is not already in progress,
                 * then send the stored keys from queue
                 */                  
                if(AppCheckNotificationStatus()&&
                   !g_btcar_data.data_tx_in_progress &&
                   !g_btcar_data.waiting_for_fw_buffer)
                {
                    SendKeyStrokesFromQueue();
                }
            }
             /* If the keyboard is slow advertising, start fast
             * advertisements
             */
            else if(g_btcar_data.state == btcar_slow_advertising)
            {
                g_btcar_data.start_adverts = TRUE;

                /* Delete the advertisement timer */
                TimerDelete(g_btcar_data.app_tid);
                g_btcar_data.app_tid = TIMER_INVALID;
                g_btcar_data.advert_timer_value = TIMER_INVALID;

                GattStopAdverts();
            }

            /* If the keyboard is already fast advertising, we need 
             * not do anything. If the keyboard state is btcar_init,
             * it will start advertising. If the keyboard is in
             * btcar_disconnecting state, it will start advertising
             * after disconnection is complete and it finds that
             * data is pending in the queue.
             */
            else if(g_btcar_data.state == btcar_idle)
            {
                appStartAdvert();
            }
        }
#endif /* 0 */        
    }
}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      AddKeyStrokeToQueue
 *
 *  DESCRIPTION
 *      This function is used to add key strokes to circular queue maintained 
 *      by application. The key strokes will get notified to Host machine once
 *      notifications are enabled by the remote client.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void AddKeyStrokeToQueue(uint8 report_id, uint8 *report,
                                                            uint8 report_length)
{
    uint8 *p_temp_input_report = NULL;
    uint8 add_idx;

    /* Add new key stroke to the end of circular queue. If Max circular queue 
     * length has reached the oldest key stroke will get overwritten 
     */
    add_idx = (g_btcar_data.pending_key_strokes.start_idx + 
    g_btcar_data.pending_key_strokes.num)% MAX_PENDING_KEY_STROKES;

    SET_CQUEUE_INPUT_REPORT_ID(add_idx, report_id);

    p_temp_input_report = GET_CQUEUE_INPUT_REPORT_REF(add_idx);

    MemCopy(p_temp_input_report, report, report_length);

    if(g_btcar_data.pending_key_strokes.num < MAX_PENDING_KEY_STROKES)
        ++ g_btcar_data.pending_key_strokes.num;
    else /* Oldest key stroke overwritten, move the index */
        g_btcar_data.pending_key_strokes.start_idx = (add_idx + 1) 
                                                  % MAX_PENDING_KEY_STROKES;

    g_btcar_data.data_pending = TRUE;
}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      HandlePairingButtonPress
 *
 *  DESCRIPTION
 *      This function is called when the pairing removal button is pressed down
 *      for a period specified by PAIRING_REMOVAL_TIMEOUT
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
extern void HandlePairingButtonPress(timer_id tid)
{
    if(tid == pairing_removal_tid)
    {
        /* The firmware will have deleted the pairing removal timer. Set the
         * value of the timer maintained by application to TIMER_INVALID
         */
        pairing_removal_tid = TIMER_INVALID;

        /* Handle pairing button press only if the last pairing button press
         * handling is complete(app_data.pairing_button_pressed will be set
         * to false in such a case)
         */
        if(!g_btcar_data.pairing_button_pressed)
        {
            /* Pairing button pressed for PAIRING_REMOVAL_TIMEOUT period -
             * Remove bonding information
             */
            g_btcar_data.bonded = FALSE;

            /* Write bonded status to NVM */
            Nvm_Write((uint16*)&g_btcar_data.bonded, sizeof(g_btcar_data.bonded), 
                                                        NVM_OFFSET_BONDED_FLAG);

            /* Reset circular buffer queue and ignore any pending key strokes */
            resetQueueData();

            /* Disconnect if we are connected */
            if((g_btcar_data.state == btcar_connected) || (g_btcar_data.state == 
                btcar_passkey_input))
            {
                /* Reset and clear the whitelist */
                LsResetWhiteList();
                
                appSetState(btcar_disconnecting);
            }
            else
            {
                /* Initialise application and services data structures */
                DataInit();

                if(IS_BTCAR_ADVERTISING())
                {
                    g_btcar_data.pairing_button_pressed = TRUE;

                    if(g_btcar_data.state != btcar_direct_advert)
                    {
                        /* Delete the advertising timer as in race conditions, 
                         * it may expire before GATT_CANCEL_CONNECT_CFM reaches 
                         * the application. If this happens, we end up calling
                         * GattStopAdverts() again
                         */

                        TimerDelete(g_btcar_data.app_tid);
                        g_btcar_data.app_tid = TIMER_INVALID;
                        g_btcar_data.advert_timer_value = TIMER_INVALID;

                        /* Stop advertisements as it may be making use of white 
                         * list 
                         */
                        GattStopAdverts();
                    }
                }
                
                /* For btcar_idle state */
                else if(g_btcar_data.state == btcar_idle)
                {
                    /* Reset and clear the whitelist */
                    LsResetWhiteList();

                    g_btcar_data.pairing_button_pressed = FALSE;

                    /* Start undirected advertisements */
                    appSetState(btcar_fast_advertising);
                    
                } /* Else the keyboard will be in btcar_init state. It will
                   * anyways start advertising
                   */
            }
        }
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      SendKeyStrokesFromQueue
 *
 *  DESCRIPTION
 *      This function is used to send key strokes in circular queue 
 *      maintained by application. 
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void SendKeyStrokesFromQueue(void)
{
    uint8 start_idx = g_btcar_data.pending_key_strokes.start_idx;

    if(HidSendInputReport(g_btcar_data.st_ucid,
                        GET_CQUEUE_INPUT_REPORT_ID(start_idx),
                        GET_CQUEUE_INPUT_REPORT_REF(start_idx)))
    {
        
        /* Set the data being transferred flag to TRUE */
        g_btcar_data.data_tx_in_progress = TRUE;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppCheckNotificationStatus
 *
 *  DESCRIPTION
 *      This function checks the notification status of HID service. First, the
 *      notification status of proprietary HID service is checked followed by
 *      notification on HID service.
 *
 *  RETURNS
 *      TRUE if notification is enabled on proprietary HID service or HID
 *      service.
 *
 *  NOTE
 *      This function can be removed when Proprietary HID service is not
 *      supported. Wherever this function is called, it can be replaced
 *      by a call to HidIsNotificationEnabled().
 *
 *----------------------------------------------------------------------------*/

extern bool AppCheckNotificationStatus(void)
{
    bool notification_status = FALSE;

    /* First check whether notifications are enabled on Prorietary HID service.
     * If not check for the HID service notification status.
     */
    {
        if(HidIsNotificationEnabled())
        {
            notification_status = TRUE;
        }
    }
    return notification_status;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppPowerOnReset
 *
 *  DESCRIPTION
 *      This function is called just after a power-on reset (including after
 *      a firmware panic).
 *
 *      NOTE: this function should only contain code to be executed after a
 *      power-on reset or panic. Code that should also be executed after an
 *      HCI_RESET should instead be placed in the reset() function.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
void AppPowerOnReset(void)
{
    /* Configure the application constants */
}


/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppInit
 *
 *  DESCRIPTION
 *      This function is called after a power-on reset (including after a
 *      firmware panic) or after an HCI Reset has been requested.
 *
 *      NOTE: In the case of a power-on reset, this function is called
 *      after app_power_on_reset.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
void AppInit(sleep_state LastSleepState)
{
    uint16 gatt_database_length;
    uint16 *gatt_database_pointer = NULL;

    /* Initialise the application timers */
    TimerInit(MAX_APP_TIMERS, (void*)app_timers);

    /* Initialise GATT entity */
    GattInit();

    /* Install GATT Server support for the optional Write procedures */
    GattInstallServerWrite();

#ifdef NVM_TYPE_EEPROM
    /* Configure the NVM manager to use I2C EEPROM for NVM store */
    NvmConfigureI2cEeprom();
#elif NVM_TYPE_FLASH
    /* Configure the NVM Manager to use SPI flash for NVM store. */
    NvmConfigureSpiFlash();
#endif /* NVM_TYPE_EEPROM */

    Nvm_Disable();

    /* HID Service Initialisation on Chip reset */
    HidInitChipReset();

    /* Battery Service Initialisation on Chip reset */
    BatteryInitChipReset();

    /* Scan Parameter Service Initialisation on Chip reset */
    ScanParamInitChipReset();

    /* Read persistent storage */
    readPersistentStore();

    /* Tell Security Manager module about the value it needs to initialize it's
     * diversifier to.
     */
    SMInit(g_btcar_data.diversifier);
    
    /* No input/output devices on the btcar     */
    SMSetIOCapabilities(SM_IO_CAP_NO_INPUT_NO_OUTPUT);

    /* Initialise Keyboard application data structure */
    DataInit();

    /* Initialize the queue related variables */
    resetQueueData();

    /* Initialise Hardware to set PIO controller for PIOs scanning */
    InitHardware();

    /* Initialise Keyboard state */
    g_btcar_data.state = btcar_init;

    /* Tell GATT about our database. We will get a GATT_ADD_DB_CFM event when
     * this has completed.
     */
    gatt_database_pointer = GattGetDatabase(&gatt_database_length);
    GattAddDatabaseReq(gatt_database_length, gatt_database_pointer);

}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessSystemEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a system event, such
 *      as a battery low notification, is received by the system.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
void AppProcessSystemEvent(sys_event_id id, void *data)
{
    
    switch(id)
    {
    case sys_event_pio_ctrlr: /* if the event is from the PIO controller. */
    {
#if 0
        /* Process new report */
        ProcessReport(raw_report);
#endif 

    }
    break;
    
    case sys_event_pio_changed:
    {
        HandlePIOChangedEvent(((pio_changed_data*)data)->pio_cause);
    }
    break;

    default:
        /* Do nothing. */
    break;
    }
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      AppProcessLmEvent
 *
 *  DESCRIPTION
 *      This user application function is called whenever a LM-specific event is
 *      received by the system.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

bool AppProcessLmEvent(lm_event_code event_code, LM_EVENT_T *event_data)
{
    
/*AV: LM Tracing info */    
    if (event_code != LM_EV_NUMBER_COMPLETED_PACKETS) {
        g_event_arr[g_event_arr_idx] = event_code;
        g_event_arr_data[g_event_arr_idx] = *event_data;
                
        if (++g_event_arr_idx == EVENT_ARR_IDX_MAX) {
            g_event_arr_idx = 0;
        }
    }

/*--------------------*/
    
    
    switch(event_code)
    {

        /* Below messages are received in btcar_init state */
        case GATT_ADD_DB_CFM:
            handleSignalGattAddDBCfm((GATT_ADD_DB_CFM_T*)event_data);
        break;

        case LM_EV_CONNECTION_COMPLETE:
            /* Handle the LM connection complete event. */
            handleSignalLmEvConnectionComplete(
                                      (LM_EV_CONNECTION_COMPLETE_T*)event_data);
        break;

        /* Below messages are received in advertising state. */
        case GATT_CONNECT_CFM:
            handleSignalGattConnectCfm((GATT_CONNECT_CFM_T*)event_data);
        break;
        
        case GATT_CANCEL_CONNECT_CFM:
            handleSignalGattCancelConnectCfm();
        break;

        /* Below messages are received in btcar_connected/btcar_passkey_input state
         */
        case GATT_ACCESS_IND:
        {
            /* Received when HID Host tries to read or write to any
             * characteristic with FLAG_IRQ.
             */             
            handleSignalGattAccessInd((GATT_ACCESS_IND_T*)event_data);
        }
        break;

        case LM_EV_DISCONNECT_COMPLETE:
            handleSignalLmEvDisconnectComplete(
            &((LM_EV_DISCONNECT_COMPLETE_T *)event_data)->data);
        break;
        
        case LM_EV_ENCRYPTION_CHANGE:
            handleSignalLMEncryptionChange(event_data);
        break;
        
        case SM_KEYS_IND:
            handleSignalSmKeysInd((SM_KEYS_IND_T*)event_data);
        break;

        case SM_PAIRING_AUTH_IND:
            /* Authorize or Reject the pairing request */
            handleSignalSmPairingAuthInd((SM_PAIRING_AUTH_IND_T*)event_data);
        break;

        case SM_SIMPLE_PAIRING_COMPLETE_IND:
            handleSignalSmSimplePairingCompleteInd(
                (SM_SIMPLE_PAIRING_COMPLETE_IND_T*)event_data);
        break;

        case SM_PASSKEY_INPUT_IND:
            handleSignalSMpasskeyInputInd();
        break;

        /* Received in response to the L2CAP_CONNECTION_PARAMETER_UPDATE request
         * sent from the slave after encryption is enabled. If the request has
         * failed, the device should again send the same request only after
         * Tgap(conn_param_timeout). Refer Bluetooth 4.0 spec Vol 3 Part C,
         * Section 9.3.9 and HID over GATT profile spec section 5.1.2.         
         */
        case LS_CONNECTION_PARAM_UPDATE_CFM:
            handleSignalLsConnParamUpdateCfm((LS_CONNECTION_PARAM_UPDATE_CFM_T*)
                event_data);
        break;


        case LM_EV_CONNECTION_UPDATE:
            /* This event is sent by the controller on connection parameter 
             * update. 
             */
            handleSignalLmConnectionUpdate(
                            (LM_EV_CONNECTION_UPDATE_T*)event_data);
        break;

        case LS_CONNECTION_PARAM_UPDATE_IND:
            handleSignalLsConnParamUpdateInd(
                            (LS_CONNECTION_PARAM_UPDATE_IND_T *)event_data);
        break;

        case SM_DIV_APPROVE_IND:
            handleSignalSmDivApproveInd((SM_DIV_APPROVE_IND_T *)event_data);
        break;

        /* Received when the data sent from application has been successfully
         * queued to be sent to the remote device
         */
        case GATT_CHAR_VAL_NOT_CFM:
            handleSignalGattCharValNotCfm((GATT_CHAR_VAL_IND_CFM_T *)event_data);
        break;

        /* tx_data radio events are enabled while sending data from the queue.
         * This event is received by the application when data has been
         * successfully sent to the remote device
         */
        case LS_RADIO_EVENT_IND:
            handleSignalLsRadioEventInd();
        break;
        
        default:
            /* Unhandled event. */    
        break;
        
    }

    return TRUE;
}


void av_add_str_to_queue (void) {

    uint8 str[] = "Hello!";
    uint8 *p_str;
    uint8 char_to_send, char_to_report;

    p_str = str;
    char_to_send = sizeof(str);
    while(char_to_send) {
        char_to_report = (char_to_send < ATTR_LEN_HID_INPUT_REPORT) ? 
                            char_to_send : ATTR_LEN_HID_INPUT_REPORT;

        AddKeyStrokeToQueue(HID_INPUT_REPORT_ID, p_str, char_to_report);

        p_str += char_to_report;
        char_to_send -= char_to_report;
    }

    return;
}
 /*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 * FILE
 *    btcar_hw.c
 *
 *  DESCRIPTION
 *    This file defines the BTCAR hardware specific routines.
 *
 ******************************************************************************/

/*=============================================================================*
 *  Local Header Files
 *============================================================================*/
#include <gatt.h>
#include <gatt_prim.h>

#include "btcar_hw.h"
#include "hid_service.h"
#include "app_gatt_db.h"
#include "user_config.h"
#include "btcar.h"

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <sleep.h>
#include <mem.h>
#include <uart.h>

/*=============================================================================*
 *  Private Definitions
 *============================================================================*/

/* PIO direction */
#define PIO_DIRECTION_INPUT               (FALSE)
#define PIO_DIRECTION_OUTPUT              (TRUE)

/* PIO state */
#define PIO_STATE_HIGH                    (TRUE)
#define PIO_STATE_LOW                     (FALSE)

/* Maximum number of scan codes possible for a given keyboard. */
#define MAXMATRIX                         (ROWS * COLUMNS + 1)

/* De-bouncing timer for the pairing removal button */
#define PAIRING_BUTTON_DEBOUNCE_TIME      (100 * MILLISECOND)

/* The time for which the pairing removal button needs to be pressed
 * to remove pairing
 */
#define PAIRING_REMOVAL_TIMEOUT           (1 * SECOND)


/* 
 * Uart specific definitions
 */

  /* The application is required to create two buffers, one for receive, the
  * other for transmit. The buffers need to meet the alignment requirements
  * of the hardware. See the macro definition in uart.h for more details.
  */

/* Create 64-byte receive buffer for UART data */
UART_DECLARE_BUFFER(rx_buffer, UART_BUF_SIZE_BYTES_128);

/* Create 64-byte transmit buffer for UART data */
UART_DECLARE_BUFFER(tx_buffer, UART_BUF_SIZE_BYTES_128);

#if 0
/* UART receive callback to receive serial commands */
static uint16 uartRxDataCallback(void   *p_rx_buffer,
                                 uint16  length,
                                 uint16 *p_req_data_length);
#endif

/* UART transmit callback when a UART transmission has finished */
static void uartTxDataCallback(void);

/*=============================================================================*
 *  Private Data Types
 *============================================================================*/

typedef struct pwm_config_t
{
    uint8 dull_on;
    uint8 dull_off;
    uint8 dull_hold;
    uint8 bright_on; 
    uint8 bright_off;
    uint8 bright_hold;
    uint8 ramp_rate;
} PWM_CONFIG_T;

#define PWM_CFG_NUM 10

struct pwm_cfg_set_t
{   
    uint8 cfg_num;
    uint8 curr_idx;
    PWM_CONFIG_T    pwm_cfg[PWM_CFG_NUM];
} pwm_cfg_set = 

{ PWM_CFG_NUM, 0,
/*             30usec    30usec      16ms        30usec      30usec        16ms        30usec     */
/*          {_DULL_ON, _DULL_OFF, _DULL_HOLD, _BRIGHT_ON, _BRIGHT_OFF, _BRIGHT_HOLD, _RAMP_RATE}; */
        {
            {       0,       200,        25,         100,          0,           50,        100},
            {       0,       200,        25,         100,        100,           25,         50},
            {       0,       200,         0,           0,        200,           50,          2},

            {     200,         0,        50,           0,        200,            0,          0},
            {     100,       100,        50,           0,        200,            0,          0},
            {       0,       200,        50,           0,        200,            0,          0},

            {       0,       200,         0,           0,        100,            0,          1},
            {       0,       200,         0,           0,        100,            0,          1},
            {       0,       200,         0,           0,        200,            0,          1},
            {       0,       200,         0,           0,        200,            0,          1},

        }
};


/*=============================================================================*
 *  Public Data
 *============================================================================*/

/* Timer to hold the debouncing timer for the pairing removal button press.
 * The same timer is used to hold the debouncing timer for the pairing
 * button
 */
timer_id pairing_removal_tid;

uint8 aux_led_toggle;

/*=============================================================================*
 *  Private Function Prototypes
 *============================================================================*/
static void handlePairPioStatusChange(timer_id tid);

/*=============================================================================*
 *  Private Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      handlePairPioStatusChange
 *
 *  DESCRIPTION
 *      This function is called upon detecting a pairing button press. On the
 *      actual hardware this may be seen as a 'connect' button.
 *
 *  RETURNS
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

static void handlePairPioStatusChange(timer_id tid)
{
    if(tid == pairing_removal_tid)
    {
        
        /* Get the PIOs status to check whether the pairing button is pressed or
         * released
         */
        uint32 pios = PioGets();

        /* Reset the pairing removal timer */
        pairing_removal_tid = TIMER_INVALID;

        /* If the pairing button is still pressed after the expiry of debounce
         * time, then create a timer of PAIRING_REMOVAL_TIMEOUT after which
         * pairing will be removed. Before the expiry of this
         * timer(PAIRING_REMOVAL_TIMEOUT), if a pairing button removal is
         * detected, then this timer will be deleted
         */
        if(!(pios & PAIRING_BUTTON_PIO_MASK))
        {
            /* Create a timer after the expiry of which pairing information
             * will be removed
             */
            pairing_removal_tid = TimerCreate(
                   PAIRING_REMOVAL_TIMEOUT, TRUE, HandlePairingButtonPress);
        }

        /* Re-enable events on the pairing button */
        PioSetEventMask(PAIRING_BUTTON_PIO_MASK, pio_event_mode_both);

    } /* Else ignore the function call as it may be due to a race condition */
}
 
/*=============================================================================*
 *  Public Function Implementations
 *============================================================================*/

/*-----------------------------------------------------------------------------*
 *  NAME
 *      InitHardware  -  intialise keyboard scanning hardware
 *
 *  DESCRIPTION
 *      This function is called when a connection has been established and
 *      is used to start the scanning of the keyboard's keys.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

void InitHardware(void)
{

    /* Don't wakeup on UART RX line toggling */
    SleepWakeOnUartRX(FALSE);

    /* set up pairing button on PIO1 with a pull up (was UART) */
    PioSetMode(PAIRING_BUTTON_PIO, pio_mode_user);
    PioSetDir(PAIRING_BUTTON_PIO, PIO_DIRECTION_INPUT);
    PioSetPullModes(PAIRING_BUTTON_PIO_MASK, pio_mode_strong_pull_up);

    PioSetEventMask(PAIRING_BUTTON_PIO_MASK, pio_event_mode_both);


    /* Initialise UART and configure with default baud rate and port
     * configuration
     */
    UartInit(NULL,
             uartTxDataCallback,
             rx_buffer, UART_BUF_SIZE_BYTES_64,
             tx_buffer, UART_BUF_SIZE_BYTES_64,
             uart_data_unpacked);

#define UART_RATE_38K4    0x009e    

    UartConfig(UART_RATE_38K4, 0);

    /* Enable UART */
    UartEnable(TRUE);

    PioSetMode(AUX_LED_PIO, pio_mode_user);
    PioSetDir(AUX_LED_PIO, PIO_DIRECTION_OUTPUT);
    PioSet(AUX_LED_PIO, FALSE);
    aux_led_toggle = 0;
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      EnablePairLED  -  Enable/Disable the Pair LED mapped to PWM0
 *
 *  DESCRIPTION
 *      This function is called to enable/disable the Pair LED controlled 
 *      through PWM0
 *
 *  RETURNS/MODIFIES
 *      Nothing.
*------------------------------------------------------------------------------*/
void EnablePairLED(bool enable)
{

#ifdef ENABLE_PAIR_LED

    if(enable)
    {
        /* Setup Pair LED - PIO-7 controlled through PWM0.*/
        PioSetMode(PAIR_LED_PIO, pio_mode_pwm0);

        /* Configure the PWM0 parameters */
        PioConfigPWM(PAIR_LED_PWM_INDEX, pio_pwm_mode_push_pull,
                     PAIR_LED_DULL_ON_TIME, PAIR_LED_DULL_OFF_TIME,
                     PAIR_LED_DULL_HOLD_TIME, PAIR_LED_BRIGHT_OFF_TIME,
                     PAIR_LED_BRIGHT_ON_TIME, PAIR_LED_BRIGHT_HOLD_TIME,
                     PAIR_LED_RAMP_RATE);
        
        /* Enable the PWM0 */
        PioEnablePWM(PAIR_LED_PWM_INDEX, TRUE);
    }
    else
    {
        /* Disable the PWM0 */
        PioEnablePWM(PAIR_LED_PWM_INDEX, FALSE);
        /* Reconfigure PAIR_LED_PIO to pio_mode_user. This reconfiguration has
         * been done because when PWM is disabled, PAIR_LED_PIO value may remain
         * the same as it was, at the exact time of disabling. So if
         * PAIR_LED_PIO was on, it may remain ON even after disabling PWM. So,
         * it is better to reconfigure it to user mode. It will get reconfigured
         * to PWM mode when we re-enable the LED.
         */
        PioSetMode(PAIR_LED_PIO, pio_mode_user);
        PioSetDir(PAIR_LED_PIO, PIO_DIRECTION_OUTPUT);
        PioSet(PAIR_LED_PIO, FALSE);

    }

#endif /* ENABLE_PAIR_LED */

}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      HwDataInit
 *
 *  DESCRIPTION
 *      This function is used to initialise data structure used by btcar_hw.c
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/

extern void HwDataInit(void)
{

}

/*----------------------------------------------------------------------------*
 *  NAME
 *      HandlePIOChangedEvent
 *
 *  DESCRIPTION
 *      This function handles PIO Changed event
 *
 *  RETURNS
 *      Nothing.
 *
 *---------------------------------------------------------------------------*/

extern void HandlePIOChangedEvent(uint32 pio_changed)
{

    /* Get the PIOs status to check whether the buttons are
    *  pressed or released
    */
    uint32 pios = PioGets();

    if(pio_changed & PAIRING_BUTTON_PIO_MASK)
    {

        /* Delete the presently running timer */
        TimerDelete(pairing_removal_tid);

        /* If the pairing button is pressed....*/
        if(!(pios & PAIRING_BUTTON_PIO_MASK))
        {

            /* Disable any further events due to change in status of
             * pairing button PIO. These events are re-enabled once the
             * debouncing timer expires
             */
            PioSetEventMask(PAIRING_BUTTON_PIO_MASK, pio_event_mode_disable);

            pairing_removal_tid = TimerCreate(PAIRING_BUTTON_DEBOUNCE_TIME,
                                               TRUE, handlePairPioStatusChange);
        }

        else /* It's a pairing button release */
        {
            pairing_removal_tid = TIMER_INVALID;
        }                
    }

    return;
}


void set_pwm_cfg(uint8 cfg_idx) 
{
    PWM_CONFIG_T *pt_pwm_cfg;

    if (cfg_idx >= PWM_CFG_NUM)
        return;
    
    /* Setup Pair LED - PIO-7 controlled through PWM0.*/
    PioSetMode(PAIR_LED_PIO, pio_mode_pwm0);
    
    pt_pwm_cfg = &pwm_cfg_set.pwm_cfg[cfg_idx];
    PioConfigPWM(PAIR_LED_PWM_INDEX, pio_pwm_mode_push_pull,
                 pt_pwm_cfg->dull_on, pt_pwm_cfg->dull_off, pt_pwm_cfg->dull_hold,
                 pt_pwm_cfg->bright_on, pt_pwm_cfg->bright_off, pt_pwm_cfg->bright_hold,
                 pt_pwm_cfg->ramp_rate);

    pwm_cfg_set.curr_idx = cfg_idx;
    
    /* Enable the PWM0 */
    PioEnablePWM(PAIR_LED_PWM_INDEX, TRUE);
    
}

#if 0
/*----------------------------------------------------------------------------*
 *  NAME
 *      uartRxDataCallback
 *
 *  DESCRIPTION
 *      This is an internal callback function (of type uart_data_in_fn) that
 *      will be called by the UART driver when any data is received over UART.
 *      See DebugInit in the Firmware Library documentation for details.
 *
 * PARAMETERS
 *      p_rx_buffer [in]   Pointer to the receive buffer (uint8 if 'unpacked'
 *                         or uint16 if 'packed' depending on the chosen UART
 *                         data mode - this application uses 'unpacked')
 *
 *      length [in]        Number of bytes ('unpacked') or words ('packed')
 *                         received
 *
 *      p_additional_req_data_length [out]
 *                         Number of additional bytes ('unpacked') or words
 *                         ('packed') this application wishes to receive
 *
 * RETURNS
 *      The number of bytes ('unpacked') or words ('packed') that have been
 *      processed out of the available data.
 *----------------------------------------------------------------------------*/
static uint16 uartRxDataCallback(void   *p_rx_buffer,
                                 uint16  length,
                                 uint16 *p_additional_req_data_length)
{
   
    /* Inform the UART driver that we'd like to receive another byte when it
     * becomes available
     */
    *p_additional_req_data_length = (uint16)1;
    
    /* Return the number of bytes that have been processed */
    return length;
}
#endif
/*----------------------------------------------------------------------------*
 *  NAME
 *      uartTxDataCallback
 *
 *  DESCRIPTION
 *      This is an internal callback function (of type uart_data_out_fn) that
 *      will be called by the UART driver when data transmission over the UART
 *      is finished. See DebugInit in the Firmware Library documentation for
 *      details.
 *
 * PARAMETERS
 *      None
 *
 * RETURNS
 *      Nothing
 *----------------------------------------------------------------------------*/
static void uartTxDataCallback(void)
{
    // send last transaction if overflow was detected

}

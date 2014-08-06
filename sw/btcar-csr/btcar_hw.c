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

/* De-bouncing timer for AV button */
#define AV_BUTTON_DEBOUNCE_TIME           (100 * MILLISECOND)

/* The time for which the pairing removal button needs to be pressed
 * to remove pairing
 */
#define PAIRING_REMOVAL_TIMEOUT           (1 * SECOND)

/* De-bouncing time for key press */
#define KEY_PRESS_DEBOUNCE_TIME           (25 * MILLISECOND)

/* Key press states */
#define IDLE                              (0)
#define PRESSING                          (1)
#define DOWN                              (2)
#define RELEASING                         (3)

/* Number of scan cycles to be spent in the 'PRESSING' state. */
#define N_SCAN_CYCLES_IN_PRESSING_STATE   (2)

/* Number of scan cycles to be spent in the 'DOWN' state. */
/* In 'DOWN' state, the 'key release' events are ignored assuming that they are
 * due to bouncing. So, the number of scan cycles for which a key will be in
 * 'DOWN' state should be such that all such events are lost by the time the
 * key exits out of 'DOWN' state.
 *
 * NOTE: If this value is changed to suit a different hardware(keyboard),
 * then 'KEY_PRESS_DEBOUNCE_TIME' value should also be increased/decreased
 * by the same time. For example, if this macro is increased to 4, then,
 * 'KEY_PRESS_DEBOUNCE_TIME' which is now 25 ms should be changed to 30 ms
 * (increased by 5 ms which is the time duration of one scan cycle).
 */
#define N_SCAN_CYCLES_IN_DOWN_STATE       (3)

/*=============================================================================*
 *  Private Data Types
 *============================================================================*/

/* Structure which holds the previously generated reports by a key press for all
 * the types of reports used by the keyboard.
 */
typedef struct
{
    /* Last report formed by ProcessKeyPress() from the raw data received from
     * the PIO controller shared memory.
     */
    uint8 last_report[LARGEST_REPORT_SIZE];

     /* Different types of reports formed from the last raw_report */
    uint8 last_input_report[ATTR_LEN_HID_INPUT_REPORT];
}LAST_REPORTS_T;

/* Key press debounce structure which holds the key press state and tmaintains 
 * a count for the number of times a key press is seen to detect key bouncing
 */
typedef struct 
{
    uint8 state:8;
    uint8 count:8;
} debounce;

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
 *  Private Data
 *============================================================================*/

LAST_REPORTS_T last_generated_reports;


/* Timer to hold the debouncing timer for the pairing removal button press.
 * The same timer is used to hold the debouncing timer for the pairing
 * button
 */
timer_id pairing_removal_tid;

timer_id av_butt_removal_tid;


/*=============================================================================*
 *  Private Function Prototypes
 *============================================================================*/

void pio_ctrlr_code(void);  /* Included externally in PIO controller code.*/
static void handlePairPioStatusChange(timer_id tid);
static void handleAVPioStatusChange(timer_id tid);


/*=============================================================================*
 *  Private Function Implementations
 *============================================================================*/


static void handleAVPioStatusChange(timer_id tid)
{
        
    /* Get the PIOs status to check whether the pairing button is pressed or
        * released
        */
    uint32 pios = PioGets();

    /* Reset the pairing removal timer */
    av_butt_removal_tid = TIMER_INVALID;

    if(!(pios & AV_BUTTON_PIO_MASK))
    {
        set_pwm_cfg(1);
        
        {
            uint8       out[ATTR_LEN_HID_INPUT_REPORT];
            
            MemSet(out, 0x77, ATTR_LEN_HID_INPUT_REPORT);
            
            GattCharValueNotification(g_btcar_data.st_ucid, 
                                      HANDLE_HID_INPUT_REPORT, 
                                      ATTR_LEN_HID_INPUT_REPORT, &out[0]);
        
        }

    }

    /* Re-enable events on the pairing button */
    PioSetEventMask(AV_BUTTON_PIO_MASK, pio_event_mode_both);

}

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
 
/*-----------------------------------------------------------------------------*
 *  NAME
 *      handleDebounceKey
 *
 *  DESCRIPTION
 *      This function handles debouncing for each scan code
 *
 *  RETURNS
 *      BOOLEAN - TRUE: Key press detected
 *                FALSE: Key press not detected
 *
 *----------------------------------------------------------------------------*/



/*-----------------------------------------------------------------------------*
 *  NAME
 *      debounceResetTimer
 *
 *  DESCRIPTION
 *      This function handles expiry of debounce timer
 *
 *  RETURNS
 *      Nothing
 *
 *----------------------------------------------------------------------------*/

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

    /* AV TODO: Complete reqork required */

    /* Set up the PIO controller. */
    PioCtrlrInit((uint16*)&pio_ctrlr_code);

    /* Give the PIO controller access to the keyboard PIOs */
    PioSetModes(PIO_CONTROLLER_BIT_MASK, pio_mode_pio_controller);

    /* Set the pull mode of the PIOs. We need strong pull ups on inputs and
       outputs because outputs need to be open collector. This allows rows and
       columns to be shorted together in the key matrix */
    PioSetPullModes(PIO_CONTROLLER_BIT_MASK, pio_mode_strong_pull_up);

    /* Don't wakeup on UART RX line toggling */
    SleepWakeOnUartRX(FALSE);

    /* set up pairing button on PIO1 with a pull up (was UART) */
    PioSetMode(PAIRING_BUTTON_PIO, pio_mode_user);
    PioSetDir(PAIRING_BUTTON_PIO, PIO_DIRECTION_INPUT);
    PioSetPullModes(PAIRING_BUTTON_PIO_MASK, pio_mode_strong_pull_up);

    /* Setup button on PIO1 */
    PioSetEventMask(PAIRING_BUTTON_PIO_MASK, pio_event_mode_both);


    /* set up AV button on PIO0 with a pull up (was UART) */
    PioSetMode(AV_BUTTON_PIO, pio_mode_user);
    PioSetDir(AV_BUTTON_PIO, PIO_DIRECTION_INPUT);
    PioSetPullModes(AV_BUTTON_PIO, pio_mode_strong_pull_up);

    /* Setup button on PIO0 */
    PioSetEventMask(AV_BUTTON_PIO_MASK, pio_event_mode_both);

    
    /* set up AV button on PIO0 with a pull up (was UART) */
    PioSetMode(AV_BUTTON_PIO3, pio_mode_user);
    PioSetDir(AV_BUTTON_PIO3, PIO_DIRECTION_INPUT);
    PioSetPullModes(AV_BUTTON_PIO3, pio_mode_strong_pull_up);

    /* Setup button on PIO0 */
    PioSetEventMask(PIO_BIT_MASK(AV_BUTTON_PIO3), pio_event_mode_both);
    
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
 *      StartHardware  -  start scanning the keyboard's keys
 *
 *  DESCRIPTION
 *      This function is called when a connection has been established and
 *      is used to start the scanning of the keyboard's keys.
 *
 *  RETURNS/MODIFIES
 *      Nothing.
 *
 *----------------------------------------------------------------------------*/
void StartHardware(void)
{
    /* Start running the PIO controller code */
     /* PioCtrlrStart(); */
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
    MemSet(last_generated_reports.last_report, 0, LARGEST_REPORT_SIZE);
    MemSet(last_generated_reports.last_input_report, 0,
                                                  ATTR_LEN_HID_INPUT_REPORT);
}

/*-----------------------------------------------------------------------------*
 *  NAME
 *      ProcessKeyPress
 *
 *  DESCRIPTION
 *      This function is when the user presses a key on the keyboard and hence 
 *      there is a "sys_event_pio_ctrlr" event that has to be handled by the
 *      application. The function also checks for ghost keys.
 *
 *  RETURNS/MODIFIES
 *      True if there is new data to be sent.
 *
 *----------------------------------------------------------------------------*/

extern bool ProcessKeyPress(uint8 *rows, uint8 *raw_report)
{

    /* AV TODO: rework/clean up required */
    bool   report_changed = FALSE;
    
    /* Return true if there is a new report generated. */
    return report_changed;
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

    if(pio_changed & AV_BUTTON_PIO_MASK)
    {

        /* If the pairing button is pressed....*/
        if(!(pios & AV_BUTTON_PIO_MASK))
        {

            /* Disable any further events due to change in status of
             * pairing button PIO. These events are re-enabled once the
             * debouncing timer expires
             */
            PioSetEventMask(AV_BUTTON_PIO_MASK, pio_event_mode_disable);

            av_butt_removal_tid = TimerCreate(AV_BUTTON_DEBOUNCE_TIME,
                                               TRUE, handleAVPioStatusChange);
        }

    }

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
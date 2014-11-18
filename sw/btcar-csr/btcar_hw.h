/*******************************************************************************
 *  Based on CSR uEnergy SDK 2.2.2
 *
 *  FILE
 *      btcar_hw.h
 *
 *  DESCRIPTION
 *      Header definitions for btcar hardware specific functions.
 *
 ******************************************************************************/

#ifndef __BTCAR_HW_H__
#define __BTCAR_HW_H__

/*=============================================================================*
 *  SDK Header Files
 *============================================================================*/

#include <types.h>
#include <pio.h>
#include <pio_ctrlr.h>
#include <timer.h>

/*=============================================================================*
 *   Public Definitions
 *============================================================================*/
/* Bit-mask of a PIO. */
#define PIO_BIT_MASK(pio)             (0x01UL << (pio))

/* PIO associated with the pairing button on device. */
#define PAIRING_BUTTON_PIO            (11)

/* PIO which serves as power supply for the EEPROM. */
#define EEPROM_POWER_PIO              (2)

#define PAIRING_BUTTON_PIO_MASK   PIO_BIT_MASK(PAIRING_BUTTON_PIO)

/* To disable the Pair LED, comment the below macro. */
#define ENABLE_PAIR_LED

#ifdef ENABLE_PAIR_LED

/* Use PWM0 to control the Pair LED */
#define PAIR_LED_PWM_INDEX            (0)

/* PWM0 Configuration Parameters */
/* DULL ON Time - 0 * 30us = 0us */
#define PAIR_LED_DULL_ON_TIME         (0)

/* DULL OFF Time - 200 * 30us =  6000 us */
#define PAIR_LED_DULL_OFF_TIME        (200)

/* DULL HOLD Time - 124 * 16ms =  1984 ms */
#define PAIR_LED_DULL_HOLD_TIME       (124)

/* BRIGHT OFF Time - 1 * 30us = 30us */
#define PAIR_LED_BRIGHT_OFF_TIME      (1)

/* BRIGHT ON Time - 0 * 30us = 0us */
#define PAIR_LED_BRIGHT_ON_TIME       (0)

/* BRIGHT HOLD Time - 1 * 16ms = 16ms */
#define PAIR_LED_BRIGHT_HOLD_TIME     (1)

/* RAMP RATE - 0 * 30us = 0us */
#define PAIR_LED_RAMP_RATE            (0)

#endif /* ENABLE_PAIR_LED */


#define PAIR_LED_PIO                  (4)
#define AUX_LED_PIO                   (10)

/*============================================================================*
 *  Public Data Declarations
 *============================================================================*/

extern timer_id pairing_removal_tid;
extern uint8 aux_led_toggle;

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

 /* AV TODO: Hardware must be reqorked for BTCAR scpecific (currently inherited from keyboard app sample) */
/* This function intializes device scanning hardware */
extern void InitHardware(void);

/* This function enables/disables the Pair LED controlled through PWM0 */
extern void EnablePairLED(bool enable);

/* This function initializes the data structure used by btcar_hw.c */
extern void HwDataInit(void);

/* This function handles PIO Changed event */
extern void HandlePIOChangedEvent(uint32 pio_changed);

extern void set_pwm_cfg(uint8 cfg_idx);

#endif /* __BTCAR_HW_H__ */

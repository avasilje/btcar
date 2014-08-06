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

/* PIO associated with the pairing button on device. */
#define AV_BUTTON_PIO                 (0)

#define PAIRING_BUTTON_PIO            (1)

/* PIO which serves as power supply for the EEPROM. */
#define EEPROM_POWER_PIO              (2)

#define AV_BUTTON_PIO3                (3)

/* Bit-mask of a PIO. */
#define PIO_BIT_MASK(pio)             (0x01UL << (pio))

//#define LOWPOWER_LED_BIT_MASK     PIO_BIT_MASK(LOWPOWER_LED_PIO)

#define PAIRING_BUTTON_PIO_MASK   PIO_BIT_MASK(PAIRING_BUTTON_PIO)
#define AV_BUTTON_PIO_MASK        PIO_BIT_MASK(AV_BUTTON_PIO)

/* AV TODO: remove it */
/* Bit-mask of all the LED PIOs used by keyboard, other than PAIR LED. */
#define KEYBOARD_LEDS_BIT_MASK    NUMLOCK_LED_BIT_MASK | CAPSLOCK_LED_BIT_MASK \
                                  | LOWPOWER_LED_BIT_MASK

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

/* Mask for PIOs used by PIO controller for Key scanning. All the PIOs of
 * CSR 100x except the above used PIOs are configured to be used by the PIO
 * Controller. -PIO0-, PIO3, PIO4, PIO9 to PIO23 - scan matrix rows and
 * PIO24 to PIO31 - scan matrix columns
 */
#define PIO_CONTROLLER_BIT_MASK       (0xfffffe18UL)

/* Pair LED PIO, the same PIO is used for SPI MOSI when connected to a
 * debugger board using SPI interface.
 */
#define PAIR_LED_PIO                  (4)

/*============================================================================*
 *  Public Data Declarations
 *============================================================================*/

extern timer_id pairing_removal_tid;

/*=============================================================================*
 *  Public Function Prototypes
 *============================================================================*/

 /* AV TODO: Hardware must be reqorked for BTCAR scpecific (currently inherited from keyboard app sample) */
/* This function intializes device scanning hardware */
extern void InitHardware(void);

/* This function enables/disables the Pair LED controlled through PWM0 */
extern void EnablePairLED(bool enable);

/* This function starts scanning the keyboard's keys */
extern void StartHardware(void);

/* This function initializes the data structure used by btcar_hw.c */
extern void HwDataInit(void);

/* AV TODO: rework completely */
/* This function is called when the user presses a key on the keyboard */
extern bool ProcessKeyPress(uint8 *rows, uint8 *report);

/* This function handles PIO Changed event */
extern void HandlePIOChangedEvent(uint32 pio_changed);

extern void set_pwm_cfg(uint8 cfg_idx);

#endif /* __BTCAR_HW_H__ */

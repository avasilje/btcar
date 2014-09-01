/*******************************************************************************
 *  
 *
 *  FILE
 *    btcar_common.h
 *
 *  DESCRIPTION
 *    
 *
 ******************************************************************************/

#ifndef __BTCAR_COMMON_H__
#define __BTCAR_COMMON_H__

#include <avr/interrupt.h>

typedef enum last_err_enum {
    ERR_OK = 0,
    ERR_NOK
} LAST_ERR_t;


extern uint8_t guc_global_isr_dis_cnt;
extern LAST_ERR_t gt_last_err;


#define GLOBAL_IRQ_FORCE_EN()   \
do                              \
{                               \
    guc_global_isr_dis_cnt = 0; \
    sei();                      \
} while(0)

#define GLOBAL_IRQ_DIS()        \
do                              \
{                               \
    guc_global_isr_dis_cnt++;   \
    cli();                      \
} while(0)

#define GLOBAL_IRQ_RESTORE()            \
do                                      \
{                                       \
    if (guc_global_isr_dis_cnt == 0)    \
    FATAL_TRAP();                       \
                                        \
    if (--guc_global_isr_dis_cnt == 0)  \
    sei();                              \
} while(0)


#define MOVE_IRQ()                       \
     {                                   \
         int8_t tmp_mcucr;               \
         __asm__ __volatile__(           \
         "in  %0, %1" "\n\t"             \
         "sbr %0, 1"  "\n\t"             \
         "out %1, %0" "\n\t"             \
         "cbr %0, 1"  "\n\t"             \
         "sbr %0, 2"  "\n\t"             \
         "out %1, %0" "\n\t"             \
         : "=d" (tmp_mcucr)              \
         : "I" (_SFR_IO_ADDR(MCUCR))     \
         );                              \
        /* Hello from paranoia */        \
        if ((MCUCR & (1<<IVSEL)) == 0)   \
            FATAL_TRAP();                \
                                         \
     }

extern void _fatal_trap(uint16_t us_line_num);
#define FATAL_TRAP() _fatal_trap(__LINE__)

#define ASSERT(cond) \
    if (!(cond)) FATAL_TRAP()

#endif // __BTCAR_COMMON_H__


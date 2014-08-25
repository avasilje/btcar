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


extern void _fatal_trap(uint16_t us_line_num);
#define FATAL_TRAP() _fatal_trap(__LINE__)

#define ASSERT(cond) \
    if (!(cond)) FATAL_TRAP()

#endif // __BTCAR_COMMON_H__


/*******************************************************************************
 *  
 *  PROJECT
 *    BTCar
 *
 *  FILE
 *    unit_test_dbg_buff.c
 *
 *  DESCRIPTION
 *    
 *
 ******************************************************************************/

// TODO:
// 1. Unable to exclude unit_test_xx obj from particular project's
//    configuration. Try to move unit test to the dedicated proj.


/* Define DBG log signature */
#include "local_fids.h"
#define LOCAL_FILE_ID FID_UT_DBG_BUFF

#include <stdint.h>
#include <string.h>

#include "btcar_common.h"
#include "dbg_log.h"
#include "dbg_log_int.h"

extern DBG_BUFF_t gt_dbg_log_buff;  /* Private DBG_BUFF data. Visible for UT only */

/* Temp defines. Move to somewhere else later */
#define DISABLE_TIMER_4     TIMSK4 = 0            // Disable Overflow Interrupt
#define ENABLE_TIMER_4      TIMSK4 = (1<<TOIE4);  // Enable Overflow Interrupt
#define TIMER_4_CNT_RELOAD  (0xFFFF - 1)          // Clarify
#define TIMER_4_PRESCALER   3                     // Prescaler value (3->1/64)

uint8_t guc_unit_test_cnt;

/* 
 * Simulate single message unload from dbg buffer
 */
void unit_test_dbg_buf_unload(void)
{
    uint8_t uc_rd_idx, uc_mark;
    int16_t s_to_read;
    
    s_to_read = (int16_t)gt_dbg_log_buff.uc_wr_idx - gt_dbg_log_buff.uc_rd_idx;
    
    if (s_to_read < 0)
        s_to_read  += DBG_BUFF_LEN;
        
    if (s_to_read == 0)        
        return;
    
    uc_rd_idx = gt_dbg_log_buff.uc_rd_idx;
    
    uc_mark = gt_dbg_log_buff.uca_data[uc_rd_idx++];
    if (uc_mark == DBG_BUFF_MARK_OVFL)
    {
        uc_rd_idx ++;
        gt_dbg_log_buff.uc_rd_idx = uc_rd_idx;
        return;    
    }
    else if (uc_mark == DBG_BUFF_MARK_LOG || uc_mark == DBG_BUFF_MARK_LOG_ISR)
    {
        uint8_t uc_len;
        if (gt_dbg_log_buff.uca_data[uc_rd_idx++] != LOCAL_FILE_ID)
            FATAL_TRAP();
        uc_rd_idx += sizeof(int16_t);   // line num
        uc_len = gt_dbg_log_buff.uca_data[uc_rd_idx++];
        while(uc_len--)
        {
            gt_dbg_log_buff.uca_data[uc_rd_idx++] = 0xCC;
        }
        gt_dbg_log_buff.uc_rd_idx = uc_rd_idx;
    }
    else 
    {
        FATAL_TRAP();
    }

    return;    
}

void unit_test_dbg_log(void)
{
    
#if 0    
    // Sanity 
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // 64
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // 64
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // 64
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // 64 Overflow here
    DBG_LOG("0123456789abcdef0123456789abcdef");                                    // Fit OK
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // overlfow 
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // Multiple overflow
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // Multiple overflow
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // Multiple overflow
   
    // Checkpoint 
    // Verify control sum
    
    unit_test_dbg_buf_unload();
    
    // Check index wrap
    DBG_LOG("0123456789abcdef0123456789abcdef");    // Fit OK
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // 64 Overflow here
    DBG_LOG("0123456789abcdef0123456");    // Fit tight
    DBG_LOG("");    // Overflow
    DBG_LOG("");    // Multiple Overflow

    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    
    for (uint8_t uc_i = 0; uc_i < 255; uc_i++)
    {
        unit_test_dbg_buf_unload();
        unit_test_dbg_buf_unload();
        unit_test_dbg_buf_unload();
        unit_test_dbg_buf_unload();

        unit_test_dbg_buf_unload();
        unit_test_dbg_buf_unload();
        unit_test_dbg_buf_unload();
        unit_test_dbg_buf_unload();

        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32

        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
    }

    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
#endif // 0
        
    /*
     *  ISR
     */
    
    /*
        To be tested:
            1. ISR WR 
            2. ISR WR + User WR
            3. ISR OVFL + User WR
            4. Multi ISR OVFL + User WR
            5. User OVFL + ISR OVFL
    */

    TCCR4B = TIMER_4_PRESCALER;

    // 1. ISR WR
    for (uint8_t uc_i = 0; uc_i < 2; uc_i ++)
    { 
        guc_unit_test_cnt = 1;
        TCNT4 = 0xFFFE;    // Triggers immediately
        ENABLE_TIMER_4;
        GLOBAL_IRQ_FORCE_EN();
        while (guc_unit_test_cnt);
        GLOBAL_IRQ_DIS();
        DISABLE_TIMER_4;
        DBG_LOG("-USER-6789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
        unit_test_dbg_buf_unload();
        unit_test_dbg_buf_unload();
    }

    // 2. ISR WR + User WR
    for (uint8_t uc_i = 0; uc_i < 2; uc_i ++)
    { 
        guc_unit_test_cnt = 1;
        TCNT4 = 0xFFFB;    // Different const for DBG & Release
        ENABLE_TIMER_4;
        GLOBAL_IRQ_FORCE_EN();
        DBG_LOG("-USER-6789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
        while (guc_unit_test_cnt);
        GLOBAL_IRQ_DIS();
        DISABLE_TIMER_4;
        unit_test_dbg_buf_unload();
        unit_test_dbg_buf_unload();
    }    

    // 3. ISR OVFL + User WR
    guc_unit_test_cnt = 1;
    TIFR4 = 0; 
    TCNT4 = TIMER_4_CNT_RELOAD;
    ENABLE_TIMER_4;
    DBG_LOG("-USER-6789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    DBG_LOG("-USER-6789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    DBG_LOG("-USER-6789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    DBG_LOG("-USER-6789abcdef0123457");
    GLOBAL_IRQ_FORCE_EN();
    while (guc_unit_test_cnt);
    GLOBAL_IRQ_DIS();
    DISABLE_TIMER_4;

    DBG_LOG("-USER-");
    DBG_LOG("-OVFL-");

    guc_unit_test_cnt = 1;
    TIFR4 = 0; 
    TCNT4 = TIMER_4_CNT_RELOAD; 
    ENABLE_TIMER_4;
    GLOBAL_IRQ_FORCE_EN();
    while (guc_unit_test_cnt);
    GLOBAL_IRQ_DIS();
    DISABLE_TIMER_4;
    
    DBG_LOG("-OVFL-");
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();

    // 4. Multi ISR OVFL + User WR
    guc_unit_test_cnt = 3;
    TIFR4 = 0;
    TCNT4 = TIMER_4_CNT_RELOAD;
    ENABLE_TIMER_4;
    DBG_LOG("-USER-6789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    DBG_LOG("-USER-6789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
    DBG_LOG("-USER-6789abcdef012345789a");
    GLOBAL_IRQ_FORCE_EN();
    while (guc_unit_test_cnt);
    GLOBAL_IRQ_DIS();
    DISABLE_TIMER_4;

    DBG_LOG("-USER-");

    // 5. Multi User OVFL + Multi ISR OVFL
    DBG_LOG("-USER-");
    guc_unit_test_cnt = 6;
    TIFR4 = 0;
    TCNT4 = TIMER_4_CNT_RELOAD;
    ENABLE_TIMER_4;
    GLOBAL_IRQ_FORCE_EN();
    while (guc_unit_test_cnt);
    GLOBAL_IRQ_DIS();
    DISABLE_TIMER_4;
    DBG_LOG("-USER-");
    DBG_LOG("-USER-");
    
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    DBG_LOG("-USER-6789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");

}

ISR(TIMER4_OVF_vect) {

    DBG_LOG_ISR("-ISR-56789abcdef0123456789abcdef");

    guc_unit_test_cnt --;

    if (guc_unit_test_cnt)
    {
        TCNT4 = TIMER_4_CNT_RELOAD;
    }
    
}

/* 
 * The entry points used by unit test. Must be configured as entry in linker 
 * by -e option.
 * 
 */
void unit_tests_main ()
{
    /* Entry point (aka MAIN) for Unit Tests */    
    dbg_buffer_init();

    /* Check user & ISR write to buffer with overflows */
    unit_test_dbg_log();

}


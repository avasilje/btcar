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
#include "actions.h"

extern DBG_BUFF_t gt_dbg_log_buff;  /* Private DBG_BUFF data. Visible for UT only */
extern DBG_BUFF_t gt_cmd_rsp_buff;  /* Private DBG_BUFF data. Visible for UT only */

/* Temp defines. Move to somewhere else later */
#define DISABLE_TIMER_4     TIMSK4 = 0            // Disable Overflow Interrupt
#define ENABLE_TIMER_4      TIMSK4 = (1<<TOIE4);  // Enable Overflow Interrupt
#define TIMER_4_CNT_RELOAD  (0xFFFF - 1)          // Clarify
#define TIMER_4_PRESCALER   3                     // Prescaler value (3->1/64)

uint8_t guc_unit_test_cnt;
uint8_t guca_test_data_256[DBG_BUFF_LEN];

extern void unit_test_servo_ch(void);

/* 
 * Simulate single message unload from dbg buffer
 */
void unit_test_dbg_buf_unload(DBG_BUFF_t *dbg_buff)
{
    uint8_t uc_rd_idx, uc_mark;
    int16_t s_to_read;
    
    s_to_read = (int16_t)dbg_buff->uc_wr_idx - dbg_buff->uc_rd_idx;
    
    if (s_to_read < 0)
        s_to_read  += DBG_BUFF_LEN;
        
    if (s_to_read == 0)        
        return;
    
    uc_rd_idx = dbg_buff->uc_rd_idx;
    
    uc_mark = dbg_buff->uca_data[uc_rd_idx++];
    if (uc_mark == DBG_BUFF_MARK_OVFL)
    {
        uc_rd_idx ++;
        dbg_buff->uc_rd_idx = uc_rd_idx;
        return;    
    }
    else if (uc_mark == DBG_BUFF_MARK_LOG     || 
             uc_mark == DBG_BUFF_MARK_LOG_ISR || 
             uc_mark == DBG_BUFF_MARK_RSP )
    {
        uint8_t uc_len;

        /* Part specific for DBG_LOG buffer only */
        if ( uc_mark == DBG_BUFF_MARK_LOG || 
             uc_mark == DBG_BUFF_MARK_LOG_ISR)
        { // Check DBG_LOG header
            if (dbg_buff->uca_data[uc_rd_idx++] != LOCAL_FILE_ID)
                FATAL_TRAP();
            uc_rd_idx += sizeof(int16_t);   // line num
        }

        //Common part for all buffers
        uc_len = dbg_buff->uca_data[uc_rd_idx++];
        while(uc_len--)
        {
            dbg_buff->uca_data[uc_rd_idx++] = 0xCC;
        }
        dbg_buff->uc_rd_idx = uc_rd_idx;
    }
    else 
    {
        FATAL_TRAP();
    }

    return;    
}

void unit_test_dbg_log_d(void)
{

    uint8_t tmp_arr[32];
    uint8_t uc_len;

    DBG_LOG_D("Just note", NULL);

    DBG_LOG_D("Just note 1st line"
              "Just note 2nd line", 
              NULL
    );

    DBG_LOG_D("signature %(sign_struct)",
              VAR2ADDR_SIZE(gt_hw_info.t_sign));

    DBG_LOG_D("signature %(sign_struct) just digit %(uint8_t)",
              VAR2ADDR_SIZE(gt_hw_info.t_sign),
              VAR2ADDR_SIZE(uc_len));

    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);

    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // 64
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // 64
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // 64

    DBG_LOG_D("Overflow here",
              VAR2ADDR_SIZE(tmp_arr));


}

void unit_test_dbg_log(void)
{
    
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
    
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    
    // Check index wrap
    DBG_LOG("0123456789abcdef0123456789abcdef");    // Fit OK
    DBG_LOG("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");    // 64 Overflow here
    DBG_LOG("0123456789abcdef0123456");    // Fit tight
    DBG_LOG("");    // Overflow
    DBG_LOG("");    // Multiple Overflow

    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    
    for (uint8_t uc_i = 0; uc_i < 255; uc_i++)
    {
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);

        unit_test_dbg_buf_unload(&gt_dbg_log_buff);
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);

        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32

        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
        DBG_LOG("0123456789abcdef0123456789abcdef0");    // 32
    }

    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
        
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
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);
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
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);
        unit_test_dbg_buf_unload(&gt_dbg_log_buff);
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
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);

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
    
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
    unit_test_dbg_buf_unload(&gt_dbg_log_buff);
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

void unit_test_cmd_resp(void)
{
   
    uint8_t uc_rc;

    // 1. Simple write Overflow check
    uc_rc = cmd_resp_wr(guca_test_data_256, DBG_BUFF_LEN+1); // Return NOK
    ASSERT(uc_rc == ERR_NOK);
    
    uc_rc = cmd_resp_wr(guca_test_data_256, DBG_BUFF_LEN);   // Return NOK
    ASSERT(uc_rc == ERR_NOK);
    
    uc_rc = cmd_resp_wr(guca_test_data_256, DBG_BUFF_LEN-1); // Return NOK
    ASSERT(uc_rc == ERR_NOK);
    
    uc_rc = cmd_resp_wr(guca_test_data_256, DBG_BUFF_LEN-2); // Return NOK
    ASSERT(uc_rc == ERR_NOK);

    uc_rc = cmd_resp_wr(guca_test_data_256, DBG_BUFF_LEN-3); // Return OK
    ASSERT(uc_rc == ERR_OK);
    cmd_resp_wr_commit(uc_rc);

    unit_test_dbg_buf_unload(&gt_cmd_rsp_buff);

    // 2. Single write trivials
    uc_rc = ERR_OK;
    uc_rc |= cmd_resp_wr(guca_test_data_256, 0);
    cmd_resp_wr_commit(uc_rc);

    uc_rc |= cmd_resp_wr(guca_test_data_256, 1);
    cmd_resp_wr_commit(uc_rc);

    ASSERT(uc_rc == ERR_OK);
    
    unit_test_dbg_buf_unload(&gt_cmd_rsp_buff);
    unit_test_dbg_buf_unload(&gt_cmd_rsp_buff);
    
    // 3. Single write with overlap
    uc_rc |= cmd_resp_wr(guca_test_data_256, (DBG_BUFF_LEN >> 1)-3);
    cmd_resp_wr_commit(uc_rc);

    uc_rc |= cmd_resp_wr(guca_test_data_256, (DBG_BUFF_LEN >> 1)-3);
    cmd_resp_wr_commit(uc_rc);

    ASSERT(uc_rc == ERR_OK);

    unit_test_dbg_buf_unload(&gt_cmd_rsp_buff);
    unit_test_dbg_buf_unload(&gt_cmd_rsp_buff);

    // 4. Multi write
    for (int8_t uc_i = 0; uc_i < 0x10; uc_i ++)
    {
        uc_rc |= cmd_resp_wr(guca_test_data_256, 0x00);
        uc_rc |= cmd_resp_wr(guca_test_data_256, 0x02);
        uc_rc |= cmd_resp_wr(guca_test_data_256, 0x0F);
        uc_rc |= cmd_resp_wr(guca_test_data_256, 0x00);
        cmd_resp_wr_commit(uc_rc);

        uc_rc |= cmd_resp_wr(guca_test_data_256, 0x01);
        uc_rc |= cmd_resp_wr(guca_test_data_256, 0x01);
        uc_rc |= cmd_resp_wr(guca_test_data_256, 0x0F);
        uc_rc |= cmd_resp_wr(guca_test_data_256, 0x0F);
        cmd_resp_wr_commit(uc_rc);

        unit_test_dbg_buf_unload(&gt_cmd_rsp_buff);
        unit_test_dbg_buf_unload(&gt_cmd_rsp_buff);
    }
    ASSERT(uc_rc == ERR_OK);

    // Check buffer guardians
    DBG_BUFF_CHECK_GUARDIANS(&gt_cmd_rsp_buff);



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

    for(int16_t s_i = 0; s_i < DBG_BUFF_LEN; s_i++)
    {
        guca_test_data_256[s_i] = (uint8_t)s_i;
    }
    /* Check user & ISR write to buffer with overflows */
//    unit_test_dbg_log();

      unit_test_dbg_log_d();

//    unit_test_cmd_resp();

//    unit_test_servo_ch();



}


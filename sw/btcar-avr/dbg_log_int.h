/*******************************************************************************
 *  
 *
 *  FILE
 *    dbg_log_int.h
 *
 *  DESCRIPTION
 *    Internal defines of DBG_BUFF. My be used by DBG_LOG and Unittest
 *
 ******************************************************************************/
#ifndef __DBG_LOG_INT_H__
#define __DBG_LOG_INT_H__


/* 
 * ATT!: Buffer length set as 2^8 in order to avoid module addressing 
 *       Changing the buffer size requires complete index handling rework!
 */
#define DBG_BUFF_LEN    256

#define DBG_BUFF_CHECK_GUARDIANS(buff_ptr)           \
    ASSERT (((buff_ptr)->ul_guard_s == 0xADBEEDFE) &&  \
            ((buff_ptr)->ul_guard_e = 0xEFBEADDE))

#define MEM_SIGN_LEN    4


#define DBG_BUFF_UNLOAD_MAX 16

// Communication defines shared outside
// TODO: move to header common for fw & application side
#define DBG_BUFF_MARK_SIZE      2
#define DBG_BUFF_MARK_OVFL      0xDD
#define DBG_BUFF_MARK_RSP       0xD2
#define DBG_BUFF_MARK_LOG       0xD5
#define DBG_BUFF_MARK_LOG_D     0xD6
#define DBG_BUFF_MARK_LOG_ISR   0xD9

#define DBG_BUFF_IDX_DBGL    0
#define DBG_BUFF_IDX_CRSP    1
#define DBG_BUFF_IDX_LAST    2
#define DBG_BUFF_CNT DBG_BUFF_IDX_LAST

typedef struct dbg_buff_tag {
    uint8_t  uca_mem_sign[MEM_SIGN_LEN];
    uint8_t  uc_rd_idx;
    uint8_t  uc_wr_idx;
    union aux_wr_idx_union{
        uint8_t  uc_isr;     // Used for dbg_log buffer only
        uint8_t  uc_tmp;     // Used for cmd_rsp buffer only
    } aux_wr_idx;
    uint32_t ul_guard_s;
    uint8_t  uca_data[DBG_BUFF_LEN];
    uint32_t ul_guard_e;
} DBG_BUFF_t;

typedef struct dbg_log_tag {
    DBG_BUFF_t *buff[DBG_BUFF_CNT];
    uint8_t     uc_unload_buff_idx;
} DBG_LOG_t;


/* Defined as extern mostly for UnitTest */
extern DBG_BUFF_t gt_dbg_log_buff;
extern DBG_BUFF_t gt_cmd_rsp_buff;
extern DBG_BUFF_t gt_csr_log_buff;
extern DBG_LOG_t  gt_dbg_log;


#endif // __DBG_LOG_INT_H__

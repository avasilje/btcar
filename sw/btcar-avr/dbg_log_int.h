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

#define DBG_BUFF_MARK_SIZE      2
#define DBG_BUFF_MARK_OVFL      0xDD
#define DBG_BUFF_MARK_RSP       0xD2
#define DBG_BUFF_MARK_LOG       0xD5
#define DBG_BUFF_MARK_LOG_ISR   0xD9

// Buffer IDs used for buff creation routine just to avoid memory allocation
#define DBG_BUFF_ID_DBGL    1
#define DBG_BUFF_ID_CSRL    2
#define DBG_BUFF_ID_CRSP    3

/* Defined as extern mostly for UnitTest */
extern DBG_BUFF_t gt_csr_log_buff;
extern DBG_BUFF_t gt_dbg_log_buff;
extern DBG_BUFF_t gt_cmd_resp_log_buff;

#endif // __DBG_LOG_INT_H__

/*******************************************************************************
 *  
 *
 *  FILE
 *    dbg_log.h
 *
 *  DESCRIPTION
 *    
 *
 ******************************************************************************/
#ifndef __DBG_LOG_H__
#define __DBG_LOG_H__

#define MEM_SIGN_LEN    4
#define DBG_BUFF_LEN    256

#define STR2SIZE_STR(text)    sizeof(text), text

#define DBG_LOG(text) dbg_log(__LINE__, LOCAL_FILE_ID, STR2SIZE_STR(text))
#define DBG_LOG_ISR(text) dbg_log_isr(__LINE__, LOCAL_FILE_ID, STR2SIZE_STR(text))

typedef struct dbg_buff_tag {
    uint8_t  uca_mem_sign[MEM_SIGN_LEN];
    uint8_t  uc_rd_idx;
    uint8_t  uc_wr_idx;
    uint8_t  uc_isr_wr_idx;
    uint16_t us_bytes_free_sh;
    uint32_t ul_guard_s;
    uint8_t  uca_data[DBG_BUFF_LEN];
    uint32_t ul_guard_e;
} DBG_BUFF_t;

extern void dbg_buffer_init();
extern void dbg_log_isr(uint16_t us_line, uint8_t uc_fid, size_t t_text_size, char *pc_text);
extern void dbg_log(uint16_t us_line, uint8_t uc_fid, size_t t_text_size, char *pc_text);

#endif // __DBG_LOG_H__

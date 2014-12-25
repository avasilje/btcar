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

#define STR2SIZE_STR(text)    sizeof(text), text

#define DBG_LOG(text) dbg_log(__LINE__, LOCAL_FILE_ID, STR2SIZE_STR(text))
#define DBG_LOG_ISR(text) dbg_log_isr(__LINE__, LOCAL_FILE_ID, STR2SIZE_STR(text))

extern void init_hw_fifo(void);
extern void dbg_unload (uint8_t uc_forced_buff);
extern void dbg_buffer_init();
extern void dbg_log_isr(uint16_t us_line, uint8_t uc_fid, size_t t_text_size, char *pc_text);
extern void dbg_log(uint16_t us_line, uint8_t uc_fid, size_t t_text_size, char *pc_text);

extern uint8_t cmd_resp_wr(uint8_t *puc_src, size_t t_inp_len);
extern void    cmd_resp_wr_commit(uint8_t uc_wr_rc);

extern uint8_t g_acc_dbg_en;

#endif // __DBG_LOG_H__

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

#define VAR2ADDR_SIZE(var)   &var, sizeof(var)
// Note: argument "text" not stored in memory. It appear in source files only
//       and processed by external parsers.
#define DBG_LOG_D(text, ...) dbg_log_d((uint16_t)__LINE__, (uint8_t)LOCAL_FILE_ID, ##__VA_ARGS__, NULL)

extern void dbg_log_d(uint16_t us_line, uint8_t uc_fid, ...);


#endif // __DBG_LOG_H__

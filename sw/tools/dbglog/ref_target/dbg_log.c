/*******************************************************************************
 *  
 *
 *  FILE
 *    ref_target.c
 *
 *  DESCRIPTION
 *    
 *
 ******************************************************************************/
#include "local_fids.h"
#define LOCAL_FILE_ID FID_DBG_LOG

#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include <stdio.h>

uint8_t gt_dbg_log_buff[1024];

extern FILE *out_file;
#define ASSERT(cond)

#define DBG_BUFF_MARK_LOG_D     0xD6
void dbg_log_d (uint16_t us_line, uint8_t uc_fid, ...)
{
// Variable params must be passed in form : "addr0, size0, ... addrN, sizeN, NULL"
    void  *pt_data;
    va_list args;

    uint8_t uc_wr_idx;
    uint8_t uc_size;
    size_t  t_tot_len;

    // Get total data length to preserve 
    // space in log buffer
    t_tot_len =         // Header size
            1 + //  DBG Log mark
            1 + //  Size
            1 + //  File ID
            2;  //  Line
            

    va_start(args, uc_fid);
        if (args)
        {
            while((pt_data = va_arg(args, void*)))
            {
                uc_size = (uint8_t)va_arg(args, size_t);
                //t_tot_len += sizeof(uc_size); // sizeof length field of particular token
                t_tot_len += uc_size;         // size of particular token
            };
        }
    va_end(args);

    // Sanity check
    ASSERT(t_tot_len < UINT8_MAX);

    uc_wr_idx = 0;
    
    // Write header
    gt_dbg_log_buff[uc_wr_idx++] = DBG_BUFF_MARK_LOG_D;
    gt_dbg_log_buff[uc_wr_idx++] = t_tot_len;
    gt_dbg_log_buff[uc_wr_idx++] = uc_fid;
    gt_dbg_log_buff[uc_wr_idx++] = us_line  & 0xFF;
    gt_dbg_log_buff[uc_wr_idx++] = us_line  >> 8;

    fwrite(gt_dbg_log_buff, 1, uc_wr_idx, out_file);

    va_start(args, uc_fid);
        if (args)
        {
            while((pt_data = va_arg(args, void*)))
            {
                uc_size = (uint8_t)va_arg(args, size_t);
            
                // write data size

                // fwrite( (void*)&uc_size, 1, sizeof(uc_size), out_file);

                // Write data body
                fwrite( (void*)pt_data, 1, uc_size, out_file);
            };
        }
    va_end(args);

    return;

}

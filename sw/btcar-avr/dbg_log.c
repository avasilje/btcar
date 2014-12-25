/*******************************************************************************
 *  
 *
 *  FILE
 *    dbg_log.c
 *
 *  DESCRIPTION
 *    
 *
 ******************************************************************************/

#include <stdint.h>
#include <string.h>

#include "btcar_common.h"
#include "hw_fifo.h"
#include "dbg_log.h"
#include "dbg_log_int.h"

uint8_t g_acc_dbg_en = 0;

DBG_BUFF_t gt_dbg_log_buff;
DBG_BUFF_t gt_cmd_rsp_buff;

DBG_LOG_t gt_dbg_log = {
    { [DBG_BUFF_IDX_DBGL] = &gt_dbg_log_buff,
      [DBG_BUFF_IDX_CRSP] = &gt_cmd_rsp_buff },
    0,
};

/* 
 * Function preserves some space in buffer in order to avoid ISR blocking during 
 * write to buffer. The function returns write index. The function might be used
 * from user space only. Only DBG messages might be written from both ISR and
 * user space, thus it uses hardcoded buffer. In case of requested size doesn't
 * fit to the buffer get_last_error returns NOK
 */
uint8_t dbg_buff_preserve(uint8_t uc_pres_len){

/*
      0 1 2 3 4 5 6 7
  wr              :       wr 6
      x x x x x x x x     size 8; free = rd - wr - 1 + size = 2
  rd  ^                   rd 0

      0 1 2 3 4 5 6 7
  wr    :                 wr 1
      x x x x x x x x     size 8; free = rd - wr - 1 = 4
  rd              ^       rd 6

*/
    int16_t s_free;
    uint8_t uc_wr_idx;
    
    gt_last_err = ERR_OK;

    // Calc free space in buffer
    GLOBAL_IRQ_DIS();
    uc_wr_idx = gt_dbg_log_buff.uc_wr_idx;
    s_free = (int16_t)gt_dbg_log_buff.uc_rd_idx - uc_wr_idx - 1;
    
    if (s_free < 0)
        s_free += DBG_BUFF_LEN;

    // If doesn't fit then marks overflow, set ERR_NOK and exit
    if (s_free < uc_pres_len + DBG_BUFF_MARK_SIZE)
    { // Overflow
        if (s_free < DBG_BUFF_MARK_SIZE)
        { // Already overflowed. Update Overflow mark.
            gt_dbg_log_buff.uca_data[uc_wr_idx-1]++;
        }
        else
        { // Write new overflow mark
            gt_dbg_log_buff.uca_data[uc_wr_idx++] = DBG_BUFF_MARK_OVFL;
            gt_dbg_log_buff.uca_data[uc_wr_idx++] = 1;
            gt_dbg_log_buff.uc_isr_wr_idx = uc_wr_idx;
            gt_dbg_log_buff.uc_wr_idx = uc_wr_idx;
        }
        GLOBAL_IRQ_RESTORE();
        gt_last_err = ERR_NOK;
        return 0;   // Don't care
    }
    
    gt_dbg_log_buff.uc_isr_wr_idx += uc_pres_len;
    GLOBAL_IRQ_RESTORE();

    return uc_wr_idx;
}

void dbg_buff_wr_commit(void)
{
    int8_t  uc_wr_idx = gt_dbg_log_buff.uc_isr_wr_idx;
    
    GLOBAL_IRQ_DIS();
    gt_dbg_log_buff.uc_wr_idx = uc_wr_idx;
    GLOBAL_IRQ_RESTORE();

    // Check buffer fill level
    // Unload if reasonable
    // dbg_unload(DBG_BUFF_IDX_DBGL);
    return;
}

void dbg_buff_wr_wrap(uint8_t *puc_buff_data, uint8_t uc_wr_idx, uint8_t* puc_src, uint8_t uc_len)
{
    uint8_t *puc_dst = &puc_buff_data[uc_wr_idx];
    uint8_t uc_n;

    // Get number of bytes till idx wrap
    uc_n = DBG_BUFF_LEN - uc_wr_idx;
    if (uc_n > uc_len) uc_n = uc_len;

    uc_len -= uc_n;

    // write till index wrap
    while (uc_n--)
    {
        *puc_dst++ = *puc_src++;
    }

    // wrap around and continue if needed
    puc_dst = &puc_buff_data[0];
    while (uc_len--)
    {
        *puc_dst++ = *puc_src++;
    }

    return;
}

uint8_t cmd_resp_wr(uint8_t *puc_src, size_t t_inp_len)
{
    uint8_t uc_wr_idx, uc_len;
    int16_t s_free;

    uc_wr_idx = gt_cmd_rsp_buff.uc_tmp_wr_idx;
    uc_len = (uint8_t)t_inp_len;

    // sanity check
    if (t_inp_len > UINT8_MAX) return ERR_NOK;

    s_free = (int16_t)gt_cmd_rsp_buff.uc_rd_idx - uc_wr_idx - 1;
    if (s_free < 0) s_free += DBG_BUFF_LEN;

    // Is first fragment of response (tmp == curr) ?
    if (uc_wr_idx == gt_cmd_rsp_buff.uc_wr_idx)
    { // Add header on very first write

        // Check if header fits to buffer
        if (s_free < uc_len + 2) // Packet header size + payload
            return ERR_NOK;

        gt_cmd_rsp_buff.uca_data[uc_wr_idx++] = DBG_BUFF_MARK_RSP;
        gt_cmd_rsp_buff.uca_data[uc_wr_idx++] = 0;  // Init packet length
    }
    else
    { // Next consequent write. 
        if (s_free < uc_len)
            return ERR_NOK;
    }

    dbg_buff_wr_wrap(gt_cmd_rsp_buff.uca_data, uc_wr_idx, puc_src, uc_len);

    gt_cmd_rsp_buff.uc_tmp_wr_idx = uc_wr_idx + uc_len;

    return ERR_OK;
}

void cmd_resp_wr_commit(uint8_t uc_wr_rc)
{
    uint8_t uc_wr_idx, uc_tmp_wr_idx;
    int16_t s_len;

    // Undo write if write return code is NOK
    if (uc_wr_rc == ERR_NOK)
    {
        gt_cmd_rsp_buff.uc_tmp_wr_idx = gt_cmd_rsp_buff.uc_wr_idx;
        return;
    }

    uc_wr_idx = gt_cmd_rsp_buff.uc_wr_idx;
    uc_tmp_wr_idx = gt_cmd_rsp_buff.uc_tmp_wr_idx;

    s_len = (int16_t)uc_tmp_wr_idx - uc_wr_idx;
    if (s_len < 0) s_len += DBG_BUFF_LEN;

    // Write packet length to header
    uc_wr_idx ++;
    gt_cmd_rsp_buff.uca_data[uc_wr_idx] = (uint8_t)s_len; // -2 pck header

    gt_cmd_rsp_buff.uc_wr_idx = uc_tmp_wr_idx;

    // Check buffer fill level
    // Unload if reasonable
    // dbg_unload(DBG_BUFF_IDX_CRSP);

}

/*
 *    The function writes LOG message to DBG buffer form user space.
 *    The function might be interrupted by ISR, thus it first preserves
 *    memory in the buffer and then commit it. 
 */
void dbg_log(uint16_t us_line, uint8_t uc_fid, size_t t_text_size, char *pc_text)
{
    uint8_t uc_wr_idx;
    size_t  t_tot_len;

    // preserve space in buffer
    // Calculate total msg size
    t_tot_len =
            1 + //  DBG Log mark
            1 + //  File ID
            2 + //  Line
            1 + //  Size
            t_text_size;

    // Sanity check
    ASSERT(t_tot_len < UINT8_MAX);

    uc_wr_idx = dbg_buff_preserve((uint8_t)t_tot_len);
    if (gt_last_err == ERR_NOK)
        return;
    
    // Write header
    gt_dbg_log_buff.uca_data[uc_wr_idx++] = DBG_BUFF_MARK_LOG;
    gt_dbg_log_buff.uca_data[uc_wr_idx++] = t_tot_len;
    gt_dbg_log_buff.uca_data[uc_wr_idx++] = uc_fid;
    gt_dbg_log_buff.uca_data[uc_wr_idx++] = us_line  & 0xFF;
    gt_dbg_log_buff.uca_data[uc_wr_idx++] = us_line  >> 8;

    // Write body
    dbg_buff_wr_wrap(gt_dbg_log_buff.uca_data, uc_wr_idx, (uint8_t*)pc_text, (uint8_t)t_text_size);

    // Commit log message
    dbg_buff_wr_commit();

    return;
}

/*
 *  The function write LOG message to DBG buffer from an ISR context.
 *  The function writes using ISR-dedicated write pointer.
 */
void dbg_log_isr(uint16_t us_line, uint8_t uc_fid, size_t t_text_size, char *pc_text)
{
    uint8_t uc_isr_wr_idx;
    size_t  t_tot_len;
    uint8_t uc_tot_len;
    uint8_t uc_user_wr_ongoing;
    int16_t s_free;

    // Preserve space in buffer
    
    // Calculate total msg size
    t_tot_len =
            1 + //  DBG Log mark
            1 + //  File ID
            2 + //  Line
            1 + //  Size
            t_text_size;

    // Sanity check
    ASSERT(t_tot_len < UINT8_MAX);

    uc_tot_len = t_tot_len;
    uc_isr_wr_idx = gt_dbg_log_buff.uc_isr_wr_idx;
    
    // If ISR and non-ISR write indices are not equal that means 
    // user space write is ongoing. Thus NOT need to update user WR idx.
    // It will be updated by user space by WR commit
    uc_user_wr_ongoing = (uc_isr_wr_idx != gt_dbg_log_buff.uc_wr_idx);

    s_free = (int16_t)gt_dbg_log_buff.uc_rd_idx - uc_isr_wr_idx - 1;
    if (s_free < 0)
    {
        s_free += DBG_BUFF_LEN;
    }        

    // If doesn't fit then marks overflow, set ERR_NOK and exit
    if (s_free < uc_tot_len + DBG_BUFF_MARK_SIZE)
    { // Overflow
        if (s_free < DBG_BUFF_MARK_SIZE)
        { // Already overflowed. Update Overflow mark.
            gt_dbg_log_buff.uca_data[uc_isr_wr_idx-1]++;
        }
        else
        { // Write new overflow mark
            gt_dbg_log_buff.uca_data[uc_isr_wr_idx++] = DBG_BUFF_MARK_OVFL;
            gt_dbg_log_buff.uca_data[uc_isr_wr_idx++] = 1;
            gt_dbg_log_buff.uc_isr_wr_idx = uc_isr_wr_idx;
            
            if (!uc_user_wr_ongoing)
            {
                gt_dbg_log_buff.uc_wr_idx = uc_isr_wr_idx;   
            }
        }
        return;
    }

    gt_dbg_log_buff.uc_isr_wr_idx = uc_isr_wr_idx + uc_tot_len;
   
    if (!uc_user_wr_ongoing)
    {
        gt_dbg_log_buff.uc_wr_idx = uc_isr_wr_idx + uc_tot_len;
    }

    // Write Header
    gt_dbg_log_buff.uca_data[uc_isr_wr_idx++] = DBG_BUFF_MARK_LOG_ISR;
    gt_dbg_log_buff.uca_data[uc_isr_wr_idx++] = uc_tot_len;
    gt_dbg_log_buff.uca_data[uc_isr_wr_idx++] = uc_fid;
    gt_dbg_log_buff.uca_data[uc_isr_wr_idx++] = us_line  & 0xFF;
    gt_dbg_log_buff.uca_data[uc_isr_wr_idx++] = us_line  >> 8;

    // Write Body
    dbg_buff_wr_wrap(gt_dbg_log_buff.uca_data, uc_isr_wr_idx, (uint8_t*)pc_text, (uint8_t) t_text_size);

    return;
}


void dbg_buffer_init()
{

    // Initialize DBG LOG buffer
    gt_dbg_log_buff.ul_guard_s = 0xADBEEDFE;
    gt_dbg_log_buff.ul_guard_e = 0xEFBEADDE;
    gt_dbg_log_buff.uc_rd_idx = 0;
    gt_dbg_log_buff.uc_wr_idx = 0;
    memcpy(gt_dbg_log_buff.uca_mem_sign, "DBGL", MEM_SIGN_LEN);

    // Initialize Command Response buffer
    gt_cmd_rsp_buff.ul_guard_s = 0xADBEEDFE;
    gt_cmd_rsp_buff.ul_guard_e = 0xEFBEADDE;
    gt_cmd_rsp_buff.uc_rd_idx = 0;
    gt_cmd_rsp_buff.uc_wr_idx = 0;
    memcpy(gt_cmd_rsp_buff.uca_mem_sign, "CRSP", MEM_SIGN_LEN);

    
    return;
}

void dbg_unload (uint8_t uc_forced_buff)
{
    uint8_t uc_byte;
    uint8_t uc_tbs;
    uint8_t uc_rd_idx;
    uint8_t uc_wr_idx;
    uint8_t uc_buff_idx;

    DBG_BUFF_t *pt_curr_buff;

    // Ignore other buffers if specified explicitly 
    if (uc_forced_buff == 0xFF)
    {
        uc_buff_idx = gt_dbg_log.uc_unload_buff_idx;
    }
    else
    {
        uc_buff_idx = uc_forced_buff;
    }

    // Check all buffers starting from current.
    // Break if not empty, check next otherwise.
    do
    {
        // Get pointer to buffer currently unloading
        pt_curr_buff = gt_dbg_log.buff[uc_buff_idx];

        uc_rd_idx = pt_curr_buff->uc_rd_idx;
        uc_wr_idx = pt_curr_buff->uc_wr_idx;
        
        // Get number of bytes to be send
        uc_tbs = uc_wr_idx - uc_rd_idx;

        // Check next buffer if current is empty
        if (uc_tbs == 0)
        {
            // Ignore other buffers if specified explicitly 
            if (uc_forced_buff != 0xFF)
            {
                return;
            }

            // Get next buffer index
            uc_buff_idx ++;
            if (DBG_BUFF_CNT == uc_buff_idx)
            {
                uc_buff_idx = 0;
            }

            // Exit if all buffer checked & empty (idx rollover)
            if (uc_buff_idx == gt_dbg_log.uc_unload_buff_idx)
            {
                return;
            }
            continue;
        } // End of buffer empty

        break;
    } while(1); // End of dbg log buffers check
     

    // Some data to be send out...
    DISABLE_RX_TIMER;
    // Limit number of bytes to be send
    if (uc_tbs > DBG_BUFF_UNLOAD_MAX)
    {
        uc_tbs = DBG_BUFF_UNLOAD_MAX;
    }

    // Write stream header  
    uc_byte = uc_tbs | (uc_buff_idx << 6);
    FIFO_WR(uc_byte);

    // Write stream data
    while (uc_tbs)
    {
        uc_byte = pt_curr_buff->uca_data[uc_rd_idx++];

        FIFO_WR(uc_byte);
        uc_tbs -= 1;
    }

    ENABLE_RX_TIMER;

    pt_curr_buff->uc_rd_idx = uc_rd_idx;

    // Next time start from next buffer
    uc_buff_idx ++;
    if (DBG_BUFF_CNT == uc_buff_idx)
    {
        uc_buff_idx = 0;
    }

    gt_dbg_log.uc_unload_buff_idx = uc_buff_idx;

}


//TODO:   hello 
//2. move DBG staff into dedicated file.
//3. Move unittest staff to dedicated file.


#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/boot.h> 
#include <string.h>
#include "actions.h"

#define FID_MAIN 0
#define LOCAL_FILE_ID FID_MAIN

extern void _fatal_trap(uint16_t us_line_num);
#define FATAL_TRAP() _fatal_trap(__LINE__)

uint8_t guc_global_isr_dis_cnt = 0;

#define GLOBAL_IRQ_FORCE_EN()       \
    do                              \
    {                               \
        guc_global_isr_dis_cnt = 0; \
        sei();                      \
    } while(0)
 
#define GLOBAL_IRQ_DIS()            \
    do                              \
    {                               \
       guc_global_isr_dis_cnt++;    \
       cli();                       \
    } while(0)
    
#define GLOBAL_IRQ_RESTORE()                \
    do                                      \
    {                                       \
        if (guc_global_isr_dis_cnt == 0)    \
            FATAL_TRAP();                   \
                                            \
        if (--guc_global_isr_dis_cnt == 0)  \
            sei();                          \
    } while(0)


// -------------------------------------------------
// --- Fuse settings:
// ---    1111 1100   1001 1000   1101 1111 = 0xFC98DF 
// ---          ^^^   ^^^^        ^^^^ ^^^^   
// ---            |   |||| ||||   || |    |            
// ---            |   |||| ||||   || |    |            
// ---            |   |||| ||||   || |    +-- CKSEL3..0  (1111)* Full swing 16Mhz, 
// ---            |   |||| ||||   || +------- SUT1..0      (01)*
// ---            |   |||| ||||   |+--------- CKOUT         (1)
// ---            |   |||| ||||   +---------- CKDIV8        (1)*
// ---            |   |||| ||||                                        
// ---            |   |||| |||+--------- BOOTRST     (0)*
// ---            |   |||| ||+---------- BOOTSZ0     (0)
// ---            |   |||| |+----------- BOOTSZ1     (0)
// ---            |   |||| +------------ EESAVE      (1)
// ---            |   ||||
// ---            |   |||+-------------- WDTON       (1)
// ---            |   ||+--------------- Enable SPI  (0)
// ---            |   |+---------------- Enable JTAG (0)
// ---            |   +----------------- Enable OCD  (1)
// ---            |
// ---            +---BODLEVEL: 100 4.1V ... 4.5V
// --- 
// ---- ---------------------------------------------
 
#define ACTION_SELF_WRITE   0x01    // Not tabled function (hardcoded)
#define ACTION_SELF_READ    0x02    // Not tabled function (hardcoded)
#define ACTION_PING         0x03    // Not tabled function (hardcoded)

//------ Global variables ---------------------
HW_INFO gt_hw_info __attribute__ ((section (".hw_info")));
uint8_t volatile guc_mcu_cmd;
uint8_t volatile guc_mem_guard;
uint16_t gus_trap_line;

//------- Internal function declaration -------
uint8_t proceed_command(uint8_t uc_cmd);
uint8_t action_ping();
uint8_t action_self_write();
uint8_t action_self_read();

#define LB1 0
#define LB2 1

//uint16_t      gusa_verify_buff[VERIFY_BUFF_LEN];
#define MEM_SIGN_LEN    4
#define DBG_BUFF_LEN    256

typedef enum last_err_enum {
    ERR_OK = 0,
    ERR_NOK
} LAST_ERR_t;

LAST_ERR_t gt_last_err;

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

DBG_BUFF_t gt_csr_log_buff;
DBG_BUFF_t gt_dbg_log_buff;
DBG_BUFF_t gt_cmd_resp_log_buff;

DBG_BUFF_t *gpt_csr_log_buff      = &gt_csr_log_buff;
DBG_BUFF_t *gpt_dbg_log_buff      = &gt_dbg_log_buff;
DBG_BUFF_t *gpt_cmd_resp_log_buff = &gt_cmd_resp_log_buff; 

uint8_t guc_unit_test_flag;
uint8_t guc_unit_test_cnt;

void _fatal_trap(uint16_t us_line_num)
{

    gus_trap_line = us_line_num;

    // clear all hazards on external pins
    // ...

    while(1);

}


DBG_BUFF_t* dbg_buffer_create(const char *pc_mem_sign)
{
    // Allocate buffer in memory
    // Write signature to the mem_sign, init guardians
    // return pointer to the buffer
    return NULL;
}

void dbg_buffer_init(DBG_BUFF_t* buff, const char *pc_mem_sign)
{
    buff->ul_guard_s = 0xADBEEDFE;
    buff->ul_guard_e = 0xEFBEADDE;
    buff->us_bytes_free_sh = DBG_BUFF_LEN;
    buff->uc_rd_idx = 0;
    buff->uc_wr_idx = 0;
    memcpy(buff->uca_mem_sign, pc_mem_sign, MEM_SIGN_LEN);
    return;
}

void init_hw_unused(void)
{
    // Set unused pins to pull-up inputs
    PORTA |= (1 << PINA7);  // Due to schematic Bug

    PORTB |= (1 << PINB7);

    PORTG |= (1 << PING0) |
    (1 << PING2) |
    (1 << PING4) |
    (1 << PING5);

    PORTF |= (1 << PINF0) |
    (1 << PINF1) |
    (1 << PINF2) |
    (1 << PINF3);

    PORTH |= (1 << PINH4) | // ADDR_3_UNUSED_MASK
    (1 << PINH5) |
    (1 << PINH6) |
    (1 << PINH7);

    PORTJ |= (1 << PINJ0) |
    (1 << PINJ1) |
    (1 << PINJ7);

}

void init_hw_fifo(void)
{

    // Init USB FIFO control pins
    // USB SI       (active low) - output 1
    // RD output 1  (active low)
    // WR output 0  (active high)
    // TXF# pullup input
    // RXE# pullup input

    FIFO_CNTR_PORT |= (0<<FIFO_CNTR_WR )|
    (1<<FIFO_CNTR_RD )|
    (1<<FIFO_CNTR_RXF)|
    (1<<FIFO_CNTR_TXE)|
    (1<<FIFO_CNTR_SI );

    FIFO_CNTR_DIR  |= (1<<FIFO_CNTR_RD )|
    (1<<FIFO_CNTR_WR )|
    (1<<FIFO_CNTR_SI );

    // FIFO data bus tristated

}

void init_hw_io(void)
{
    // Init LED control pins (all OFF)
    LED_DIR |= ((1<<LED_RED)|
                (1<<LED_GREEN));
}

#define STR2SIZE_STR(text)    sizeof(text), text

#define DBG_LOG(text) dbg_log(__LINE__, LOCAL_FILE_ID, STR2SIZE_STR(text))
#define DBG_LOG_ISR(text) dbg_log_isr(__LINE__, LOCAL_FILE_ID, STR2SIZE_STR(text))

/* 
 * Function preserves some space in buffer in order to avoid ISR blocking during 
 * write to buffer. The function returns write index Function might be used
 * from user space only Only DBG messages might be written from both ISR and
 * user space, thus it uses hardcoded buffer. In case of requested size doesn't
 * fit to the buffer get_last_error returns NOK
 */
uint8_t dbg_buff_preserve(uint8_t uc_pres_len){

#define DBG_BUFF_MARK_SIZE      2
#define DBG_BUFF_MARK_OVFL      0xDD
#define DBG_BUFF_MARK_LOG       0xD5
#define DBG_BUFF_MARK_LOG_ISR   0xD9
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
    GLOBAL_IRQ_DIS();
    gt_dbg_log_buff.uc_wr_idx = gt_dbg_log_buff.uc_isr_wr_idx;
    GLOBAL_IRQ_RESTORE();

    
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

/*
 *    Function might be interrupted by ISR, thus it first preserves
 *    memory in the buffer and then commit it
 */
void dbg_log(uint16_t us_line, uint8_t uc_fid, size_t t_text_size, char *pc_text)
{
    uint8_t uc_wr_idx;
    uint8_t uc_tot_len;

    // Sanity check
    if (t_text_size > UINT8_MAX)
        FATAL_TRAP();

    // preserve space in buffer
        // Calculate total msg size
        uc_tot_len = (uint8_t)
                1 + //  DBG Log mark
                1 + //  File ID
                2 + //  Line
                1 + //  Size
                t_text_size;

    uc_wr_idx = dbg_buff_preserve(uc_tot_len);
    if (gt_last_err == ERR_NOK)
        return;
    
    // Write header
    gpt_dbg_log_buff->uca_data[uc_wr_idx++] = DBG_BUFF_MARK_LOG;
    gpt_dbg_log_buff->uca_data[uc_wr_idx++] = uc_fid;
    gpt_dbg_log_buff->uca_data[uc_wr_idx++] = us_line  & 0xFF;
    gpt_dbg_log_buff->uca_data[uc_wr_idx++] = us_line  >> 8;
    gpt_dbg_log_buff->uca_data[uc_wr_idx++] = t_text_size;

    // Write body
    dbg_buff_wr_wrap(gpt_dbg_log_buff->uca_data, uc_wr_idx, (uint8_t*)pc_text, (uint8_t) t_text_size);

    // Commit log message
    dbg_buff_wr_commit();

    return;
}

/*
 *    Function might be interrupted by ISR, thus it first preserves
 *    memory in the buffer and then commit it
 */
void dbg_log_isr(uint16_t us_line, uint8_t uc_fid, size_t t_text_size, char *pc_text)
{
    uint8_t uc_isr_wr_idx;
    uint8_t uc_tot_len;
    uint8_t uc_user_wr_ongoing;
    int16_t s_free;

    // Sanity check
    if (t_text_size > UINT8_MAX)
        FATAL_TRAP();

    // Preserve space in buffer
    
    // Calculate total msg size
    uc_tot_len = (uint8_t)
            1 + //  DBG Log mark
            1 + //  File ID
            2 + //  Line
            1 + //  Size
            t_text_size;

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
    gpt_dbg_log_buff->uca_data[uc_isr_wr_idx++] = DBG_BUFF_MARK_LOG_ISR;
    gpt_dbg_log_buff->uca_data[uc_isr_wr_idx++] = uc_fid;
    gpt_dbg_log_buff->uca_data[uc_isr_wr_idx++] = us_line  & 0xFF;
    gpt_dbg_log_buff->uca_data[uc_isr_wr_idx++] = us_line  >> 8;
    gpt_dbg_log_buff->uca_data[uc_isr_wr_idx++] = t_text_size;

    // Write Body
    dbg_buff_wr_wrap(gpt_dbg_log_buff->uca_data, uc_isr_wr_idx, (uint8_t*)pc_text, (uint8_t) t_text_size);

    return;
}

void log_test(){

    DBG_LOG("Hello from mars");

}

/* Simulate single message unload from dbg buffer */
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
    TCNT4 = 0xFFFE; 
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
    TCNT4 = 0xFFFE; 
    ENABLE_TIMER_4;
    GLOBAL_IRQ_FORCE_EN();
    while (guc_unit_test_cnt);
    GLOBAL_IRQ_DIS();
    DISABLE_TIMER_4;
    
    DBG_LOG("-OVFL-");
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    unit_test_dbg_buf_unload();
    
    
   

}



//---------------------------------------------
int main() {

    uint8_t uc_i;

    // --------------------------------------------------
    // --- Protect bootloader from internal update 
    // --- Protect chip
    // --------------------------------------------------
    // set locks bit for Boot Loader Section
    // Prohibit read from BLS, 
    // Prohibit to change BLS from Application section

//    boot_lock_bits_set( (1<<BLB12) | (1<<BLB11) |  (1<<LB1) | (1<<LB2));

    // Move Interrupt vectors to BootLoader
    MCUCR = (1<<IVCE);
    MCUCR = (1<<IVSEL);

    // --------------------------------------------------
    // --- Init perephirial devices
    // --------------------------------------------------
    init_hw_unused();

    init_hw_fifo();

    init_hw_io();
                
    leds_control(0xFF);

    dbg_buffer_init(gpt_csr_log_buff,      "CSRL");
    dbg_buffer_init(gpt_dbg_log_buff,      "DBGL");
    dbg_buffer_init(gpt_cmd_resp_log_buff, "CRSP");

    unit_test_dbg_log();
    DBG_LOG("BTCAR mcu started");
//    csr_log("Interface initiated");

    // RX timer - Timer3. Incrementing counting till UINT16_MAX
    TCCR3B = RX_CNT_TCCRxB;
    TCNT3  = RX_CNT_RELOAD;
    ENABLE_RX_TIMER;

    // Wait a little just in case
    for(uc_i = 0; uc_i < 255U; uc_i++){

        __asm__ __volatile__ ("    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"\
                              "    nop\n    nop\n    nop\n    nop\n"
                                ::);
    }


    guc_mcu_cmd = 0;

    leds_control(0x00);

    // Init HW info structure
    gt_hw_info.uc_ver_maj = pgm_read_byte_near(&(gca_version[0]));
    gt_hw_info.uc_ver_min = pgm_read_byte_near(&(gca_version[1]));
    gt_hw_info.pf_led_func = leds_control;


    for(uc_i = 0; uc_i < SIGN_LEN; uc_i++){
        gt_hw_info.ca_signature[uc_i] = 
            pgm_read_byte_near(&(gca_signature[uc_i]));
    }


    // Enable Interrupts
    GLOBAL_IRQ_FORCE_EN();

    while(1){
        if (guc_mcu_cmd) {
            guc_mcu_cmd = proceed_command(guc_mcu_cmd);
        }
    }

    return 0;
}


uint8_t proceed_command(uint8_t uc_mcu_cmd){

    T_ACTION    *pt_action;
    uint8_t (*pf_func)();
    uint8_t uc_act_cmd;
    uint8_t uc_ret_cmd;
    uint8_t uc_i;


    DISABLE_RX_TIMER;

    // Received command in guc_mcu_cmd

    // Message format 
    // Action(8bit) Action depended data(N*8bit)
    
    // --------------------------------------------------
    // --- Find call address of ACTION in ACTION's table
    // --------------------------------------------------
   
    uc_ret_cmd = 0;
    // Setup action table pointer
    pt_action = gta_action_table;

    guc_mem_guard = 0;
    // --------------------------------------------------
    // --- Non changable function for Firmware Update
    // --------------------------------------------------
    // Check is command SelfWrite or SelfRead
    switch (uc_mcu_cmd){
        case ACTION_PING : 
            uc_ret_cmd = action_ping();
            break;
        case ACTION_SELF_WRITE:

            // Paranoic Flash write protection
            for (uc_i = 0; uc_i < 200; uc_i++){
                if (uc_mcu_cmd != ACTION_SELF_WRITE) {
                return 0;
                }
            }
            if (uc_i != 200) return 0;
            guc_mem_guard = 1;
            uc_ret_cmd = action_self_write();
            break;
        case ACTION_SELF_READ:
            uc_ret_cmd = action_self_read();
            break;
        default:

            // Check is action table valid
            pf_func = (uint8_t (*)())pgm_read_word_near(&pt_action->pf_func);
            if (pf_func == (uint8_t (*)())0xFEED) {
    
                // Find action in table
                do{
                    pt_action ++;
                    // Check end of table
                    uc_act_cmd = pgm_read_byte_near(&pt_action->uc_cmd);

                    if (uc_act_cmd == 0xFF) break;

                    if (uc_mcu_cmd == uc_act_cmd){
                        // get functor from table
                        pf_func = (uint8_t (*)())pgm_read_word_near(&pt_action->pf_func);
                        uc_ret_cmd = pf_func();
                        break;
                    }
                }while(1); // End of action table scan
            }// emd of table signature ok
            break; // break from switch
    }// end of switch 


    // Wait for another command
    ENABLE_RX_TIMER;

    return uc_ret_cmd;
}

uint8_t action_ping(){

    // 0x11 0xNN        0xDD
    // CMD  DATA LEN    DATA

    uint8_t   uc_data_len, uc_data;
    
    // Write header 
    FIFO_WR(0x03);

    // Read Len 
    FIFO_RD(&uc_data_len);
    
    // Write Length back
    FIFO_WR(uc_data_len+1);

    // Write Platform
    uc_data = 0x40;
    FIFO_WR(uc_data);

    while(uc_data_len){
        FIFO_RD(&uc_data);
        uc_data_len--;
        FIFO_WR(uc_data);
    }

    return 0;
}


ISR(TIMER3_OVF_vect) {

    // --------------------------------------------------
    // --- RX direction routine
    // --------------------------------------------------

    // Set FIFO data port direction
    FIFO_DATA_PORT = 0xFF;  // Pull-up
    FIFO_DATA_DIR = 0x00;   // Input

    // Get data from FIFO if present 
    if (bit_is_clear(FIFO_CNTR_PIN, FIFO_CNTR_RXF) ){

        // RD active 
        FIFO_CNTR_PORT &= ~(1<<FIFO_CNTR_RD);
        __asm__ __volatile__ ("nop" ::);

        // Read single data byte
        guc_mcu_cmd = FIFO_DATA_PIN;

        // Release RD strobe
        FIFO_CNTR_PORT |=  (1<<FIFO_CNTR_RD);

    }

    // --------------------------------------------------
    // --- Reload Timer
    // --------------------------------------------------

    // Wait until RX fifo filled again
    TCNT3 = RX_CNT_RELOAD;
}

void leds_control(uint8_t uc_leds){

    uint8_t uc_temp;

    uc_temp = LED_PORT;
    // Set Green LED
    if (bit_is_set(uc_leds, 0))
        uc_temp |= (1<<LED_GREEN);
    else
        uc_temp &= ~(1<<LED_GREEN);

    // Set Red LED
    if (bit_is_set(uc_leds, 4))
        uc_temp |= (1<<LED_RED);
    else
        uc_temp &= ~(1<<LED_RED);

    LED_PORT = uc_temp;

}

// Self write bodyguard
void dummy1(){
	uint8_t uc_i;

	for (uc_i = 0; uc_i < 200; uc_i++){
		while(1){
	        __asm__ __volatile__ ("nop" ::);
	        __asm__ __volatile__ ("nop" ::);
	        __asm__ __volatile__ ("nop" ::);
	        __asm__ __volatile__ ("nop" ::);
			guc_mem_guard = 0;
		}
	}
	guc_mem_guard = 0;
}

#if 0   // prohibit PGM write on a development phase. Just for safety.
uint8_t action_self_write(){

	uint8_t   uc_i, uc_j;
//  Hardcoded 0x01, action_self_write
    UINT16  t_len;
    UINT16  t_addr_h;
    UINT16  t_addr;
    UINT16  t_data;

    uint16_t  us_i;

    // 0x01 0xAAAAAAAA      0xLLLL          
    // CMD  Address0 32bit  Length0 16bit
    //
    // 0xDDDD   0xDDDD  ... 0xDDDD   0xDDDD
    // Data00    Data01 ... DataN0    DataN1
    
    // Read Address 
    FIFO_RD(&t_addr.b[0]);
    FIFO_RD(&t_addr.b[1]);
    FIFO_RD(&t_addr_h.b[0]); // Ignored
    FIFO_RD(&t_addr_h.b[1]); // Ignored

    // Read Length
    FIFO_RD(&t_len.b[0]);
    FIFO_RD(&t_len.b[1]);

    // t_addr_h ignored
    
	// Hello from Paranoia - FLASH guard
	for(uc_i = 0; uc_i < 20; uc_i++){
		for(uc_j = 0; uc_j < 20; uc_j++){
			if (!guc_mem_guard) while(1);
		}
	}

	if (!guc_mem_guard) while(1);
	if (t_addr_h.b[1] != 0) while(1);

    // Main Loop
    while(t_len.s > 0){
        if (t_addr.s >= 0x7000) break;

		if (!guc_mem_guard) while(1);
        boot_page_erase(t_addr.s);
        boot_spm_busy_wait();      // Wait until the memory is erased.

        for (us_i = 0; us_i < SPM_PAGESIZE; us_i += 2){
            if (t_len.s){
                // Read low data from fifo
                FIFO_RD(&t_data.b[0]);
                t_len.s--;
            }else{
                t_data.b[0] = 0xCC;
            }

            // Read high data from fifo
            if (t_len.s){
                FIFO_RD(&t_data.b[1]);
                t_len.s--;
            }else{
                t_data.b[1] = 0xCC;
            }
            
            // Read high data from fifo
            boot_page_fill(t_addr.s + us_i, t_data.s);
        }

		if (!guc_mem_guard) while(1);
        boot_page_write (t_addr.s); // Store buffer in flash page.
        boot_spm_busy_wait();       // Wait until the memory is written.

        // Reenable RWW-section again. We need this if we want to jump back
        // to the application after bootloading.
        boot_rww_enable();

        // Update address
        t_addr.s += us_i;
    }
	guc_mem_guard = 0;
	guc_mem_guard = 0;
	guc_mem_guard = 0;

    FIFO_WR(0x77);

    return 0;
}
#else
uint8_t action_self_write()
{
    return 0;
}
#endif

uint8_t action_self_read(){

//  Hardcoded 0x02, action_self_read
    UINT16  t_len;
    UINT16  t_addr_h;
    UINT16  t_addr;
    uint8_t   uc_data;

    // 0x01 0xAAAAAAAA      0xLLLL          
    // CMD  Address0 32bit  Length0 16bit
    //
   
    // Read Address 
    FIFO_RD(&t_addr.b[0]);
    FIFO_RD(&t_addr.b[1]);
    FIFO_RD(&t_addr_h.b[0]); // Ignored
    FIFO_RD(&t_addr_h.b[1]); // Ignored

    // Read Length
    FIFO_RD(&t_len.b[0]);
    FIFO_RD(&t_len.b[1]);

    // t_addr_h ignored
   
    // Main Loop
    while(t_len.s > 0){
        if (t_addr.s >= 0x7000) break;

        uc_data = pgm_read_byte_near(t_addr.s);
        FIFO_WR(uc_data);

        // Update address
        t_addr.s++;

        // Decrease length
        t_len.s--;
    }

    return 0;
}


ISR(TIMER4_OVF_vect) {

    DBG_LOG_ISR("-ISR-56789abcdef0123456789abcdef");

    guc_unit_test_cnt --;    

    // single occurrence. No reload.
       
}

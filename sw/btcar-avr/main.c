/*******************************************************************************
 *  
 *  PROJECT
 *    BTCar
 *
 *  FILE
 *    dbg_log.c
 *
 *  DESCRIPTION
 *    
 *
 ******************************************************************************/
//TODO: 
//2. move DBG staff into dedicated file.
//3. Move unittest staff to dedicated file.

/* Define DBG log signature */
#include "local_fids.h"
#define LOCAL_FILE_ID FID_MAIN

#include <stdint.h>
#include <string.h>

#include <avr/pgmspace.h>
#include <avr/boot.h> 

#include "btcar_common.h"
#include "dbg_log.h"
#include "actions.h"

// Global interrupt disabling reference counter
// Stores number of times of ISR disabling. 0 -> ISR Enabled
uint8_t guc_global_isr_dis_cnt;        

#if 0
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
 #endif
 
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

LAST_ERR_t gt_last_err;

DBG_BUFF_t *gpt_dbg_log_buff;

//DBG_BUFF_t *gpt_csr_log_buff      = &gt_csr_log_buff;
//DBG_BUFF_t *gpt_cmd_resp_log_buff = &gt_cmd_resp_log_buff; 

uint8_t guc_unit_test_flag;
uint8_t guc_unit_test_cnt;

void _fatal_trap(uint16_t us_line_num)
{

    gus_trap_line = us_line_num;

    // clear all hazards on external pins
    // ...

    while(1);

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
    // --- Init perepherial devices
    // --------------------------------------------------
    init_hw_unused();

    init_hw_fifo();

    init_hw_io();
                
    leds_control(0xFF);

    dbg_buffer_init();

    unit_test_dbg_log();
    DBG_LOG("BTCAR mcu started");


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

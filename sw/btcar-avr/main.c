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
/* Define DBG log signature */
#include "local_fids.h"
#define LOCAL_FILE_ID FID_MAIN

#include <stdint.h>
#include <string.h>

#include <avr/pgmspace.h>
#include <avr/boot.h> 

#include "btcar_common.h"
#include "hw_fifo.h"
#include "dbg_log.h"
#include "servos.h"
#include "actions.h"
#include "acceler.h"

// Global interrupt disabling reference counter
// Stores number of times of ISR disabling. 0 -> ISR Enabled
uint8_t guc_global_isr_dis_cnt;

// BT IF related data
#define BT_IF_STREAM_SERVO  0x17
int8_t  gc_bt_if_curr_stream;
int8_t  guc_bt_if_curr_idx;

//      BT IF SERVO related data
#define BT_IF_SERVO_DATA_LEN    2
uint8_t guca_bt_if_servo_data[BT_IF_SERVO_DATA_LEN];

//#if defined(SIMULATOR) | defined(UNIT_TEST)
const char fusedata[] __attribute__ ((section (".fuse"))) = 
    {0xDF, 0x98, 0xFC, 0xFF, 0xFF, 0xFF};
//#endif 

extern void unit_tests_main();

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

 #if 0
    // Timers assignment:
    //  0. ( 8bit) Accelerators ADC
    //  1. (16bit)
    //  2. ( 8bit)
    //  3. (16bit) USB RX timer
    //  4. (16bit) DBG buffer unload (not fully in use)
    //  5. (16bit) Servo

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
//uint8_t action_self_write();
uint8_t action_self_read();

#define LB1 0
#define LB2 1

LAST_ERR_t gt_last_err;

void _fatal_trap(uint16_t us_line_num)
{

    GLOBAL_IRQ_DIS();
    gus_trap_line = us_line_num;

    // clear all hazards on external pins
    // ...

    disable_servos();

    while(1);

}

void init_hw_unused(void)
{

    //DDRD = 0xFF;
    //DDRL = 0xFF;
    //DDRE = 0xFF;
    //DDRK = 0xFF;
    //DDRA = 0xFF;
    //DDRH = 0xFF;

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

void init_bt_uart (void)
{

#define PINRXD2 PINH0
#define PINTXD2 PINH1

    // TX - output 0
    // RX - Pull-up 
    // RXD2 => PH0
    // TXD2 => PH1

    DDRH  &= ~((1<<PINRXD2) | (1<<PINTXD2));
    PORTH &= ~((1<<PINRXD2) | (1<<PINTXD2));

    DDRH  |= ((0<<PINRXD2) |
              (1<<PINTXD2));

    PORTH |= ((1<<PINRXD2) |
              (0<<PINTXD2));

    UBRR2 = 25;             // 25,0 ==> 38400
    UCSR2A &= ~(1<<U2X2);

    // Set frame format: 8data, 1stop bit */
    UCSR2C =
        (0<<UPM20) |      // No parity
        (0<<USBS2) |      // 1 stop bit
        (3<<UCSZ20);      // 8 data bits

    // Enable receiver and transmitter
    UCSR2B =
        (1<<RXCIE2)|     // RX Interrupt enable
        (0<<TXCIE2)|     // TX Interrupt disable
        (1<<RXEN2) |     // Receiver enabled
        (1<<TXEN2);      // Transmitter enabled

    gc_bt_if_curr_stream = 0;
}

void init_hw_io(void)
{
    // Init LED control pins (all OFF)
    LED_DIR |= ((1<<LED_RED)|
                (1<<LED_GREEN));
}


//---------------------------------------------
int real_main()
{

    uint8_t uc_i;

    // Move Interrupt vectors to BootLoader
    MOVE_IRQ();

    // --------------------------------------------------
    // --- Init perepherial devices
    // --------------------------------------------------
    init_hw_unused();

    init_hw_fifo();

    init_hw_io();

    init_accel();

    init_servos();

    init_bt_uart();


    leds_control(0xFF);

    dbg_buffer_init();

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
    gt_hw_info.t_sign.uc_ver_maj = pgm_read_byte_near(&(gca_version[0]));
    gt_hw_info.t_sign.uc_ver_min = pgm_read_byte_near(&(gca_version[1]));
    gt_hw_info.pf_led_func = leds_control;


    for(uc_i = 0; uc_i < SIGN_LEN; uc_i++)
    {
        gt_hw_info.t_sign.ca_sign[uc_i] = 
            pgm_read_byte_near(&(gca_signature[uc_i]));
    }


    // Enable Interrupts
    GLOBAL_IRQ_FORCE_EN();

    DBG_LOG("---BTCAR mcu started---");

    DBG_LOG_D("Just note", NULL);

    DBG_LOG_D("Just note 1st line"
              "Just note 2nd line", 
              NULL
    );

    DBG_LOG_D("signature %(sign_struct)",
              VAR2ADDR_SIZE(gt_hw_info.t_sign));

    DBG_LOG_D("signature %(sign_struct) just digit %(uint8_t)",
              VAR2ADDR_SIZE(gt_hw_info.t_sign),
              VAR2ADDR_SIZE(uc_i));


    while(1)
    {
        if (guc_mcu_cmd) 
        {
            guc_mcu_cmd = proceed_command(guc_mcu_cmd);
        }

        dbg_unload(-1);

    }

    return 0;
}


uint8_t proceed_command(uint8_t uc_mcu_cmd){

    T_ACTION    *pt_action;
    uint8_t (*pf_func)();
    uint8_t uc_act_cmd;
    uint8_t uc_ret_cmd;
//    uint8_t uc_i;


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
/*
//         case ACTION_SELF_WRITE:
// 
//             // Paranoic Flash write protection
//             for (uc_i = 0; uc_i < 200; uc_i++){
//                 if (uc_mcu_cmd != ACTION_SELF_WRITE) {
//                 return 0;
//                 }
//             }
//             if (uc_i != 200) return 0;
//             guc_mem_guard = 1;
//             uc_ret_cmd = action_self_write();
//             break;
*/
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

    // 0x03 0xNN        0xDD
    // CMD  DATA LEN    DATA

    uint8_t   uc_rc;
    uint8_t   uc_data_len, uc_data;
    static  uint8_t uc_fixed_header[2] = 
        { ACTION_PING, 0x40 };
    
    // Read Len 
    FIFO_RD(&uc_data_len);

    uc_rc = ERR_OK;

    // Write Header
    uc_rc |= cmd_resp_wr(&uc_fixed_header[0], sizeof(uc_fixed_header));

    // Write Data
    while(uc_data_len){
        FIFO_RD(&uc_data);
        uc_data_len--;
        uc_rc |= cmd_resp_wr(&uc_data, 1);
    }

    cmd_resp_wr_commit(uc_rc);
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
    if (bit_is_clear(FIFO_CNTR_PIN, FIFO_CNTR_RXF))
    {

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


/*
 * Dummy main used to redefine entry point for unit tests
 * Linker option "-Wl, -ereal_main" doesn't work. Reason unknown.
 */
int main()
{
    
#ifndef UNIT_TEST
#define UNIT_TEST 0
#endif

#if (UNIT_TEST)
    unit_tests_main();
#else
    real_main();
#endif // UNIT_TEST

}

// TODO: move servo related part to servo.c
ISR(USART2_RX_vect) {

    uint8_t uc_status;
    uint8_t uc_data;

    // get RX status, trap if something wrong
    uc_status = UCSR2A;
    uc_data = UDR2;

    // Check Frame Error (FE2) and DATA overflow (DOR2)
    if ( uc_status & (_BV(FE2) | _BV(DOR2)) )
        FATAL_TRAP();

    if (gc_bt_if_curr_stream)
    {
        // Continue currently active stream
        // Servo stream processed with highest priority
        if (gc_bt_if_curr_stream == BT_IF_STREAM_SERVO)
        {
            guca_bt_if_servo_data[guc_bt_if_curr_idx] = uc_data;
            guc_bt_if_curr_idx++;
            if (guc_bt_if_curr_idx == BT_IF_SERVO_DATA_LEN)
            {
                // SERVO data completed. Update servo channels
                servo_ch_set_pw(0, guca_bt_if_servo_data[0]);
                servo_ch_set_pw(1, guca_bt_if_servo_data[1]);
                gc_bt_if_curr_stream = 0;
            }
        }
        else
        {
            FATAL_TRAP();
        }
    }
    else
    {
        // start new stream
        if (uc_data == BT_IF_STREAM_SERVO)
        {
            gc_bt_if_curr_stream = uc_data;
            guc_bt_if_curr_idx = 0;
        }
        else
        {
            FATAL_TRAP();
        }

        return;
    }
    
    return;    
}


void init_hw_fifo(void)
{

    // Init USB FIFO control pins
    // USB SI       (active low) - output 1
    // RD output 1  (active low)
    // WR output 0  (active high)
    // TXF# pullup input
    // RXE# pullup input

    // AV NOTE: on FT232BL WR is active high, but as it is 
    //          pulled up, initialization to 0 cause false 
    //          write to FIFO and breaks data flow. Thus,
    //          initialized to 1.
    FIFO_CNTR_PORT |= (1<<FIFO_CNTR_WR );

    FIFO_CNTR_PORT |= (1<<FIFO_CNTR_RD )|
                      (1<<FIFO_CNTR_RXF)|
                      (1<<FIFO_CNTR_TXE)|
                      (1<<FIFO_CNTR_SI );

    FIFO_CNTR_DIR  |= (1<<FIFO_CNTR_RD )|
                      (1<<FIFO_CNTR_WR )|
                      (1<<FIFO_CNTR_SI );

    // FIFO data bus tristated
}

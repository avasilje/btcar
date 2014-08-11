#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/boot.h> 
#include "actions.h"

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
uint8 volatile guc_mcu_cmd;
uint8 volatile guc_mem_guard;
uint16 gus_trap_line;

//------- Internal function declaration -------
uint8 proceed_command(uint8 uc_cmd);
uint8 action_ping();
uint8 action_self_write();
uint8 action_self_read();

#define LB1 0
#define LB2 1

//---------------------------------------------
int main() {

    uint8 uc_i;

    // --------------------------------------------------
    // --- Protect bootloader from internal update 
    // --- Protect chip
    // --------------------------------------------------
    // set locks bit for Boot Loader Section
    // Prohibit read from BLS, 
    // Prohibit to change BLS from Application section

//    boot_lock_bits_set( (1<<BLB12) | (1<<BLB11) |  (1<<LB1) | (1<<LB2));

    // --------------------------------------------------
    // --- Init perephirial devices
    // --------------------------------------------------

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

    // Move Interrupt vectors to BootLoader
    MCUCR = (1<<IVCE);
    MCUCR = (1<<IVSEL);

    // RX timer - Timer3. Incrementing counting till UINT16_MAX
    TCCR3B = RX_CNT_TCCRxB;
    TCNT3  = RX_CNT_RELOAD;
    ENABLE_RX_TIMER;

    // Address and data bus output zeros
    DATA_L_DIR = 0xFF;
    DATA_H_DIR = 0xFF;

    ADDR_0_DIR = 0xFF;
    ADDR_1_DIR = 0xFF;
    ADDR_2_DIR = 0xFF;
    ADDR_3_DIR = 0xFF;

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

    // Init LED control pins (all OFF)
    LED_DIR |= ((1<<LED_RED)| 
                (1<<LED_GREEN));

    // Init Control port
    // Power enable (active low) - output 1
    // SIMM reset   (active low) - output 1
    // FWR01        (active low) - output 1
    // FWR1         (active low) - output 1
    // BLK0         (active low) - output 1
    // BLK1         (active low) - output 1
    // FRD          (active low) - output 1
    

    CNTR_PORT |= (0 << CNTRL_FWR01  )|
                 (0 << CNTRL_FWR1   )| 
                 (0 << CNTRL_BLK0   )|
                 (0 << CNTRL_BLK1   )|
                 (0 << CNTRL_FRD    )|
                 (0 << CNTRL_SRST   )|
                 (1 << CNTRL_POWER  );

    CNTR_DIR |= (1 << CNTRL_FWR01  )|
                (1 << CNTRL_FWR1   )| 
                (1 << CNTRL_BLK0   )|
                (1 << CNTRL_BLK1   )|
                (1 << CNTRL_FRD    )|
                (1 << CNTRL_SRST   )|
                (1 << CNTRL_POWER  );
                
    leds_control(0xFF);

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
    sei();

    while(1){
        if (guc_mcu_cmd) {
            guc_mcu_cmd = proceed_command(guc_mcu_cmd);
        }
    }

    return 0;
}

void FATAL_TRAP(uint16 us_line_num){

	gus_trap_line = us_line_num;

	// clear all hazards on external pins
	// ...

	while(1);

}


uint8 proceed_command(uint8 uc_mcu_cmd){

    T_ACTION    *pt_action;
    uint8 (*pf_func)();
    uint8 uc_act_cmd;
    uint8 uc_ret_cmd;
	uint8 uc_i;


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
            pf_func = (uint8 (*)())pgm_read_word_near(&pt_action->pf_func);
            if (pf_func == (uint8 (*)())0xFEED) {
    
                // Find action in table
                do{
                    pt_action ++;
                    // Check end of table
                    uc_act_cmd = pgm_read_byte_near(&pt_action->uc_cmd);

                    if (uc_act_cmd == 0xFF) break;

                    if (uc_mcu_cmd == uc_act_cmd){
                        // get functor from table
                        pf_func = (uint8 (*)())pgm_read_word_near(&pt_action->pf_func);
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

uint8 action_ping(){

    // 0x11 0xNN        0xDD
    // CMD  DATA LEN    DATA

    uint8   uc_data_len, uc_data;
    
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

void leds_control(uint8 uc_leds){

    uint8 uc_temp;

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
	uint8 uc_i;

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

uint8 action_self_write(){

	uint8   uc_i, uc_j;
//  Hardcoded 0x01, action_self_write
    UINT16  t_len;
    UINT16  t_addr_h;
    UINT16  t_addr;
    UINT16  t_data;

    uint16  us_i;

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


uint8 action_self_read(){

//  Hardcoded 0x02, action_self_read
    UINT16  t_len;
    UINT16  t_addr_h;
    UINT16  t_addr;
    uint8   uc_data;

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

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "actions.h"

//------ Global variables in Program Memory ---------------------
// In program memory


// Minor and Major version
// -----------------------------------------------Do not forget update text signature -----
char  gca_version[2] __attribute__ ((section (".act_const"))) = {0, 0}; 
// ----------------------------------------------------------------------------------------
char  gca_signature[SIGN_LEN] __attribute__ ((section (".act_const"))) = "BTCAR v0";


//------ Global variables SRAM ---------------------
//uint16      gusa_verify_buff[VERIFY_BUFF_LEN];

//------ Local functions ---------------------
__attribute__ ((section (".actions"))) 
void wait_64us(){
    uint8 uc_i;
    for(uc_i = 0; uc_i < 64; uc_i++){
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
        __asm__ __volatile__ ("nop;" ::);
    }

}

__attribute__ ((section (".actions"))) 
uint8 action_leds_on(){

    uint8   uc_data;
	uint8   uc_len;

    // 0x16 0x01 0xRG
    // CMD  Len  LED ON mask

    // Read Len (must be 1)
    FIFO_RD(&uc_len);

    FIFO_RD(&uc_data);

    gt_hw_info.pf_led_func(uc_data);

    return 0;
}


#if 0
__attribute__ ((section (".actions"))) 
uint8 fill_verify_buf(uint16 us_len_w){

    uint8   uc_data, uc_i, uc_wtv;
    uint8   *puc_buff;

    // if uc_len_w == 0, then full length == N*256, where N>0
    // Check number of word to read
    uc_wtv= (uint8)VERIFY_BUFF_LEN;
    if (uc_wtv > us_len_w) uc_wtv = (uint8)us_len_w;
    
    // Prepare return value
    uc_i = uc_wtv;

    // read reference data from FIFO
    puc_buff = (uint8*)gusa_verify_buff;

    // Read First low part
    while(bit_is_set(FIFO_CNTR_PIN, FIFO_CNTR_RXF) );
    FIFO_CNTR_PORT &= ~(1<<FIFO_CNTR_RD);
    __asm__ __volatile__ ("nop" ::); 
    uc_data = FIFO_DATA_PIN;
    FIFO_CNTR_PORT |=  (1<<FIFO_CNTR_RD);

    uc_i--;

    while(uc_i){

        while(bit_is_set(FIFO_CNTR_PIN, FIFO_CNTR_RXF) );
        FIFO_CNTR_PORT &= ~(1<<FIFO_CNTR_RD);
        *puc_buff++ = uc_data;  // __asm__ __volatile__ ("nop" ::); 
        uc_data = FIFO_DATA_PIN;
        FIFO_CNTR_PORT |=  (1<<FIFO_CNTR_RD);

        uc_i--;

        while(bit_is_set(FIFO_CNTR_PIN, FIFO_CNTR_RXF) );
        FIFO_CNTR_PORT &= ~(1<<FIFO_CNTR_RD);
        *puc_buff++ = uc_data;  // __asm__ __volatile__ ("nop" ::); 
        uc_data = FIFO_DATA_PIN;
        FIFO_CNTR_PORT |=  (1<<FIFO_CNTR_RD);
    }

    // Read Last high part
    __asm__ __volatile__ ("nop" ::); 
    while(bit_is_set(FIFO_CNTR_PIN, FIFO_CNTR_RXF) );
    FIFO_CNTR_PORT &= ~(1<<FIFO_CNTR_RD);
    *puc_buff++ = uc_data;  
    uc_data = FIFO_DATA_PIN;
    FIFO_CNTR_PORT |=  (1<<FIFO_CNTR_RD);

    *puc_buff++ = uc_data; 

    return uc_wtv;
}


__attribute__ ((section (".actions"))) 
uint8 action_single_write(){
// Must be placed to reprogrammable flash area
// {"swr ", 0x14, action_single_write},

    uint8   uca_data[2];
    uint8   uca_addr[4];
    uint8   uc_type;
    uint8   uc_flash_type;

    // 0x14 0xTT  0xAAAAAAAA      0xDDDD
    // CMD  Type  Address 32bit   DATA 16-bit

    // Read Access type
    FIFO_RD(&uc_type);

    // Read Address from FIFO
    FIFO_RD(&uca_addr[0]);
    FIFO_RD(&uca_addr[1]);
    FIFO_RD(&uca_addr[2]);
    FIFO_RD(&uca_addr[3]);

    HWERR1_WA(&uca_addr[2], &uca_addr[3]);

    // Read Data from FIFO
    FIFO_RD(&uca_data[0]);
    FIFO_RD(&uca_data[1]);

    // Check BUS power
    // Do nothing if BUS not powered
    // Allow write to Module 0 only
    if ( bit_is_set(CNTR_PORT, CNTRL_POWER) ||      // Revised
        (uc_type & _BV(SEL_M0)) == 0){
        return 0;
    }

    // Load Address
    ADDR_0_PORT = uca_addr[0];
    ADDR_1_PORT = uca_addr[1];
    ADDR_2_PORT = uca_addr[2];

    // Determine flash type PXLF or CGF
    // Set A26 as pull-up input
    ADDR_3_PORT  =  (1 << ADDR_3_A26);
    ADDR_3_DIR  &= ~(1 << ADDR_3_A26);

    __asm__ __volatile__ ("nop" ::);    // Syncronization delay
    __asm__ __volatile__ ("nop" ::);    // For heavy loaded bus charge
    __asm__ __volatile__ ("nop" ::);    // For heavy loaded bus charge
    __asm__ __volatile__ ("nop" ::);    // For heavy loaded bus charge
    __asm__ __volatile__ ("nop" ::);    // For heavy loaded bus charge


    uc_flash_type = ADDR_3_PIN & (1 << ADDR_3_A26);
    if (uc_flash_type == 0){

        // CGF Flash if A26 == 0
        if (bit_is_set(ADDR_2_PORT, ADDR_2_A22))
            CNTR_PORT &= ~(1<<CNTRL_BLK1);  // Revised
        else
            CNTR_PORT &= ~(1<<CNTRL_BLK0);  // Revised

    }

    // Set HH adresses 
    // A25,,A28
    ADDR_3_PORT = uca_addr[3];

    // Restore A26 direction
    ADDR_3_DIR  |= (1 << ADDR_3_A26);

    // Configure MCU bus for output
    DATA_L_DIR = 0xFF;
    DATA_H_DIR = 0xFF;
    
    // Set data on bus
    DATA_L_PORT = uca_data[0];
    DATA_H_PORT = uca_data[1];

    if (uc_flash_type == 0){

        // CGF Flash if A26 == 0

        CNTR_PORT &= ~(1<<CNTRL_FWR01);     // Revised
        __asm__ __volatile__ ("nop" ::);    // 1 nop  60ns @16MHz 
        __asm__ __volatile__ ("nop" ::);    // 2 nop 120ns @16MHz 
        __asm__ __volatile__ ("nop" ::);    // 3 nop 180ns @16MHz 
        __asm__ __volatile__ ("nop" ::);    // Paranoia

        
        // Remove RD, BLK0, BLK1
        CNTR_PORT |= ((1 << CNTRL_FWR01)|   // Revised
                      (1 << CNTRL_BLK0 )|
                      (1 << CNTRL_BLK1 ));

    }else{
        // PXL Flash

        CNTR_PORT &= ~(1<<CNTRL_FWR01);     // Revised
        CNTR_PORT &= ~(1<<CNTRL_FWR1 );     // Revised
        __asm__ __volatile__ ("nop" ::);    // 1 nop  60ns @16MHz 
        __asm__ __volatile__ ("nop" ::);    // 2 nop 120ns @16MHz 
        __asm__ __volatile__ ("nop" ::);    // 3 nop 180ns @16MHz 
        __asm__ __volatile__ ("nop" ::);    // Paranoia
        
        // Remove WR
        CNTR_PORT |= (1 << CNTRL_FWR01);    // Revised
        CNTR_PORT |= (1 << CNTRL_FWR1);     // Revised

    }

    return 0;
}
#endif

__attribute__ ((section (".actions"))) 
uint8 action_signature(){

    uint8 uc_i;
	uint8 uc_len;

    // Read cmd len (must be 0)
    FIFO_RD(&uc_len);

    // 0x11 0xLL    0xMM    0xMM    0xCC
    // CMD  Length  Major   Minor   String name

    // Write header 
    FIFO_WR(0x11);

    // Write Length
    FIFO_WR(SIGN_LEN+2);

    // Write Major Version
    FIFO_WR(gt_hw_info.uc_ver_maj);

    // Write Minor Version
    FIFO_WR(gt_hw_info.uc_ver_min);

    for(uc_i = 0; uc_i < SIGN_LEN; uc_i++){
        FIFO_WR(gt_hw_info.ca_signature[uc_i]);
    }

    return 0;
}

//
T_ACTION gta_action_table[64] __attribute__ ((section (".act_const"))) = {

    {"mark", 0x00, (uint8 (*)())0xFEED          },    // Table signature
    {"sign", 0x11, action_signature             },    
//    {"pwr ", 0x12, action_power                 },    
    {"leds", 0x16, action_leds_on               },
    {"eot ", 0xFF, 0}                               // End of table
};

 /*
Clone header file
*/
#include <stdint.h>

#define SIGN_LEN 40

typedef struct {
        uint8_t  uc_ver_maj;
        uint8_t  uc_ver_min;
        char   ca_signature[SIGN_LEN];
        void (*pf_led_func)(uint8_t);
        uint8_t  reserved[128-44];
}HW_INFO;


typedef union {
        uint8_t  b[2];
        uint16_t s;
}ADDR16;

typedef union{
        uint8_t   b[2];
        uint16_t  s;
}LEN16;

typedef union{
        uint8_t   b[2];
        uint16_t  s;
}UINT16;

typedef union{
        uint8_t   b[4];
        uint16_t  s[2];
        uint32_t  l;
}UINT32;



#define BURN_WR_BUFF_SIZE  32

typedef struct {

    uint8_t   uc_mask;
    ADDR16    t_addr_l;
    ADDR16    t_addr_h;
    LEN16     t_len;
    uint8_t   uc_cntrl_add;
    uint8_t   uca_data[2*BURN_WR_BUFF_SIZE];
}WR_STREAM;

typedef struct {
    ADDR16  t_addr_h;
    uint8_t   uc_mask;
}VERIFY_INFO;

typedef struct action_tag{
    char  ca_name[4];
    uint8_t uc_cmd;
    uint8_t (*pf_func)();
} T_ACTION;

extern HW_INFO gt_hw_info;
extern T_ACTION gta_action_table[];
extern char  gca_version[2];
extern char  gca_signature[SIGN_LEN];

void leds_control(uint8_t uc_leds);

#define HWERR1_WA(addr_l, addr_h)\
    *(addr_h) = ((*(addr_h) << 1) | (*((unsigned char*)(addr_l)) >> 7))

#define VERIFY_BUFF_LEN    32  // in 16bit words
#define FLASH_MODULE_NUM   4   // Revised

#define CLK_FRQ 16              // MHz

// TX_BURST_SIZE = N * FT_PACKET_SIZE(62) and <= TX FIFO(384)
#define TX_BURST_SIZE   248 // have to fit to 8bit 
#define TX_FIFO_SIZE    384

#define DISABLE_RX_TIMER    TIMSK3 = 0            // Disable Overflow Interrupt
#define ENABLE_RX_TIMER     TIMSK3 = (1<<TOIE3);  // Enable Overflow Interrupt
#define RX_CNT_RELOAD       (0xFFFF - 10)         // Clarify


#define RX_CNT_TCCRxB      3    // Prescaler value (3->1/64)


#define DISABLE_TIMER_4    TIMSK4 = 0            // Disable Overflow Interrupt
#define ENABLE_TIMER_4     TIMSK4 = (1<<TOIE4);  // Enable Overflow Interrupt
#define TIMER_4_CNT_RELOAD (0xFFFF - 10)         // Clarify
#define TIMER_4_PRESCALER   3                    // Prescaler value (3->1/64)


#define FIFO_DATA_PORT  PORTC
#define FIFO_DATA_DIR   DDRC
#define FIFO_DATA_PIN   PINC
    
#define FIFO_CNTR_PORT  PORTJ
#define FIFO_CNTR_DIR   DDRJ
#define FIFO_CNTR_PIN   PINJ

#define FIFO_CNTR_SI    PINJ6
#define FIFO_CNTR_RXF   PINJ5
#define FIFO_CNTR_TXE   PINJ4
#define FIFO_CNTR_WR    PINJ3
#define FIFO_CNTR_RD    PINJ2

#define FIFO_WR(data)                                   \
    FIFO_DATA_DIR = 0xFF;                               \
    while (bit_is_set(FIFO_CNTR_PIN, FIFO_CNTR_TXE));   \
    FIFO_CNTR_PORT |=  (1<<FIFO_CNTR_WR);               \
    FIFO_DATA_PORT = data;                              \
    FIFO_CNTR_PORT &= ~(1<<FIFO_CNTR_WR)

#define FIFO_RD(p_data)                                 \
    FIFO_DATA_DIR = 0x00;   /* sync delay here */       \
    while(bit_is_set(FIFO_CNTR_PIN, FIFO_CNTR_RXF) );   \
    FIFO_CNTR_PORT &= ~(1<<FIFO_CNTR_RD);               \
    __asm__ __volatile__ ("nop" ::);                    \
    *p_data = FIFO_DATA_PIN;                            \
    FIFO_CNTR_PORT |=  (1<<FIFO_CNTR_RD)



#define LED_PORT  PORTG
#define LED_DIR   DDRG
#define LED_PIN   PING

#define LED_RED        PING1
#define LED_GREEN    PING3


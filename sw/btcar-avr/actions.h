 /*
Clone header file
*/
#include <stdint.h>
typedef int8_t      int8;   
typedef uint8_t     uint8;  
typedef int16_t     int16;
typedef uint16_t    uint16;
typedef int32_t     int32;
typedef uint32_t    uint32;
typedef int64_t     int64;
typedef uint64_t    uint64; 

#define SIGN_LEN 40

typedef struct {
        uint8  uc_ver_maj;
        uint8  uc_ver_min;
        char   ca_signature[SIGN_LEN];
        void (*pf_led_func)(uint8);
        uint8  reserved[128-44];
}HW_INFO;


typedef union {
        uint8  b[2];
        uint16 s;
}ADDR16;

typedef union{
        uint8   b[2];
        uint16  s;
}LEN16;

typedef union{
        uint8   b[2];
        uint16  s;
}UINT16;

typedef union{
        uint8   b[4];
        uint16  s[2];
        uint32  l;
}UINT32;



#define BURN_WR_BUFF_SIZE  32

typedef struct {

    uint8   uc_mask;
    ADDR16  t_addr_l;
    ADDR16  t_addr_h;
    LEN16   t_len;
    uint8   uc_cntrl_add;
    uint8   uca_data[2*BURN_WR_BUFF_SIZE];
}WR_STREAM;

typedef struct {
    ADDR16  t_addr_h;
    uint8   uc_mask;
}VERIFY_INFO;

typedef struct action_tag{
    char  ca_name[4];
    uint8 uc_cmd;
    uint8 (*pf_func)();
} T_ACTION;

extern HW_INFO gt_hw_info;
extern T_ACTION gta_action_table[];
extern char  gca_version[2];
extern char  gca_signature[SIGN_LEN];

void leds_control(uint8 uc_leds);

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


#define SEL_M0  7

#define CNTR_PORT  PORTB
#define CNTR_DIR   DDRB
#define CNTR_PIN   PINB

#define CNTRL_FWR01   PINB4
#define CNTRL_FWR1    PINB5
#define CNTRL_BLK0    PINB5
#define CNTRL_BLK1    PINB6
#define CNTRL_FRD     PINB1
#define CNTRL_SRST    PINB2
#define CNTRL_POWER   PINB3

#define LED_PORT  PORTG
#define LED_DIR   DDRG
#define LED_PIN   PING


#define LED_RED		PING1
#define LED_GREEN	PING3


#define DATA_L_PORT  PORTD
#define DATA_L_DIR   DDRD
#define DATA_L_PIN   PIND

#define DATA_H_PORT  PORTL
#define DATA_H_DIR   DDRL
#define DATA_H_PIN   PINL


#define ADDR_0_PORT  PORTE
#define ADDR_0_DIR   DDRE
#define ADDR_0_PIN   PINE

#define ADDR_1_PORT  PORTK
#define ADDR_1_DIR   DDRK
#define ADDR_1_PIN   PINK

#define ADDR_2_PORT  PORTA
#define ADDR_2_DIR   DDRA
#define ADDR_2_PIN   PINA

#define ADDR_2_A22   PINA5
#define ADDR_2_A23   PINA6


#define ADDR_3_PORT  PORTH
#define ADDR_3_DIR   DDRH
#define ADDR_3_PIN   PINH

#define ADDR_3_A24 PINH0
#define ADDR_3_A25 PINH1
#define ADDR_3_A26 PINH2
#define ADDR_3_A27 PINH3
#define ADDR_3_A28 PINH4   // Not present physicaly

#define ADDR_3_MASK   0x0F
#define ADDR_3_UNUSED_MASK 0xF0


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


#define WR_TST                                                                 \
	{                                                                          \
	    CNTR_PORT &= ~(1<<CNTRL_FRD); /* Read Strobe  Revised */               \
	                                                                           \
	    __asm__ __volatile__ ("nop;" ::);                                      \
	    __asm__ __volatile__ ("nop;" ::);                                      \
	    __asm__ __volatile__ ("nop;" ::);                                      \
	    __asm__ __volatile__                                                   \
	    (                                                                      \
	        "lds __tmp_reg__, 0x109" "\n\t"  /* Load DATA HIGH              */ \
	        "sbrc __tmp_reg__,7"     "\n\t"  /* Check MSB bit HIGH byte     */ \
	        "sbr  %1, %4"            "\n\t"  /* Set tst1 bit if HMSB not set*/ \
	        "sbic 0x09,7"            "\n\t"  /* Check MSB in LOW byte       */ \
	        "sbr  %0, %4"            "\n\t"  /* Set tst0 bit if LMSB not set*/ \
	        : "=d" (uc_tst_0), "=d" (uc_tst_1)                                 \
            : "0"  (uc_tst_0), "1"  (uc_tst_1), "M" (_BV(SEL_M0))              \
	    );                                                                     \
	                                                                           \
	    CNTR_PORT |= (1<<CNTRL_FRD);   /* Revised */                           \
	}            
            
            
#define WR_IB_TST                                                \
    {                                                            \
 	    uint8 uc_tmp;                                            \
        CNTR_PORT &= ~(1<<CNTRL_FRD); /* Read Strobe Revised */  \
                                                                 \
        __asm__ __volatile__ ("nop;" ::);                        \
        __asm__ __volatile__ ("nop;" ::);                        \
        __asm__ __volatile__ ("nop;" ::);                        \
        __asm__ __volatile__ ("nop;" ::);                        \
                                                                 \
        uc_tmp  = DATA_L_PIN;                                    \
        uc_tmp &= DATA_H_PIN;                                    \
                                                                 \
        __asm__ __volatile__                                     \
        (                                                        \
            "sbrc %2,7"     "\n\t"                               \
            "ori  %0, %3"   "\n\t"                               \
            : "=d" (uc_tst)                                      \
            : "0"  (uc_tst), "d" (uc_tmp), "M" (_BV(SEL_M0))     \
        );                                                       \
                                                                 \
        CNTR_PORT |= (1<<CNTRL_FRD);  /* Revised */              \
    }

#define WR_IB_16_TST                                             \
    {                                                            \
	    uint8 uc_tmp;                                            \
        CNTR_PORT &= ~(1<<CNTRL_FRD); /* Read Strobe Revised */  \
                                                                 \
        __asm__ __volatile__ ("nop;" ::);                        \
        __asm__ __volatile__ ("nop;" ::);                        \
        __asm__ __volatile__ ("nop;" ::);                        \
        __asm__ __volatile__ ("nop;" ::);                        \
                                                                 \
        uc_tmp  = DATA_L_PIN;                                    \
                                                                 \
        __asm__ __volatile__                                     \
        (                                                        \
            "sbrc %2,7"     "\n\t"                               \
            "ori  %0, %3"    "\n\t"                               \
            : "=d" (uc_tst)                                      \
            : "0"  (uc_tst), "d" (uc_tmp), "M" (_BV(SEL_M0))     \
        );                                                       \
                                                                 \
        CNTR_PORT |= (1<<CNTRL_FRD);    /* Revised */            \
    }



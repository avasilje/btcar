 /*
Clone header file
*/
#include <stdint.h>

#define SIGN_LEN 40

typedef struct {
    struct sign_struct {
        uint8_t  uc_ver_maj;
        uint8_t  uc_ver_min;
        char     ca_sign[SIGN_LEN];
    } t_sign;
    void (*pf_led_func)(uint8_t);
    uint8_t  reserved[128-sizeof(struct sign_struct) - sizeof(void*)];
}HW_INFO;

typedef struct dummy_struct{
    uint16_t int16_A;
    uint8_t int8_A;
} T_DUMMY;

typedef struct {
    HW_INFO *pointer_A;
    HW_INFO  struct_A;
    struct test_struct {

        union {
            uint8_t union_B1;
            uint16_t union_B2;
        } named_union_B;

        T_DUMMY  str_str_A;
        uint8_t  field_A;
        uint8_t  field_B;
        char     array_A[5];
        char     array_AA[4][3];
    } struct_B;
    
    void (*pf_func_A)(uint8_t);
    void (*pf_func_B)(HW_INFO *hw_info_p);
    struct test_struct struct_array_BB[3][5];

    enum {
        enum_str_A = 1,
        enum_str_B = 2,
        enum_str_C = 5,
        enum_str_D = 10
    } enum_A;

}DBGLOG_TEST;   



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

typedef struct action_struct {
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

#define LED_PORT  PORTG
#define LED_DIR   DDRG
#define LED_PIN   PING

#define LED_RED        PING1
#define LED_GREEN    PING3

#define ACT_SIGN     0x11
#define ACT_SERVO    0x20


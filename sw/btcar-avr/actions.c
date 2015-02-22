/* Define DBG log signature */
#include "local_fids.h"
#define LOCAL_FILE_ID FID_ACTIONS


#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "btcar_common.h"
#include "hw_fifo.h"
#include "dbg_log.h"
#include "servos.h"
#include "actions.h"

//------ Global variables in Program Memory ---------------------
// In program memory


// Minor and Major version
// -----------------------------------------------Do not forget update text signature -----
char  gca_version[2] __attribute__ ((section (".act_const"))) = {0, 0}; 
// ----------------------------------------------------------------------------------------
char  gca_signature[SIGN_LEN] __attribute__ ((section (".act_const"))) = "BTCAR v0";


//------ Global variables SRAM ---------------------

//------ Local functions ---------------------
__attribute__ ((section (".actions"))) 
void wait_64us(){
    uint8_t uc_i;
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
uint8_t action_leds_on(){

    uint8_t   uc_data;
    uint8_t   uc_len;

    // 0x16 0x01 0xRG
    // CMD  Len  LED ON mask

    // Read Len (must be 1)
    FIFO_RD(&uc_len);

    FIFO_RD(&uc_data);

    gt_hw_info.pf_led_func(uc_data);

    if (g_acc_dbg_en)
    {
        DBG_LOG("ACC dbg OFF");

    }
    DBG_LOG("Leds cmd received");
    
    g_acc_dbg_en ^= 1;

    return 0;
}

__attribute__ ((section (".actions")))
uint8_t action_servo_ch()
{

    uint8_t   uc_data;
    uint8_t   uc_ch;
    uint8_t   uc_len;

    // 0x20  0x02 0xDD  0xDD
    // CMD   Len  Ch    Val 
    // Val -> pulse width
    //   0 -> 1ms 
    // 255 -> 2ms
 
    FIFO_RD(&uc_len);
    FIFO_RD(&uc_ch);
    FIFO_RD(&uc_data);

    if (servo_ch_check_ch(uc_ch)) 
    {
        // Error 
        return 0;
    }

    servo_ch_set_pw(uc_ch, uc_data);
    return 0;
}

/*
 * Function read remaining of CMD from in buffer and writes response
 * to CMD_RESP buffer
 */

__attribute__ ((section (".actions"))) 
uint8_t action_signature(void) {

    uint8_t uc_rc;
    uint8_t uc_len;
    static  uint8_t uc_fixed_header[] = {ACT_SIGN};

    // Read cmd len (must be 0)
    FIFO_RD(&uc_len);

    // 0x11 0xMM    0xMM    0xCC
    // CMD  Major   Minor   String name

    uc_rc = ERR_OK;

    DBG_LOG_D("signature %(HW_INFO) just digit %(uint8_t)",
              VAR2ADDR_SIZE(gt_hw_info),
              VAR2ADDR_SIZE(uc_len));

    // Write header
    uc_rc |= cmd_resp_wr(&uc_fixed_header[0], sizeof(uc_fixed_header));
    uc_rc |= cmd_resp_wr((uint8_t*)&gt_hw_info.t_sign, sizeof(struct sign_struct));
    cmd_resp_wr_commit(uc_rc);
    return 0;
}

//
T_ACTION gta_action_table[64] __attribute__ ((section (".act_const"))) = {

    {"mark", 0x00, (uint8_t (*)())0xFEED     },    // Table signature
    {"sign", ACT_SIGN, action_signature          },    
    {"leds", 0x16, action_leds_on            },
    {"srvc", ACT_SERVO, action_servo_ch           },
    {"eoat", 0xFF, 0}                               // End of table
};

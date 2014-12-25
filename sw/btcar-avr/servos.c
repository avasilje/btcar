#include "local_fids.h"
#define LOCAL_FILE_ID FID_SERVOS

#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "servos.h"

//------ Global variables in Program Memory ---------------------
// In program memory


//------ Global variables SRAM ---------------------
extern uint8_t guc_unit_test_cnt;

#define SERVOS_CH_TMAX_MS       2
#define SERVOS_CH_TMIN_MS       1

// Calculate Timer period
// f = fclk/(N * (1 + TOP))
// 2.5ms = 400Hz = 16MHz / (64 * (1 + TOP))
// ==> TOP = 624 = ICR
// 400 = 16 * 10^6 / 64 / 625
#define SERVOS_CH_TMR_PRD     (2*625U) /* 5ms */
#define SERVOS_CH_TMR_TOP     (SERVOS_CH_TMR_PRD - 1)
#define SERVOS_CH_PRD_MS      5U

#define SERVOS_CH_UP_FCT      5   /* upscale precision factor */
#define SERVOS_CH_DW_FCT      5   /* downscale precision factor */
#define SERVOS_CH_OCR_1MS     ((SERVOS_CH_TMR_PRD << SERVOS_CH_UP_FCT) / SERVOS_CH_PRD_MS)
#define SERVOS_CH_OCR_COEF    (SERVOS_CH_OCR_1MS >> SERVOS_CH_DW_FCT)

#define SERVOS_CH_CFG_EOFT      { -1, "EOFT", 0, 0 }

#define SERVO_PIN       PINL
#define SERVO_DDR       DDRL
#define SERVO_PORT      PORTL

#define SERVO_TIMER     5

SERVOS_t servos;

// TODO: Move to PGM space
const SERVO_CH_CFG_t    servos_cfg[] = {
//    CH  NAME   OCR   SKIP     T/O     PW
//               CHAN  FACTOR   (CH.T)  DEF(%)
    { 0, "ACCL", 0,      0,     15,     128     },   /* Motor Servo      */
    { 1, "SWHL", 1,      0,     15,     128     },   /* Steering wheel   */
    SERVOS_CH_CFG_EOFT
};

//------ Local functions ---------------------
uint8_t servo_ch_check_ch(uint8_t ch)
{
    if (ch >= SERVOS_CH_NUM)
    {
        return  1;
    }

    if (servos.servo_ch[ch].cfg.tag[0] == 0)
    {
        return  2;
    }

    return 0;
}
uint16_t servo_ch_set_pw(uint8_t ch, uint8_t pw)
{
    uint16_t    ocr;
    SERVO_CH_t  *servo_ch_p = &servos.servo_ch[ch];
    servo_ch_p->pw = pw;

    ocr   = (SERVOS_CH_OCR_COEF * pw);
    ocr >>= (8 - SERVOS_CH_DW_FCT);         /* compensate downscale */
    ocr  += SERVOS_CH_OCR_1MS;              /* +1ms offset          */
    ocr  += 1 << (SERVOS_CH_UP_FCT - 1);    /* rounding             */
    ocr >>= SERVOS_CH_UP_FCT;               /* compensate upscale   */
    ocr   = SERVOS_CH_TMR_TOP - ocr;        /* get inverse value    */
    
    servo_ch_p->ocr_val = ocr;
    servo_ch_p->to_cnt = servo_ch_p->cfg.timeout;

    return ocr;
}
void disable_servos (void)
{

    TIMSK5 &= ~(1 << TOIE5);
    SERVO_PORT &= ~(1 << PL3);
    SERVO_DDR  &= ~(1 << PL3);

    SERVO_PORT &= ~(1 << PL4);
    SERVO_DDR  &= ~(1 << PL4);
}

uint8_t init_servos (void)
{
    const SERVO_CH_CFG_t    *srv_cfg_p;

    srv_cfg_p = &servos_cfg[0];
    memset(&servos, 0, sizeof(servos));

    TCCR5A = 0;
    TCCR5A |= (3 << COM5C0);    /* 3 ==> Set on match */
    OCR5C = -1;

    while(srv_cfg_p->ch != (uint8_t)-1)
    {
        SERVO_CH_t  *srv_ch_p;
        srv_ch_p = &servos.servo_ch[srv_cfg_p->ch];
        memcpy(&srv_ch_p->cfg, srv_cfg_p, sizeof(SERVO_CH_CFG_t));

        // Init pins to default state - Output low
        if (srv_ch_p->cfg.ocr_ch == 0)
        {
            SERVO_PORT &= ~(1 << PL3);
            SERVO_DDR  |=  (1 << PL3);
            TCCR5A |= (3 << COM5A0);    /* 3 ==> Set on match */
            OCR5A = -1;

        }
        else if (srv_ch_p->cfg.ocr_ch == 1)
        {
            SERVO_PORT &= ~(1 << PL4);
            SERVO_DDR  |=  (1 << PL4);
            TCCR5A |= (3 << COM5B0);    /* 3 ==> Set on match */
            OCR5B = -1;
        }

        servo_ch_set_pw(srv_ch_p->cfg.ch, srv_ch_p->cfg.pw_default);
        srv_ch_p->ocr_def = srv_ch_p->ocr_val;
        
        srv_cfg_p ++;
    }

    servos.curr_ch = 0;
    servos.curr_ch_p = &servos.servo_ch[0];

    // Select clock source
    // TCCR5B ( COM5[1..0] = 3 ==> Set on match
    // Mode = 14
    // WGMn[3..0] = 1 1 1 0 
    // CS - clock select ==> prescaler 256
    // CS[2..0] = 1 0 0
    // 
    TCCR5A |= (0 << COM5C0) |
              (1 << WGM51 ) |
              (0 << WGM50 );

    TCCR5B = (1 << WGM53) |
             (1 << WGM52) |
             (3 << CS50 );      /* 4 => 256; 3 => 64*/

    ICR5 = SERVOS_CH_TMR_TOP;

    // Enable interrupt
    TCNT5 = 0;
    TIFR5 = -1;
    TIMSK5 = (1 << TOIE5);

    return 0;
}

ISR(TIMER5_OVF_vect) 
{
    uint8_t     curr_ch;
    uint16_t    ocr_val;
    SERVO_CH_t  *curr_ch_p;

    curr_ch = servos.curr_ch;
    curr_ch_p = servos.curr_ch_p;

    
    if (curr_ch_p->cfg.tag[0] != 0)
    {
        // Clear current channel
        // Previously configured channel just started
        // by HW. I.e. OCR register fetched, so it is 
        // safe to update it.
        if (curr_ch_p->cfg.ocr_ch == 0) OCR5A = -1;        /* should never trigger */
        if (curr_ch_p->cfg.ocr_ch == 1) OCR5B = -1;        /* should never trigger */
    }

    // Prepare next channel
    curr_ch++;
    curr_ch_p++;

    if (curr_ch == SERVOS_CH_NUM)
    {
        curr_ch = 0;
        curr_ch_p = &servos.servo_ch[0];
    }

    if (curr_ch_p->cfg.tag[0] != 0)
    {
        if (0 == curr_ch_p->to_cnt)
        { // Set default value if timeout triggered 
            ocr_val = curr_ch_p->ocr_def;
        }
        else
        { // Refresh value and decrement TO counter
            ocr_val = curr_ch_p->ocr_val;
            curr_ch_p->to_cnt --;
        }

        if (curr_ch_p->cfg.ocr_ch == 0) OCR5A = ocr_val;
        if (curr_ch_p->cfg.ocr_ch == 1) OCR5B = ocr_val;
    }
    
    servos.curr_ch = curr_ch;
    servos.curr_ch_p = curr_ch_p;

}


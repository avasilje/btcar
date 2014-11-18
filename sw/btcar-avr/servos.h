#ifndef __SERVOS_H__
#define __SERVOS_H__

#define SERVOS_CH_NUM           4

typedef struct servo_ch_cfg_tag {
    uint8_t     ch;
    uint8_t     tag[4];
    uint8_t     ocr_ch;
    uint8_t     prd_skip;     /* Period skip factor 0,1,2,3... */
    uint8_t     timeout;
    uint8_t     pw_default;
} SERVO_CH_CFG_t;

typedef struct servo_ch_tag {
    SERVO_CH_CFG_t cfg;
    uint16_t       ocr_val;  /* Value to written to OCRx reg */
    uint16_t       ocr_def;  /* Value to written to OCRx reg at TO */
    uint8_t        to_cnt;   /* Timeout counter.  */
    uint8_t        pw;       /* pulse width 0 -> 1ms, 255 -> 2ms */
    uint16_t       pw_usec;  /* Value to written to OCRx reg */
} SERVO_CH_t;

typedef struct servos_tag {
    SERVO_CH_t  servo_ch[SERVOS_CH_NUM];
    uint8_t     curr_ch;
    SERVO_CH_t  *curr_ch_p;
} SERVOS_t;

void disable_servos (void);
uint8_t init_servos(void);
uint8_t servo_ch_check_ch(uint8_t ch);
uint16_t servo_ch_set_pw(uint8_t ch, uint8_t pw);

extern SERVOS_t servos;

#endif /* __SERVOS_H__ */
#include "local_fids.h"
#define LOCAL_FILE_ID FID_ACCELER

#include <stdint.h>
#include <avr/interrupt.h>
#include <string.h>

#include "dbg_log.h"
#include "acceler.h"

#define ACCEL_PIN       PINK
#define ACCEL_DDR       DDRK
#define ACCEL_PORT      PORTK

#define ACCEL_CH_NUM                6
#define ACCEL_TIMER_PRESCALER       256
#define ACCEL_SMPL_PRD              ((800 * 16) / ACCEL_TIMER_PRESCALER)  // 12800 clocks
#define ACCEL_SMPL_INTERVAL_NEXT    ((128 * 16) / ACCEL_TIMER_PRESCALER)  // 2048 clock ==> 128usec 
                                                                          // conversion time + some guard time 
#define ACCEL_SMPL_INTERVAL_LAST    (ACCEL_SMPL_PRD - ((ACCEL_CH_NUM - 1) * ACCEL_SMPL_INTERVAL_NEXT))


typedef struct accel_tag {
    int8_t  curr_ch;
    int8_t  tick;
    int16_t sampl[ACCEL_CH_NUM];
    int8_t  adc_ref;

} ACCEL_t;

ACCEL_t accel;


// Calculate Timer period
#define SERVOS_CH_TMR_PRD     (2*625U) /* 5ms */
#define SERVOS_CH_TMR_TOP     (SERVOS_CH_TMR_PRD - 1)


#define ACCEL_TMR   (256 - )

//------ Global variables in Program Memory ---------------------
// In program memory


//------ Global variables SRAM ---------------------


//------ Local functions ---------------------
void disable_accel (void)
{
}

uint8_t init_accel (void)
{
    // Accelerometer bandwidth (BW) = 500Hz
    // ACC sampling rate > 2*BW = 1.250kHz
    // Set timer to 800usec (1.250 kHz)
    // 800usec @16Mhz ==> 12800 clock
    // Set prescaler to 

    memset(&accel, 0, sizeof(accel));
    accel.curr_ch = 0;
    accel.adc_ref = (3 << REFS0);   // 2.56V => +/- 2.32G

    // TRmp debug
    ACCEL_PORT = 0;
    ACCEL_DDR =  
        (1 << PINK6) |
        (1 << PINK7);


    // ------------------------------------------------------
    // --- Configure ADC 
    // ------------------------------------------------------
    ADCSRA = 0;         // Disable ADC. Just in case.

    // Power Enable. Just in case. Enabled at startup
    PRR0 &= ~(1<<PRADC);

    // Selects auto trigger source
    ADCSRB =
        (3<<ADTS0 ) |   // 5->compare match 1A
        (1<<MUX5  ) |   // Choose ADC channel 8..15
        (0<<ACME  );    // Comparator disabled

    // Disable digital input circuit on ADC pins 8..13
    DIDR0 = 
        (1 <<  ADC8D) |
        (1 <<  ADC9D) |
        (1 << ADC10D) |
        (1 << ADC11D) |
        (1 << ADC12D) |
        (1 << ADC13D);

    ADMUX = accel.adc_ref |
            (accel.curr_ch << MUX0);

    ADCSRA = 
        (1<<ADEN ) |    // ADC Enable
        (1<<ADSC ) |    // Sart very first conversion
        (1<<ADATE) |    // Auto trigger enabled
        (0<<ADIE ) |    // ADC interrupt enabled (enabled in OCR interrupt)
        (1<<ADIF ) |    // Clear ADC complete interrupt
        (7<<ADPS0);     // ADC clock  7->125kHz @ 16MHz

    // ------------------------------------------------------
    // --- Configure Timer 2 
    // ------------------------------------------------------
    // Disable timer interrupts
    TIMSK0 = 0;
    TIFR0  = -1;
    TCNT0  = 0;

    TCCR0A = (2 << WGM00);   // CTC mode
             
    TCCR0B = (0 << WGM02) |  // CTC mode
             (4 << CS00 );   // 4 => 256;  see ACCEL_TIMER_PRESCALER

    // Set to MAX to ensure ADC initialization 
    // finished at that time. Working value set in
    // OCR interrupt.
    OCR0A = 0xFF;

    // Enable OCR2A interrupt
    TIMSK0 = (1 << OCIE0A);

    return 0;
}

ISR(TIMER0_COMPA_vect) 
{
    // ADC conversation start triggered by this 
    // interrupt.
    
    ACCEL_PORT |= (1 << PINK6);

    // Enable ADC interrupt (disable at init)
    ADCSRA |= (1<<ADIE );

    if (accel.curr_ch == ACCEL_CH_NUM - 1)
    {
        OCR0A = ACCEL_SMPL_INTERVAL_LAST - 1;
    }
    else 
    {
        OCR0A = ACCEL_SMPL_INTERVAL_NEXT - 1;
    }
}

ISR(ADC_vect) 
{

    ACCEL_PORT &= ~(1 << PINK6);

    // Save ADC conversion result
    accel.sampl[accel.curr_ch] = ADCW;
    
    // Switch channel
    accel.curr_ch ++;
    if (accel.curr_ch == ACCEL_CH_NUM)
    {
        if (g_acc_dbg_en)
        {
            // write channels to out buff
            dbg_log_isr(__LINE__, LOCAL_FILE_ID, 16, (char *)&accel);    // sizeof(accel)
        }

        accel.curr_ch = 0;
        accel.tick ++;
    }
    
    // Switch ADC multiplex
    ADMUX = accel.adc_ref |
            (accel.curr_ch << MUX0);

}

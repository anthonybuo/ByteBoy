#include <MSP430.h>

#include "rng.h"

volatile unsigned char rng;

void RNGSetup(void) {
    /* Configure ADC
     * Accelerometer output pins connected to A12, A13, A14
     */
    ADC10CTL0 &= ~ADC10ENC;  // disable conversion to allow setting other stuff
    // was ADC10SHT_2 before, had to increase sampling time
    // because the ADC interrupt was too frequent
    // and the timer interrupt was never firing
    ADC10CTL0 |= ADC10SHT_15 | ADC10ON;  // S&H = 16 ADC clks, ADC on
    ADC10CTL1 |= ADC10SHP;  // ADCCLK = MODOSC; sampling timer (idk)
    ADC10CTL2 |= ADC10RES;  // 10-bit resolution
    ADC10MCTL0 |= ADC10INCH_12;  // A12 input select, Vref = Vcc
    ADC10IE |= ADC10IE0;  // Enable ADC conv complete interrupt
    ADC10CTL0 |= ADC10ENC | ADC10SC;  // Enable and start conversion
}

unsigned char GenerateRandomByte(void) {
    return rng;
}

#pragma vector = ADC10_VECTOR
__interrupt void ADC10_ISR(void) {
    switch(__even_in_range(ADC10IV, 12)) {
        case 0: break;
        case 2: break;
        case 4: break;
        case 6: break;
        case 8: break;
        case 10: break;
        // 12) Conversion complete interrupt
        case 12:
            ADC10CTL0 &= ~ADC10ENC;  // disable conversion
            rng = ADC10MEM0;
            ADC10CTL0 |= ADC10ENC | ADC10SC;  // Enable and start
                 break;
        default: break;
    }
}

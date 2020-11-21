#include <msp430.h> 

#include "chip8.h"

void ClockInit(void) {
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
    CSCTL0 = 0xA500;  // Write password to modify CS registers
    CSCTL1 = DCORSEL | DCOFSEL0;  // DCO = 20MHz
    CSCTL2 = SELM0 | SELM1 | SELA0 | SELA1 | SELS0 | SELS1;  // MCK = ACLK = SMCLK = DCO
}

int main(void) {
	ClockInit();
    Chip8Main();
}

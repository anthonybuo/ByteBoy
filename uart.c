#include <msp430.h>

#include "uart.h"
#include "chip8.h"

volatile unsigned char done_loading_game = 0;

static void UART_setup(void) {
    UCA0CTLW0 = UCSSEL0;  // Clock UCA with ACLK
    UCA0BRW = 130;  // See User's Guide (Table 18-5)
    UCA0MCTLW = 0x2500 | UCOS16 | UCBRF3; // See User's Guide (Table 18-5)
    UCA0IE |= UCRXIE;  // rx interrupt enable

    // P2.0 and P2.1 for UCA0TX and UCA0RX
    P2SEL0 &= ~(BIT0 | BIT1);
    P2SEL1 |= BIT0 | BIT1;
}

void RequestGame(void) {
    UART_setup();
    _delay_cycles(1000);
    UCA0TXBUF = 0x55;
    while (!(UCA0IFG & UCTXIFG));
}

#pragma vector = USCI_A0_VECTOR
__interrupt void UART_RX_INTERRUPT(void) {
    static unsigned int state = 0;
    static unsigned int game_file_size = 0;

    unsigned char rx_byte = UCA0RXBUF;
    if (rx_byte == 255 && state == 0) {
        // start byte, do nothing
        state++;
    }
    else if (state == 1) {
        // file size high byte
        game_file_size = rx_byte << 8;
        state++;
    } else if (state == 2) {
        // file size low byte
        game_file_size |= rx_byte;
        if (game_file_size > SIZE_GAME_FILE1) {
            // error, file too large
            state = 0;
            return;
        }
        state++;
    } else if (state > 2) {
        // start reading into game file memory
        GAME_FILE1[state - 3] = rx_byte;
        state++;
        if (state >= game_file_size + 3) {
            done_loading_game = 1;
        }
    }
}

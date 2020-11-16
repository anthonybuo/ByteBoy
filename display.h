#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <msp430.h>

// SH1106 Commands
#define TURN_ON_SCREEN 175
#define SET_COLUMN_ADDR_LOWER  0b00000000
#define SET_COLUMN_ADDR_HIGHER 0b00010000
#define SET_PAGE_ADDR          0b10110000

volatile unsigned char tx_data = 0b10100101;
volatile unsigned char rx_data = 0;

static void SPI_init(void) {
    // Initialize UCB0 as Master
    // UCA0CTLW0 = 0xA981; // 0xA141;
    UCB0CTLW0 |= UCSWRST |  // reset UCA0 to enable editing settings
                 UCSSEL1 |  // SMCLK source
                 UCSYNC |   // synchronous
                 UCMST |    // master mode
                 UCMSB |    // MSB first
                 UCCKPL; //|  // clock polarity, inactive high
                 //UCCKPH;    // clock phase select, data cap'd on first UCLK edge and changed on following
    UCB0BRW = 100;  // Change SPI clock frequency
    UCB0CTLW0 &= ~UCSWRST;  // turn on SPI A1
}

static void TimerB1_init(void) {
    /* Configure Timer B1 */
    TB1CCR0 = 5000;  // CCR0 compare set point (20k * 20MHz -> 1ms period, tested down to 200 ticks)
    TB1CCR2 = 5000;  // CCR2 compare set point (20k * 20MHz -> 1ms period, tested down to 200 ticks)
    TB1CTL |= TBSSEL__ACLK |  // Source TB1 with ACLK
              MC_1 |          // Up mode (counts to CCR0)
              TBCLR;          // Clear the counter register TBR
}

static void GPIO_init(void) {
    /* Configure pins for UCB0 */
    P2SEL1 |= BIT2; // CLK
    P2SEL0 &= ~BIT2;
    P1SEL1 |= BIT6;  // SIMO
    P1SEL0 &= ~BIT6;

    /* Configure debug LEDs */
    P3DIR |= BIT4;
    P3OUT &= ~BIT4;

    /* Configure P1.5 -> CS */
    P1DIR |= BIT5;
    P1OUT &= ~BIT5;

    /* Configure P4.0 -> DC */
    P4DIR |= BIT0;
    P4OUT &= ~BIT0;
}

static void OLED_init(void) {
    P1OUT &= ~BIT5;  // CS low
    P4OUT |= BIT0;  // DC in data mode
}


static void DMA_init(void) {
    DMACTL0 = DMA0TSEL_10;  // TB1CCR2 triggers DMA0
    DMA0SA = (unsigned int)&tx_data;  // DMA source address
    DMA0DA = (unsigned int)&UCA0TXBUF;  // DMA destination address
    DMA0SZ = 1;  // transfer block size
    DMA0CTL = DMADT_4 |  // repeated single transfer
              DMASBDB;  // source byte to destination byte
    DMA0CTL |= DMAEN | DMAREQ;  // enable and start a request
}

static void SPI_write(unsigned char data, unsigned char A0) {
    if (A0 == 0) {
        P4OUT &= ~BIT0;
    } else if (A0 == 1) {
        P4OUT |= BIT0;
    } else {
        return;  // error
    }

    P1OUT &= ~BIT5;
    UCB0TXBUF = data;
    _delay_cycles(1000);
    P1OUT |= BIT5;
}

int DisplayDrawPixel(unsigned char x, unsigned char y) {
    if (x > 131) {
        return -1;
    }
    if (y > 63) {
        return -1;
    }
    SPI_write(SET_COLUMN_ADDR_LOWER | (x & 0b1111), 0);
    SPI_write(SET_COLUMN_ADDR_HIGHER | ((x & 0b11110000) >> 4), 0);
    SPI_write(SET_PAGE_ADDR | (y >> 3), 0);  // >>3 == divide by 8
    SPI_write(1 << (y & 7), 1);  // &7 == % 8
    return 1;
}

void clearScreen(void) {
    int y;
    for (y = 0; y < 8; y++) {
        SPI_write(SET_PAGE_ADDR | y, 0);
        int x;
        for (x = 0; x < 132; x++) {
            SPI_write(0, 1);
        }
    }
}

void DisplaySetup(void) {
    TimerB1_init();
    GPIO_init();
    SPI_init();
    // DMA_init();
    OLED_init();
    _enable_interrupts();

    SPI_write(TURN_ON_SCREEN, 0);
    clearScreen();
}

#endif  // _DISPLAY_H_


#include <MSP430.h>

#include "keypad.h"

#define GPIO_RISE_TIME_DELAY 200
#define NUM_COLUMNS 4
#define NUM_ROWS 4

unsigned char labels[NUM_COLUMNS][NUM_ROWS] = {
    // Column 1, Rows 1->4
    { 0x1, 0x4, 0x7, 0xE },
    // Column 2, Rows 1->4
    { 0x2, 0x5, 0x8, 0x0 },
    // Column 3, Rows 1->4
    { 0x3, 0x6, 0x9, 0xF },
    // Column 4, Rows 1->4
    { 0xA, 0xB, 0xC, 0xD }
};

void KeypadSetup(void) {
    // Set all rows to input
    P1DIR &= ~(BIT3 | BIT2 | BIT1 | BIT0);
    // Set all rows to have pull up/down resistors
    P1REN |= (BIT3 | BIT2 | BIT1 | BIT0);
    // Make them all pull-down
    P1OUT &= ~(BIT3 | BIT2 | BIT1 | BIT0);

    // Set all columns to output
    P3DIR |= (BIT3 | BIT2 | BIT1 | BIT0);
}

/*
 * For the specified column, tests to see if each row
 * has been pressed. Updates the key variable
 * accordingly.
 */
static void ScanRows(unsigned char column_num) {
    unsigned char i;
    P3OUT |= (1 << column_num);
    _delay_cycles(GPIO_RISE_TIME_DELAY);
    for (i = 0; i < NUM_ROWS; i++) {
        if ((P1IN & (1 << i)) == (1 << i)) {
            keys |= (1 << labels[column_num][i]);
        } else {
            keys &= ~(1 << labels[column_num][i]);
        }
    }
    P3OUT &= ~(1 << column_num);
}

void KeypadPoll(void) {
    unsigned char i;
    for (i = 0; i < NUM_COLUMNS; i++) {
        ScanRows(i);
    }
}

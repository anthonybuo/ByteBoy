#ifndef _KEYPAD_H_
#define _KEYPAD_H_

extern unsigned int keys;

void KeypadSetup(void) {
    keys = 1;
}

void KeypadPoll(void) {
}

#endif  // _KEYPAD_H_

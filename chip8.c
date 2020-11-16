#include "chip8.h"
#include "display.h"
#include "keypad.h"
#include "uart.h"

// Memory accessible to the CHIP-8 program at runtime
#pragma PERSISTENT(memory)
#define SIZE_MEMORY 4096
unsigned char memory[SIZE_MEMORY] = {0};

// 64 x 32 Graphics buffer
#pragma PERSISTENT(graphics)
#define SIZE_GRAPHICS 2048
unsigned char graphics[SIZE_GRAPHICS] = {0};

// Game file "golden copy"
#pragma PERSISTENT(GAME_FILE1)
unsigned char GAME_FILE1[SIZE_GAME_FILE1];

// Copy of game file accessible to CHIP-8 program at runtime
#pragma PERSISTENT(GAME_FILE2)
#define SIZE_GAME_FILE2 1024
unsigned char GAME_FILE2[SIZE_GAME_FILE2] = {0};

// General purpose registers
#pragma PERSISTENT(REGISTERS)
#define SIZE_REGISTERS 16
unsigned char REGISTERS[SIZE_REGISTERS] = {0};

// Program counter
#pragma PERSISTENT(PC)
unsigned int PC = 0;

// Stack pointer
#pragma PERSISTENT(SP)
unsigned char SP = 0;

// Stack
#pragma PERSISTENT(stack)
#define SIZE_STACK 16
unsigned int stack[SIZE_STACK] = {0};

// Delay timer (60Hz)
#pragma PERSISTENT(delay_timer)
unsigned char delay_timer = 0;

// Sound timer (60Hz)
#pragma PERSISTENT(sound_timer)
unsigned char sound_timer = 0;

// User input keypad
#pragma PERSISTENT(keys)
unsigned int keys = 0;

void Chip8Main(void) {
    DisplaySetup();
    KeypadSetup();
    RequestGame();

    unsigned int i = 0;
    unsigned char j = 0;
    while(1) {
            // memory[i] = j;
            // graphics[i] = j;
            // GAME_FILE1[i] = j;
            // GAME_FILE2[i] = j;
            // REGISTERS[i] = j;
            i++; j++;
            i %= SIZE_REGISTERS;
            unsigned int k;
            for(k = 0; k < 64; k++) {
                DisplayDrawPixel(k, k);
            }
      }
}

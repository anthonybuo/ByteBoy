#include <msp430.h>

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
unsigned char GAME_FILE1[SIZE_GAME_FILE1] = {0};

// Copy of game file accessible to CHIP-8 program at runtime
#pragma PERSISTENT(GAME_FILE2)
#define SIZE_GAME_FILE2 1024
unsigned char GAME_FILE2[SIZE_GAME_FILE2] = {0};

// General purpose registers
#pragma PERSISTENT(REG)
#define SIZE_REGISTERS 16
unsigned char REG[SIZE_REGISTERS] = {0};

// CHIP-8 program start address within main memory
#define PROGRAM_START 0x200

// Program counter
#pragma PERSISTENT(PC)
unsigned int PC = PROGRAM_START;

// Stack pointer
#pragma PERSISTENT(SP)
unsigned char SP = 0;

// Pointer
#pragma PERSISTENT(I)
unsigned int I = 0;

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

// Carry flag
#pragma PERSISTENT(carry)
unsigned char carry = 0;

// Opcode bit manipulation macros
#define GET_0x0111(x) (x & 0x0FFF)
#define GET_0x0011(x) (x & 0x00FF)
#define GET_0x1000(x) (((x) & 0xF000) >> 12)
#define GET_0x0100(x) (((x) & 0x0F00) >> 8)
#define GET_0x0010(x) (((x) & 0x00F0) >> 4)
#define GET_0x0001(x) (((x) & 0x000F) >> 0)

static unsigned char opcode;

/*
 * Clears the screen.
 */
static inline void OP_00E0(void) {
    ClearScreen();
}

/*
 * Returns from a subroutine.
 */
static inline void OP_00EE(void) {
    SP--;
    PC = stack[SP];
    PC += 2;
}

/*
 * Jump to address NNN.
 */
static inline void OP_1NNN(void) {
    PC = GET_0x0111(opcode);
}

/*
 * Calls subroutine at NNN.
 */
static inline void OP_2NNN(void) {
    stack[SP] = PC;
    SP++;
    PC = GET_0x0111(opcode);
}

/*
 * Skips the next instruction if VX equals NN
 */
static inline void OP_3XNN(void) {
    if (REG[GET_0x0100(opcode)] == GET_0x0011(opcode)) {
        PC += 2;
    }
    PC += 2;
}

/*
 * Skips the next instruction if VX doesn't equals NN
 */
static inline void OP_4XNN(void) {
    if (REG[GET_0x0100(opcode)] != GET_0x0011(opcode)) {
        PC += 2;
    }
    PC += 2;
}

/*
 * Skips the next instruction if VX equals VY.
 */
static inline void OP_5XY0(void) {
    if (REG[GET_0x0100(opcode)] == REG[GET_0x0010(opcode)]) {
        PC += 2;
    }
    PC += 2;
}

/*
 * Sets VX to NN.
 */
static inline void OP_6XNN(void) {
    REG[GET_0x0100(opcode)] = GET_0x0011(opcode);
    PC += 2;
}

/*
 * Adds NN to VX (carry not changed).
 */
static inline void OP_7XNN(void) {
    REG[GET_0x0100(opcode)] += GET_0x0011(opcode);
    PC += 2;
}

/*
 * Sets VX to the value of VY.
 */
static inline void OP_8XY0(void) {
    REG[GET_0x0100(opcode)] = REG[GET_0x0010(opcode)];
    PC += 2;
}

/*
 * Sets VX to VX bitwise-or VY.
 */
static inline void OP_8XY1(void) {
    REG[GET_0x0100(opcode)] |= REG[GET_0x0010(opcode)];
    PC += 2;
}

/*
 * Sets VX to VX bitwise-and VY.
 */
static inline void OP_8XY2(void) {
    REG[GET_0x0100(opcode)] &= REG[GET_0x0010(opcode)];
    PC += 2;
}

/*
 * Sets VX to VX ^ VY.
 */
static inline void OP_8XY3(void) {
    REG[GET_0x0100(opcode)] ^= REG[GET_0x0010(opcode)];
    PC += 2;
}

/*
 *  Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't.
 */
static inline void OP_8XY4(void) {
    if (0xFFFF - REG[GET_0x0100(opcode)] < REG[GET_0x0010(opcode)]) {
        carry = 1;
    } else {
        carry = 0;
    }
    REG[GET_0x0100(opcode)] += REG[GET_0x0010(opcode)];
    PC += 2;
}

/*
 * VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
 */
static inline void OP_8XY5(void) {
    if (REG[GET_0x0010(opcode)] > REG[GET_0x0100(opcode)]) {
        REG[0xF] = 0;
    } else {
        REG[0xF] = 1;
    }
    REG[GET_0x0100(opcode)] -= REG[GET_0x0010(opcode)];
    PC += 2;
}

/*
 * Stores the least significant bit of VX in VF and then shifts VX to the right by 1.
 */
static inline void OP_8XY6(void) {
    REG[0xF] = (REG[GET_0x0100(opcode)] >> 1);
    REG[GET_0x0100(opcode)] >>= 1;
    PC += 2;
}

/*
 * Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
 */
static inline void OP_8XY7(void) {
    if (REG[GET_0x0100(opcode)] > REG[GET_0x0010(opcode)]) {
        REG[0xF] = 0;
    } else {
        REG[0xF] = 1;
    }
    REG[GET_0x0100(opcode)] = REG[GET_0x0010(opcode)] - REG[GET_0x0100(opcode)];
    PC += 2;
}

/*
 * Stores the most significant bit of VX in VF and then shifts VX to the left by 1.
 */
static inline void OP_8XYE(void) {
    REG[0xF] = (REG[GET_0x0100(opcode)] << 1);
    REG[GET_0x0100(opcode)] <<= 1;
    PC += 2;
}

/*
 * Skips the next instruction if VX doesn't equal VY.
 */
static inline void OP_9XY0(void) {
    if (REG[GET_0x0100(opcode)] != REG[GET_0x0010(opcode)]) {
        PC += 2;
    }
    PC += 2;
}

/*
 * Sets I to address NNN.
 */
static inline void OP_ANNN(void) {
    I = GET_0x0111(opcode);
    PC += 2;
}

/*
 * Jumps to address NNN plus V0.
 */
static inline void OP_BNNN(void) {
    PC = GET_0x0111(opcode) + REG[0];
}

static inline void OP_CXNN(void) {}
static inline void OP_DXYN(void) {}
static inline void OP_EX9E(void) {}
static inline void OP_EXA1(void) {}

/*
 * Sets VX to the value of the delay timer.
 */
static inline void OP_FX07(void) {
    REG[GET_0x0100(opcode)] = delay_timer;
    PC += 2;
}

static inline void OP_FX0A(void) {}

/*
 * Sets the delay timer to VX.
 */
static inline void OP_FX15(void) {
    delay_timer = REG[GET_0x0100(opcode)];
    PC += 2;
}

static inline void OP_FX18(void) {}

/*
 * Adds VX to I. VF is not affected.
 */
static inline void OP_FX1E(void) {
    I += REG[GET_0x0100(opcode)];
    PC += 2;
}

static inline void OP_FX29(void) {}

/*
 * Stores the BCD representation of VX, with the MSD at I, middle digit at I+1, and LSD at I+2.
 */
static inline void OP_FX33(void) {
    unsigned char x = GET_0x0100(opcode);
    memory[I] = x / 100;
    memory[I + 1] = (x / 10) % 10;
    memory[I + 2] = x % 10;
    PC += 2;
}

/*
 * Stores V0 to VX (including VX) in memory starting at address I.
 * The offset from I is increased by 1 for each value written, but I itself is left unmodified
 */
static inline void OP_FX55(void) {
    unsigned char i;
    for (i = 0; i < GET_0x0100(opcode); i++) {
        memory[I + i] = REG[i];
    }
    PC += 2;
}

/*
 * Fills V0 to VX (including VX) with values from memory starting at address I.
 * The offset from I is increased by 1 for each value written, but I itself is left unmodified
 */
static inline void OP_FX65(void) {
    unsigned char i;
    for (i = 0; i < GET_0x0100(opcode); i++) {
        REG[i] = memory[I + i];
    }
    PC += 2;
}

/*
 * Invalid opcode encountered.
 */
static inline void OP_ERROR(void) {
    while (1);
}

/*
 * Copy game file into CHIP-8 runtime memory.
 */
static void CopyGame(void) {
    memcpy(memory + PROGRAM_START, GAME_FILE1, SIZE_GAME_FILE1);
}

/*
 * Fetch instruction from memory and execute it.
 */
static void ExecuteInstruction(void) {
    // Fetch instruction
    opcode = memory[PC];

    // Execute instruction
    switch (opcode & 0xF000) {
    case 0x0:
        switch (opcode & 0x00FF) {
        case 0xE0:
            OP_00E0();
            break;
        case 0xEE:
            OP_00EE();
            break;
        default:
            OP_ERROR();
            break;
        }
        break;
    case 0x1:
        OP_1NNN();
        break;
    case 0x2:
        OP_2NNN();
        break;
    case 0x3:
        OP_3XNN();
        break;
    case 0x4:
        OP_4XNN();
        break;
    case 0x5:
        OP_5XY0();
        break;
    case 0x6:
        OP_6XNN();
        break;
    case 0x7:
        OP_7XNN();
        break;
    case 0x8:
        switch (opcode & 0x000F) {
        case 0x0:
            OP_8XY0();
            break;
        case 0x1:
            OP_8XY1();
            break;
        case 0x2:
            OP_8XY2();
            break;
        case 0x3:
            OP_8XY3();
            break;
        case 0x4:
            OP_8XY4();
            break;
        case 0x5:
            OP_8XY5();
            break;
        case 0x6:
            OP_8XY6();
            break;
        case 0x7:
            OP_8XY7();
            break;
        case 0xE:
            OP_8XYE();
            break;
        default:
            OP_ERROR();
            break;
        }
        break;
    case 0x9:
        OP_9XY0();
        break;
    case 0xA:
        OP_ANNN();
        break;
    case 0xB:
        OP_BNNN();
        break;
    case 0xC:
        OP_CXNN();
        break;
    case 0xD:
        OP_DXYN();
        break;
    case 0xE:
        switch (opcode & 0x00FF) {
        case 0x9E:
            OP_EX9E();
            break;
        case 0xA1:
            OP_EXA1();
            break;
        default:
            OP_ERROR();
            break;
        }
        break;
    case 0xF:
        switch (opcode & 0x00FF) {
        case 0x07:
            OP_FX07();
            break;
        case 0x0A:
            OP_FX0A();
            break;
        case 0x15:
            OP_FX15();
            break;
        case 0x18:
            OP_FX18();
            break;
        case 0x1E:
            OP_FX1E();
            break;
        case 0x29:
            OP_FX29();
            break;
        case 0x33:
            OP_FX33();
            break;
        case 0x55:
            OP_FX55();
            break;
        case 0x65:
            OP_FX65();
            break;
        default:
            OP_ERROR();
            break;
        }
        break;
    default:
        OP_ERROR();
        break;
    }
}

static void DebugExecuteInstruction(void) {

}

void Chip8Main(void) {
    // debug led
    P3DIR |= BIT4;
    P3OUT &= ~BIT4;

    DisplaySetup();
    KeypadSetup();
    RequestGame();
    while (!done_loading_game);
    P3OUT |= BIT4;
    CopyGame();

    while(1) {
        // ExecuteInstruction();
        DebugExecuteInstruction();

        // Debug: write to screen
        /*
        unsigned int k;
        for(k = 0; k < 64; k++) {
            DisplayDrawPixel(k, k);
        }
        ClearScreen();
        */
      }
}

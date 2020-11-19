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
#pragma PERSISTENT(gfx)
#define SIZE_GFX 2048
unsigned char gfx[SIZE_GFX] = {0};

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
volatile unsigned char delay_timer = 0;

// Sound timer (60Hz)
#pragma PERSISTENT(sound_timer)
volatile unsigned char sound_timer = 0;

// User input keypad
#pragma PERSISTENT(keys)
unsigned int keys = 0;

// Opcode bit manipulation macros
#define GET_0x0111(x) (x & 0x0FFF)
#define GET_0x0011(x) (x & 0x00FF)
#define GET_0x1000(x) (((x) & 0xF000) >> 12)
#define GET_0x0100(x) (((x) & 0x0F00) >> 8)
#define GET_0x0010(x) (((x) & 0x00F0) >> 4)
#define GET_0x0001(x) (((x) & 0x000F) >> 0)

#define CHAR_SIZE 5
#define FONTSET_SIZE 80

unsigned char chip8_fontset[FONTSET_SIZE] =
{
    0xF0, 0x90, 0x90, 0x90, 0xF0, //0
    0x20, 0x60, 0x20, 0x20, 0x70, //1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, //2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, //3
    0x90, 0x90, 0xF0, 0x10, 0x10, //4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, //5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, //6
    0xF0, 0x10, 0x20, 0x40, 0x40, //7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, //8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, //9
    0xF0, 0x90, 0xF0, 0x90, 0x90, //A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, //B
    0xF0, 0x80, 0x80, 0x80, 0xF0, //C
    0xE0, 0x90, 0x90, 0x90, 0xE0, //D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, //E
    0xF0, 0x80, 0xF0, 0x80, 0x80  //F
};

static unsigned char opcode;

static unsigned char draw_flag = 0;

unsigned char msp430_rng(void) {
    return opcode & 0x00FF;
}

/*
 * Clears the screen.
 */
static inline void OP_00E0(void) {
    unsigned int i;
    for (i = 0; i < SIZE_GFX; i++) {
        gfx[i] = 0;
    }
    draw_flag = 1;
    PC += 2;
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
    if (0xFF - REG[GET_0x0100(opcode)] < REG[GET_0x0010(opcode)]) {
        REG[0xF] = 1;
    } else {
        REG[0xF] = 0;
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
    REG[0xF] = (REG[GET_0x0100(opcode)] & 1);
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
    REG[0xF] = (REG[GET_0x0100(opcode)] >> 7);
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

/*
 * Sets VX to the result of a bitwise-and operation on a random byte and NN.
 */
static inline void OP_CXNN(void) {
    REG[GET_0x0100(opcode)] = GET_0x0011(opcode) & msp430_rng();
    PC += 2;
}

/*
 * Draws a sprite at (VX, VY) that was a width of 8
 * pixels and a height of N pixels. VF is set to 1 if
 * any screen pixels are changed from set to cleared
 * and 0 otherwise.
 *
 * Each row of 8 pixels is read as bit-coded starting
 * from memory location I. I is left unchanged.
 */
static inline void OP_DXYN(void) {
    unsigned char x = REG[GET_0x0100(opcode)];
    unsigned char y = REG[GET_0x0010(opcode)];
    unsigned char height = GET_0x0001(opcode);
    unsigned char pixel;

    REG[0xF] = 0;
    unsigned char yline = 0;
    for (yline = 0; yline < height; yline++)
    {
        pixel = memory[I + yline];
        unsigned char xline = 0;
        for(xline = 0; xline < 8; xline++)
        {
            if((pixel & (0x80 >> xline)) != 0)
            {
                if(gfx[(x + xline + ((y + yline) * 64))] == 1)
                {
                    REG[0xF] = 1;
                }
                gfx[x + xline + ((y + yline) * 64)] ^= 1;
            }
        }
    }

    draw_flag = 1;
    PC += 2;
}

/*
 * Skips the next instruction if the key stored in VX is pressed.
 */
static inline void OP_EX9E(void) {
    if ((keys & (REG[GET_0x0100(opcode)])) == (REG[GET_0x0100(opcode)])) {
        PC += 2;
    }
    PC += 2;
}

/*
 * Skips the next instruction if the key stores in VX isn't pressed.
 */
static inline void OP_EXA1(void) {
    if ((keys & (REG[GET_0x0100(opcode)])) != (REG[GET_0x0100(opcode)])) {
        PC += 2;
    }
    PC += 2;
}

/*
 * Sets VX to the value of the delay timer.
 */
static inline void OP_FX07(void) {
    REG[GET_0x0100(opcode)] = delay_timer;
    PC += 2;
}

/*
 * A key press is awaited then stored in VX (blocking)
 */
static inline void OP_FX0A(void) {
    unsigned char key_pressed = 0;
    unsigned char i;
    for (i = 0; i < 16; i++) {
        if ((keys & (1 << i)) == (1 << i)) {
            REG[GET_0x0100(opcode)] = (1 << i);
            key_pressed = 1;
        }
    }

    if (!key_pressed) {
        return;
    }
    PC += 2;
}

/*
 * Sets the delay timer to VX.
 */
static inline void OP_FX15(void) {
    delay_timer = REG[GET_0x0100(opcode)];
    PC += 2;
}

/*
 * Sets the sound timer to VX.
 */
static inline void OP_FX18(void) {
    sound_timer = REG[GET_0x0100(opcode)];
    PC += 2;
}

/*
 * Adds VX to I. VF is not affected.
 */
static inline void OP_FX1E(void) {
    I += REG[GET_0x0100(opcode)];
    PC += 2;
}

/*
 * Sets I to the location of the sprite for the character in VX.
 */
static inline void OP_FX29(void) {
    I = REG[GET_0x0100(opcode)] * CHAR_SIZE;
    PC += 2;
}

/*
 * Stores the BCD representation of VX, with the MSD at I, middle digit at I+1, and LSD at I+2.
 */
static inline void OP_FX33(void) {
    unsigned char x = GET_0x0100(opcode);
    memory[I] = REG[x] / 100;
    memory[I + 1] = (REG[x] / 10) % 10;
    memory[I + 2] = REG[x] % 10;
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
    // Fetch 2-byte instruction
    opcode = (memory[PC] << 8) | memory[PC + 1];

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

static void DelayTimerSetup(void) {
    /* Configure Timer B1 */
    // CCR0
    TB1CCR0 = 33333;  // Up mode reset value
    // TB1 Control Register
    TB1CTL |= TBSSEL__ACLK |  // Source TB1 with ACLK
                MC_1 |          // Up mode (counts to TAxCCR0)
                TBCLR |         // Clear the counter register TBR
                TBIE;           // Enable interrupts for TB1 (overflow)
}

static void CopyFontset(void) {
    unsigned char i;
    for (i = 0; i < 80; i++) {
        memory[i] = chip8_fontset[i];
    }
}

void Chip8Main(void) {

    // debug led
    P3DIR |= BIT4;
    P3OUT &= ~BIT4;

    DisplaySetup();
    KeypadSetup();
    // DelayTimerSetup();
    CopyFontset();
    RequestGame();
    ClearScreen();
    while (!done_loading_game) {
        // Press A button to start with currently loaded game
        KeypadPoll();
        if ((keys & (1 << 0xA)) == (1 << 0xA)) {
            break;
        }
    }
    P3OUT |= BIT4;
    CopyGame();

    // Emulation loop
    while(1) {
        ExecuteInstruction();

        KeypadPoll();

        if (draw_flag) {
            draw_flag = 0;
            UpdateScreenWithGfx();
        }
    }
}

// 60Hz Delay timer ISR
#pragma vector = TIMER1_B1_VECTOR
__interrupt void Timer_B1(void) {
    static unsigned char i = 0;
    switch(__even_in_range(TB1IV,14))
     {
       case  0: break;                          // No interrupt
       case  2: break;                          // CCR1
       case  4: break;                          // CCR2
       case  6: break;                          // reserved
       case  8: break;                          // reserved
       case 10: break;                          // reserved
       case 12: break;                          // reserved
       case 14: // overflow
           i++;
           if (i > 9) {
               i = 0;
               if (delay_timer > 0) {
                   delay_timer--;
               }
           }
                break;
       default: break;
     }
}

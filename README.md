# MECH423 Final Project: CHIP-8 on MSP430

The CHIP-8 emulator is implemented on the MSP430FR5739 microcontroller. Game
files are optionally loaded over UART through a WinForm interface. User input
is read through a 4x4 matrix keypad and a VMA437 OLED display is used to
display graphics using SPI.

## Images
todo

## Pinout

P1.0 -> Keypad R0

P1.1 -> Keypad R1

P1.2 -> Keypad R2

P1.2 -> Keypad R3

P3.0 -> Keypad C0

P3.1 -> Keypad C1

P3.2 -> Keypad C2

P3.3 -> Keypad C3


P2.0 -> UART

P2.1 -> UART


P2.2 -> OLED CLK

P1.6 -> OLED MOSI

P4.0 -> OLED DC

P1.5 -> OLED CS

P2.5 -> OLED RES


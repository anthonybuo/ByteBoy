#ifndef _UART_H_
#define _UART_H_

static volatile unsigned int state = 0;
static volatile unsigned int game_file_size = 0;

#define SIZE_GAME_FILE1 1024

void RequestGame(void);

__interrupt void UART_RX_INTERRUPT(void);

#endif

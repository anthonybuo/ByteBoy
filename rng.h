#ifndef _RNG_H_
#define _RNG_H_

extern volatile unsigned char rng;

void RNGSetup(void);

unsigned char GenerateRandomByte(void);

#endif

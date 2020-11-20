#ifndef _DISPLAY_H_
#define _DISPLAY_H_

extern unsigned char gfx[2048];
extern unsigned char gfx2[256];

int DisplayDrawPixel(unsigned char x, unsigned char y);

void ClearScreen(void);

void FillScreen(void);

void DisplaySetup(void);

void UpdateScreenWithGfx(void);

#endif  // _DISPLAY_H_


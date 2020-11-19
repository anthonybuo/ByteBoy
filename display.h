#ifndef _DISPLAY_H_
#define _DISPLAY_H_

extern unsigned char gfx[2048];

int DisplayDrawPixel(unsigned char x, unsigned char y);

void ClearScreen(void);

void FillScreen(void);

void DisplaySetup(void);

void UpdateScreenWithGfx(void);

#endif  // _DISPLAY_H_


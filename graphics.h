#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>

void Graphics_Init(uint64_t fb_base, uint32_t width, uint32_t height,
                   uint32_t ppsl);
void Graphics_PutChar(int x, int y, char c, uint32_t color);
void Graphics_Print(int x, int y, const char *str, uint32_t color);
void Graphics_PrintHex(int x, int y, uint64_t value, uint32_t color);
void Graphics_Clear(uint32_t color);

#endif

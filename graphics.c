#include "graphics.h"
#include "font.h"

static uint64_t frame_buffer;
static uint32_t screen_width;
static uint32_t screen_height;
static uint32_t pixels_per_scanline;

void Graphics_Init(uint64_t fb_base, uint32_t width, uint32_t height,
                   uint32_t ppsl) {
  frame_buffer = fb_base;
  screen_width = width;
  screen_height = height;
  pixels_per_scanline = ppsl;
}

void Graphics_PutChar(int x, int y, char c, uint32_t color) {
  uint32_t *fb = (uint32_t *)frame_buffer;
  const uint8_t *bitmap = font8x16[(uint8_t)c];

  for (int dy = 0; dy < 16; dy++) {
    for (int dx = 0; dx < 8; dx++) {
      if (bitmap[dy] & (0x80 >> dx)) {
        fb[(y + dy) * pixels_per_scanline + (x + dx)] = color;
      }
    }
  }
}

void Graphics_Print(int x, int y, const char *str, uint32_t color) {
  int cur_x = x;
  while (*str) {
    Graphics_PutChar(cur_x, y, *str, color);
    cur_x += 8;
    str++;
  }
}

void Graphics_PrintHex(int x, int y, uint64_t value, uint32_t color) {
  char hex_str[19];
  hex_str[0] = '0';
  hex_str[1] = 'x';
  for (int i = 0; i < 16; i++) {
    uint8_t nibble = (value >> (60 - i * 4)) & 0xF;
    hex_str[i + 2] = (nibble < 10) ? (nibble + '0') : (nibble - 10 + 'A');
  }
  hex_str[18] = '\0';
  Graphics_Print(x, y, hex_str, color);
}

void Graphics_Clear(uint32_t color) {
  uint32_t *fb = (uint32_t *)frame_buffer;
  for (uint32_t y = 0; y < screen_height; y++) {
    for (uint32_t x = 0; x < screen_width; x++) {
      fb[y * pixels_per_scanline + x] = color;
    }
  }
}

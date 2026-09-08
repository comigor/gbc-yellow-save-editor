#ifndef GRAPHICS_H
#define GRAPHICS_H
#include <gb/gb.h>
#ifdef YELLOW_GRAPHICS
void graphics_init(void) NONBANKED;
void graphics_front(uint8_t dex) NONBANKED;
void graphics_icon(uint8_t dex, uint8_t row) NONBANKED;
#endif
#endif

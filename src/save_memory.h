#ifndef SAVE_MEMORY_H
#define SAVE_MEMORY_H
#include <stdint.h>
#ifdef __SDCC
#include <gb/gb.h>
#define SAVE_CALL OLDCALL
#else
#define SAVE_CALL
#endif
uint8_t save_get(uint16_t offset) SAVE_CALL;
void save_set(uint16_t offset, uint8_t value) SAVE_CALL;
#endif

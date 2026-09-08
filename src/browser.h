#ifndef BROWSER_H
#define BROWSER_H
#include "storage.h"
uint8_t browser_choose(char *path) BANKED;
void show_storage_error(uint8_t result) BANKED;
void storage_progress(uint8_t stage, uint16_t completed,
                      uint16_t total) NONBANKED;
#endif

#ifndef BROWSER_H
#define BROWSER_H
#include "storage.h"
uint8_t browser_choose(char *path) BANKED;
void show_storage_error(uint8_t result);
void save_progress(uint8_t stage);
#endif

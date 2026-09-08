#ifndef UI_H
#define UI_H
#include <stdint.h>
uint8_t ui_key(void);
void ui_page(const char *title);
void ui_line(uint8_t row, const char *text);
void ui_number(uint32_t value);
uint8_t ui_confirm(const char *title, const char *message);
uint8_t ui_edit_number(const char *title, uint32_t *value, uint32_t minimum,
                       uint32_t maximum, void (*label)(uint16_t, char *));
uint8_t ui_edit_name(char *name);
void ui_notice(const char *title, const char *message);
#endif

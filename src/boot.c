#include "graphics.h"
#include <gb/cgb.h>
#include <gb/gb.h>
#include <stdio.h>
void editor_run(void) BANKED;

void main(void) {
  if (_cpu != CGB_TYPE) {
    printf("YELLOW SAVE EDITOR\n\nGAME BOY COLOR\nMODE REQUIRED");
    for (;;)
      vsync();
  }
  SVBK_REG = 1;
  set_default_palette();
#ifdef YELLOW_GRAPHICS
  graphics_init();
#endif
  editor_run();
  for (;;)
    vsync();
}

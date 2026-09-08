#include "save_memory.h"

#ifdef __SDCC
/* No stack or interrupt may touch D000-DFFF while its WRAM bank is switched. */
uint8_t save_get(uint16_t offset) OLDCALL __naked {
    offset;
    __asm
        push bc
        ldhl sp, #4
        ld a, (hl+)
        ld h, (hl)
        ld l, a
        ld a, h
        swap a
        and #0x0f
        ld b, a
        ld a, h
        and #0x0f
        or #0xd0
        ld h, a
        di
        ld a, b
        ldh (#0x70), a
        ld e, (hl)
        ld a, #1
        ldh (#0x70), a
        ei
        pop bc
        ret
    __endasm;
}

void save_set(uint16_t offset, uint8_t value) OLDCALL __naked {
    offset; value;
    __asm
        push bc
        ldhl sp, #4
        ld e, (hl)
        inc hl
        ld d, (hl)
        inc hl
        ld c, (hl)
        ld a, d
        swap a
        and #0x0f
        ld b, a
        ld a, d
        and #0x0f
        or #0xd0
        ld h, a
        ld l, e
        di
        ld a, b
        ldh (#0x70), a
        ld (hl), c
        ld a, #1
        ldh (#0x70), a
        ei
        pop bc
        ret
    __endasm;
}
#else
uint8_t save_bytes[0x8000];
uint8_t save_get(uint16_t offset) { return save_bytes[offset]; }
void save_set(uint16_t offset, uint8_t value) { save_bytes[offset] = value; }
#endif

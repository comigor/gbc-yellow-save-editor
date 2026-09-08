import argparse
import json
import re
from pathlib import Path
from pyboy import PyBoy
from fixtures import yellow_save, disk_image, Volume
from x7_model import X7Model

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/rom-smoke'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom', type=Path, default=ROOT/'build/yellow-editor.gbc')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    rom = args.rom.read_bytes()
    symbols = {}
    for line in args.rom.with_suffix('.noi').read_text().splitlines():
        match = re.match(r'DEF (\S+) (0x[0-9A-Fa-f]+)', line)
        if match: symbols[match[1]] = int(match[2], 16)
    save = yellow_save()
    image = disk_image(save, fragmented=True, collision=True)
    model = X7Model(image)
    gb = PyBoy(str(args.rom), window='null', sound_emulated=False, cgb=True)
    gb.set_emulation_speed(0)
    model.install(gb, rom)
    log = []
    gb.hook_register(0, symbols['.put_char'], lambda _: log.append(chr(gb.register_file.A)), None)

    def wait(text, frames=20000):
        for _ in range(frames//10):
            gb.tick(10, True)
            if text in ''.join(log): return
        gb.screen.image.save(OUT/'failure.png')
        raise AssertionError((text, ''.join(log)[-400:], gb.register_file.PC))

    def key(button):
        log.clear()
        gb.button_press(button); gb.tick(10, True); gb.button_release(button); gb.tick(20, True);

    def snap(name):
        gb.screen.image.save(OUT/f'{name}.png')

    try:
        wait('START: Browse SD'); snap('welcome')
        key('start'); wait('Choose Yellow save'); snap('browser')
        key('a'); wait('Yellow editor'); snap('menu')
        key('a'); wait('Pokemon storage')
        key('a'); wait('Left/Right: Tabs'); snap('pokemon')
        key('right'); wait('Moves / PP')
        key('right'); wait('HP DV')
        key('right'); wait('Stat experience')
        key('right'); wait('Summary')
        key('left'); wait('Stat experience')
        key('right'); wait('Summary')
        key('down'); key('down'); key('a'); wait('Level (updates EXP)')
        key('up'); key('a'); wait('Left/Right: Tabs')
        key('b'); wait('Pokemon storage')
        key('right'); wait('Box 1')
        key('right'); wait('Box 2')
        key('a'); wait('Left/Right: Tabs')
        key('down'); key('down'); key('a'); wait('Level (updates EXP)')
        key('right'); key('up'); key('a'); wait('Left/Right: Tabs')
        key('b'); wait('Pokemon storage')
        key('b'); wait('UNSAVED CHANGES'); snap('dirty')
        key('down'); key('a'); wait('Pikachu friendship')
        key('a'); wait('Step')
        key('up'); key('a'); wait('Pikachu friendship')
        key('b'); wait('Yellow editor')
        key('down'); key('a'); wait('Add item'); snap('bag')
        key('a'); wait('Quantity')
        key('down'); key('a'); wait('Step')
        key('up'); key('a'); wait('Quantity')
        key('b'); wait('Add item')
        key('b'); wait('Yellow editor')
        key('start'); wait('Write edited save?'); snap('confirm')
        key('a'); wait('SAVE VERIFIED', frames=120000)
        gb.tick(30, True); snap('verified')
        volume = Volume(image)
        assert volume.file(b'PK000001BAK') == b'older backup'
        assert volume.file(b'PK000002BAK') == save
        edited = volume.file(b'YELLOW  SRM')
        assert edited[:0x2000] == save[:0x2000]
        assert edited[0x2f34+33] == 6
        assert int.from_bytes(edited[0x2f34+14:0x2f34+17], 'big') == 216
        box2 = 0x4000+0x462+22
        assert int.from_bytes(edited[box2+14:box2+17], 'big') == 3375
        assert edited[0x25f3:0x25f6] == bytes([0,0x30,1])
        assert edited[0x25cb] == 11
        assert edited[0x3523] == (~sum(edited[0x2598:0x3523])) & 255
        assert volume.file(b'KEEP    BIN') == b'unchanged sentinel'
        assert not model.unlocked and model.control & 1
        assert gb.memory[0xff70] & 7 == 1
        (OUT/'edited.srm').write_bytes(edited)
        (OUT/'evidence.json').write_text(json.dumps({'commands':dict(model.commands), 'party_level':6,'box2_level':15,'money':3001,'potion_quantity':11,'backup_byte_identical':True},indent=2)+'\n')
        print('PASS compiled ROM: browse, party/box edits, trainer, bag, backup, save readback; SPI commands', model.commands)
    finally:
        gb.stop(save=False)


if __name__ == '__main__':
    main()

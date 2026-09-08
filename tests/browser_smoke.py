import re
import struct
from pathlib import Path
from pyboy import PyBoy
from fixtures import yellow_save, disk_image, Volume, lfn_entries
from x7_model import X7Model

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/browser-smoke'




def browser_image():
    image = disk_image(yellow_save())
    volume = Volume(image)
    original = volume.entries()[b'YELLOW  SRM']
    entries = []
    for index in range(35):
        alias = f'OTHER{index:03}TXT'.encode()
        entry = bytearray(32)
        entry[:11] = alias
        entry[11] = 32
        entries.append(entry)
    for index in range(10):
        alias = f'POKE~{index:03}SRM'.encode()
        entries.extend(lfn_entries(f'Pokemon Yellow journey {index:03}.srm', alias))
        entry = bytearray(original)
        entry[:11] = alias
        entries.append(entry)
    alias = b'SAVES~01   '
    entries.extend(lfn_entries('Pokemon save folder', alias))
    entry = bytearray(32)
    entry[:11] = alias
    entry[11] = 16
    struct.pack_into('<H', entry, 26, 200)
    entries.append(entry)
    nested = b''.join(lfn_entries('Yellow inside folder.srm', b'YELLOW  SRM')) + original + bytes(32)
    offset = volume.start + (200 - 2) * 512
    image[offset:offset+512] = nested.ljust(512, b'\0')
    root = b''.join(entries) + bytes(32)
    chain = [2] + list(range(100, 100 + (len(root)-1)//512))
    for fat in (volume.fat, volume.fat + volume.fatsz):
        struct.pack_into('<I', image, fat + 200 * 4, 0xfffffff)
        for index, cluster in enumerate(chain):
            struct.pack_into('<I', image, fat + cluster*4, chain[index+1] if index+1<len(chain) else 0xfffffff)
    for index, cluster in enumerate(chain):
        offset = volume.start + (cluster-2)*512
        image[offset:offset+512] = root[index*512:(index+1)*512].ljust(512, b'\0')
    return image


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rom_path = ROOT/'build/yellow-editor.gbc'
    rom = rom_path.read_bytes()
    symbols = dict((name, int(address,16)) for name,address in re.findall(r'^DEF (\S+) (0x[0-9A-Fa-f]+)', rom_path.with_suffix('.noi').read_text(), re.M))
    image = browser_image()
    model = X7Model(image)
    gb = PyBoy(str(rom_path), window='null', sound_emulated=False, cgb=True)
    gb.set_emulation_speed(0)
    model.install(gb, rom)
    log = []
    gb.hook_register(0, symbols['.put_char'], lambda _: log.append(chr(gb.register_file.A)), None)
    def tap(key):
        log.clear()
        gb.button_press(key); gb.tick(10, True); gb.button_release(key); gb.tick(30, True)
    def wait(text):
        for _ in range(3000):
            if text in ''.join(log): return
            gb.tick(10, True)
        raise AssertionError((text, ''.join(log)[-300:]))
    try:
        wait('START: Browse SD'); tap('start'); wait('START: Exit browser')
        reads = model.commands[17]
        tap('down'); wait('START: Exit browser')
        delta = model.commands[17] - reads
        print('Cursor movement SD reads:', delta)
        assert delta == 0, f'Cursor movement reread {delta} SD sectors'
        assert 'Pokemon Yellow' in ''.join(log), 'Browser displays only short aliases'
        tap('select'); wait('A / B: Return')
        gb.tick(30, True)
        gb.screen.image.save(OUT/'full-name.png')
        assert '001.srm' in ''.join(log)
        assert model.commands[17] == reads
        tap('b'); wait('START: Exit browser')
        tap('right'); wait('START: Exit browser')
        assert model.commands[17] > reads
        page_reads = model.commands[17]
        tap('down'); wait('START: Exit browser')
        assert model.commands[17] == page_reads
        tap('down'); wait('START: Exit browser')
        tap('a'); wait('START: Exit browser')
        assert 'Yellow inside' in ''.join(log)
        tap('b'); wait('START: Exit browser')
        assert 'Pokemon Yellow' in ''.join(log)
        tap('right'); wait('START: Exit browser')
        tap('down'); wait('START: Exit browser')
        gb.screen.image.save(OUT/'browser.png')
        tap('a'); wait('Yellow editor')
        assert model.commands[24] == 0
        print('PASS browser: no cursor I/O, long-name display, paging, alias-based save open')
    finally:
        gb.stop(save=False)


if __name__ == '__main__':
    main()

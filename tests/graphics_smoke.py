import argparse
import re
from pathlib import Path

from fixtures import disk_image, yellow_save
from pyboy import PyBoy
from x7_model import X7Model

ROOT = Path(__file__).resolve().parents[1]


def tile_pixels(data, width):
    pixels = [[0] * (width * 8) for _ in range(len(data) // (width * 16) * 8)]
    for tile in range(len(data) // 16):
        for y in range(8):
            low, high = data[tile * 16 + y * 2 : tile * 16 + y * 2 + 2]
            for x in range(8):
                bit = 7 - x
                pixels[(tile // width) * 8 + y][(tile % width) * 8 + x] = (
                    (low >> bit) & 1
                ) | (((high >> bit) & 1) << 1)
    return pixels


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--rom", type=Path, default=ROOT / "build/private/yellow-editor.gbc"
    )
    parser.add_argument("--assets", type=Path, default=ROOT / "build/private/graphics")
    args = parser.parse_args()
    output = args.rom.parent / "graphics-smoke"
    output.mkdir(parents=True, exist_ok=True)
    symbols = {
        name: int(address, 16)
        for name, address in re.findall(
            r"^DEF (\S+) (0x[0-9A-Fa-f]+)",
            args.rom.with_suffix(".noi").read_text(),
            re.MULTILINE,
        )
    }
    generated = []
    for group in range(8):
        source = (args.assets / f"front_{group}.c").read_text()
        data = bytes(
            int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})\b", source)
        )
        generated.extend(data[i : i + 784] for i in range(0, len(data), 784))
    assert len(generated) == 151 and all(len(data) == 784 for data in generated)
    icons_source = (args.assets / "icons.c").read_text()
    ids_section = (
        icons_source.split("yellow_menu_icon_ids", 1)[1]
        .split("{", 1)[1]
        .split("}", 1)[0]
    )
    icon_ids = [
        int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})\b", ids_section)
    ]
    tiles_section = icons_source.split("yellow_menu_icons", 1)[1].split("=", 1)[1]
    icon_tiles = bytes(
        int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})\b", tiles_section)
    )
    image = disk_image(yellow_save())
    model = X7Model(image)
    gb = PyBoy(str(args.rom), window="null", sound_emulated=False, cgb=True)
    gb.set_emulation_speed(0)
    model.install(gb, args.rom.read_bytes())
    log = []
    gb.hook_register(
        0, symbols[".put_char"], lambda _: log.append(chr(gb.register_file.A)), None
    )

    def tap(key):
        log.clear()
        gb.button_press(key)
        gb.tick(10, True)
        gb.button_release(key)
        gb.tick(20, True)

    def wait(text):
        for _ in range(12000):
            if text in "".join(log):
                return
            gb.tick(10, True)
        raise AssertionError((text, "".join(log)[-200:]))

    def verify_pixels(expected, width, left, top):
        screen = gb.screen.image.convert("RGB")
        colors = {}
        for y, row in enumerate(tile_pixels(expected, width)):
            for x, pixel in enumerate(row):
                color = screen.getpixel((left + x, top + y))
                if pixel in colors:
                    assert colors[pixel] == color, ("screen differs", x, y)
                else:
                    colors[pixel] = color
        assert len(set(colors.values())) == len(colors), "Palette lost colors"

    def verify(dex):
        verify_pixels(generated[dex - 1], 7, 104, 40)
        gb.screen.image.save(output / f"dex-{dex:03}.png")

    try:
        wait("START: Browse SD")
        tap("start")
        wait("START: Exit browser")
        tap("a")
        wait("Yellow editor")
        tap("a")
        wait("Pokemon storage")
        gb.tick(30, True)
        icon = icon_ids[25]
        verify_pixels(icon_tiles[icon * 64 : (icon + 1) * 64], 2, 8, 24)
        gb.screen.image.save(output / "menu-icons.png")
        tap("a")
        wait("Left/Right: Tabs")
        gb.tick(20, True)
        verify(25)
        reads = model.commands[17]
        for dex in [1, 20, 39, 58, 77, 96, 115, 134, 151]:
            tap("a")
            wait("Species (Dex number)")
            tap("right")
            tap("right")
            tap("down")
            tap("down")
            tap("left")
            tap("left")
            for _ in range(dex - 1):
                tap("up")
            tap("a")
            wait("Left/Right: Tabs")
            gb.tick(20, True)
            verify(dex)
        tap("right")
        wait("Moves / PP")
        tap("left")
        wait("Summary")
        gb.tick(20, True)
        verify(151)
        assert model.commands[17] == reads, "Selecting Pokemon graphics read SD"
        assert model.commands[24] == 0, "Viewing sprites wrote SD"
        print(
            "PASS private graphics: sprite ROM banks, rendered front/icon pixels, no SD I/O"
        )
    finally:
        gb.stop(save=False)


if __name__ == "__main__":
    main()

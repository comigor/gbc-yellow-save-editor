import argparse
import hashlib
import re
from pathlib import Path

from fixtures import checksum, disk_image, yellow_save
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
        "--rom", type=Path, default=ROOT / "build/private/yellow-editor-sprites.gbc"
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
    # Hashes from pokeyellow's decoder at verified bank:address pairs, not this manifest.
    known_fronts = {
        1: "9171e685081a5e75b170aa0797a649d2ff440ceef1a387b54989a893ea0d0d6f",
        25: "8d07df66160a1590be7a5d08492c278f355c888fdb65f1f978987dc48d1f2f13",
        151: "093b9f1530f2ae153855048b775902570b02d83a033bb4e1ec4c47e9716087c3",
    }
    for dex, digest in known_fronts.items():
        assert hashlib.sha256(generated[dex - 1]).hexdigest() == digest, (
            "Wrong front",
            dex,
        )
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
    for dex, category in {
        1: 7,
        3: 7,
        4: 0,
        24: 8,
        25: 10,
        89: 0,
        90: 2,
        151: 0,
    }.items():
        assert icon_ids[dex] == category, ("Wrong menu category", dex)
    save = yellow_save()
    save[0x2F35:0x2F37] = (65535).to_bytes(2, "big")
    save[0x2F56:0x2F58] = (65535).to_bytes(2, "big")
    save[0x3523] = checksum(save, 0x2598, 0x3523)
    image = disk_image(save)
    model = X7Model(image)
    gb = PyBoy(str(args.rom), window="null", sound_emulated=False, cgb=True)
    gb.set_emulation_speed(0)
    model.install(gb, args.rom.read_bytes())
    log = []
    input_ready = False

    def ready(_):
        nonlocal input_ready
        input_ready = True

    gb.hook_register(0, symbols["_ui_key"], ready, None)
    gb.hook_register(
        0, symbols[".put_char"], lambda _: log.append(chr(gb.register_file.A)), None
    )

    def tap(key):
        nonlocal input_ready
        input_ready = False
        log.clear()
        gb.button_press(key)
        gb.tick(10, True)
        gb.button_release(key)
        gb.tick(20, True)

    def wait(text):
        for _ in range(12000):
            if text in "".join(log) and input_ready:
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
        gb.screen.image.save(output / "graphics-enabled.png")
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
        screen = gb.screen.image.convert("RGB")
        current_hp = screen.crop((24, 48, 64, 56))
        maximum_hp = screen.crop((32, 56, 72, 64))
        assert len(current_hp.getcolors()) > 1, "Current HP is missing"
        assert current_hp.tobytes() == maximum_hp.tobytes(), "HP digits clipped"
        tab_crops = {}
        for name in ("Moves / PP", "HP DV", "Stat experience"):
            tap("right")
            wait(name)
            gb.tick(20, True)
            tab_crops[name] = gb.screen.image.crop((104, 40, 160, 96)).tobytes()
            gb.screen.image.save(
                output / (name.replace(" / ", "-").replace(" ", "-") + ".png")
            )
        tap("right")
        wait("Summary")
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
        for name, expected in tab_crops.items():
            tap("right")
            wait(name)
            gb.tick(20, True)
            actual = gb.screen.image.crop((104, 40, 160, 96)).tobytes()
            assert actual == expected, ("Stale species artwork on tab", name)
        tap("right")
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

import argparse
import gzip
import hashlib
import json
from pathlib import Path

MANIFEST = Path(__file__).with_name("yellow_graphics.json")


class Bits:
    def __init__(self, data, offset):
        self.data = data
        self.position = offset * 8

    def take(self, count):
        value = 0
        for _ in range(count):
            byte, bit = divmod(self.position, 8)
            value = (value << 1) | ((self.data[byte] >> (7 - bit)) & 1)
            self.position += 1
        return value


def unpack_plane(bits, width, height):
    count = width * height * 32
    groups = []
    literal = bits.take(1)
    while len(groups) < count:
        if literal:
            while len(groups) < count:
                pair = bits.take(2)
                if pair == 0:
                    break
                groups.append(pair)
        else:
            length_bits = 1
            while bits.take(1):
                length_bits += 1
            run = (1 << length_bits) - 1 + bits.take(length_bits)
            if len(groups) + run > count:
                raise ValueError("Sprite zero run exceeds plane")
            groups.extend([0] * run)
        literal ^= 1
    plane = bytearray(width * height * 8)
    rows = height * 8
    for column in range(width):
        for y in range(rows):
            plane[column * rows + y] = sum(
                groups[(column * 4 + pair) * rows + y] << (6 - pair * 2)
                for pair in range(4)
            )
    return plane


def undifference(plane, width, height):
    rows = height * 8
    for y in range(rows):
        previous = 0
        for column in range(width):
            position = column * rows + y
            value = 0
            for bit in range(7, -1, -1):
                previous ^= (plane[position] >> bit) & 1
                value |= previous << bit
            plane[position] = value


def decode_sprite(rom, offset):
    bits = Bits(rom, offset)
    width, height = bits.take(4), bits.take(4)
    if not (1 <= width <= 7 and 1 <= height <= 7):
        raise ValueError("Unsupported sprite dimensions")
    order = bits.take(1)
    planes = [None, None]
    planes[order] = unpack_plane(bits, width, height)
    mode = bits.take(1)
    if mode:
        mode += bits.take(1)
    planes[order ^ 1] = unpack_plane(bits, width, height)
    undifference(planes[order], width, height)
    if mode != 1:
        undifference(planes[order ^ 1], width, height)
    if mode:
        for i in range(len(planes[0])):
            planes[order ^ 1][i] ^= planes[order][i]
    tiles = bytearray()
    for tile_y in range(height):
        for tile_x in range(width):
            for y in range(8):
                position = tile_x * height * 8 + tile_y * 8 + y
                tiles.extend((planes[0][position], planes[1][position]))
    return bytes(tiles), width, height


def pad_front(tiles, width, height):
    padded = bytearray(784)
    left, top = (8 - width) // 2, 7 - height
    for y in range(height):
        start = ((top + y) * 7 + left) * 16
        padded[start : start + width * 16] = tiles[
            y * width * 16 : (y + 1) * width * 16
        ]
    return bytes(padded)


def extract_icons(rom, manifest):
    vram = bytearray(1024)
    for i in range(14):
        start = manifest["menu_copy_descriptors_offset"] + i * 6
        entry = rom[start : start + 6]
        source = entry[3] * 16384 + int.from_bytes(entry[:2], "little") - 0x4000
        size = entry[2] * 16
        dest = int.from_bytes(entry[4:6], "little") - 0x8000
        vram[dest : dest + size] = rom[source : source + size]
    icons = []
    for category in range(11):
        source = vram[category * 64 : (category + 1) * 64]
        if category == 2:
            icons.append(bytes(source))
        else:
            top, bottom = source[:16], source[32:48]
            mirror_top = bytes(int(f"{value:08b}"[::-1], 2) for value in top)
            mirror_bottom = bytes(int(f"{value:08b}"[::-1], 2) for value in bottom)
            icons.append(bytes(top) + mirror_top + bytes(bottom) + mirror_bottom)
    start = manifest["menu_categories_offset"]
    packed = rom[start : start + 76]
    ids = bytes(
        [0]
        + [
            (packed[(dex - 1) // 2] >> (4 if dex & 1 else 0)) & 15
            for dex in range(1, 152)
        ]
    )
    return icons, ids


def c_bytes(data):
    return "\n".join(
        "  " + ", ".join(f"0x{value:02x}" for value in data[i : i + 16]) + ","
        for i in range(0, len(data), 16)
    )


def array(name, rows):
    return (
        f"const unsigned char {name}[{len(rows)}][{len(rows[0])}] = {{\n"
        + ",\n".join("{\n" + c_bytes(row) + "\n}" for row in rows)
        + "\n};\n"
    )


def extract(rom_path, output):
    manifest = json.loads(MANIFEST.read_text())
    rom = rom_path.read_bytes()
    if rom[:2] == b"\x1f\x8b":
        rom = gzip.decompress(rom)
    if (
        len(rom) != manifest["rom_size"]
        or hashlib.sha256(rom).hexdigest() != manifest["rom_sha256"]
    ):
        raise ValueError(
            "Expected the verified English Yellow ROM (SHA-256 "
            + manifest["rom_sha256"]
            + ")"
        )
    fronts = [
        pad_front(*decode_sprite(rom, offset)) for offset in manifest["front_offsets"]
    ]
    icons, ids = extract_icons(rom, manifest)
    output.mkdir(parents=True, exist_ok=True)
    header = ["#ifndef GRAPHICS_ASSETS_H", "#define GRAPHICS_ASSETS_H"]
    for group in range(8):
        name = f"yellow_front_{group}"
        rows = fronts[group * 19 : (group + 1) * 19]
        (output / f"front_{group}.c").write_text(
            f"#pragma bank {8 + group}\n" + array(name, rows)
        )
        header.append(f"extern const unsigned char {name}[{len(rows)}][784];")
    (output / "icons.c").write_text(
        "#pragma bank 7\n"
        + f"const unsigned char yellow_menu_icon_ids[152] = {{\n{c_bytes(ids)}\n}};\n"
        + array("yellow_menu_icons", icons)
    )
    header.extend(
        [
            "extern const unsigned char yellow_menu_icon_ids[152];",
            "extern const unsigned char yellow_menu_icons[11][64];",
            "#endif",
        ]
    )
    (output / "graphics_assets.h").write_text("\n".join(header) + "\n")
    print("Extracted 151 front sprites and 11 menu icons into private build output")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        extract(args.rom, args.output)
    except (OSError, ValueError, EOFError) as error:
        parser.exit(1, f"Graphics extraction failed: {error}\n")


if __name__ == "__main__":
    main()

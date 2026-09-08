#!/usr/bin/env python3
"""Regenerate src/yellow_data.c from pinned reference repositories.

Sources (see NOTICE for licensing and pinned commits):
  - kasbuunk/Pokemon @ ac72f04af1026b4023015c239b34730b8cccb15e
    (crates/pksave/src/gen1/data/generated/{species,moves,items,types}.rs)
  - PKHeX @ e0e63bc87837ad2d9c8f8fda4efdbf5f2933db08
    (PKHeX.Core/Resources/byte/personal/personal_y) for the four
    Yellow-specific catch rates.

Usage:
    python3 tools/generate_yellow_data.py <kasbuunk-checkout> <pkhex-checkout>

Writes src/yellow_data.c (tables section only; the accessor functions
below the tables are hand-written in this file and appended verbatim).
"""

import re
import sys
import os

KASBUUNK_COMMIT = "ac72f04af1026b4023015c239b34730b8cccb15e"
PKHEX_COMMIT = "e0e63bc87837ad2d9c8f8fda4efdbf5f2933db08"
PKHEX_RECORD_SIZE = 0x1C
PERSONAL_Y_CATCH_RATE_OFF = 0x08

# Reference names use a few glyphs the ASCII UI cannot render.
ASCII_MAP = {
    "\u2642": "(M)",  # male sign
    "\u2640": "(F)",  # female sign
    "\u00e9": "e",    # e-acute (POKe)
    "\u00c9": "e",
    "\u2019": "'",
}


def to_ascii(name):
    out = []
    for ch in name:
        if ch in ASCII_MAP:
            out.append(ASCII_MAP[ch])
        else:
            code = ord(ch)
            if not 32 <= code < 127:
                raise SystemExit(f"unmapped character {ch!r} in {name!r}")
            out.append(ch)
    return "".join(out)


def parse_rust_u8_array(text, name, expected_len):
    m = re.search(re.escape(name) + r".{0,40}?\[(.*?)\];", text, re.S)
    if m is None:
        raise SystemExit(f"table {name} not found")
    vals = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{1,2})", m.group(1))]
    if len(vals) != expected_len:
        raise SystemExit(f"table {name}: expected {expected_len}, got {len(vals)}")
    return vals


def parse_rust_str_array(text, name, expected_len):
    m = re.search(re.escape(name) + r".{0,40}?\[(.*?)\n\];", text, re.S)
    if m is None:
        raise SystemExit(f"table {name} not found")
    strs = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(1))
    if len(strs) != expected_len:
        raise SystemExit(f"table {name}: expected {expected_len}, got {len(strs)}")
    return strs


def parse_base_stats(text):
    m = re.search(r"BASE_STATS: \[BaseStats; 152\] = \[(.*?)\n\];", text, re.S)
    blocks = re.findall(r"BaseStats \{(.*?)\}", m.group(1), re.S)
    if len(blocks) != 152:
        raise SystemExit(f"BASE_STATS: expected 152, got {len(blocks)}")
    out = []
    for block in blocks:
        vals = dict(re.findall(r"(\w+): (\d+)", block))
        out.append((int(vals["hp"]), int(vals["attack"]), int(vals["defense"]),
                    int(vals["speed"]), int(vals["special"]), int(vals["type1"]),
                    int(vals["type2"]), int(vals["catch_rate"]),
                    int(vals["exp_yield"]), int(vals["growth_rate"])))
    return out


def parse_moves(text):
    m = re.search(r"MOVES: \[MoveInfo; 166\] = \[(.*?)\n\];", text, re.S)
    blocks = re.findall(r"MoveInfo \{(.*?)\}", m.group(1), re.S)
    if len(blocks) != 166:
        raise SystemExit(f"MOVES: expected 166, got {len(blocks)}")
    out = []
    for block in blocks:
        name = re.search(r'name: "(.*)"', block).group(1)
        vals = dict(re.findall(r"(\w+): (\d+)", block))
        out.append((name, int(vals["power"]), int(vals["type_"]),
                    int(vals["accuracy"]), int(vals["pp"])))
    return out


def yellow_catch_rates(personal_y_path):
    """Dex-number-indexed catch rates from the PKHeX Yellow table."""
    data = open(personal_y_path, "rb").read()
    if len(data) % PKHEX_RECORD_SIZE != 0:
        raise SystemExit("personal_y: bad size")
    return {dex: data[dex * PKHEX_RECORD_SIZE + PERSONAL_Y_CATCH_RATE_OFF]
            for dex in range(len(data) // PKHEX_RECORD_SIZE)}


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    kasbuunk, pkhex = sys.argv[1], sys.argv[2]

    gen = os.path.join(kasbuunk, "crates/pksave/src/gen1/data/generated")
    species_rs = open(os.path.join(gen, "species.rs")).read()
    moves_rs = open(os.path.join(gen, "moves.rs")).read()
    items_rs = open(os.path.join(gen, "items.rs")).read()
    types_rs = open(os.path.join(gen, "types.rs")).read()

    index_to_dex = parse_rust_u8_array(species_rs, "INDEX_TO_DEX", 256)
    dex_to_index = parse_rust_u8_array(species_rs, "DEX_TO_INDEX", 152)
    species_names = parse_rust_str_array(species_rs, "SPECIES_NAMES", 152)
    base_stats = parse_base_stats(species_rs)
    moves = parse_moves(moves_rs)
    item_names = parse_rust_str_array(items_rs, "ITEM_NAMES", 256)
    type_names = parse_rust_str_array(types_rs, "TYPE_NAMES", 27)

    # sanity: the two maps must be inverses for all real species
    for dex in range(1, 152):
        if index_to_dex[dex_to_index[dex]] != dex:
            raise SystemExit(f"dex roundtrip broken at {dex}")

    # Yellow-specific catch rates (the only field where Yellow differs
    # from Red/Blue in the fields we use).
    ycr = yellow_catch_rates(os.path.join(
        pkhex, "PKHeX.Core/Resources/byte/personal/personal_y"))
    changed = []
    for dex in range(1, 152):
        if base_stats[dex][7] != ycr[dex]:
            changed.append(species_names[dex])
    if sorted(changed) != sorted(["PIKACHU", "KADABRA", "DRAGONAIR",
                                  "DRAGONITE"]):
        raise SystemExit(f"unexpected catch-rate diff set: {changed}")

    lines = []
    a = lines.append
    a("/*")
    a(" * yellow_data.c - immutable Pokemon Yellow data tables (ROM bank 4).")
    a(" *")
    a(" * Base stats/names/moves/items derived from pret/pokered via the")
    a(" * kasbuunk/Pokemon (MIT) generated tables; Yellow-specific catch")
    a(" * rates (Pikachu, Kadabra, Dragonair, Dragonite) cross-checked")
    a(" * against PKHeX personal_y. See NOTICE for attribution.")
    a(" *")
    a(" * Strings are stored as ASCII char arrays; name accessors copy")
    a(" * them into caller buffers so no pointer crosses a switched bank.")
    a(" *")
    a(f" * Generated by tools/generate_yellow_data.py from")
    a(f" * kasbuunk/Pokemon @ {KASBUUNK_COMMIT} and")
    a(f" * kwsch/PKHeX @ {PKHEX_COMMIT} (reference only). Do not edit.")
    a(" */")
    a("")
    a('#include "yellow.h"')
    a("")
    a('#include "yellow_data.h"')
    a('#ifdef __SDCC')
    a('#pragma bank 4')
    a('#endif')
    a("")
    a("/* species names by Dex number 1..151 (slot 0 unused) */")
    a("const char yellow_species_names[YELLOW_SPECIES_COUNT + 1][13] = {")
    a('    "?????",')
    for dex in range(1, 152):
        a(f'    "{to_ascii(species_names[dex])}",  /* {dex} */')
    a("};")
    a("")
    a("/* base stats by Dex number 1..151 (slot 0 zeroed); catch rates are")
    a(" * the Yellow cartridge values (Pikachu 163, Kadabra 96,")
    a(" * Dragonair 27, Dragonite 9) */")
    a("const YellowBaseStats yellow_base_stats_table[YELLOW_SPECIES_COUNT + 1] = {")
    a("    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },")
    for dex in range(1, 152):
        hp, atk, dfn, spd, spc, t1, t2, cr, ey, gr = base_stats[dex]
        cr = ycr[dex]
        a(f"    {{ {hp}, {atk}, {dfn}, {spd}, {spc}, {t1}, {t2}, {cr},"
          f" {ey}, {gr} }},  /* {to_ascii(species_names[dex])} */")
    a("};")
    a("")
    a("/* internal species index -> Dex number (0 = invalid); 256 entries */")
    a("const uint8_t yellow_index_to_dex_table[256] = {")
    for row in range(0, 256, 16):
        a("    " + ", ".join("%3d" % v for v in index_to_dex[row:row + 16]) + ",")
    a("};")
    a("")
    a("/* Dex number -> internal species index (slot 0 unused); 152 entries */")
    a("const uint8_t yellow_dex_to_index_table[YELLOW_SPECIES_COUNT + 1] = {")
    for row in range(0, 152, 16):
        a("    " + ", ".join("0x%02X" % v for v in dex_to_index[row:row + 16]) + ",")
    a("};")
    a("")
    a("/* moves by id 1..165 (slot 0 zeroed) */")
    a("const YellowMoveInfo yellow_moves_table[YELLOW_MOVE_COUNT + 1] = {")
    a('    { "", 0, 0, 0, 0 },')
    for i in range(1, 166):
        name, power, type_id, acc, pp = moves[i]
        a(f'    {{ "{to_ascii(name)}", {power}, {type_id}, {acc}, {pp} }},')
    a("};")
    a("")
    a("/* item names by id; \"\" marks invalid/glitch ids */")
    a("const char yellow_item_names[256][13] = {")
    for i in range(256):
        a(f'    "{to_ascii(item_names[i])}",  /* 0x{i:02X} */')
    a("};")
    a("")
    a("/* Gen 1 type names by id 0..26 */")
    a("const char yellow_type_names[27][10] = {")
    for t in range(27):
        a(f'    "{to_ascii(type_names[t])}",  /* {t} */')
    a("};")

    out_path = os.path.join(os.path.dirname(__file__), "..", "src",
                            "yellow_data.c")
    with open(out_path, "r") as f:
        current = f.read()
    marker = "/* Copy a NUL-terminated table string"
    if marker not in current:
        raise SystemExit("accessor section marker not found in yellow_data.c")
    accessors = current[current.index(marker):]
    with open(out_path, "w") as f:
        f.write("\n".join(lines) + "\n\n" + accessors)
    print(f"regenerated {os.path.normpath(out_path)} "
          f"({len(lines) + len(accessors.splitlines())} lines)")


if __name__ == "__main__":
    main()

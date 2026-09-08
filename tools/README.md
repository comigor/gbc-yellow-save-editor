# Data regeneration

The ROM builds from committed C tables; reference checkouts are not build dependencies.

To regenerate the tables, obtain these exact revisions:

- `kasbuunk/pokemon`: `ac72f04af1026b4023015c239b34730b8cccb15e`
- `kwsch/PKHeX`: `e0e63bc87837ad2d9c8f8fda4efdbf5f2933db08`

Then run from the repository root:

```sh
python3 tools/generate_yellow_data.py /path/to/pokemon /path/to/PKHeX
```

The generator updates the table prefix in `src/yellow_data.c` while retaining its hand-written accessor implementation. Verify the resulting ROM with `make check` and the compiled-ROM smoke test before using it.

`check_map.py` rejects ROM-bank overflow and static data encroaching on the editor's banked save memory. GBDK's linker can otherwise emit an overlapping ROM while only warning.

## Private graphics

`extract_yellow_graphics.py` accepts the SHA-256-pinned English Yellow ROM, raw or gzip-compressed. `yellow_graphics.json` records factual ROM offsets, not artwork: 151 front pointers in Pokédex order, the packed menu-category table, and the menu graphics copy descriptors. The reference is `pret/pokeyellow` at `e89ead154b9968aa50eed9328ff2b38b6c194382` (`home/pics.asm`, `home/uncompress.asm`, `engine/gfx/mon_icons.asm`, `engine/items/town_map.asm`, and `data/icon_pointers.asm`).

The decoder expands zero runs, undoes differential/XOR bitplanes, converts column-major data into row-major 2bpp tiles, and aligns fronts in a 7×7 tile canvas. Menu icons use the game's first-frame OAM mirroring rules, including the asymmetric helix icon. Banks 8–15 hold 19 fronts each (18 in the last bank); bank 7 holds the menu icons and category IDs. The renderer occupies fixed ROM so changing asset banks cannot bank out its own code.

`tests/graphics_smoke.py` pins front-tile SHA-256 digests computed with pret's independent `pkmncompress -u` decoder at Bulbasaur `0D:4000`, Pikachu `0B:4D55`, and Mew `09:69D2`, then independently pads the tile grid. These hashes do not come from this extractor or its manifest. Menu-category checks pin the `MonPartyData` values at category transitions (including Dex 24/25 and 89/90), detecting swapped nybbles and a missing index-zero entry. The rendering run checks full-width HP values and compares the sprite-area pixel bytes on each non-summary tab before and after changing species.

Use the private build commands in the root README. Generated C, binaries and screenshots contain game artwork; keep them out of public source and artifacts.

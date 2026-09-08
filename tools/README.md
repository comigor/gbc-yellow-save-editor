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

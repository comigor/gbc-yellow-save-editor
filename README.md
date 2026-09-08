# GBC Yellow Save Editor

A native Game Boy Color ROM that edits Pokémon Yellow battery-save files directly on an EverDrive GB X7's FAT32 SD card. It runs on a ModRetro Chromatic with stock firmware: no USB bridge, custom FPGA image, Gowin compiler, phone, or PC save-file round trip is needed for editing.

This is a fan project, not affiliated with Nintendo, Game Freak, The Pokémon Company, ModRetro, or Krikzz.

## Supported editing

- Existing Pokémon in the party and all 12 PC boxes: species, nickname, level, experience, moves, current PP, PP Ups, DVs, and stat experience.
- Level and experience remain coherent. Species changes update types and catch rate; relevant edits recalculate party stats and adjust HP while preserving fainting.
- Money, individual badges, and Pikachu friendship.
- Bag item type and quantity, adding and removing items (20-slot limit).
- The current box uses the game's authoritative working copy rather than its potentially stale bank copy.
- Pokémon detail tabs: Summary, Moves, DVs and Stat EXP. Optional private graphics builds add authentic Yellow front sprites and shared Gen I menu icons.

World/event state, PC items, Pokédex, daycare, original-trainer identity, and Pokémon creation/deletion/reordering are not editor features. Unknown save bytes are preserved. Changing species/moves does not enforce encounter legality or legal learnsets.

## Requirements and limitations

- Game Boy Color mode: the editor uses CGB banked WRAM. The Chromatic supplies this in normal color mode.
- EverDrive GB X7 and an SDv2 FAT32 card with 512-byte sectors. This is an X7-specific register driver, not a generic flash-cart SD API. Other flash carts are unsupported.
- An **English/international Pokémon Yellow** raw battery save of exactly 32,768 bytes (`.srm` or `.sav`). Japanese save layouts, RTC trailers, emulator containers, and save states are unsupported.
- File browser displays validated FAT long filenames (up to 255 UTF-16 code units); unsupported glyphs appear as `?`. **SELECT** shows the full selected name. Missing or corrupt long-name records fall back to the 8.3 alias. File access still uses short aliases internally, with paths bounded to 239 characters.
- Game identity cannot be proven from SRAM alone. A weak/absent Yellow marker requires confirmation; select the correct game yourself. The format validator rejects malformed layouts and the UI refuses saves with checksum failures.
- Editing files on the X7 SD card does not edit a separate original Pokémon cartridge. That cartridge's save must already have been transferred to the X7 game save if you want to edit it here.

## Before first use

**Use a spare card and keep an independent backup of the entire card.** The low-level SD writer was reused from a project that physically created and independently read back a file on the Chromatic/X7. This editor's full workflow has not yet been tested on physical hardware.

1. Save in Pokémon Yellow.
2. Return through the EverDrive's normal menu and switch games so the pending battery save is written to SD. Do not edit a stale SD copy while Yellow's newer save is still pending in the cartridge.
3. Copy `yellow-editor-sprites.gbc` from your private build to the card and launch it through the EverDrive menu. Startup must show **Graphics: ON**. The public `yellow-editor.gbc` works without artwork and displays **Graphics: OFF**.
4. Press **START**, browse to Yellow's `.srm`/`.sav` file (often under `GBCSYS/SAVE`), and open it.
5. Edit in memory. Nothing is written to SD until you explicitly confirm **Save to SD**.
6. Wait for **SAVE VERIFIED**. Reload Yellow through the EverDrive menu; **do not load an older save state**, which can replace the edited game state and later overwrite the battery save.

## Controls

| Screen | Controls |
|---|---|
| File browser | Up/Down selects; Left/Right pages; SELECT shows full name; A opens; B goes to parent; START exits |
| Editor menus | Up/Down selects; A opens; B returns; START on the main menu saves |
| Pokémon list | Left/Right switches party and boxes; Up/Down selects Pokémon |
| Pokémon details | Left/Right switches Summary / Moves / DVs / Stat EXP; Up/Down selects fields; A edits; B returns to the list |
| Numeric editor | Up/Down changes value; Left/Right changes decimal step; A applies; B cancels |
| Nickname editor | Left/Right moves cursor; Up/Down changes character; SELECT inserts a space; A applies; B cancels |
| Badges | A toggles selected badge |

Directory pages are cached: cursor movement and full-name viewing perform no SD reads. Changing directory or page scans the directory, so the initial listing can still take time in large folders.

Slow operations display a phase and progress bar: mounting, directory scans, loading, validation, preparation, original-file checks, backup-name selection, backup creation/sync/readback, save writing/sync/readback. File transfers report bytes processed as a percentage; operations without a known total show an activity bar instead. Completion is reported only after the phase's close/checksum checks succeed. **100% on one phase is not permission to power off; wait for SAVE VERIFIED.**

Selecting another save with pending edits requires explicit discard confirmation. The editor modifies existing Pokémon only; empty boxes remain empty.

## Save safety and recovery

On confirmed save, the editor:

1. Rereads the source and checks it against the CRC captured when loaded.
2. Creates a **new, non-overwriting backup** in the same directory: `PK000001.BAK` through `PK000999.BAK`, skipping occupied names.
3. Closes the backup, remounts the filesystem to invalidate caches, and checks the complete backup against the original CRC.
4. Rechecks the source, then overwrites only its existing bytes from offset `0x2000` onward. It does not truncate, rename, delete, or replace the original directory entry. Bank 0 (including Hall of Fame/sprite buffers) is not rewritten.
5. Closes, remounts, rereads, and verifies the edited file against the expected CRC and edited memory before reporting success.

Only checksums covering edited regions are updated. Opening and leaving an unedited save does not rewrite it.

**FAT32 and SD writes are not transactional.** Power loss or media errors can corrupt the target, backup, or filesystem metadata; a backup on the same card is not an independent backup. Never remove the card/cartridge or interrupt power during writes. The editor stops on errors rather than blindly retrying more writes. A failed backup can leave a partial `.BAK`; a failed overwrite can leave a partial edited save.

If a write fails, stop using that save. With the device powered off, use a card reader to preserve an image of the card and recover from the verified backup or your independent backup. Copy the original `.BAK` bytes back to the game's original save filename only after checking which backup is complete. Recovery is not performed automatically by this ROM. Archive/remove old backups on a PC if all 999 names are occupied.

FatFs deliberately invalidates the FAT32 FSInfo free-space hint when allocating. An offline filesystem check may report that the free-space count is unset; that alone is not structural corruption. Do not ignore other filesystem errors.

## Build with Docker

Docker BuildKit builds the ROM and runs host-side save integrity tests. GBDK **4.5.0** is downloaded from its official release and checked against a pinned SHA-256, with Linux amd64 and arm64 supported.

```sh
docker build --output type=local,dest=dist .
```

Output: `dist/yellow-editor.gbc` (**artwork-free**, **Graphics: OFF**). GitHub's `yellow-editor-artwork-free` artifact is the same variant. For Pokémon sprites, use the private build below. No Nintendo ROMs, saves, keys, or licensed FPGA software are required for the public build.

To retain a compiler image for development:

```sh
docker build --target toolchain -t yellow-editor-toolchain .
docker run --rm -v "$PWD:/src" yellow-editor-toolchain make -j2
docker run --rm -v "$PWD:/src" yellow-editor-toolchain make check
```

The compiler and source inputs are pinned; OS packages are resolved from Debian 12 repositories at build time, so the container image itself is not bit-for-bit pinned.

## Optional sprites from your own Yellow ROM

The default build and public CI artifacts are **artwork-free**. To enable sprites, supply your own English Pokémon Yellow ROM locally. The extractor accepts the verified 1 MiB release with SHA-256 `8cbaa499397e4f1a679c992ea9382a2dd7942ab398b48c19829c2d9529de47bf`; other revisions are rejected, not guessed.

```sh
make GBDK_HOME=/path/to/gbdk YELLOW_ROM="/path/to/Pokemon Yellow.gb" -j2
```

Output: **`build/private/yellow-editor-sprites.gbc`** (256 KiB), separate from the 128 KiB artwork-free build. Startup shows **Graphics: ON**. All 151 front sprites are decoded during the build, padded to 56×56, and stored in ROM banks. Party/box lists use Yellow's shared 16×16 category icons. The gamepad UI loads tiles directly from ROM, not SD. Sprites are monochrome, using the same four-shade palette as the editor.

Docker uses a gzip-compressed BuildKit secret: the verified ROM compresses below BuildKit's 500 KiB secret limit, and is decompressed only in the extractor's memory. The source game ROM is never copied into an image layer.

```sh
mkdir -p private
gzip -9 -n -c "/path/to/Pokemon Yellow.gb" > private/yellow.gb.gz
docker build --target private-artifact --no-cache-filter private-build \
  --secret id=yellow_rom,src=private/yellow.gb.gz \
  --output type=local,dest=dist-private .
```

Output: **`dist-private/yellow-editor-sprites.gbc`**. Copy this file—not `dist/yellow-editor.gbc`—to the X7 for sprites. BuildKit secret contents do not affect its cache key; `--no-cache-filter private-build` ensures a different supplied ROM is validated rather than reusing an earlier private layer.

**Keep the resulting ROM, generated tiles, screenshots and private Docker build cache private.** They contain copyrighted game artwork and are not covered by this project's source-code license. The source ROM is read-only; extraction never modifies it. No download of game ROMs or sprite assets occurs during a build. `private/`, private outputs and common ROM/save extensions are excluded from Git and Docker's ordinary build context. Do not force-add them or upload private cache layers to a public registry.

Private graphics verification (requires the private build and the emulator dependencies below):

```sh
.venv/bin/python tests/graphics_smoke.py
.venv/bin/python tests/rom_smoke.py --rom build/private/yellow-editor-sprites.gbc
```

## Build locally

Install [GBDK 4.5.0](https://github.com/gbdk-2020/gbdk-2020/releases/tag/4.5.0), then:

```sh
make GBDK_HOME=/path/to/gbdk -j2
make check
```

`make check` needs Python 3 and a C99 compiler (`clang` by default, override with `CC`). It builds the same format and storage code for the host and uses disposable FAT32 images, never a physical card. Build output is `build/yellow-editor.gbc` with symbol/map files alongside it.

To exercise the compiled ROM's gamepad UI, banked RAM, filesystem and X7 SPI transport against a disposable SD image:

```sh
python3 -m venv .venv
.venv/bin/pip install -r tests/requirements.txt
.venv/bin/python tests/rom_smoke.py
.venv/bin/python tests/browser_smoke.py
```

Screenshots and structured evidence are written under `build/rom-smoke/`. Fixtures are synthetic; no retail Pokémon ROM is included or required. Emulator verification cannot establish electrical compatibility or power-loss safety on a real card.

### Verification status

- Host checks cover party and all 12 boxes, coherent stats/EXP/PP, main/box checksums, malformed input rejection and unchanged save regions.
- Disposable FAT32 scenarios cover fragmented files, existing backup names, a full card, backup write failures, corrupt backup readback and overwrite failures.
- The compiled ROM has completed a gamepad-driven PyBoy run through the X7 SPI model: party/box level changes, money, bag quantity, a new backup, overwrite and remounted readback verification. The backup was byte-identical to the original and the sentinel file remained unchanged.
- The browser regression measured nine SD sector reads per cursor movement before the fix and zero afterward. It covers long-name display, full-name viewing, page changes and opening the selected save through its short alias. Host cases cover 255-character names, cross-sector LFN records and corrupt-LFN fallback.
- Private graphics: all 151 decoded fronts matched an independent decoder. PyBoy verified front/icon pixels, all eight graphics ROM banks, detail-tab navigation and save readback. Selecting graphics performed no SD reads or writes. Native macOS and Docker Linux builds are byte-identical for both artwork-free and private ROMs. Graphics have not yet been checked on physical hardware.
- Graphics regressions pin independent Bulbasaur/Pikachu/Mew artwork hashes and known menu categories, verify five-digit HP values stay clear of the sprite, and compare actual pixels when clearing artwork on all three non-summary tabs.
- The owner has run the editor on Chromatic/X7 and reported successful save loading and responsive editing; these browser fixes are emulator-verified pending a hardware retry. **Not yet verified:** a complete physical save-write/readback roundtrip, power interruption recovery, or an edited save booted in retail Yellow.

## Implementation

| Module | Responsibility |
|---|---|
| `src/yellow.c`, `src/yellow_data.c` | Gen I save format, editing, checksums, species/move/item tables |
| `src/save_memory.c` | Stack-safe access to 24 KiB of CGB WRAM banks 2–7; bank 1 restored before return |
| `src/storage.c` | Save-file browser access, source identity, backup and readback verification |
| `src/x7_disk.c` | EverDrive X7 SPI SD transport |
| `src/main.c`, `src/browser.c`, `src/ui.c` | 160×144 gamepad interface |
| `src/graphics.c`, `tools/extract_yellow_graphics.py` | Optional ROM-banked rendering and private build-time extraction |
| `vendor/fatfs` | FatFs R0.16 plus official patches 1 and 2; GBDK banked entry points |
| `tests` | Synthetic save fixtures, disposable FAT32 images, core/storage checks and ROM SPI model |

The save's 32 KiB do not fit in ordinary Game Boy RAM. The editor holds banks 1–3 in six CGB WRAM banks and streams untouched bank 0 from SD during backup verification. The WRAM accessors briefly disable interrupts and avoid stack access while another WRAM bank is selected.

## References and licensing

- [PKHeX](https://github.com/kwsch/PKHeX): Gen I format/behavior reference. Its C# application is not embedded in this ROM.
- [kasbuunk/pokemon](https://github.com/kasbuunk/pokemon): MIT-licensed Gen I editor and data reference.
- [pret/pokered](https://github.com/pret/pokered) and [pret/pokeyellow](https://github.com/pret/pokeyellow): game-format/behavior references.
- [GBDK-2020](https://github.com/gbdk-2020/gbdk-2020), [FatFs](https://elm-chan.org/fsw/ff/), and [untoxa/VGM_player](https://github.com/untoxa/VGM_player): toolchain, filesystem, and original X7 driver lineage.

See `NOTICE` and the license files under `vendor/` for attribution and source revisions. No commercial game ROM or user save is distributed.

import argparse
import json
import re
from pathlib import Path

from fixtures import Volume, disk_image, yellow_save
from pyboy import PyBoy
from x7_model import X7Model

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/rom-smoke"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", type=Path, default=ROOT / "build/yellow-editor.gbc")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    rom = args.rom.read_bytes()
    symbols = {}
    for line in args.rom.with_suffix(".noi").read_text().splitlines():
        match = re.match(r"DEF (\S+) (0x[0-9A-Fa-f]+)", line)
        if match:
            symbols[match[1]] = int(match[2], 16)
    save = yellow_save()
    image = disk_image(save, fragmented=True, collision=True)
    model = X7Model(image)
    gb = PyBoy(str(args.rom), window="null", sound_emulated=False, cgb=True)
    gb.set_emulation_speed(0)
    model.install(gb, rom)
    log = []
    transcript = []
    progress_samples = []
    progress_capture = None
    input_ready = False
    bar_samples = {stage: set() for stage in (2, 3, 4, 5, 6, 7)}
    sampled_frames = 0

    def sample_bar():
        stage = gb.memory[symbols["_storage_stage"]]
        if stage not in bar_samples:
            return
        strip = gb.screen.image.convert("RGB").crop((0, 40, 160, 48))
        cells = [strip.crop((x * 8, 0, (x + 1) * 8, 8)) for x in range(20)]
        ink = [len(cell.getcolors()) > 1 for cell in cells]
        if not (ink[0] and ink[-1]):
            return
        filled = sum(ink[1:-1])
        if ink[1:-1] != [True] * filled + [False] * (18 - filled):
            return
        patterns = {cell.tobytes() for cell in cells[1 : filled + 1]}
        if len(patterns) <= 1:
            bar_samples[stage].add(filled)

    def ready(_):
        nonlocal input_ready
        input_ready = True

    gb.hook_register(0, symbols["_ui_key"], ready, None)

    def printed(_):
        nonlocal progress_capture
        char = chr(gb.register_file.A)
        log.append(char)
        transcript.append(char)
        if char == "%":
            match = re.search(r"(\d+)%$", "".join(transcript[-8:]))
            if match:
                percent = int(match[1])
                progress_samples.append((gb.memory[symbols["_storage_stage"]], percent))
                if 45 <= percent <= 55:
                    progress_capture = gb.memory[symbols["_storage_stage"]]

    gb.hook_register(0, symbols[".put_char"], printed, None)

    def tick(frames):
        nonlocal progress_capture, sampled_frames
        for _ in range(frames):
            gb.tick(1, True)
            sampled_frames += 1
            if sampled_frames % 8 == 0:
                sample_bar()
            if progress_capture is not None:
                gb.screen.image.save(OUT / f"progress-{progress_capture}.png")
                progress_capture = None

    def wait(text, frames=20000):
        for _ in range(frames // 10):
            tick(10)
            if text in "".join(log) and input_ready:
                return
        gb.screen.image.save(OUT / "failure.png")
        raise AssertionError((text, "".join(log)[-400:], gb.register_file.PC))

    def key(button):
        nonlocal input_ready
        input_ready = False
        log.clear()
        gb.button_press(button)
        tick(10)
        gb.button_release(button)
        tick(20)

    def snap(name):
        gb.screen.image.save(OUT / f"{name}.png")

    try:
        wait("START: Browse SD")
        snap("welcome")
        key("start")
        wait("Choose Yellow save")
        snap("browser")
        key("a")
        wait("Yellow editor")
        snap("menu")
        key("a")
        wait("Pokemon storage")
        key("a")
        wait("Left/Right: Tabs")
        snap("pokemon")
        key("right")
        wait("Moves / PP")
        key("right")
        wait("HP DV")
        key("right")
        wait("Stat experience")
        key("right")
        wait("Summary")
        key("left")
        wait("Stat experience")
        key("right")
        wait("Summary")
        key("down")
        key("down")
        key("a")
        wait("Level (updates EXP)")
        key("up")
        key("a")
        wait("Left/Right: Tabs")
        key("b")
        wait("Pokemon storage")
        key("right")
        wait("Box 1")
        key("right")
        wait("Box 2")
        key("a")
        wait("Left/Right: Tabs")
        key("down")
        key("down")
        key("a")
        wait("Level (updates EXP)")
        key("right")
        key("up")
        key("a")
        wait("Left/Right: Tabs")
        key("b")
        wait("Pokemon storage")
        key("b")
        wait("UNSAVED CHANGES")
        snap("dirty")
        key("down")
        key("a")
        wait("Pikachu friendship")
        key("a")
        wait("Step")
        key("up")
        key("a")
        wait("Pikachu friendship")
        key("b")
        wait("Yellow editor")
        key("down")
        key("a")
        wait("Add item")
        snap("bag")
        key("a")
        wait("Quantity")
        key("down")
        key("a")
        wait("Step")
        key("up")
        key("a")
        wait("Quantity")
        key("b")
        wait("Add item")
        key("b")
        wait("Yellow editor")
        key("start")
        wait("Write edited save?")
        snap("confirm")
        key("a")
        wait("SAVE VERIFIED", frames=120000)
        tick(30)
        snap("verified")
        volume = Volume(image)
        assert volume.file(b"PK000001BAK") == b"older backup"
        assert volume.file(b"PK000002BAK") == save
        edited = volume.file(b"YELLOW  SRM")
        assert edited[:0x2000] == save[:0x2000]
        assert edited[0x2F34 + 33] == 6
        assert int.from_bytes(edited[0x2F34 + 14 : 0x2F34 + 17], "big") == 216
        box2 = 0x4000 + 0x462 + 22
        assert int.from_bytes(edited[box2 + 14 : box2 + 17], "big") == 3375
        assert edited[0x25F3:0x25F6] == bytes([0, 0x30, 1])
        assert edited[0x25CB] == 11
        assert edited[0x3523] == (~sum(edited[0x2598:0x3523])) & 255
        assert volume.file(b"KEEP    BIN") == b"unchanged sentinel"
        assert not model.unlocked and model.control & 1
        assert gb.memory[0xFF70] & 7 == 1
        for stage in (2, 3, 4, 5, 6, 7):
            values = [
                percent
                for seen_stage, percent in progress_samples
                if seen_stage == stage
            ]
            assert (
                0 in values
                and 100 in values
                and any(0 < value < 100 for value in values)
            ), (stage, values)
            widths = bar_samples[stage]
            assert any(0 <= width <= 4 for width in widths), (stage, widths)
            assert any(8 <= width <= 10 for width in widths), (stage, widths)
            assert any(15 <= width <= 18 for width in widths), (stage, widths)
        (OUT / "progress-bars.json").write_text(
            json.dumps({stage: sorted(widths) for stage, widths in bar_samples.items()})
            + "\n"
        )
        (OUT / "progress.json").write_text(json.dumps(progress_samples) + "\n")
        (OUT / "edited.srm").write_bytes(edited)
        (OUT / "evidence.json").write_text(
            json.dumps(
                {
                    "commands": dict(model.commands),
                    "party_level": 6,
                    "box2_level": 15,
                    "money": 3001,
                    "potion_quantity": 11,
                    "backup_byte_identical": True,
                },
                indent=2,
            )
            + "\n"
        )
        print(
            "PASS compiled ROM: browse, party/box edits, trainer, bag, backup, save readback; SPI commands",
            model.commands,
        )
    finally:
        gb.stop(save=False)


if __name__ == "__main__":
    main()

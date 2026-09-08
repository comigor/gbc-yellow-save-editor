import os
import re
import subprocess
from pathlib import Path
from fixtures import yellow_save, disk_image, Volume

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/check'


def run():
    OUT.mkdir(parents=True, exist_ok=True)
    binary = OUT / 'core-storage-check'
    subprocess.run([os.environ.get('CC', 'clang'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-g', '-Isrc', '-Ivendor/fatfs', '-DX7_FATFS',
                    'tests/core_storage_check.c', 'tests/disk_image.c', 'src/save_memory.c',
                    'src/yellow.c', 'src/yellow_data.c', 'src/storage.c', 'src/checksum.c',
                    'vendor/fatfs/ff.c', '-o', str(binary)], cwd=ROOT, check=True)
    save = yellow_save()
    fixture = OUT / 'yellow.srm'
    fixture.write_bytes(save)
    subprocess.run([str(binary), 'core', str(fixture)], check=True)
    for mode in ['normal', 'fragmented', 'collision', 'full', 'backup-write', 'backup-corrupt', 'save-write']:
        image = OUT / (mode + '.img')
        before = disk_image(save, full=mode=='full', fragmented=mode=='fragmented', collision=mode=='collision')
        image.write_bytes(before)
        output = subprocess.check_output([str(binary), 'storage', str(image), mode], text=True)
        result = int(re.search(r'result=(\d+)', output)[1])
        after = image.read_bytes()
        volume = Volume(after)
        assert volume.file(b'KEEP    BIN') == b'unchanged sentinel'
        assert after[volume.fat:volume.fat+volume.fatsz] == after[volume.fat+volume.fatsz:volume.fat+2*volume.fatsz]
        if mode in ['full', 'backup-write', 'backup-corrupt', 'save-write']:
            assert result != 0, output
            assert volume.file(b'YELLOW  SRM') == save
            if mode == 'save-write': assert volume.file(b'PK000001BAK') == save
        else:
            assert result == 0, output
            backup = b'PK000002BAK' if mode == 'collision' else b'PK000001BAK'
            assert volume.file(backup) == save
            edited = volume.file(b'YELLOW  SRM')
            assert len(edited) == 32768 and edited[:0x2000] == save[:0x2000]
            assert edited[0x2f34+33] == 50
            assert int.from_bytes(edited[0x2f34+14:0x2f34+17], 'big') == 125000
            assert edited[0x3523] == (~sum(edited[0x2598:0x3523])) & 255
            assert edited[0x4000:] == save[0x4000:]
            if mode == 'collision': assert volume.file(b'PK000001BAK') == b'older backup'
        print('PASS storage', mode, output.strip())
        image.unlink()
    print('PASS: core/save write checks; no physical card accessed')


if __name__ == '__main__':
    run()

import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text()
sections = re.findall(r'^(_\w+)\s+([0-9A-F]{8})\s+([0-9A-F]{8})\s+=', text, re.M)
for name, address, size in sections:
    address, size = int(address, 16), int(size, 16)
    if name.startswith('_CODE_') and size:
        assert (address & 65535) + size <= 0x8000, f'{name} exceeds its 16KiB ROM bank'
    elif name in ('_CODE', '_HOME', '_GSINIT', '_GSFINAL', '_INITIALIZER') and size:
        assert address + size <= 0x4000, f'{name} exceeds fixed ROM bank'
    elif name in ('_DATA', '_BSS', '_INITIALIZED') and size:
        assert address + size <= 0xd000, f'{name} overlaps banked save WRAM'
assert sections, 'No linker sections found'
print('PASS ROM bank capacities and save WRAM isolation')

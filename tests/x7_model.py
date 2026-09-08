from collections import Counter, deque


class X7Model:
    def __init__(self, image):
        self.image = image
        self.control = 1
        self.unlocked = False
        self.idle = True
        self.packet = []
        self.response = deque()
        self.received = 255
        self.busy = 0
        self.commands = Counter()
        self.phase = 0
        self.block = bytearray()
        self.sector = 0

    def write(self, address, value):
        if address == 0xbd0a:
            assert value in (0, 0xa5)
            if not value: assert self.control & 1
            self.unlocked = value == 0xa5
            return
        assert self.unlocked
        if address == 0xbd01:
            assert value & ~3 == 0
            self.control = value
            if value & 1:
                assert not self.phase
                self.packet.clear(); self.response.clear()
            return
        assert address == 0xbd00
        self.busy = 2
        self.received = 255
        if self.control & 1: return
        if self.response:
            assert value == 255
            self.received = self.response.popleft()
        elif self.phase == 1:
            if value == 255: return
            assert value == 0xfe
            self.phase = 2; self.block.clear()
        elif self.phase == 2:
            self.block.append(value)
            if len(self.block) == 514:
                self.image[self.sector*512:(self.sector+1)*512] = self.block[:512]
                self.phase = 0
                self.response.extend([5, 255, 0, 0, 255])
        elif self.packet or value & 0xc0 == 0x40:
            self.packet.append(value)
            if len(self.packet) == 6:
                self.command(); self.packet.clear()

    def read(self, address):
        assert self.unlocked
        if address == 0xbd01:
            if self.busy:
                self.busy -= 1
                return self.control | 128
            return self.control
        assert address == 0xbd00
        return self.received

    def command(self):
        cmd = self.packet[0] & 63
        arg = int.from_bytes(bytes(self.packet[1:5]), 'big')
        self.commands[cmd] += 1
        if cmd == 0:
            assert arg == 0 and self.packet[5] == 0x95
            self.idle = True; response = [1]
        elif cmd == 8:
            assert arg == 0x1aa and self.packet[5] == 0x87
            response = [1, 0, 0, 1, 0xaa]
        elif cmd == 55: response = [int(self.idle)]
        elif cmd == 41:
            assert arg == 0x40000000
            self.idle = False; response = [0]
        elif cmd == 58: response = [0, 0xc0, 255, 128, 0]
        elif cmd == 13: response = [0, 0]
        elif cmd in (17, 24):
            assert not self.idle and (arg+1)*512 <= len(self.image)
            if cmd == 17: response = [0, 0xfe, *self.image[arg*512:(arg+1)*512], 255, 255]
            else:
                self.phase = 1; self.sector = arg; response = [0]
        else: raise AssertionError(f'Unexpected SD command {cmd}')
        self.response.extend(response)

    def install(self, gb, rom):
        def replace(pattern, offset, size, callback):
            pattern = bytes.fromhex(pattern)
            assert rom.count(pattern) == 1, (pattern.hex(), rom.count(pattern))
            address = rom.index(pattern) + offset
            gb.memory[0, address:address+size] = [0]*size
            gb.hook_register(0, address, callback, None)
        replace('ea01bdc94f', 0, 3, lambda _: self.write(0xbd01, gb.register_file.A))
        replace('2100bd71010010', 3, 1, lambda _: self.write(0xbd00, gb.register_file.C))
        replace('fa01bd0730', 0, 3, lambda _: setattr(gb.register_file, 'A', self.read(0xbd01)))
        replace('fa00bdc9210abd', 0, 3, lambda _: setattr(gb.register_file, 'A', self.read(0xbd00)))
        replace('210abd36a5fa01bd', 3, 2, lambda _: self.write(0xbd0a, 0xa5))
        replace('fa01bde603', 0, 3, lambda _: setattr(gb.register_file, 'A', self.read(0xbd01)))
        replace('210abd3600c9', 3, 2, lambda _: self.write(0xbd0a, 0))

import struct


def checksum(data, start, end):
    return (~sum(data[start:end])) & 255


def yellow_save():
    data = bytearray(32768)
    data[:0x2000] = bytes((i * 17 + 3) & 255 for i in range(0x2000))
    data[0x2598:0x25a3] = bytes([0x80, 0x92, 0x87, 0x50] + [0x50] * 7)
    data[0x25f6:0x2601] = bytes([0x86, 0x80, 0x91, 0x98, 0x50] + [0x50] * 6)
    data[0x25c9:0x25ce] = bytes([1, 4, 10, 255, 0])
    data[0x25f3:0x25f6] = bytes([0, 0x30, 0])
    data[0x27e6:0x27e8] = bytes([0, 255])
    data[0x2605:0x2607] = bytes([0x12, 0x34])
    data[0x271c] = 90
    data[0x29c3] = 0x54
    data[0x284c] = 0x80
    data[0x2f2c:0x2f2f] = bytes([1, 0x54, 255])
    mon = bytearray(44)
    mon[0] = 0x54
    mon[1:4] = bytes([0, 20, 5])
    mon[5:8] = bytes([23, 23, 163])
    mon[8:12] = bytes([84, 45, 0, 0])
    mon[12:14] = bytes([0x12, 0x34])
    mon[14:17] = (125).to_bytes(3, 'big')
    mon[27:29] = bytes([0x98, 0x76])
    mon[29:33] = bytes([30, 40, 0, 0])
    mon[33] = 5
    for i, value in enumerate([20, 11, 8, 14, 10]):
        mon[34 + i * 2:36 + i * 2] = value.to_bytes(2, 'big')
    data[0x2f34:0x2f60] = mon
    data[0x303c:0x3047] = data[0x2598:0x25a3]
    name = bytes([0x8f, 0x88, 0x8a, 0x80, 0x82, 0x87, 0x94, 0x50, 0x50, 0x50, 0x50])
    data[0x307e:0x3089] = name
    for box in range(12):
        base = (0x4000 if box < 6 else 0x6000) + box % 6 * 0x462
        data[base:base+3] = bytes([1, 0x54, 255])
        data[base+22:base+55] = mon[:33]
        data[base+0x2aa:base+0x2b5] = data[0x2598:0x25a3]
        data[base+0x386:base+0x391] = name
    data[0x30c0:0x3522] = data[0x4000:0x4462]
    for bank in (0x4000, 0x6000):
        data[bank+0x1a4c] = checksum(data, bank, bank+0x1a4c)
        for box in range(6):
            start = bank + box * 0x462
            data[bank+0x1a4d+box] = checksum(data, start, start+0x462)
    data[0x3523] = checksum(data, 0x2598, 0x3523)
    return data


class Volume:
    def __init__(self, data):
        self.data = data
        self.spc = data[13]
        self.fat = struct.unpack_from('<H', data, 14)[0] * 512
        self.fatsz = struct.unpack_from('<I', data, 36)[0] * 512
        self.start = self.fat + 2 * self.fatsz

    def chain(self, cluster):
        result = []
        while cluster < 0x0ffffff8:
            assert cluster >= 2 and cluster not in result
            result.append(cluster)
            cluster = struct.unpack_from('<I', self.data, self.fat+cluster*4)[0] & 0xfffffff
        return result

    def contents(self, cluster):
        size = self.spc * 512
        return b''.join(self.data[self.start+(c-2)*size:self.start+(c-1)*size] for c in self.chain(cluster))

    def entries(self, cluster=2):
        root = self.contents(cluster)
        return {root[i:i+11]: root[i:i+32] for i in range(0, len(root), 32) if root[i] not in (0, 0xe5)}

    def file(self, name):
        entry = self.entries()[name]
        cluster = struct.unpack_from('<H', entry, 26)[0] | struct.unpack_from('<H', entry, 20)[0] << 16
        size = struct.unpack_from('<I', entry, 28)[0]
        return self.contents(cluster)[:size] if cluster else b''


def disk_image(save, full=False, fragmented=False, collision=False, spc=1):
    clusters = 65530
    fatsz = (clusters + 129) // 128
    start = 32 + 2 * fatsz
    data = bytearray((start + clusters * spc) * 512)
    data[:3] = b'\xeb\x58\x90'; data[3:11] = b'PKEDIT  '
    struct.pack_into('<H', data, 11, 512); data[13] = spc
    struct.pack_into('<H', data, 14, 32); data[16] = 2; data[21] = 248
    struct.pack_into('<I', data, 32, len(data)//512)
    struct.pack_into('<I', data, 36, fatsz)
    struct.pack_into('<I', data, 44, 2)
    struct.pack_into('<HH', data, 48, 1, 6)
    data[64] = 128; data[66] = 41
    data[71:82] = b'PKEDIT TEST'; data[82:90] = b'FAT32   '; data[510:512] = b'\x55\xaa'
    struct.pack_into('<I', data, 512, 0x41615252)
    struct.pack_into('<I', data, 996, 0x61417272)
    struct.pack_into('<II', data, 1000, 0xffffffff, 0xffffffff)
    struct.pack_into('<I', data, 1020, 0xaa550000)
    data[3072:4096] = data[:1024]
    fat = bytearray(fatsz*512)
    for c, v in [(0,0xffffff8),(1,0xfffffff),(2,0xfffffff)]: struct.pack_into('<I',fat,c*4,v)
    used = {2}
    def add(index, name, content, chain):
        used.update(chain)
        for i,c in enumerate(chain):
            struct.pack_into('<I',fat,c*4,chain[i+1] if i+1<len(chain) else 0xfffffff)
            pos=(start+(c-2)*spc)*512
            chunk=content[i*spc*512:(i+1)*spc*512]
            data[pos:pos+len(chunk)]=chunk
        entry=start*512+index*32
        data[entry:entry+11]=name; data[entry+11]=32
        struct.pack_into('<H',data,entry+26,chain[0] if chain else 0)
        struct.pack_into('<I',data,entry+28,len(content))
    count=(len(save)+spc*512-1)//(spc*512)
    chain=list(range(3,3+count*2,2)) if fragmented else list(range(3,3+count))
    add(0,b'YELLOW  SRM',save,chain)
    sentinel=max(chain)+1
    add(1,b'KEEP    BIN',b'unchanged sentinel', [sentinel])
    if collision: add(2,b'PK000001BAK',b'older backup', [sentinel+1])
    if full:
        chain=[c for c in range(3,clusters+2) if c not in used]
        fill_index = 3 if collision else 2
        add(fill_index,b'FILL    BIN',b'',chain)
        struct.pack_into('<I',data,start*512+fill_index*32+28,len(chain)*spc*512)
    for i in range(2): data[(32+i*fatsz)*512:(32+(i+1)*fatsz)*512]=fat
    return data
def lfn_entries(name, alias):
    checksum = 0
    for byte in alias:
        checksum = (((checksum & 1) << 7) + (checksum >> 1) + byte) & 255
    encoded = list(struct.unpack('<' + 'H' * (len(name.encode('utf-16le')) // 2), name.encode('utf-16le')))
    if len(encoded) % 13:
        encoded.append(0)
        encoded.extend([65535] * ((-len(encoded)) % 13))
    entries = []
    for ordinal in range(len(encoded) // 13, 0, -1):
        entry = bytearray(32)
        entry[0] = ordinal | (64 if ordinal == len(encoded) // 13 else 0)
        entry[11] = 15
        entry[13] = checksum
        for offset, char in zip([1,3,5,7,9,14,16,18,20,22,24,28,30], encoded[(ordinal-1)*13:ordinal*13]):
            struct.pack_into('<H', entry, offset, char)
        entries.append(entry)
    return entries

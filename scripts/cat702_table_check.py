"""Offline check of a PIU title's CAT702 challenge/response tables against keys.

usage (from the repository root): python scripts/cat702_table_check.py pumpitpc [profile ...]

The game keeps 100 challenges (17-byte slots, a length per entry) and the
expected responses in its data object. This reproduces the engine's CAT702
model and the game's own transaction, and reports, for every key found in
roms/*.zip, how many of the 100 entries match. Task 751.
"""
import struct, sys, zipfile, glob, os

INITIAL = [0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x7F]

class Cat702:
    def __init__(self, transform):
        self.t = list(transform)
        self.select = 1; self.clock = 1; self.data_in = 1
        self.state = 0; self.bit = 0; self.data_out = 1
    def coeff(self, select, bit):
        if select == 0:
            return self.t[bit]
        r = self.coeff((select - 1) & 7, (bit - 1) & 7)
        r = ((r << 1) | (((r >> 7) & 1) ^ ((r >> 6) & 1))) & 0xFF
        if bit != 7:
            return r
        return r ^ self.coeff(select, 0)
    def apply_bit(self, select):
        r = 0
        for i in range(8):
            if (self.state >> i) & 1:
                r ^= self.coeff(select, i)
        self.state = r
    def apply(self, sbox):
        r = 0
        for i in range(8):
            if (self.state >> i) & 1:
                r ^= sbox[i]
        self.state = r
    def write_select(self, s):
        if self.select == s:
            return
        if s == 0:
            self.state = 0xFC; self.bit = 0; self.apply(INITIAL)
        else:
            self.data_out = 1
        self.select = s
    def write_clock(self, c):
        if c and self.clock == 0 and self.select == 0:
            if self.data_in == 0:
                self.apply_bit(self.bit)
            self.bit = (self.bit + 1) & 7
            if self.bit == 0:
                self.apply(INITIAL)
            self.data_out = (self.state >> self.bit) & 1
        self.clock = c

def respond(key, challenge):
    """The game's own transaction (pumpitpc 0x010195BE): select low; per bit,
    LSB first: clock low, data = NOT bit, clock high, sample. Two more clocks
    with data low. The sampled stream lags by two bits."""
    c = Cat702(key)
    c.write_select(1); c.write_clock(1)
    c.write_select(0)
    stream = []
    for byte in challenge:
        for bit in range(8):
            c.write_clock(0)
            c.data_in = 0 if (byte >> bit) & 1 else 1
            c.write_clock(1)
            stream.append(c.data_out)
    for _ in range(2):
        c.write_clock(0)
        c.data_in = 0
        c.write_clock(1)
        stream.append(c.data_out)
    c.write_select(1)
    out = []
    for i in range(len(challenge)):
        value = 0
        for k in range(8):
            value |= stream[8 * i + 2 + k] << k
        out.append(value)
    return out

def load_tables(exe):
    d = open(exe, 'rb').read()
    le = d.find(b'LE\x00\x00')
    page = struct.unpack_from('<I', d, le + 0x28)[0]
    objtab = struct.unpack_from('<I', d, le + 0x40)[0]
    nobj = struct.unpack_from('<I', d, le + 0x44)[0]
    datapages = struct.unpack_from('<I', d, le + 0x80)[0]
    objs = [struct.unpack_from('<6I', d, le + objtab + k * 24) for k in range(nobj)]
    # The check: mov ecx,100; xor edx,edx; mov eax,ebx; div ecx
    hits = []
    for sig in (bytes.fromhex('b96400000031d289d8f7f1'), bytes.fromhex('bb64000000c1fa1ff7fb')):
        start = 0
        while True:
            i = d.find(sig, start)
            if i < 0:
                break
            hits.append(i); start = i + 1
    for i in hits:
        w = d[i: i + 0x90]
        a = w.find(b'\x8a\x82')
        b = w.find(b'\xba', a + 6 if a >= 0 else 0)
        c = w.find(b'\x3a\x9c\x02')
        if a < 0 or c < 0:
            continue
        lens = struct.unpack_from('<I', w, a + 2)[0]
        # challenge base: "mov edx, imm32; add edx, eax"
        k = w.find(b'\x01\xc2')
        chal = struct.unpack_from('<I', w, k - 4)[0]
        exp = struct.unpack_from('<I', w, c + 3)[0]
        # The data object is the largest one.
        data_obj = max(objs, key=lambda o: o[0])
        base = datapages + (data_obj[3] - 1) * page
        def rd(off, n):
            return d[base + off: base + off + n]
        entries = []
        for idx in range(100):
            n = rd(lens + idx, 1)[0]
            entries.append((n, rd(chal + idx * 17, n), rd(exp + idx * 17, n)))
        return hex(lens), hex(chal), hex(exp), entries
    return None

keys = {}
for z in sorted(glob.glob('roms/*.zip')):
    zf = zipfile.ZipFile(z)
    for i in zf.infolist():
        if 'cat702' in i.filename.lower():
            keys.setdefault(zf.read(i.filename), []).append(os.path.basename(z)[:-4])

for title in sys.argv[1:]:
    exe = 'build/runtime_mounts/%s/PIU/PIU.EXE' % title
    t = load_tables(exe)
    if t is None:
        print(title, ': check routine not found'); continue
    lens, chal, exp, entries = t
    print('%s: tables len=%s challenge=%s expected=%s, lengths %d..%d' % (
        title, lens, chal, exp, min(e[0] for e in entries), max(e[0] for e in entries)))
    print('   entry0 challenge=%s expected=%s' % (entries[0][1].hex(), entries[0][2].hex()))
    for key, owners in keys.items():
        ok = sum(1 for n, c, e in entries if bytes(respond(key, c)) == e)
        print('   key %s (%s): %d/100 match' % (key.hex(), ','.join(owners), ok))

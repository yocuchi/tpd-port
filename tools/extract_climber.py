#!/usr/bin/env python3
# Extrae de main.dol las tablas del modo Escalador (CFlowClimberGame/CClimber) -> data/climber.bin
# uso: extract_climber.py [main.dol] [salida]
import struct, sys
DOL = sys.argv[1]
OUT = sys.argv[2]
dol = open(DOL, "rb").read()
offs = struct.unpack('>18I', dol[0:72]); addrs = struct.unpack('>18I', dol[72:144]); sizes = struct.unpack('>18I', dol[144:216])
def rd(a, n):
    for o, ad, s in zip(offs, addrs, sizes):
        if s and ad <= a and a + n <= ad + s: return dol[o + a - ad:o + a - ad + n]
    raise ValueError(hex(a))
R2 = 0x805BEEE0
def f2(off): return struct.unpack('>f', rd(R2 + off, 4))[0]
garbage = struct.unpack('>10i', rd(0x803f4528, 40))          # DAT_803f4528: nivel de basura por etapa
dirs = struct.unpack('>16i', rd(0x803f3218, 64))              # DAT_803f3218: 8 pares (dx,dy)
# constantes r2 (FUN_800ab6b0 / FUN_800aa5cc / FUN_800acb70 / FUN_8005e924)
consts = [f2(-0x7a74), f2(-0x7a70), f2(-0x7a68), f2(-0x7a58), f2(-0x7a54), f2(-0x7a50), f2(-0x7a4c), f2(-0x7a48), f2(-0x7a44), f2(-0x7e0c), f2(-0x7e08), f2(-0x7a6c)]
# nombres de anim: 14 x 0x80 bytes en 0x803fc914
names = []
for i in range(14):
    b = rd(0x803fc914 + i * 0x80 + 4, 0x7c); names.append(b.split(b'\0')[0].decode('ascii', 'replace'))
# n.o de banderas por etapa (FUN_800b45dc, individual): 0->0, 1->1, 2..4->2, resto->3
flagCount = [0, 1, 2, 2, 2, 3, 3, 3, 3, 3]
# tipo de bandera -> (x base, n valores): 0 -> rand%3, 1 -> 3+rand&1, 2 -> 5+rand&1, 3 -> 7+rand%3 ; y = 5 + rand%10 (+0x14)
ftype = [(0, 3), (3, 2), (5, 2), (7, 3)]
out = b'CLMB' + struct.pack('<I', 1) + struct.pack('<10i', *garbage) + struct.pack('<10B', *flagCount) + struct.pack('<16i', *dirs)
out += struct.pack('<12f', *consts) + b''.join(struct.pack('<2B', *t) for t in ftype)
out += struct.pack('<4i', 5, 0x27, 0x16, 5)  # inicio x, inicio y, fila meta (y < 0x16), fila base de bandera (5 + 0x14 se suma en juego)
out += struct.pack('<B', len(names)) + b''.join(n.encode().ljust(40, b'\0') for n in names)
open(OUT, 'wb').write(out)
print(OUT, len(out), 'bytes'); print('garbage', garbage); print('dirs', dirs); print('consts', consts); print(names)

#!/usr/bin/env python3
"""Extrae las 30 fases de Bombliss de main.dol -> data/bombliss.bin (30 x 200 bytes, fila 0 arriba, 10 columnas).
Mapa DAT_80469920 (fase -> patron) y patrones DAT_80469940. Valores: 0 vacio, 1..0x14 color, 0x15 bomba, 0x16 bomba grande (ancla), 0x17 parte de bomba grande."""
import struct, sys
dol = open(sys.argv[1], 'rb').read()
offs = struct.unpack('>18I', dol[0:72]); addrs = struct.unpack('>18I', dol[72:144]); sizes = struct.unpack('>18I', dol[144:216])
def rd(a, n):
    for o, ad, s in zip(offs, addrs, sizes):
        if s and ad <= a < ad + s: return dol[o + a - ad:o + a - ad + n]
m = rd(0x80469920, 30); p = rd(0x80469940, 6000)
open(sys.argv[2], 'wb').write(b''.join(p[k * 200:(k + 1) * 200] for k in m))

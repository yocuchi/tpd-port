#!/usr/bin/env python3
# extract_drop.py <main.dol> <salida drop.bin>
# Datos del Corredor de etapas (CFlowDropScrollGame / FUN_80062e78) extraídos del main.dol:
#   0x80412698  obstáculos u16 [curso(3)*0xc80 + patrón(40)*0x50 + fila(40)*2], bit i = columna i
#   0x803f60a8  tabla de etapas 10 x 3 bytes (parámetro curso 0, parámetro curso 1, espejo)
# Formato drop.bin (little endian): "DROP", u32 3*40*40, u16[4800] obstáculos, u32 10, u8[30] tabla de etapas.
import struct, sys
dol = open(sys.argv[1], 'rb').read()
offs = struct.unpack('>18I', dol[0:72]); addrs = struct.unpack('>18I', dol[72:144]); sizes = struct.unpack('>18I', dol[144:216])
def v2o(a):
    for o, ad, s in zip(offs, addrs, sizes):
        if s and ad <= a < ad + s: return o + a - ad
    raise ValueError(hex(a))
o = v2o(0x80412698); obs = struct.unpack('>4800H', dol[o:o + 9600])
t = v2o(0x803f60a8); stages = dol[t:t + 30]
out = b'DROP' + struct.pack('<I', 4800) + struct.pack('<4800H', *obs) + struct.pack('<I', 10) + bytes(stages)
open(sys.argv[2], 'wb').write(out)
print('obstáculos', len(obs), 'etapas', list(stages))
if len(sys.argv) > 3:  # vista previa: python3 extract_drop.py dol out curso patrón
    c, p = int(sys.argv[3]), int(sys.argv[4])
    for r in range(40):
        v = obs[c * 1600 + p * 40 + r]; print('%2d ' % r + ''.join('#' if v >> i & 1 else '.' for i in range(10)) + '  %04x' % v)

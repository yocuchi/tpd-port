#!/usr/bin/env python3
"""Genera los recursos del port a partir de TU copia del juego (volcado del disco de Wii).

Uso:  tools/extract_assets.py "/ruta/al/volcado/DATA" [idioma]

  DATA    carpeta que deja Dolphin al extraer el disco (contiene sys/main.dol y files/...)
  idioma  carpeta de files/lng (es, en, de, fr, it, nl, uk, uses, usfr); por defecto "es"

Crea:
  assets_src/   layouts, texturas, fuentes, textos y sonido (copias tal cual del disco)
  data/         reglas de cada modo (TPF2) y tablas de los modos especiales, sacadas de main.dol
  romfs/        lo mismo, en la estructura que usa el NSP (romfs:/assets y romfs:/data)

Si existe dedicatoria.txt en la raíz del proyecto se copia también (pantalla opcional al arrancar).
No modifica ni redistribuye nada: todo sale de la copia que tú ya tienes.
"""
import os, shutil, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# Ficheros que se copian del disco (origen relativo a DATA/files -> nombre en assets_src)
FILES = {
    'theme/bg_%02d.arc' % i: 'bg_%02d.arc' % i for i in range(16)
}
FILES.update({
    'theme/blocks00.brres': 'blocks00.brres',
    'game/climber.arc': 'climber.arc', 'game/config_01.arc': 'config_01.arc', 'game/effect.arc': 'effect.arc',
    'game/frame00_01.arc': 'frame00_01.arc', 'game/frame00_02.arc': 'frame00_02.arc', 'game/frame01_01.arc': 'frame01_01.arc',
    'game/item.arc': 'item.arc', 'game/telop.arc': 'telop.arc', 'game/telop_lang.arc': 'telop_lang.arc',
    'menu/menu.arc': 'menu.arc', 'title/title.arc': 'title.arc', 'system/window.arc': 'window.arc',
    'system/tetrisfont00.brfna': 'tetrisfont00.brfna', 'system/tetrisfont01.brfna': 'tetrisfont01.brfna', 'system/tetrisfont02.brfna': 'tetrisfont02.brfna',
    'sound/SD_TETRIS_DEFAULT_ALL.brsar': 'sound.brsar',
})

# Reglas TPF2 incrustadas en main.dol: (dirección virtual, tamaño). Son contiguas en .data.
TPF2 = [
    (0x8044aa58, 8480), (0x8044cb78, 14640), (0x804504a8, 8544), (0x80452608, 8544), (0x80454768, 19392),
    (0x80459328, 8624), (0x8045b4d8, 16480), (0x8045f538, 11072), (0x80462078, 8544), (0x804641d8, 11568),
    (0x80466f08, 389368),
]


class Dol:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        self.offs = struct.unpack('>18I', self.d[0:72]); self.addrs = struct.unpack('>18I', self.d[72:144]); self.sizes = struct.unpack('>18I', self.d[144:216])

    def read(self, va, n):
        for o, a, s in zip(self.offs, self.addrs, self.sizes):
            if s and a <= va < a + s: return self.d[o + va - a:o + va - a + n]
        raise ValueError('dirección fuera del DOL: %08x' % va)


def main():
    if len(sys.argv) < 2: print(__doc__); sys.exit(1)
    data = sys.argv[1]; lang = sys.argv[2] if len(sys.argv) > 2 else 'es'
    files = os.path.join(data, 'files'); dol_path = os.path.join(data, 'sys', 'main.dol')
    if not os.path.isfile(dol_path): sys.exit('no encuentro %s: pasa la carpeta DATA del volcado' % dol_path)
    assets = os.path.join(ROOT, 'assets_src'); rules = os.path.join(ROOT, 'data')
    os.makedirs(os.path.join(assets, 'mess'), exist_ok=True); os.makedirs(rules, exist_ok=True)

    for src, dst in FILES.items():
        shutil.copyfile(os.path.join(files, src), os.path.join(assets, dst))
    mess = os.path.join(files, 'lng', lang, 'mess')
    if not os.path.isdir(mess): sys.exit('no existe el idioma %s (%s)' % (lang, mess))
    for f in sorted(os.listdir(mess)):
        if f.endswith('.bin'): shutil.copyfile(os.path.join(mess, f), os.path.join(assets, 'mess', f))
    print('recursos copiados: %d ficheros + textos (%s)' % (len(FILES), lang))

    dol = Dol(dol_path)
    for va, size in TPF2:
        blob = dol.read(va, size)
        assert blob[:8] == b'tpf2data', 'TPF2 inesperado en %08x' % va
        open(os.path.join(rules, '%08x.tpf2' % va), 'wb').write(blob)
    # Modo Sombra: etapa->silueta, presupuesto de piezas y 50 siluetas de 20x10
    open(os.path.join(rules, 'shadow.bin'), 'wb').write(dol.read(0x803f7800, 50) + dol.read(0x803f7834, 50) + dol.read(0x803f7868, 50 * 200))
    for script, out in (('extract_bombliss.py', 'bombliss.bin'), ('extract_climber.py', 'climber.bin'), ('extract_drop.py', 'drop.bin')):
        subprocess.check_call([sys.executable, os.path.join(HERE, script), dol_path, os.path.join(rules, out)], stdout=subprocess.DEVNULL)
    print('reglas extraídas: %d TPF2 + shadow/bombliss/climber/drop' % len(TPF2))

    ded = os.path.join(ROOT, 'dedicatoria.txt')
    if os.path.isfile(ded): shutil.copyfile(ded, os.path.join(assets, 'dedicatoria.txt'))

    romfs = os.path.join(ROOT, 'romfs')
    shutil.rmtree(romfs, ignore_errors=True)
    shutil.copytree(assets, os.path.join(romfs, 'assets')); shutil.copytree(rules, os.path.join(romfs, 'data'))
    print('romfs/ listo. Ya puedes compilar: ./build_mac.sh, make (NRO) o ./make_nsp.sh')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
# bmp2png.py a.bmp [b.bmp ...]  -> a.png ...   (necesita Pillow: pip install pillow)
import sys
from PIL import Image
for p in sys.argv[1:]:
    Image.open(p).convert('RGB').save(p.rsplit('.', 1)[0] + '.png', optimize=True); print(p)

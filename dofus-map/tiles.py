#!/usr/bin/env python3
import os, json

base = 'tiles'
index = {}

for z in os.listdir(base):
    pz = os.path.join(base, z)
    if not os.path.isdir(pz): continue
    zi = {}
    for x in os.listdir(pz):
        px = os.path.join(pz, x)
        if not os.path.isdir(px): continue
        ys = []
        for fn in os.listdir(px):
            if fn.endswith('.png'):
                try:
                    y = int(fn[:-4])
                    ys.append(y)
                except:
                    pass
        if ys:
            zi[int(x)] = sorted(ys)
    if zi:
        index[int(z)] = zi

with open('tile-index.json','w') as f:
    json.dump(index, f, separators=(',',':'))

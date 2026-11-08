#!/usr/bin/env python3
"""Builds MetalSonic.bin and MetalSaveSelect.bin from the Metal Knuckles mod's Sonic.bin / SaveSelect.bin.
Usage: python3 make_assets.py <path to 'Data/Sprites'> <output dir>"""
import struct, sys, shutil, os

def rs(f):
    n = f.read(1)[0]; return f.read(n).rstrip(b'\0').decode('latin1')
def ws(s):
    b = s.encode('latin1') + b'\0'; return bytes([len(b)]) + b

def parse(path):
    f = open(path, 'rb'); f.read(4); f.read(4)
    sheets = [rs(f) for _ in range(f.read(1)[0])]
    hit = [rs(f) for _ in range(f.read(1)[0])]
    anims = []
    for _ in range(struct.unpack('<H', f.read(2))[0]):
        name = rs(f); fc, speed = struct.unpack('<Hh', f.read(4)); loop, rot = f.read(2)
        frames = []
        for _ in range(fc):
            sh = f.read(1)[0]; vals = struct.unpack('<HHHHHHhh', f.read(16)); hb = f.read(8 * len(hit))
            frames.append((sh, vals, hb))
        anims.append([name, speed, loop, rot, frames])
    return sheets, hit, anims

def write(path, sheets, hit, anims):
    total = sum(len(a[4]) for a in anims)
    out = b'SPR\0' + struct.pack('<I', total) + bytes([len(sheets)]) + b''.join(map(ws, sheets))
    out += bytes([len(hit)]) + b''.join(map(ws, hit)) + struct.pack('<H', len(anims))
    for name, speed, loop, rot, frames in anims:
        out += ws(name) + struct.pack('<Hh', len(frames), speed) + bytes([loop, rot])
        for sh, vals, hb in frames:
            out += bytes([sh]) + struct.pack('<HHHHHHhh', *vals) + hb
    open(path, 'wb').write(out)

src, dst = sys.argv[1], sys.argv[2]
os.makedirs(dst, exist_ok=True)

# --- player sprites ---
sheets, hit, anims = parse(f"{src}/Players/Sonic.bin")
for i, s in enumerate(sheets):
    base = s.split('/')[-1]; n = base.lower().replace('sonic', '')
    shutil.copy(f"{src}/Players/{base}", f"{dst}/MetalSonic{n}")
sheets = ["Players/MetalSonic" + s.split('/')[-1].lower().replace('sonic', '') for s in sheets]
fly = anims[48]
names = ["Fly Tired", "Fly Lift", "Swim", "Swim Tired", "Swim Lift", "Ability 7"]
while len(anims) < 55:
    anims.append([names[len(anims) - 49], fly[1], fly[2], fly[3], list(fly[4])])
for i in range(49, 55):      # no empty animations: fly/swim reuse the Metal Sonic boost pose
    anims[i][1:] = [fly[1], fly[2], fly[3], list(fly[4])]
write(f"{dst}/MetalSonic.bin", sheets, hit, anims)

# --- save slot card: full copy of the SaveSelect layout, sheet 0 renamed so vanilla art is untouched ---
s2, h2, a2 = parse(f"{src}/UI/SaveSelect.bin")
sheet = s2[0].split('/')[-1]
for cand in os.listdir(f"{src}/UI"):
    if cand.lower() == sheet.lower(): shutil.copy(f"{src}/UI/{cand}", f"{dst}/MetalSaveSelect.gif")
s2[0] = "UI/MetalSaveSelect.gif"
write(f"{dst}/MetalSaveSelect.bin", s2, h2, a2)
print("card anims", len(a2), "sheets", s2)

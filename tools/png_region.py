#!/usr/bin/env python3
"""Hash a rectangle of a runner screenshot (RGB PNG), to find the frame where a
HUD element changes. Usage: png_region.py x0 y0 x1 y1 files..."""
import struct, sys, zlib, hashlib

def rows(path):
    d = open(path, "rb").read(); i = 8; idat = b""
    while i < len(d):
        n, t = struct.unpack(">I4s", d[i:i + 8])
        if t == b"IHDR": w, h = struct.unpack(">II", d[i + 8:i + 16])
        if t == b"IDAT": idat += d[i + 8:i + 8 + n]
        i += 12 + n
    raw = zlib.decompress(idat); stride = w * 3 + 1; out = []; prev = bytearray(w * 3)
    for y in range(h):
        f = raw[y * stride]; line = bytearray(raw[y * stride + 1:(y + 1) * stride])
        for x in range(len(line)):
            a = line[x - 3] if x >= 3 else 0; b = prev[x]; c = prev[x - 3] if x >= 3 else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out.append(bytes(line)); prev = line
    return out

if __name__ == "__main__":
    x0, y0, x1, y1 = map(int, sys.argv[1:5]); last = None
    for p in sys.argv[5:]:
        r = rows(p); h = hashlib.md5(b"".join(l[x0 * 3:x1 * 3] for l in r[y0:y1])).hexdigest()[:8]
        if h != last: print(p, h); last = h

#!/usr/bin/env python3
"""Enlarge a runner screenshot by a whole number with hard pixel edges.
Usage: png_scale.py in.png out.png FACTOR"""
import struct, sys, zlib
sys.path.insert(0, __file__.rsplit("/", 1)[0])
from png_region import rows

src, dst, k = sys.argv[1], sys.argv[2], int(sys.argv[3])
r = rows(src); h = len(r); w = len(r[0]) // 3
raw = b"".join(b"\0" + b"".join(line[x * 3:x * 3 + 3] * k for x in range(w)) * 1 for line in r for _ in range(k))
def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w * k, h * k, 8, 2, 0, 0, 0)) \
    + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
open(dst, "wb").write(png)

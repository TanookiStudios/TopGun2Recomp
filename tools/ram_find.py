#!/usr/bin/env python3
"""Search a --ram-trace file (2048 bytes per frame) for RAM bytes that behave a
given way. Used to build notes/ram-map.md; not part of the game build."""
import sys

def load(path):
    d = open(path, "rb").read()
    return [d[i:i + 2048] for i in range(0, len(d), 2048)]

def series(frames, addr, lo=0, hi=None):
    return [f[addr] for f in frames[lo:hi]]

def changes(frames, addr, lo=0, hi=None):
    """(frame, old, new) for every change of addr in [lo, hi)."""
    out, prev = [], frames[lo][addr]
    for i in range(lo + 1, hi or len(frames)):
        v = frames[i][addr]
        if v != prev:
            out.append((i, prev, v))
        prev = v
    return out

if __name__ == "__main__":
    frames = load(sys.argv[1])
    print(len(frames), "frames")

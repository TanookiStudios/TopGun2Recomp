#!/usr/bin/env python3
"""From a --trace-out file, list PPU VRAM writes ($2006 address, $2007 data)
with the instruction that made each one. Usage: vram_writes.py trace.txt"""
import sys
pc = None; latch = 0; hi = 0; addr = 0; inc = 1
for line in open(sys.argv[1]):
    p = line.split()
    if p[1] == "INSN": pc = p[2][3:]; continue
    if p[1] != "W": continue
    a, v = int(p[2], 16), int(p[3], 16)
    if a == 0x2000: inc = 32 if v & 4 else 1
    elif a == 0x2006:
        if latch == 0: hi = v & 0x3F
        else: addr = (hi << 8) | v
        latch ^= 1
    elif a == 0x2002: latch = 0
    elif a == 0x2007:
        print(f"PC={pc} VRAM ${addr:04X} <- {v:02X}"); addr = (addr + inc) & 0x3FFF

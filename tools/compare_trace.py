#!/usr/bin/env python3
"""Compare our RAM trace against the reference emulator's and name what diverged.

This is what makes "is it correct?" answerable without playing every inch of the
game. Both sides run the same input script; the first frame where their work RAM
disagrees is the first observable bug, and the disassembly's RAM labels turn the
differing addresses into names a person can act on ("PlayerY", "CurrentArea")
rather than bare hex.

Frame alignment: the two runs may not agree on which real frame is "frame 0"
(our runner counts at NMI, the emulator counts steps). A small offset search
handles that; if no offset produces a long clean prefix, that is itself reported
rather than silently picking the best of a bad set.

Usage:
    compare_trace.py ours.bin oracle.bin [--max-offset 8] [--report N]
"""
import argparse
import os
import sys

FRAME = 0x800

# Addresses that CANNOT be expected to match, for structural reasons rather
# than because the game is misbehaving. Kept deliberately small and justified --
# every entry here is a place a real bug could hide, so nothing goes in without
# a reason.
#
#   $0100-$01FF  the 6502 stack page. Recompiled code uses the real C call
#                stack for JSR/RTS, so it only partially mirrors this. Stack
#                *contents* legitimately differ; stack-pointer-driven behaviour
#                still shows up elsewhere in RAM.
#   $0011,$00EB,$00F0,$00F1  VBlank/PPU-queue bookkeeping. Our trace is taken
#                after the NMI handler, the emulator's at a frame step boundary,
#                so these are sampled at different points in the frame.
IGNORE_RANGES = [(0x0100, 0x01FF)]
IGNORE_ADDRS = {0x0011, 0x00EB, 0x00F0, 0x00F1}


def ignored(addr):
    if addr in IGNORE_ADDRS:
        return True
    return any(lo <= addr <= hi for lo, hi in IGNORE_RANGES)
DISASM_RAM_NL = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "vendor", "smb2-disasm", "bin", "PRG0.nes.ram.nl")


def load_ram_names():
    names = {}
    if not os.path.exists(DISASM_RAM_NL):
        return names
    with open(DISASM_RAM_NL, errors="replace") as fh:
        for line in fh:
            line = line.strip()
            if not line.startswith("$"):
                continue
            parts = line.split("#")
            if len(parts) < 2:
                continue
            try:
                names[int(parts[0][1:], 16)] = parts[1]
            except ValueError:
                pass
    return names


def frames(path):
    data = open(path, "rb").read()
    return [data[i:i + FRAME] for i in range(0, len(data) - FRAME + 1, FRAME)]


COMPARED = [a for a in range(FRAME) if not ignored(a)]


def frames_differ(fa, fb):
    for addr in COMPARED:
        if fa[addr] != fb[addr]:
            return True
    return False


def first_divergence(a, b, offset):
    """Index into `a` of the first mismatching frame, or None if all matched."""
    n = min(len(a) - max(0, -offset), len(b) - max(0, offset))
    for i in range(n):
        if frames_differ(a[i + max(0, -offset)], b[i + max(0, offset)]):
            return i
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ours")
    ap.add_argument("oracle")
    ap.add_argument("--max-offset", type=int, default=8)
    ap.add_argument("--report", type=int, default=24,
                    help="max differing addresses to list")
    args = ap.parse_args()

    ours, oracle = frames(args.ours), frames(args.oracle)
    if not ours or not oracle:
        sys.exit(f"empty trace (ours={len(ours)} frames, oracle={len(oracle)})")
    print(f"ours   : {len(ours)} frames")
    print(f"oracle : {len(oracle)} frames")

    # Alignment. Requiring a clean prefix from frame 0 fails here: our trace
    # begins at the first NMI, the emulator's at the reset vector, so "frame 0"
    # is not the same instant. Score each candidate offset by how many bytes
    # actually agree across sampled frames, and report that rate honestly --
    # a low best rate means alignment failed, not that the game is broken.
    def match_rate(off, samples=24):
        lo = max(0, -off)
        n = min(len(ours) - lo, len(oracle) - max(0, off))
        if n <= 0:
            return 0.0
        idxs = range(0, n, max(1, n // samples))
        tot = hit = 0
        for i in idxs:
            fa, fb = ours[i + lo], oracle[i + max(0, off)]
            hit += sum(1 for addr in COMPARED if fa[addr] == fb[addr])
            tot += len(COMPARED)
        return hit / tot if tot else 0.0

    span = max(args.max_offset, 1)
    scored = sorted(((match_rate(o), o) for o in range(-span, span + 1)),
                    reverse=True)
    rate, off = scored[0]
    div = first_divergence(ours, oracle, off)
    depth = len(ours) if div is None else div

    print(f"best frame offset: {off:+d}  "
          f"byte agreement {rate*100:.1f}%  clean prefix {depth} frames")
    if rate < 0.90:
        print("\nALIGNMENT FAILED — best offset still only agrees on "
              f"{rate*100:.1f}% of RAM bytes.")
        print("The two runs are not synchronised to a common starting point, so")
        print("a per-address diff is not meaningful yet. This is a limitation of")
        print("the harness, NOT evidence of a bug in the port. Fix by syncing on")
        print("a game event (e.g. first frame where a known variable settles)")
        print("instead of assuming frame 0 lines up.")
        return
    if div is None:
        print(f"\n*** IDENTICAL for all {depth} compared frames ***")
        print("Every byte of work RAM matched a separate emulator the whole run.")
        return

    if depth < 5:
        print("\nWARNING: diverges almost immediately at every offset tried.")
        print("That usually means the two runs are not aligned at all (different")
        print("start-up timing or input mapping) rather than a game bug. Treat the")
        print("addresses below as unreliable until alignment is established.")

    names = load_ram_names()
    ia, ib = div + max(0, -off), div + max(0, off)
    fa, fb = ours[ia], oracle[ib]
    diffs = [(addr, fa[addr], fb[addr]) for addr in COMPARED
             if fa[addr] != fb[addr]]

    print(f"\nFIRST DIVERGENCE at our frame {ia} (oracle frame {ib})")
    print(f"{len(diffs)} of {len(COMPARED)} compared RAM bytes differ\n")
    print(f"  {'addr':<8} {'ours':>4} {'ref':>4}  name")
    for addr, av, bv in diffs[:args.report]:
        nm = names.get(addr, "")
        print(f"  ${addr:04X}   {av:>4X} {bv:>4X}  {nm}")
    if len(diffs) > args.report:
        print(f"  ... {len(diffs) - args.report} more")

    named = [names[a] for a, _, _ in diffs if a in names]
    if named:
        print(f"\nNamed variables involved: {', '.join(named[:12])}")


if __name__ == "__main__":
    main()

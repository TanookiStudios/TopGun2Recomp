#!/usr/bin/env python3
"""Run the ROM in an INDEPENDENT emulator and record RAM every frame.

This is the reference ("oracle") half of differential testing. The other half is
our recompiled build, which dumps the same trace via RECOMP_RAM_TRACE in
extras.c. Comparing the two finds bugs automatically, without a human playing
through the game looking for them.

The emulator is nes-py, whose core is unrelated to NESRecomp's code generator --
that independence is the whole point. The runner's own interpreter tier shares
its decode table with the recompiler, so it cannot serve this role: a shared bug
would be invisible to it.

Input comes from the same script format the runner uses (input_script.h), so one
script drives both sides identically.

Usage:
    oracle_trace.py <rom.nes> <script> <out.bin> [--frames N]

Output is a flat binary: 2048 bytes of RAM per frame, frame 0 first.
"""
import argparse
import sys

BUTTON_BITS = {   # nes-py action byte layout
    "RIGHT": 0x80, "LEFT": 0x40, "DOWN": 0x20, "UP": 0x10,
    "START": 0x08, "SELECT": 0x04, "B": 0x02, "A": 0x01,
}


def parse_script(path):
    """Expand a runner input script into a per-frame list of action bytes."""
    frames = []
    held = 0
    exit_frame = None
    with open(path) as fh:
        for raw in fh:
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split()
            op = parts[0].upper()
            if op == "WAIT" and len(parts) > 1:
                for _ in range(int(parts[1])):
                    frames.append(held)
            elif op == "HOLD" and len(parts) > 1:
                held |= BUTTON_BITS.get(parts[1].upper(), 0)
            elif op == "RELEASE" and len(parts) > 1:
                held &= ~BUTTON_BITS.get(parts[1].upper(), 0)
            elif op == "EXIT":
                exit_frame = len(frames)
                break
            # SCREENSHOT / LOG / TURBO have no effect on emulation
    return frames, exit_frame


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom")
    ap.add_argument("script")
    ap.add_argument("out")
    ap.add_argument("--frames", type=int, default=0,
                    help="cap frames (0 = whatever the script implies)")
    args = ap.parse_args()

    try:
        from nes_py import NESEnv
    except ImportError:
        sys.exit("nes-py not installed. Create a venv and: pip install nes-py")

    actions, _ = parse_script(args.script)
    if args.frames:
        if len(actions) < args.frames:
            actions += [actions[-1] if actions else 0] * (args.frames - len(actions))
        actions = actions[:args.frames]
    if not actions:
        sys.exit("script produced no frames")

    env = NESEnv(args.rom)
    env.reset()
    with open(args.out, "wb") as out:
        for act in actions:
            env.step(act)
            out.write(bytes(env.ram))
    env.close()
    print(f"oracle: wrote {len(actions)} frames x 2048 bytes -> {args.out}")


if __name__ == "__main__":
    main()

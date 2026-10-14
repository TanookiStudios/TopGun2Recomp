#!/usr/bin/env python3
"""Render reference screenshots from the independent emulator.

Pairs with our runner's SCREENSHOT script command so the same moment can be
compared side by side. When our build and the reference disagree visually, this
turns "does that look wrong to you?" into a definite answer.

Usage:
    oracle_shots.py <rom.nes> <script> <outdir> [--every 60] [--frames N]
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from oracle_trace import parse_script  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom")
    ap.add_argument("script")
    ap.add_argument("outdir")
    ap.add_argument("--every", type=int, default=60)
    ap.add_argument("--frames", type=int, default=0)
    args = ap.parse_args()

    try:
        from nes_py import NESEnv
        from PIL import Image
    except ImportError as e:
        sys.exit(f"missing dependency: {e}. pip install nes-py pillow")

    actions, _ = parse_script(args.script)
    if args.frames:
        if len(actions) < args.frames:
            actions += [actions[-1] if actions else 0] * (args.frames - len(actions))
        actions = actions[:args.frames]
    os.makedirs(args.outdir, exist_ok=True)

    env = NESEnv(args.rom)
    env.reset()
    n = 0
    for i, act in enumerate(actions, start=1):
        env.step(act)
        if i % args.every == 0:
            Image.fromarray(env.screen).save(
                os.path.join(args.outdir, f"O{i:05d}.png"))
            n += 1
    env.close()
    print(f"oracle: {len(actions)} frames, wrote {n} screenshots to {args.outdir}")


if __name__ == "__main__":
    main()

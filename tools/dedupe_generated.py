#!/usr/bin/env python3
"""Remove duplicate function definitions from NESRecomp's generated C.

WORKAROUND for an upstream bug, not a fix. When a bank's generated code
crosses BANK_PART_BUDGET (1 MiB) and gets sub-sharded across several part
TUs, secondary-entry *wrappers* -- the small functions that forward into a
multi-entry `func_XXXX_body(n)` -- are emitted into more than one part. The
result is a redefinition error at compile time.

For SMB2 this hits 6 functions in bank 1 (e.g. func_83B0_b1 / InitializeEnemy,
which is entry 1 of the body at $8311).

Safety condition: every copy must be byte-identical. If it is, keeping exactly
one is semantically exact. This script REFUSES to touch anything when copies
differ, rather than silently picking one.
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

DEF_RE = re.compile(r"^void (func_[A-Za-z0-9_]+)\(void\) \{", re.M)


def extract_defs(text):
    """Yield (name, start, end) for each top-level function definition.

    Brace counting is unreliable here: the emitter interleaves #ifdef blocks
    and string literals containing braces. The emitter does, however, always
    open a definition with `void func_X(void) {` at column 0 and close it with
    a bare `}` at column 0, so scan lines for that instead.
    """
    lines = text.splitlines(keepends=True)
    starts = []  # (line_index, char_offset, name)
    off = 0
    offsets = []
    for ln in lines:
        offsets.append(off)
        off += len(ln)
    for i, ln in enumerate(lines):
        m = re.match(r"^void (func_[A-Za-z0-9_]+)\(void\) \{", ln)
        if m:
            starts.append((i, offsets[i], m.group(1)))

    out = []
    for idx, (li, start_off, name) in enumerate(starts):
        end_li = None
        for j in range(li + 1, len(lines)):
            if lines[j].rstrip("\r\n") == "}":
                end_li = j
                break
            # Safety: never run past the next definition.
            if re.match(r"^void func_[A-Za-z0-9_]+\(void\) \{", lines[j]):
                break
        if end_li is None:
            continue
        end_off = offsets[end_li] + len(lines[end_li])
        out.append((name, start_off, end_off))
    return out


def main():
    gen = Path(sys.argv[1] if len(sys.argv) > 1 else "work/generated")
    files = sorted(gen.glob("smb2_full_bank*.c"))
    if not files:
        sys.exit(f"no generated part files under {gen}")

    # name -> list of (file, start, end, body_text)
    occurrences = defaultdict(list)
    contents = {}
    for f in files:
        text = f.read_text(errors="replace")
        contents[f] = text
        for name, s, e in extract_defs(text):
            occurrences[name].append((f, s, e, text[s:e]))

    dupes = {n: v for n, v in occurrences.items() if len(v) > 1}
    if not dupes:
        print("no duplicate definitions -- nothing to do")
        return

    print(f"{len(dupes)} function(s) defined more than once:")
    conflicting = []
    for name, occ in sorted(dupes.items()):
        bodies = {o[3] for o in occ}
        status = "identical" if len(bodies) == 1 else f"!! {len(bodies)} DIFFERENT versions"
        print(f"  {name:24s} x{len(occ)}  {status}")
        for f, s, _, _ in occ:
            print(f"       {f.name}")
        if len(bodies) != 1:
            conflicting.append(name)

    if conflicting:
        sys.exit(
            "\nREFUSING to dedupe: copies are not byte-identical for "
            + ", ".join(conflicting)
            + ".\nRemoving one would change behaviour. Investigate instead."
        )

    # Keep the first occurrence (in sorted file order); comment out the rest.
    removals = defaultdict(list)
    for name, occ in dupes.items():
        for f, s, e, _ in occ[1:]:
            removals[f].append((s, e, name))

    for f, spans in removals.items():
        text = contents[f]
        for s, e, name in sorted(spans, reverse=True):
            note = (f"/* [dedupe] duplicate definition of {name} removed -- "
                    f"kept the copy in an earlier part TU. Upstream emits "
                    f"secondary-entry wrappers once per sub-shard. */\n")
            text = text[:s] + note + text[e:]
        f.write_text(text)
        print(f"\nrewrote {f.name}: removed {len(spans)} duplicate definition(s)")

    total = sum(len(v) for v in removals.values())
    print(f"\nremoved {total} duplicate definition(s); all copies were identical")


if __name__ == "__main__":
    main()

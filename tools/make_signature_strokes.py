#!/usr/bin/env python3
"""Turns assets/signature.svg (paths made of M and C, one dot) into pen strokes for the Esc menu's About page.
Usage: tools/make_signature_strokes.py > src/<game>_signature.h
Applies the SVG group's own rotate(-4 160 66) skewX(-8), samples each curve into short lines, and trims to the ink."""
import re, math, sys, os
here = os.path.dirname(os.path.abspath(__file__))
svg = open(os.path.join(here, "..", "assets", "signature.svg")).read()
vb = re.search(r'viewBox="0 0 (\d+) (\d+)"', svg); W, H = map(float, vb.groups())
def xf(x, y):
    x = x + math.tan(math.radians(-8)) * y          # skewX first, then the rotation
    a = math.radians(-4); cx, cy = 160, 66
    dx, dy = x - cx, y - cy
    return (cx + dx * math.cos(a) - dy * math.sin(a), cy + dx * math.sin(a) + dy * math.cos(a))
def bez(p0, p1, p2, p3, n=14):
    return [((1-t)**3*p0[0]+3*(1-t)**2*t*p1[0]+3*(1-t)*t*t*p2[0]+t**3*p3[0],
             (1-t)**3*p0[1]+3*(1-t)**2*t*p1[1]+3*(1-t)*t*t*p2[1]+t**3*p3[1]) for t in [i/n for i in range(1, n+1)]]
strokes = []
for d in re.findall(r'<path d="([^"]+)"', svg):
    tok = re.findall(r'[MC]|-?\d+\.?\d*', d); i = 0; pts = []; cur = None
    while i < len(tok):
        if tok[i] == "M": cur = (float(tok[i+1]), float(tok[i+2])); pts = [cur]; i += 3
        else:
            i += 1
            while i < len(tok) and tok[i] not in "MC":
                p1 = (float(tok[i]), float(tok[i+1])); p2 = (float(tok[i+2]), float(tok[i+3])); p3 = (float(tok[i+4]), float(tok[i+5]))
                pts += bez(cur, p1, p2, p3); cur = p3; i += 6
    strokes.append(pts)
m = re.search(r'<circle cx="([\d.]+)" cy="([\d.]+)" r="([\d.]+)"', svg)
if m:
    cx, cy, r = map(float, m.groups())
    strokes.append([(cx + r*math.cos(2*math.pi*k/8), cy + r*math.sin(2*math.pi*k/8)) for k in range(9)])
strokes = [[xf(x, y) for x, y in s] for s in strokes]
xs = [p[0] for s in strokes for p in s]; ys = [p[1] for s in strokes for p in s]
x0, y0, x1, y1 = min(xs)-3, min(ys)-3, max(xs)+3, max(ys)+3
flat = []
for s in strokes:
    flat.append(float(len(s)))
    for x, y in s: flat += [round(x-x0, 1), round(y-y0, 1)]
flat.append(0.0)
toks = ["%g" % v for v in flat]; lines = []; cur = ""
for t in toks:
    if len(cur) + len(t) > 100: lines.append(cur); cur = ""
    cur += t + ","
lines.append(cur)
sys.stdout.write("/* Maddie's signature as pen strokes (from assets/signature.svg, made by tools/make_signature_strokes.py):\n"
                 " * per stroke a point count then x,y pairs; ends with a count of 0. */\n"
                 "static const float kMaddieSignatureData[] = {\n    " + "\n    ".join(lines) + "\n};\n"
                 "#define MADDIE_SIGNATURE_W %gf\n#define MADDIE_SIGNATURE_H %gf\n" % (round(x1-x0, 1), round(y1-y0, 1)))

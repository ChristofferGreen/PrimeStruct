#!/usr/bin/env python3
"""split_plain.py SRC SUFFIX:LINE...

Splits a .cpp whose body is one namespace of plain definitions (no anonymous
namespace, no file-static helpers) into SRC plus SRC<Suffix>.cpp parts. Each part
repeats the prologue (everything through the namespace opening line) and the
closing namespace line. Cut lines must start a top-level item (column 0) after a
blank or comment line; they are moved to the nearest such line at or before the
requested one. Registers nothing: run register_split.py afterwards.
"""
import os
import re
import sys

src = sys.argv[1]
specs = [(a.split(':')[0], int(a.split(':')[1])) for a in sys.argv[2:]]
lines = open(src).read().split('\n')
if lines and lines[-1] == '':
    lines.pop()
open_idx = next(i for i, l in enumerate(lines) if re.match(r'^namespace [\w:]+ \{\s*$', l))
close_idx = max(i for i, l in enumerate(lines) if re.match(r'^\} // namespace', l))
prologue = lines[:open_idx + 1]
closing = lines[close_idx:]


def snap(line_no):
    i = line_no - 1
    while i > open_idx + 1:
        if lines[i] and not lines[i][0].isspace() and not lines[i].startswith(('}', '#')) and \
                (not lines[i - 1].strip() or lines[i - 1].lstrip().startswith('//')):
            # walk up over a leading comment block
            while i - 1 > open_idx and lines[i - 1].lstrip().startswith('//'):
                i -= 1
            return i
        i -= 1
    raise SystemExit(f'no cut point at or before line {line_no}')


cuts = [snap(n) for _, n in specs]
bounds = [open_idx + 1] + cuts + [close_idx]
stem = os.path.splitext(src)[0]
for k, (suffix, _) in enumerate(specs):
    body = lines[bounds[k + 1]:bounds[k + 2]]
    out = prologue + [''] + body + ([''] if body and body[-1].strip() else []) + closing + ['']
    name = f'{stem}{suffix}.cpp'
    open(name, 'w').write('\n'.join(out))
    print(name, len(out))
first = lines[bounds[0]:bounds[1]]
open(src, 'w').write('\n'.join(prologue + first + ([''] if first and first[-1].strip() else []) + closing + ['']))
print(src, len(prologue + first + closing))

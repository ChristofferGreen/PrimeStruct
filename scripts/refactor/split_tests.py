#!/usr/bin/env python3
"""split_tests.py SRC.cpp MAXLINES

Splits a doctest source over MAXLINES lines into SRC plus <stem>_part2.cpp, <stem>_part3.cpp, ...
cutting only between top-level TEST_CASE blocks into equally sized parts.

- Every part repeats the prologue (everything before the first TEST_CASE); prologue helpers
  (functions, structs, usings, anonymous-namespace contents) a part does not mention are dropped
  so -Wunused-function stays quiet.
- Helpers and #defines that sit *between* test cases are carried into later parts that mention them.
- A trailing `#endif` (a file wrapped in `#if`) is repeated in every part.

Register the new files with register_split.py and run generate_test_inventory.py afterwards.
"""
import math
import os
import re
import sys

src = sys.argv[1]
max_lines = int(sys.argv[2])
lines = open(src).read().split('\n')
if lines and lines[-1] == '':
    lines.pop()
end_idx = max(i for i, l in enumerate(lines) if l.startswith('TEST_SUITE_END'))
tail = lines[end_idx + 1:]
while tail and not tail[-1].strip():
    tail.pop()


def lead(i):
    while i > 0 and lines[i - 1].lstrip().startswith('//'):
        i -= 1
    return i


case_starts = sorted({lead(i) for i, l in enumerate(lines) if l.startswith('TEST_CASE') and i < end_idx})
first = case_starts[0]
prologue = lines[:first]

DEF_START = re.compile(r'^(static |inline |constexpr |const |struct |class |enum |using |template\b|[A-Za-z_][\w:<>,\*&\s]*\s\**&?\w+\()')


def item_name(head):
    m = re.match(r'#define\s+(\w+)', head)
    if m:
        return m.group(1)
    m = re.search(r'(\w+)\s*\(', head) or re.search(r'(?:struct|class|enum|using)\s+(\w+)', head) or re.search(r'(\w+)\s*(?:=|\{|;)', head)
    return m.group(1) if m else None


def find_items(block, allow_anon=True):
    """Removable definitions in `block` (list of lines): [(start, end, name)]."""
    items, depth, anon_depth, i = [], 0, None, 0
    while i < len(block):
        line = block[i]
        if allow_anon and line.startswith('namespace {'):
            anon_depth = depth + 1
        base = 0 if anon_depth is None else anon_depth
        if line.startswith('#define'):
            j = i
            while block[j].rstrip().endswith('\\'):
                j += 1
            items.append((i, j, item_name(line)))
            i = j + 1
            continue
        if depth == base and DEF_START.match(line) and not line.startswith(('#', 'TEST_', 'namespace')):
            j, d = i, depth
            while True:
                d += block[j].count('{') - block[j].count('}')
                if d <= depth and block[j].rstrip().endswith(('}', '};', ';')):
                    break
                j += 1
                if j >= len(block):
                    j = None
                    break
            if j is not None:
                items.append((i, j, item_name(line)))
                i = j + 1
                continue
        depth += line.count('{') - line.count('}')
        if anon_depth is not None and depth < anon_depth:
            anon_depth = None
        i += 1
    return items


def close_over(items, block, text):
    keep = {n for n, (_, _, name) in enumerate(items) if name is None}
    changed = True
    while changed:
        changed = False
        for n, (s, e, name) in enumerate(items):
            if n not in keep and re.search(r'\b' + re.escape(name) + r'\b', text):
                keep.add(n)
                text += '\n' + '\n'.join(block[s:e + 1])
                changed = True
    return keep


def prune_prologue(body_text):
    items = find_items(prologue)
    keep = close_over(items, prologue, body_text)
    drop = set()
    for n, (s, e, _) in enumerate(items):
        if n not in keep:
            drop.update(range(s, e + 1))
    out = '\n'.join(l for i, l in enumerate(prologue) if i not in drop)
    out = re.sub(r'namespace \{\s*\} // namespace\s*', '', out)
    return out.split('\n')


# units: each case plus the non-case code that follows it (until the next case)
units = []
for k, s in enumerate(case_starts):
    e = case_starts[k + 1] if k + 1 < len(case_starts) else end_idx
    block = lines[s:e]
    # the case ends at the first top-level closing "}" line; the rest is between-case code
    depth, cut = 0, len(block)
    for idx, l in enumerate(block):
        depth += l.count('{') - l.count('}')
        if idx > 0 and depth == 0 and l.rstrip().endswith(('});', '}')) or (depth == 0 and idx == 0 and l.rstrip().endswith(');') and '{' not in l):
            cut = idx + 1
            break
    units.append((block[:cut], block[cut:]))

total = sum(len(c) + len(o) for c, o in units)
nparts = max(1, math.ceil((total + len(prologue) + 2) / max_lines))
budget = math.ceil(total / nparts)
parts, cur, size = [], [], 0
for u in units:
    if cur and size >= budget and len(parts) < nparts - 1:
        parts.append(cur)
        cur, size = [], 0
    cur.append(u)
    size += len(u[0]) + len(u[1])
if cur:
    parts.append(cur)

stem, ext = os.path.splitext(src)
between = [[o for _, o in part] for part in parts]
for k, part in enumerate(parts):
    own_lines = [l for c, o in part for l in c + o]
    body_text = '\n'.join(own_lines)
    # between-case helpers from earlier parts that this part mentions
    carried = []
    cand = []
    for earlier in between[:k]:
        for o in earlier:
            for s, e, name in find_items(o, allow_anon=False):
                if name is not None:
                    cand.append((name, o[s:e + 1]))
    cand_items = [(0, 0, name) for name, _ in cand]
    text = body_text
    keep, changed = set(), True
    while changed:
        changed = False
        for n, (name, blk) in enumerate(cand):
            if n not in keep and re.search(r'\b' + re.escape(name) + r'\b', text):
                keep.add(n)
                text += '\n' + '\n'.join(blk)
                changed = True
    carried = [blk for n, (_, blk) in enumerate(cand) if n in keep]
    pro = prune_prologue(body_text + '\n' + '\n'.join(l for b in carried for l in b))
    if carried:
        at = max(i for i, l in enumerate(pro) if l.startswith('TEST_SUITE_BEGIN')) + 1
        pro = pro[:at] + [''] + [l for b in carried for l in b + ['']] + pro[at:]
    body = []
    for c, o in part:
        body += c + o
    while body and not body[-1].strip():
        body.pop()
    out = pro + body + ['', 'TEST_SUITE_END();'] + tail + ['']
    name = src if k == 0 else f'{stem}_part{k + 1}{ext}'
    open(name, 'w').write('\n'.join(out))
    print(name, len(out))

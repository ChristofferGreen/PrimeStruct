#!/usr/bin/env python3
"""split_function_fragment.py SRC FUNC_REGEX FRAGMENT.h REMOVE_LINES [DEPTH]

Moves a run of statements (children at brace depth DEPTH, default 1) out of the body of one giant free function into a header
fragment that is `#include`d at the same place (the repository already builds
IrLowererLowerReturnEmitStage.cpp this way). The run starts at the child statement of the body
nearest the middle of the function and extends until at least REMOVE_LINES lines are moved.
Pure textual move: no behavior change, locals stay shared because the fragment is spliced in place.
"""
import re
import sys

src, func_regex, fragment, remove = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
child_depth = int(sys.argv[5]) if len(sys.argv) > 5 else 1
L = open(src).read().split('\n')


def strip(s):
    s = re.sub(r'"(\\.|[^"\\])*"', '""', s)
    s = re.sub(r"'(\\.|[^'\\])+'", "' '", s)
    return re.sub(r'//.*', '', s)


start = next(i for i, l in enumerate(L) if re.search(func_regex, l))
# body opening: first line at/after start whose stripped text ends with '{' at paren depth 0
depth = paren = 0
body_open = None
for i in range(start, len(L)):
    c = strip(L[i])
    for ch in c:
        if ch == '(':
            paren += 1
        elif ch == ')':
            paren -= 1
        elif ch == '{' and paren == 0 and body_open is None:
            body_open = i
    if body_open is not None:
        break
depth = 0
end = None
for i in range(body_open, len(L)):
    c = strip(L[i])
    depth += c.count('{') - c.count('}')
    if depth == 0:
        end = i
        break
# child statements of the body: starts at depth 1 with paren 0
children = []
depth = 0
paren = 0
for i in range(body_open, end + 1):
    c = strip(L[i])
    if i > body_open and depth == child_depth and paren == 0 and L[i].strip() and not L[i].lstrip().startswith(('//', '}', ')')) \
            and not re.match(r'\s*(else\b|\.|&&|\|\||\?|:)', L[i]):
        children.append(i)
    depth += c.count('{') - c.count('}')
    paren = 0 if i == body_open else paren + c.count('(') - c.count(')')
mid = (body_open + end) // 2
# choose a start near the middle so the moved run fits before the end
print('body', body_open + 1, end + 1, 'children', len(children))
cands = [c for c in children if c <= end - 3]
begin = min(cands, key=lambda c: abs(c - (mid - remove // 2)))
stop_candidates = [c for c in children if c > begin and c - begin >= remove]
stop = stop_candidates[0] if stop_candidates else end
# leading comment lines belong to the first moved statement
while begin > body_open and L[begin - 1].lstrip().startswith('//'):
    begin -= 1
moved = L[begin:stop]
name = fragment
open(f'{"/".join(src.split("/")[:-1])}/{name}', 'w').write('\n'.join(moved) + '\n')
L[begin:stop] = [f'#include "{name}"']
open(src, 'w').write('\n'.join(L))
print(f'moved {len(moved)} lines [{begin + 1}, {stop}] of {func_regex} into {name}; file now {len(L)} lines')

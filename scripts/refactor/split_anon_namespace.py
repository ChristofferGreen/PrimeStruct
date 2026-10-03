#!/usr/bin/env python3
"""split2.py <src.cpp> <HelpersName> <ns> <Suffix:startLine> ...
Splits the (single) anonymous namespace of src.cpp into: a header holding its types/templates/constants and
declarations of its functions (named namespace <ns>), and several .cpp parts holding the function definitions.
Everything after the anonymous namespace stays in the original file."""
import sys, re, os

src, helpers, ns = sys.argv[1:4]
specs = [(s.split(':')[0], int(s.split(':')[1])) for s in sys.argv[4:]]
lines = open(src).read().split('\n')
if lines and lines[-1] == '':
    lines.pop()
pi = next(i for i, l in enumerate(lines) if l.startswith('namespace primec'))
# namespace opening lines (may be several: primec { semantics {)
ns_open = [i for i, l in enumerate(lines) if re.match(r'^namespace [\w:]+ \{', l) and i <= next(k for k, x in enumerate(lines) if x.startswith('namespace {'))]
prologue_end = ns_open[-1]
prologue = lines[:prologue_end + 1]
ai = next(i for i, l in enumerate(lines) if l.startswith('namespace {'))
ae = next(i for i in range(ai + 1, len(lines)) if re.match(r'^\}\s*//\s*(anonymous )?namespace\s*$', lines[i]))
pre = lines[prologue_end + 1:ai]
anon = lines[ai + 1:ae]
post = lines[ae + 1:]
# trailing closers
tail = []
while post and (not post[-1].strip() or re.match(r'^\}\s*//\s*namespace', post[-1])):
    tail.insert(0, post.pop())


def strip_code(l):
    l = re.sub(r'"(\\.|[^"\\])*"', '""', l)
    l = re.sub(r"'(\\.|[^'\\])+'", "' '", l)
    l = re.sub(r'//.*', '', l)
    return l


# split into items
items = []  # (startLine(1-based of lead), leadStartIdx, endIdx, text lines)
i = 0
n = len(anon)
base = ai + 1  # index offset into lines
while i < n:
    lead_start = i
    while i < n and (not anon[i].strip() or anon[i].lstrip().startswith('//')):
        i += 1
    if i >= n:
        break
    start = i
    depth = 0
    paren = 0
    while True:
        c = strip_code(anon[i])
        depth += c.count('{') - c.count('}')
        paren += c.count('(') - c.count(')')
        if depth == 0 and paren == 0 and c.rstrip().endswith(('}', '};', ';')):
            break
        i += 1
    i += 1
    items.append((base + start + 1, base + lead_start, base + i, anon[lead_start:i]))

hdr_items = []  # list of lines
defs = []  # (origLine, text lines, signature decl lines)
kw_header = ('struct', 'class', 'enum', 'using', 'typedef', 'constexpr', 'const ', 'static constexpr', 'static const',
             'template', 'union', 'static_assert', 'inline', 'namespace', 'extern')
for (sl, ls, e, text) in items:
    code_lines = [l for l in text if l.strip() and not l.lstrip().startswith('//')]
    first = code_lines[0]
    joined = '\n'.join(code_lines)
    if first.startswith(kw_header) and not re.match(r'^const [\w:<>,\s\*&]+\s+\**&?\w+\(', first):
        hdr_items.append(text)
        continue
    # function definition?
    head = []
    for l in code_lines:
        head.append(l)
        if strip_code(l).rstrip().endswith('{'):
            break
    sig = '\n'.join(head).rstrip()
    if sig.endswith('{') and re.search(r'\)\s*(const)?\s*(noexcept)?\s*(->[^{]*)?\{$', sig):
        decl = sig[:-1].rstrip() + ';'
        decl = re.sub(r'^static ', '', decl)
        lead = [l for l in text[:text.index(code_lines[0])]]
        hdr_items.append(lead + decl.split('\n'))
        defs.append((sl, text))
    elif first.startswith('static ') and sig.endswith('{'):
        decl = re.sub(r'^static ', '', sig[:-1].rstrip() + ';')
        hdr_items.append(decl.split('\n'))
        defs.append((sl, text))
    else:
        hdr_items.append(text)
        print('NOTE header item (non-function):', code_lines[0][:80], file=sys.stderr)

d = os.path.dirname(src)
stem = os.path.splitext(os.path.basename(src))[0]
inc = [l for l in prologue if l.startswith('#include')]
soa = [l for l in lines[:3] if 'soa-surface-audit' in l]
nsline = prologue[-1]
close_lines = [l for l in tail if l.startswith('}')]
# header
h = soa + ['#pragma once', '', f'// Internal helpers of {stem}*.cpp (TODO-5384): types and declarations; the',
           f'// definitions live in the {stem}*.cpp units.'] + inc + ['']
h += [l for l in prologue if l.startswith('namespace ')] + pre + [f'namespace {ns} {{', '']
for t in hdr_items:
    h += t
h += ['', f'}} // namespace {ns}'] + close_lines + ['']
open(os.path.join(d, helpers + '.h'), 'w').write('\n'.join(h))
# parts
ns_lines = [l for l in prologue if l.startswith('namespace ')]
bounds = [s for _, s in specs] + [10 ** 9]
for k, (suffix, start) in enumerate(specs):
    chunk = [t for (sl, t) in defs if start <= sl < bounds[k + 1]]
    pro = list(prologue)
    li = max(x for x, l in enumerate(pro) if l.startswith('#include'))
    pro.insert(li + 1, f'#include "{helpers}.h"')
    body = []
    for t in chunk:
        # strip 'static ' from definition line
        tt = list(t)
        for x, l in enumerate(tt):
            if l.startswith('static '):
                tt[x] = l[len('static '):]
                break
        body += tt
    out = pro + pre + [f'namespace {ns} {{', ''] + body + ['', f'}} // namespace {ns}'] + close_lines + ['']
    name = os.path.join(d, stem + suffix + '.cpp')
    open(name, 'w').write('\n'.join(out))
    print(name, len(out))
# original: prologue + include helpers + using + post
pro = list(prologue)
li = max(x for x, l in enumerate(pro) if l.startswith('#include'))
pro.insert(li + 1, f'#include "{helpers}.h"')
out = pro + [f'using namespace {ns};', ''] + post + tail
open(src, 'w').write('\n'.join(out) + ('\n' if out[-1] != '' else ''))
print(src, len(out))

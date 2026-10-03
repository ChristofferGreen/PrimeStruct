#!/usr/bin/env python3
"""extract_lambda.py <src.cpp> <enclosingFunction> <newUnitSuffix> <headerName> <name[=RetType]>...

Moves top-level (2-space indented) lambdas of <enclosingFunction> into free functions in
<src stem><suffix>.cpp, declared in <headerName>.h; the lambda stays as a thin forwarding wrapper.
Free variables that are parameters of the enclosing function become extra parameters.
"""
import re, sys, os

src, fn, suffix, header = sys.argv[1:5]
wanted = {}
for a in sys.argv[5:]:
    n, _, r = a.partition('=')
    wanted[n] = r
L = open(src).read().split('\n')

# enclosing function signature
fs = next(i for i, l in enumerate(L) if re.match(r'^\S.*\b' + fn + r'\(', l))
sig_end = fs
while not L[sig_end].rstrip().endswith('{'):
    sig_end += 1
sig = ' '.join(x.strip() for x in L[fs:sig_end + 1])
m = re.match(r'^.*?\b' + fn + r'\((.*)\)\s*\{$', sig)
params = []
depth = 0
cur = ''
for ch in m.group(1):
    if ch in '<([':
        depth += 1
    if ch in '>)]':
        depth -= 1
    if ch == ',' and depth == 0:
        params.append(cur.strip()); cur = ''
    else:
        cur += ch
params.append(cur.strip())
fparams = {}
for p in params:
    name = re.search(r'(\w+)\s*$', p).group(1)
    fparams[name] = p
fn_end = fs
while not L[fn_end].startswith('}'):
    fn_end += 1
all_lambdas = []
i = fs
while i < fn_end:
    mm = re.match(r'^  (?:const )?auto (\w+) =\s*\[([^\]]*)\]', L[i] + ('\n' + L[i + 1] if L[i].rstrip().endswith('=') else ''))
    if mm:
        j = i
        while not L[j].startswith('  };'):
            j += 1
        all_lambdas.append((mm.group(1), i, j))
        i = j
    i += 1
lam_names = [n for n, _, _ in all_lambdas]
new_defs = []
decls = []
edits = []  # (start,end,replacement lines)
for name, s, e in all_lambdas:
    if name not in wanted:
        continue
    text = '\n'.join(L[s:e + 1])
    head_m = re.match(r'^  (?:const )?auto \w+ =\s*\[([^\]]*)\]\s*\(', text)
    k = head_m.end()
    depth = 1
    pstart = k
    while depth:
        if text[k] == '(':
            depth += 1
        elif text[k] == ')':
            depth -= 1
        k += 1
    lparams = text[pstart:k - 1]
    rest = text[k:]
    brace = rest.index('{')
    between = rest[:brace].strip()
    ret = wanted[name]
    if between.startswith('->'):
        ret = ret or between[2:].strip()
    if not ret:
        print('need return type for', name, file=sys.stderr)
        sys.exit(1)
    body = rest[brace + 1:]
    body = body[:body.rindex('};')] if body.rstrip().endswith('};') else body
    body = body.rstrip()
    used = [p for p in fparams if re.search(r'\b' + p + r'\b', body) and not re.search(r'\b' + p + r'\b', lparams)]
    others = [n for n in lam_names if n != name and re.search(r'\b' + n + r'\(', body)]
    if others:
        print(f'{name}: uses other lambdas {others}', file=sys.stderr)
    pnames = [re.search(r'(\w+)\s*$', p.strip()).group(1) for p in lparams.split(',') if p.strip()] if lparams.strip() else []
    # split lambda params respecting <>
    lp = []
    d = 0
    c = ''
    for ch in lparams:
        if ch in '<([': d += 1
        if ch in '>)]': d -= 1
        if ch == ',' and d == 0:
            lp.append(c.strip()); c = ''
        else:
            c += ch
    if c.strip(): lp.append(c.strip())
    pnames = [re.search(r'(\w+)\s*$', p).group(1) for p in lp]
    all_params = lp + [fparams[u] for u in used]
    # de-indent body by 2
    dedent = '\n'.join(x[2:] if x.startswith('  ') else x for x in body.split('\n'))
    new_defs.append(f'{ret} {name}({", ".join(all_params)}) {{{dedent}\n}}\n')
    decls.append(f'{ret} {name}({", ".join(all_params)});')
    call_args = ', '.join(pnames + used)
    wrapper = [f'  auto {name} = [&]({lparams}) -> {ret} {{',
               f'    return ::primec::{name}({call_args});',
               '  };']
    edits.append((s, e, wrapper))
for s, e, w in sorted(edits, reverse=True):
    L[s:e + 1] = w
open(src, 'w').write('\n'.join(L))
d = os.path.dirname(src)
stem = os.path.splitext(os.path.basename(src))[0]
# prologue: up to the first non-using definition after `namespace primec {`
ns = next(i for i, l in enumerate(L) if l.startswith('namespace primec {'))
k = ns + 1
while k < len(L) and (not L[k].strip() or L[k].startswith('using ')):
    k += 1
prologue = L[:k]
inc = f'#include "{header}.h"'
prologue.insert(next(i for i, l in enumerate(prologue) if l.startswith('#include "TemplateMonomorphImplicitTemplateInference.h"')) + 1, inc)
open(os.path.join(d, stem + suffix + '.cpp'), 'w').write('\n'.join(prologue) + '\n' + '\n'.join(new_defs) + '\n} // namespace primec\n')
hdr = ['#pragma once', '', '// Free-function forms of lambdas that used to live inside inferImplicitTemplateArgs', '// (TODO-5385).',
       '#include "TemplateMonomorphImplicitTemplateInference.h"', '', 'namespace primec {', ''] + decls + ['', '} // namespace primec', '']
open(os.path.join(d, header + '.h'), 'w').write('\n'.join(hdr))
# main file includes the header
t = open(src).read()
t = t.replace('#include "TemplateMonomorphImplicitTemplateInference.h"\n', '#include "TemplateMonomorphImplicitTemplateInference.h"\n' + inc + '\n', 1)
open(src, 'w').write(t)
print('moved', [n for n, _, _ in all_lambdas if n in wanted])

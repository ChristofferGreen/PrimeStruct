#!/usr/bin/env python3
"""phase_split.py <config.json>

Splits the statements of one block of a giant member function into phase member functions that share
a State struct. See config keys below.

config:
  file:        source .cpp
  func_regex:  regex matching the first line of the function definition
  class_name:  e.g. SemanticsValidator
  block_line:  1-based line of the block-opening statement (e.g. `if (expr.kind == Expr::Kind::Call) {`)
  cuts:        1-based line numbers where phases 2..N begin (must be statement starts of the block's children)
  prefix:      phase function name prefix, e.g. validateExprCall
  state_name:  State struct name
  state_header: header file name (written next to the source), holds State + PhaseStatus
  decl_header: existing class-member fragment header the phase declarations are appended to
  units:       list of [suffix, [phaseNumbers...]]; unit 0 keeps the original file
  prelude:     list of lines injected at the start of every phase (e.g. small helper lambdas)
  types:       {varName: "Type"} overrides for state variables whose type is `auto`
  lambda_ret:  {lambdaName: "RetType"} for lambdas without an explicit return type
  local_only:  [names] declared in the block that must stay local (never state)
"""
import json, re, sys, os

cfg = json.load(open(sys.argv[1]))
SV = cfg.get('state_var', 'st')
src = cfg['file']
L = open(src).read().split('\n')

# ---------- helpers ----------
def strip_code(s):
    s = re.sub(r'"(\\.|[^"\\])*"', '""', s)
    s = re.sub(r"'(\\.|[^'\\])+'", "' '", s)
    s = re.sub(r'//.*', '', s)
    return s

def find_block_end(start):
    depth = 0
    i = start
    while True:
        c = strip_code(L[i])
        depth += c.count('{') - c.count('}')
        if depth == 0 and i >= start and ('{' in c or i > start):
            return i
        i += 1

def statements(first, last):
    """statements (children) between 0-based line indexes [first, last) -> list of (startIdx, endIdxExclusive)"""
    out = []
    i = first
    while i < last:
        l = L[i]
        s = l.strip()
        if not s or s.startswith('//'):
            i += 1
            continue
        start = i
        depth = 0
        paren = 0
        j = i
        while True:
            c = strip_code(L[j])
            depth += c.count('{') - c.count('}')
            paren += c.count('(') - c.count(')')
            if depth == 0 and paren == 0 and c.rstrip().endswith(('}', '};', ';')):
                # a trailing `} else {` continues
                nxt = j + 1
                while nxt < last and not L[nxt].strip():
                    nxt += 1
                if nxt < last and L[nxt].strip().startswith(('else', '} else')) and not c.rstrip().endswith(';'):
                    j += 1
                    continue
                break
            j += 1
        out.append((start, j + 1))
        i = j + 1
    return out

# ---------- locate function / block ----------
fs = next(i for i, l in enumerate(L) if re.search(cfg['func_regex'], l))
sig_end = fs
while not L[sig_end].rstrip().endswith('{'):
    sig_end += 1
sig_text = ' '.join(x.strip() for x in L[fs:sig_end + 1])
CN = cfg['class_name']
NSN = cfg.get('namespace', 'primec::semantics')
FREE = not CN
if FREE:
    m = re.match(r'^(.*?)\b(' + cfg['func_name'] + r')\((.*)\)\s*(const)?\s*\{$', sig_text)
else:
    m = re.match(r'^(.*?)\b' + CN + r'::(\w+)\((.*)\)\s*(const)?\s*\{$', sig_text)
ret_type = m.group(1).strip()
func_name = m.group(2)
param_text = m.group(3)
is_const_fn = bool(m.group(4))
params = []
d = 0
cur = ''
for ch in param_text:
    if ch in '<([': d += 1
    if ch in '>)]': d -= 1
    if ch == ',' and d == 0:
        params.append(cur.strip()); cur = ''
    else:
        cur += ch
if cur.strip():
    params.append(cur.strip())
pnames_all = [re.search(r'(\w+)\s*(=.*)?$', p).group(1) for p in params]
params_nodefault_all = [re.sub(r'^\[\[maybe_unused\]\]\s*', '', re.sub(r'\s*=.*$', '', p)) for p in params]
val_idx = [i for i, p in enumerate(params_nodefault_all) if '&' not in p]
pnames = [n for i, n in enumerate(pnames_all) if i not in val_idx]
params_nodefault = [p for i, p in enumerate(params_nodefault_all) if i not in val_idx]
val_params = [(pnames_all[i], re.sub(r'\s*\b' + pnames_all[i] + r'\s*$', '', params_nodefault_all[i]).strip()) for i in val_idx]
bl = cfg['block_line'] - 1
be = find_block_end(bl)          # index of closing brace line
stmts = statements(bl + 1, be)
cuts = [c - 1 for c in cfg['cuts']]
starts = {s for s, _ in stmts}
for c in cuts:
    assert c in starts, f'cut line {c+1} is not a statement start: {L[c]}'
phases = []   # list of list of (s,e)
cur = []
for s, e in stmts:
    if s in cuts and cur:
        phases.append(cur); cur = []
    cur.append((s, e))
phases.append(cur)

# ---------- classify declarations ----------
KW = ('if', 'for', 'while', 'return', 'else', 'switch', 'break', 'continue', 'throw', 'delete', 'case', 'do', 'goto', 'using', 'static_assert', 'try')
def classify(text):
    """-> (kind, name, typeText, initText, declForm) for declarations else None"""
    t = text.strip()
    if t.startswith(KW):
        return None
    mm = re.match(r'^(auto\s+(\w+)\s*=\s*\[)', t) or re.match(r'^(auto\s+(\w+)\s*=\s*$)', t.split('\n')[0] + ('' if '\n' in t else ''))
    if re.match(r'^(const\s+)?auto\s+(\w+)\s*=\s*(\n\s*)?\[', t):
        name = re.match(r'^(const\s+)?auto\s+(\w+)', t).group(2)
        return ('lambda', name, None, None)
    # Type name (= | { | ; | ( )
    mm = re.match(r'^((?:const\s+)?[A-Za-z_][\w:]*(?:<[^;={}]*>)?)(\s*[&*]+\s*|\s+)(?:const\s+)?(\w+)\s*(=|\{|;|\()', t)
    if mm and not mm.group(1).strip() in ('return',):
        typ = (mm.group(1) + ' ' + mm.group(2).strip()).strip() if mm.group(2).strip() else mm.group(1).strip()
        name = mm.group(3)
        rest = t[mm.end() - 1:]
        return ('var', name, typ, rest)
    return None

state_vars = {}   # name -> dict(kind,type,phase,stmtIdx)
decl_of = {}      # (phaseIdx, stmtStart) -> name
for pi, ph in enumerate(phases):
    for s, e in ph:
        text = '\n'.join(L[s:e])
        c = classify(text)
        if c and c[1] not in cfg.get('local_only', []):
            state_vars[c[1]] = dict(kind=c[0], type=c[2], phase=pi, start=s, end=e, rest=c[3])
            decl_of[(pi, s)] = c[1]

def phase_text(pi):
    return '\n'.join('\n'.join(L[s:e]) for s, e in phases[pi])

# which state vars are used in a later phase than declared
used_later = set()
for name, info in state_vars.items():
    for pj in range(info['phase'] + 1, len(phases)):
        if re.search(r'\b' + name + r'\b', phase_text(pj)):
            used_later.add(name)
state = {n: i for n, i in state_vars.items() if n in used_later or n in cfg.get('force_state', [])}

# ---------- build State struct ----------
members = []
missing = []
for name, info in state.items():
    if info['kind'] == 'lambda':
        text = '\n'.join(L[info['start']:info['end']])
        full = ' '.join(text.split('\n'))
        mm = re.match(r'^\s*(?:const\s+)?auto\s+\w+\s*=\s*\[[^\]]*\]\s*', full)
        k = mm.end()
        lp = ''
        if full[k] == '(':
            k += 1
            depth = 1
            p0 = k
            while depth:
                if full[k] == '(': depth += 1
                elif full[k] == ')': depth -= 1
                k += 1
            lp = full[p0:k - 1]
        between = full[k:full.index('{', k)].strip()
        between = re.sub(r'^mutable\s*', '', between)
        if between.startswith('->'):
            rt = between[2:].strip()
        else:
            rt = cfg['lambda_ret'].get(name)
            if rt is None:
                missing.append(name); rt = 'bool'
        info['ret'] = rt
        info['lparams'] = lp
        lp_clean = re.sub(r'\s*=\s*[^,)]+(?=[,)])', '', re.sub(r'\s+', ' ', lp))
        members.append(f'  std::function<{rt}({lp_clean})> {name};')
    else:
        typ = cfg.get('types', {}).get(name) or info['type']
        if typ == 'auto' or typ == 'const auto':
            print(f'state var {name} has auto type; give it in types', file=sys.stderr); sys.exit(1)
        if '*' not in typ and '&' not in typ:
            typ = re.sub(r'^const\s+', '', typ)
        info['mtype'] = typ
        if typ.rstrip().endswith('&'):
            info['isref'] = True
            info['reftype'] = typ.rstrip('& ').strip()
            typ = info['reftype'] + ' *'
        for q in cfg.get('qualify', []):
            typ = re.sub(r'\b' + q + r'\b', cfg['class_name'] + '::' + q, typ)
        info['mtype'] = typ
        members.append(f'  {typ} {name}{{}};')

ns_line = f'namespace {NSN} {{'
for n, t in val_params:
    members.append(f'  {t} {n}{{}};')
hdr = ['#pragma once', '',
       f'// State shared by the {cfg["prefix"]}* phase functions (split out of {func_name}, TODO-5385).',
       *([f'#include "{i}"' for i in cfg.get('state_includes', [])] if FREE else ['#include "SemanticsValidator.h"']), '', '#include <functional>', '#include <optional>', '#include <string>', '', ns_line, '',
       *(['enum class PhaseStatus { Continue, Done };', ''] if FREE and not cfg.get('status_defined') else []),
       f'struct {cfg["state_name"]} {{', f'  {ret_type} result{{}};', '  PhaseStatus done(' + ret_type + ' value) {', '    result = std::move(value);', '    return PhaseStatus::Done;', '  }'] + members + ['};', '', f'}} // namespace {NSN}', '']
if missing:
    print('lambdas without return type:', missing); sys.exit(1)
open(os.path.join(os.path.dirname(src), cfg['state_header']), 'w').write('\n'.join(hdr))

# ---------- rewrite returns ----------
def rewrite_returns(text):
    out = []
    i = 0
    n = len(text)
    depth_stack = []   # lambda body brace stack entries: True if lambda body
    brace = 0
    lam_depth = 0
    pending_lambda = False
    paren_at_lambda = 0
    paren = 0
    stack = []
    in_str = False
    while i < n:
        ch = text[i]
        # comments
        if text.startswith('//', i):
            j = text.find('\n', i)
            j = n if j < 0 else j
            out.append(text[i:j]); i = j; continue
        if text.startswith('/*', i):
            j = text.index('*/', i) + 2
            out.append(text[i:j]); i = j; continue
        if ch == '"':
            j = i + 1
            while text[j] != '"':
                if text[j] == '\\': j += 1
                j += 1
            out.append(text[i:j + 1]); i = j + 1; continue
        if ch == "'" :
            j = i + 1
            while text[j] != "'":
                if text[j] == '\\': j += 1
                j += 1
            out.append(text[i:j + 1]); i = j + 1; continue
        if ch == '[':
            # lambda introducer if previous non-space char is not identifier/)/]
            k = len(''.join(out)) - 1
            joined = ''.join(out)
            prev = joined.rstrip()[-1:] if joined.rstrip() else ''
            if not (prev.isalnum() or prev == '_' or prev in ')]'):
                j = text.index(']', i)
                out.append(text[i:j + 1]); i = j + 1
                pending_lambda = True
                paren_at_lambda = paren
                continue
        if ch == '(':
            paren += 1
        elif ch == ')':
            paren -= 1
        if ch == '{':
            if pending_lambda and paren == paren_at_lambda:
                stack.append(True); lam_depth += 1; pending_lambda = False
            else:
                stack.append(False)
        elif ch == '}':
            if stack and stack.pop():
                lam_depth -= 1
        elif ch == ';':
            pending_lambda = False
        if lam_depth == 0 and re.match(r'return\b', text[i:]) and not (i > 0 and (text[i - 1].isalnum() or text[i - 1] == '_')):
            # find end of statement
            j = i + 6
            p = 0
            b = 0
            while True:
                cj = text[j]
                if cj == '"':
                    k = j + 1
                    while text[k] != '"':
                        if text[k] == '\\': k += 1
                        k += 1
                    j = k + 1; continue
                if cj == "'":
                    k = j + 1
                    while text[k] != "'":
                        if text[k] == '\\': k += 1
                        k += 1
                    j = k + 1; continue
                if cj in '([{': p += 1
                if cj in ')]}': p -= 1
                if cj == ';' and p == 0:
                    break
                j += 1
            expr = text[i + 6:j].strip()
            out.append(f'return {SV}.done({expr});')
            i = j + 1
            continue
        out.append(ch)
        i += 1
    return ''.join(out)

# ---------- emit phases ----------
def alias_line(n):
    inf = state[n]
    if inf.get('isref'):
        return f'  [[maybe_unused]] {inf["reftype"]} &{n} = *{SV}.{n};'
    return f'  [[maybe_unused]] auto &{n} = {SV}.{n};'

phase_defs = []
phase_decls = []
for pi, ph in enumerate(phases):
    body_lines = []
    texts = []
    declared_here = set()
    for s, e in ph:
        name = decl_of.get((pi, s))
        raw = '\n'.join(L[s:e])
        if name in state:
            info = state[name]
            if info['kind'] == 'lambda':
                # name = [..]..;
                t = re.sub(r'^(?:const\s+)?auto\s+(\w+)\s*=', f'{SV}.\\1 =', raw.lstrip(), count=1)
                t = '  ' + t
                texts.append(t)
                texts.append(f'  [[maybe_unused]] auto &{name} = {SV}.{name};')
            else:
                rest = info['rest']
                typ = info['type']
                if info.get('isref') and rest.startswith('='):
                    t = f'  {SV}.{name} = &({rest[1:].strip()[:-1]});'
                elif rest.startswith('='):
                    t = f'  {SV}.{name} = {rest[1:].strip()}'
                elif rest.startswith('{'):
                    t = f'  {SV}.{name} = {info["mtype"]}{rest}'
                elif rest.startswith('('):
                    t = f'  {SV}.{name} = {info["mtype"]}{rest}'
                else:
                    t = None
                if t:
                    texts.append(rewrite_returns(t))
                texts.append(alias_line(name))
            declared_here.add(name)
        else:
            texts.append(rewrite_returns(raw))
    body = '\n'.join(texts)
    uses = [n for n in state if state[n]['phase'] < pi and re.search(r'\b' + n + r'\b', body)]
    aliases = [alias_line(n) for n in uses]
    fname = f'{cfg["prefix"]}Phase{pi + 1}'
    sig = f'PhaseStatus {CN + "::" if CN else ""}{fname}(' + ', '.join(['[[maybe_unused]] ' + p for p in params_nodefault] + [f'{cfg["state_name"]} &{SV}']) + f'){" const" if is_const_fn else ""} {{'
    valal = [f'  [[maybe_unused]] auto &{n} = {SV}.{n};' for n, _ in val_params]
    d = [sig] + valal + cfg.get('prelude', []) + aliases + [body, '  return PhaseStatus::Continue;', '}', '']
    phase_defs.append('\n'.join(d))
    phase_decls.append(f'  PhaseStatus {fname}(' + ', '.join(params_nodefault + [f'{cfg["state_name"]} &{SV}']) + f'){" const" if is_const_fn else ""};')

# ---------- write units ----------
d = os.path.dirname(src)
stem = os.path.splitext(os.path.basename(src))[0]
nsi = next(i for i, l in enumerate(L) if l.startswith(f'namespace {NSN} {{'))
prologue = L[:nsi + 1]
li = max(i for i, l in enumerate(prologue) if l.startswith('#include'))
prologue.insert(li + 1, f'#include "{cfg["state_header"]}"')
tail_line = next(i for i in range(len(L) - 1, -1, -1) if L[i].startswith('} // namespace'))
units = cfg['units']   # [[suffix, [phase numbers]]]
unit_files = []
for suffix, phs in units:
    body = '\n'.join(phase_defs[p - 1] for p in phs)
    path = os.path.join(d, stem + suffix + '.cpp')
    if suffix == '':
        continue
    open(path, 'w').write('\n'.join(prologue + cfg.get('after_ns', [])) + '\n\n' + body + f'\n}} // namespace {NSN}\n')
    unit_files.append(path)
main_phases = next((phs for suf, phs in units if suf == ''), [])
# ---------- rewrite the original file ----------
chain = [f'    {cfg["state_name"]} {SV};'] + [f'    {SV}.{n} = {n};' for n, _ in val_params]
for pi in range(len(phases)):
    chain.append(f'    if ({cfg["prefix"]}Phase{pi + 1}(' + ', '.join(pnames + [SV]) + ') == PhaseStatus::Done) {')
    chain.append(f'      return {SV}.result;')
    chain.append('    }')
if cfg.get('tail_return'):
    chain.append(f'    return {SV}.result;')
new_L = L[:bl + 1] + chain + L[be:]
extra = '\n'.join(phase_defs[p - 1] for p in main_phases)
text = '\n'.join(new_L)
# insert phases defined in the main unit after the function end
if extra:
    idx = text.rindex(f'}} // namespace {NSN}')
    text = text[:idx] + extra + '\n' + text[idx:]
inc_anchor = text.index('\n', text.rindex('#include', 0, text.index(f'namespace {NSN} {{')))
text = text[:inc_anchor] + f'\n#include "{cfg["state_header"]}"' + text[inc_anchor:]
open(src, 'w').write(text)
# ---------- declarations ----------
if FREE:
    hp_ = os.path.join(os.path.dirname(src), cfg['state_header'])
    ht_ = open(hp_).read()
    closing = f'}} // namespace {NSN}'
    ht_ = ht_[:ht_.rindex(closing)] + '\n'.join(d_.strip() for d_ in phase_decls) + '\n\n' + closing + '\n'
    open(hp_, 'w').write(ht_)
    print('phases:', len(phases), 'state vars:', list(state))
    print('unit files:', unit_files)
    sys.exit(0)
decl_path = cfg['decl_header']
t = open(decl_path).read()
fwd = f'  struct {cfg["state_name"]};\n'
block = f'\n  // Phase functions of {func_name} (TODO-5385).\n' + f'  friend struct {cfg["state_name"]};\n' + '\n'.join(phase_decls) + '\n'
open(decl_path, 'a').write(block)
hp = 'src/semantics/SemanticsValidator.h'
ht = open(hp).read()
fw = f'struct {cfg["state_name"]};\n'
if fw not in ht:
    ht = ht.replace('\nclass SemanticsValidator {', '\n' + fw + '\nclass SemanticsValidator {', 1)
    open(hp, 'w').write(ht)
print('phases:', len(phases), 'state vars:', list(state))
print('unit files:', unit_files)

#!/usr/bin/env python3
"""split.py <src.cpp> <HelpersName> <ns> <suffix1:cut1> <suffix2:cut2> ...
Parts: first part keeps original file name and spans [bodyStart, cut1); part i spans [cut_i, cut_{i+1}).
Cuts are 1-based line numbers where a part begins (must be at top-level between members)."""
import sys,re,os
src=sys.argv[1]; helpers=sys.argv[2]; ns=sys.argv[3]; specs=sys.argv[4:]
lines=open(src).read().split('\n')
if lines and lines[-1]=='': lines.pop()
# prologue
pi=next(i for i,l in enumerate(lines) if l.startswith('namespace primec::semantics {') or l.startswith('namespace primec {') )
prologue=lines[:pi+1]
# trailing close
ti=max(i for i,l in enumerate(lines) if l.startswith('}') and 'namespace' in l)
close=lines[ti]
body=lines[pi+1:ti]
# anon block
anon=[]
pre=[]
j=0
while j<len(body) and body[j].strip()=='' : j+=1
bodystart=pi+1+j
ja=next((x for x,l in enumerate(body) if l.startswith('namespace {')),None)
if ja is not None and all((not l.strip()) or l.startswith(('using ','//','#')) for l in body[:ja]):
    pre=body[:ja]; j=ja
if j<len(body) and body[j].startswith('namespace {'):
    k=j
    while not re.match(r'^\}\s*//\s*namespace\s*$|^\}\s*//\s*anonymous namespace\s*$',body[k]): k+=1
    anon=body[j+1:k]
    bodystart=pi+1+k+1
kw=('}','//','namespace','struct','using','constexpr','enum','class','#','template','static_assert','typedef','extern','union')
def inline_fix(a):
    out=[];depth=0;paren=0
    for l in a:
        s=l
        if depth==0 and paren==0 and l and not l.startswith((' ','\t')) and not l.startswith(kw) :
            if re.match(r'^[A-Za-z_][\w:<>,\s\*&\.]*[\w>\*&]\s+\**&?[\w:~]+\(',l) or re.match(r'^static ',l):
                s=re.sub(r'^static ','',l)
                s='inline '+s
        out.append(s)
        depth+=l.count('{')-l.count('}')
        paren+=l.count('(')-l.count(')')
    return out
cuts=[(s.split(':')[0],int(s.split(':')[1])) for s in specs]
base=os.path.splitext(src)[0]
d=os.path.dirname(src)
inc=[l for l in prologue if l.startswith('#include') or l.startswith('// soa-surface-audit')]
pro_no_ns=prologue
hdr_path=os.path.join(d,helpers+'.h')
if anon:
    h=[ '// soa-surface-audit: exempt' if any('soa-surface-audit' in l for l in prologue) else None,
        '#pragma once','',f'// Helpers shared by the {os.path.basename(base)}*.cpp units (split out of',
        f'// {os.path.basename(src)} without changes, TODO-5384).']
    h=[x for x in h if x is not None]
    h+= [l for l in prologue if l.startswith('#include')]
    h+=['',prologue[-1]]+pre+[f'namespace {ns} {{']+inline_fix(anon)+[f'}} // namespace {ns}',close,'']
    open(hdr_path,'w').write('\n'.join(h))
bounds=[bodystart+1]+[c for _,c in cuts]+[ti+1]  # 1-based starts
names=[src]+[os.path.join(d,os.path.basename(base)+s+'.cpp') for s,_ in cuts]
for idx,name in enumerate(names):
    a=bounds[idx]-1; b=bounds[idx+1]-1
    seg=lines[a:b]
    while seg and seg[0].strip()=='' : seg=seg[1:]
    while seg and seg[-1].strip()=='' : seg=seg[:-1]
    pro=list(prologue)
    if anon:
        # add helpers include after last #include
        li=max(i for i,l in enumerate(pro) if l.startswith('#include'))
        pro.insert(li+1,f'#include "{helpers}.h"')
        pro.append(f'using namespace {ns};')
    out=pro+['']+seg+['',close,'']
    open(name,'w').write('\n'.join(out))
    print(name,len(out))

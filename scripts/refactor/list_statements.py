import re,sys
src=sys.argv[1]; bl=int(sys.argv[2])-1
L=open(src).read().split('\n')
src_code=open('/tmp/claude-0/probe/phase_split.py').read()
ns={}
exec("import re\n"+src_code[src_code.index('def strip_code'):src_code.index('# ---------- locate function')].replace("L[","LL["),{**ns,'LL':L,'re':re},ns) if False else None
def strip_code(s):
    s = re.sub(r'"(\\.|[^"\\])*"', '""', s); s = re.sub(r"'(\\.|[^'\\])+'", "' '", s); return re.sub(r'//.*', '', s)
def find_block_end(start):
    depth=0;i=start
    while True:
        c=strip_code(L[i]);depth+=c.count('{')-c.count('}')
        if depth==0 and ('{' in c or i>start): return i
        i+=1
def statements(first,last):
    out=[];i=first
    while i<last:
        s=L[i].strip()
        if not s or s.startswith('//'): i+=1;continue
        start=i;depth=0;paren=0;j=i
        while True:
            c=strip_code(L[j]);depth+=c.count('{')-c.count('}');paren+=c.count('(')-c.count(')')
            if depth==0 and paren==0 and c.rstrip().endswith(('}','};',';')):
                nxt=j+1
                while nxt<last and not L[nxt].strip(): nxt+=1
                if nxt<last and L[nxt].strip().startswith(('else','} else')) and not c.rstrip().endswith(';'):
                    j+=1;continue
                break
            j+=1
        out.append((start,j+1));i=j+1
    return out
be=find_block_end(bl)
st=statements(bl+1,be)
print('block',bl+1,be+1,'children',len(st))
target=int(sys.argv[3]) if len(sys.argv)>3 else 200
acc=0;last=bl+1
for s,e in st:
    if s-last>=target:
        print('cut candidate',s+1,L[s].strip()[:70]);last=s

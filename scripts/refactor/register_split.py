import sys,re
# reg.py <orig.cpp> <newpart.cpp>...  : add parts to CMakeLists after orig, drop allowlist entry
orig=sys.argv[1]; parts=sys.argv[2:]
c=open('CMakeLists.txt').read()
line=f'  {orig}\n'
assert line in c, orig
c=c.replace(line,line+''.join(f'  {p}\n' for p in parts if f'  {p}\n' not in c),1)
open('CMakeLists.txt','w').write(c)
a=open('scripts/source_file_size_allowlist.txt').read().split('\n')
a=[l for l in a if not l.startswith(orig+' ')]
open('scripts/source_file_size_allowlist.txt','w').write('\n'.join(a))

import re,sys
import os
# Writes the GLSL of four programs to t/ next to this script, for CafeGLSL's glslcompiler and dump.cpp
D=os.path.dirname(os.path.abspath(__file__))
G=os.path.join(D, '../../../../Sources/Shaders/Generated/')
os.chdir(D); os.makedirs('t', exist_ok=True)
jobs=[('DefaultSprite','DefaultSprite','DefaultSprite'),('DefaultBatchedSprites','DefaultSprite','DefaultBatchedSprites'),('Combine','Combine','Combine'),('Blur','Blur','Blur')]
for name,hdr,pref in jobs:
    s=open(G+hdr+'.h').read()
    for st,ext in (('Vs','vert'),('Fs','frag')):
        m=re.search(r'char '+pref+'_'+st+r'\[\] =\s*R"__SHDR__\((.*?)\)__SHDR__";',s,re.S)
        pre='#version 330\n#line 0\n'
        if name=='DefaultBatchedSprites': pre+='#ifndef BATCH_SIZE\n\t#define BATCH_SIZE (682)\n#endif\n#line 0\n'
        open(f't/{name}.{ext}','w').write(pre+m.group(1))

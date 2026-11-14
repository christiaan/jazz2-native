import re, sys, random, math
src = open(sys.argv[1]).read().splitlines()
W, H = 37.0, 23.0
random.seed(int(sys.argv[2]))
tex = {}
def texel(u, v):
    k = (round(u * W * 2), round(v * H * 2))
    if k not in tex: tex[k] = [random.random() for _ in range(3)]
    return tex[k]
px = (random.uniform(3, 30), random.uniform(3, 20)); inv = (1 / W, 1 / H)
C = 'xyzw'
body = []
started = False
for ln in src:
    if ln.startswith('R0f = passParameterSem138'): started = True
    if not started or ln.startswith('//') or not ln.strip() or ln.strip() == '}': continue
    m = re.match(r'(R\d+f)\.xyz = \(texture\(textureUnitPS0, vec2\((\S+),(\S+)\)\)\.xyz\);', ln)
    if m:
        ln = f"{m.group(1)}[0:3] = texel({m.group(2)}, {m.group(3)})"
    ln = ln.rstrip(';')
    m = None
    ln = re.sub(r'(R\d+f)\.([xyzw])', lambda mm: f"{mm.group(1)}[{C.index(mm.group(2))}]", ln)
    ln = ln.replace('passParameterSem138', '[px[0], px[1], 0.0, 0.0]').replace('passParameterSem139', '[inv[0], inv[1], 0.0, 0.0]')
    ln = re.sub(r'passPixelColor0 = vec4\((.*)\)$', r'OUT = [\1]', ln)
    body.append(ln)
env = {'texel': texel, 'px': px, 'inv': inv, 'fract': lambda x: x - math.floor(x), 'floor': lambda x: float(math.floor(x)),
       'mul_nonIEEE': lambda a, b: 0.0 if a == 0 or b == 0 else a * b}
for i in range(128): env[f'R{i}f'] = [0.0] * 4
for n in ['PV0fx','PV0fy','PV0fz','PV0fw','PV1fx','PV1fy','PV1fz','PV1fw','PS0f','PS1f']: env[n] = 0.0
exec('\n'.join(body), env)
out = env['OUT'][:3]
def ch(A, B, Cc, D, t):
    a = -A/2 + 1.5*B - 1.5*Cc + D/2; b = A - 2.5*B + 2*Cc - D/2; c = -A/2 + Cc/2
    return a*t**3 + b*t*t + c*t + B
fx, fy = px[0] - math.floor(px[0]), px[1] - math.floor(px[1])
pix = (math.floor(px[0]) * inv[0] - inv[0] / 2, math.floor(px[1]) * inv[1] - inv[1] / 2)
ref = []
for k in range(3):
    rows = [ch(*[texel(pix[0] + (i - 1) * inv[0], pix[1] + (j - 1) * inv[1])[k] for i in range(4)], fx) for j in range(4)]
    ref.append(ch(*rows, fy))
print('cemu', [round(x, 5) for x in out], 'ref', [round(x, 5) for x in ref], 'taps', len(tex))

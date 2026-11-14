import re, sys, random, math
class _np:
    floor = staticmethod(lambda x: float(math.floor(x)))
np = _np
f32 = float
lines = open(sys.argv[1]).read().splitlines()[105:386]
W, H = 37.0, 23.0
random.seed(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
tex = {}
def texel(u, v):
    k = (round(u * W * 2), round(v * H * 2))
    if k not in tex: tex[k] = [f32(random.random()) for _ in range(3)]
    return tex[k]
px = (f32(random.uniform(3, 30)), f32(random.uniform(3, 20)))
inv = (f32(1 / W), f32(1 / H))
R = {}
def reg(n): return R.setdefault(n, [f32(0)] * 4)
R0 = reg('R0'); R0[0], R0[1] = px; R1 = reg('R1'); R1[0], R1[1] = inv
C = 'xyzw'
PV = [f32(0)] * 4; PS = f32(0)
group = []
def val(tok, pv, ps):
    neg = tok.startswith('-'); tok = tok.lstrip('-')
    m = re.match(r'\[0x[0-9A-F]+ (\S+)\]', tok)
    if m: v = f32(float(m.group(1)))
    elif tok == 'PS': v = ps
    elif tok.startswith('PV.'): v = pv[C.index(tok[3])]
    elif re.match(r'^-?[0-9.]+$', tok): v = f32(float(tok))
    else:
        n, c = tok.split('.'); v = reg(n)[C.index(c)]
    return -v if neg else v
def run_group():
    global PV, PS
    newpv = list(PV); newps = PS; writes = []
    for slot, op, dst, srcs in group:
        a = [val(s, PV, PS) for s in srcs]
        base = op.split('/')[0]
        if base == 'MOV': r = a[0]
        elif base == 'FRACT': r = a[0] - np.floor(a[0])
        elif base == 'FLOOR': r = np.floor(a[0])
        elif base == 'MUL_IEEE': r = a[0] * a[1]
        elif base == 'MULADD_IEEE': r = a[0] * a[1] + a[2]
        elif base == 'ADD': r = a[0] + a[1]
        else: raise Exception(op)
        if op.endswith('/2'): r = r / f32(2)
        r = f32(r)
        if slot == 't': newps = r
        else: newpv[C.index(slot)] = r
        n, c = dst.split('.')
        if n not in ('__', 'PV', 'PS'): writes.append((n, C.index(c), r))
    for n, i, r in writes: reg(n)[i] = r
    PV, PS = newpv, newps
    group.clear()
for ln in lines:
    m = re.search(r'\s{2}(\d{1,3}\s+)?([xyzwt]):\s+(\S+)\s+(.*)$', ln)
    if m and 'SAMPLE' not in ln:
        if m.group(1) and group: run_group()
        ops = [s.strip() for s in re.sub(r'\s+BS:\d+', '', m.group(4)).split(',')]
        group.append((m.group(2), m.group(3), ops[0], ops[1:]))
        continue
    if group: run_group()
    m = re.search(r'SAMPLE\s+(R\d+)\.xyz_, (R\d+)\.(\w\w)', ln)
    if m:
        s = reg(m.group(2)); u = s[C.index(m.group(3)[0])]; v = s[C.index(m.group(3)[1])]
        reg(m.group(1))[0:3] = texel(u, v)
if group: run_group()
out = reg('R4')[:3]
# reference
def ch(A, B, Cc, D, t):
    a = -A/2 + 1.5*B - 1.5*Cc + D/2; b = A - 2.5*B + 2*Cc - D/2; c = -A/2 + Cc/2
    return a*t**3 + b*t*t + c*t + B
fx, fy = px[0] - np.floor(px[0]), px[1] - np.floor(px[1])
pix = (np.floor(px[0]) * inv[0] - inv[0] / 2, np.floor(px[1]) * inv[1] - inv[1] / 2)
ref = []
for k in range(3):
    rows = []
    for j in range(4):
        t = [texel(pix[0] + (i - 1) * inv[0], pix[1] + (j - 1) * inv[1])[k] for i in range(4)]
        rows.append(ch(*t, fx))
    ref.append(ch(*rows, fy))
print('emu', [round(float(x), 5) for x in out], 'ref', [round(float(x), 5) for x in ref], 'taps', len(tex))

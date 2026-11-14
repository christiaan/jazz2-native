import re, sys
h = open(sys.argv[1]).read()
progs = dict((m.group(1), bytes(int(x, 16) for x in re.findall(r'0x([0-9a-f]{2})', m.group(2))))
             for m in re.finditer(r'(_gx2(?:Vs|Ps)\d+Program)\[\][^{]*\{(.*?)\};', h, re.S))
names = {}
for m in re.finditer(r'\{ "([^"]*)", "([^"]*)", \d+,\s*\{ _gx2Vs\d+Regs, \d+, (_gx2Vs\d+Program).*?\{ _gx2Ps\d+Regs, \d+, (_gx2Ps\d+Program)', h, re.S):
    nm = m.group(1) + (('[' + m.group(2) + ']') if m.group(2) else '')
    names.setdefault(m.group(3), []).append(nm + ' VS'); names.setdefault(m.group(4), []).append(nm + ' PS')
def scan(b):
    dw = [int.from_bytes(b[i:i+4], 'little') for i in range(0, len(b), 4)]
    hits = []; pc = 0
    while pc + 1 < len(dw):
        w0, w1 = dw[pc], dw[pc + 1]
        if (w1 >> 29) & 1:  # ALU clause
            addr = (w0 & 0x3FFFFF) * 2; count = ((w1 >> 18) & 0x7F) + 1
            i = addr; end = addr + count * 2; group = []; pending = []
            while i < end:
                a0, a1 = dw[i], dw[i + 1]; i += 2
                op3 = ((a1 >> 13) & 0x1F) >= 8
                wm = 1 if op3 else (a1 >> 4) & 1
                srcs = [(a0 & 0x1FF, (a0 >> 10) & 3), ((a0 >> 13) & 0x1FF, (a0 >> 23) & 3)] + ([(a1 & 0x1FF, (a1 >> 10) & 3)] if op3 else [])
                group.append((wm, (a1 >> 21) & 0x7F, (a1 >> 29) & 3, srcs))
                if a0 >> 31:  # last in group
                    lit = max([c + 1 for s, c in sum([g[3] for g in group], []) if s == 253] or [0])
                    i += ((lit + 1) // 2) * 2
                    used = 0; units = []
                    for g in group:
                        u = g[2]
                        if used & (1 << u): u = 4
                        used |= 1 << u; units.append(u)
                    vec = {(g[1], g[2]) for g, u in zip(group, units) if u < 4 and g[0]}
                    if pending:
                        nxt = sum([g[3] for g in group], [])
                        for (pi, pg, pc_) in pending:
                            if (254, pc_) in nxt: hits.append((pi, pg, 'xyzw'[pc_]))
                    pending = [(i, g[1], g[2]) for g, u in zip(group, units) if u == 4 and g[0] and (g[1], g[2]) in vec]
                    group = []
        elif (w1 >> 21) & 1:
            break
        pc += 2
    return hits
tot = 0
for p, b in sorted(progs.items()):
    hs = scan(b)
    if hs:
        tot += 1
        print(p, len(hs), 'R%d.%s' % hs[0][1:], ', '.join(names.get(p, ['?'])))
print('programs affected:', tot, 'of', len(progs))

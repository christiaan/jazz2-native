import sys, zlib, struct, glob, os
WATER = int(sys.argv[2])
for f in sorted(glob.glob(os.path.join(sys.argv[1], '*.j2l'))):
    d = open(f, 'rb').read()
    if d[180:184] != b'LEVL': continue
    cs = struct.unpack('<8I', d[230:262])
    off = 262
    streams = []
    for i in range(4):
        streams.append(zlib.decompress(d[off:off+cs[i*2]])); off += cs[i*2]
    info = streams[0]
    minl, startl = info[9], info[10]
    name = info[19:51].split(b'\0')[0].decode('latin1')
    ev = streams[1]
    water = any((struct.unpack_from('<I', ev, i)[0] & 0xFF) == WATER for i in range(0, len(ev) - 3, 4))
    print(f"{os.path.basename(f):16} light {startl*100//64:3}% water {int(water)}  {name}")

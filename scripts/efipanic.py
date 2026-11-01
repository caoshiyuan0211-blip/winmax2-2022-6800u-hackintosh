#!/usr/bin/env python3
# Decode AAPL,PanicInfo000* efivars (7-bit packed panic text) to stdout.
import glob, sys
d = sys.argv[1] if len(sys.argv) > 1 else '/sys/firmware/efi/efivars'
raw = b''.join(open(f, 'rb').read()[4:] for f in sorted(glob.glob(d + '/AAPL,PanicInfo*')))
out, acc, n = bytearray(), 0, 0
for b in raw:
    acc |= b << n; n += 8
    while n >= 7:
        out.append(acc & 0x7f); acc >>= 7; n -= 7
sys.stdout.write(out.decode('ascii', 'replace'))

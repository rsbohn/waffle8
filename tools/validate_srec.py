#!/usr/bin/env python3
import sys
from pathlib import Path

p = Path(__file__).resolve().parents[1] / 'bbb.srec'
if not p.exists():
    print('bbb.srec not found at', p)
    sys.exit(2)

lines = [l.rstrip('\n') for l in p.read_text().splitlines() if l.strip()]

def hexbyte(b):
    return int(b,16)

ok = True
for i,line in enumerate(lines,1):
    print(f'Line {i}: {line}')
    if not line.startswith('S') or len(line) < 4:
        print('  Invalid S-record format')
        ok = False
        continue
    typ = line[1]
    try:
        count = int(line[2:4],16)
    except Exception as e:
        print('  Bad byte count field:', e)
        ok = False
        continue
    rest = line[4:]
    # count includes address, data and checksum bytes
    # determine address length by type
    addr_len = {'0':2,'1':2,'2':3,'3':4,'5':2,'7':4,'8':3,'9':2}.get(typ)
    if addr_len is None:
        print('  Unknown record type S'+typ)
        addr_len = 2
    addr_chars = addr_len*2
    if len(rest) < addr_chars+2:
        print('  Line too short for address+checksum')
        ok = False
        continue
    addr_field = rest[:addr_chars]
    try:
        addr = int(addr_field,16)
    except Exception as e:
        print('  Bad address field:', e)
        ok = False
        continue
    # data + checksum
    data_chk = rest[addr_chars:]
    if len(data_chk) < 2:
        print('  Missing checksum')
        ok = False
        continue
    chk_field = data_chk[-2:]
    data_field = data_chk[:-2]
    # build byte list for checksum calc: count byte + address bytes + data bytes
    bytes_hex = [line[2:4]]
    # split addr_field into bytes
    for j in range(0,len(addr_field),2):
        bytes_hex.append(addr_field[j:j+2])
    # data bytes
    for j in range(0,len(data_field),2):
        bytes_hex.append(data_field[j:j+2])
    try:
        s = sum(int(x,16) for x in bytes_hex) & 0xFF
        calc_chk = (~s) & 0xFF
    except Exception as e:
        print('  Bad hex in fields:', e)
        ok = False
        continue
    found_chk = int(chk_field,16)
    print(f'  Type: S{typ}, Count: {count}, Address: 0x{addr:0{addr_len*2}X}, Data bytes: {len(data_field)//2}, Checksum: found=0x{found_chk:02X}, calc=0x{calc_chk:02X}, OK={found_chk==calc_chk}')
    if found_chk != calc_chk:
        ok = False
    # additional checks for data records (S1/S2/S3)
    if typ in ('1','2','3'):
        if addr % 2 != 0:
            print('  Warning: address is odd (not even) -> data may be misaligned for 16-bit words')
        if (len(data_field)//2) % 2 != 0:
            print('  Warning: data byte count is odd -> cannot form complete 16-bit words')
        # interpret little-endian 16-bit words
        data_bytes = [int(data_field[j:j+2],16) for j in range(0,len(data_field),2)]
        words = []
        for j in range(0,len(data_bytes)-1,2):
            lo = data_bytes[j]
            hi = data_bytes[j+1]
            w = (hi<<8)|lo
            words.append(w)
        print('  First 8 words (hex):', ' '.join(f'0x{w:04X}' for w in words[:8]))
        print('  First 8 words masked to 12 bits:', ' '.join(f'0x{(w&0xFFF):03X}' for w in words[:8]))

print('\nSummary:')
if ok:
    print('  bbb.srec appears to be a valid SREC file with correct checksums and consistent data layout.')
    sys.exit(0)
else:
    print('  Issues were found in bbb.srec (see messages above).')
    sys.exit(1)

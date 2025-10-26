#!/usr/bin/env python3
"""
demo/scripts/cal3demo.py

Upload demo/cal3.srec to the webdp8 server, set the PDP-8 switch register (S),
run the program in chunks until HALT is observed, fetch the line-printer output,
clean ANSI escapes, and save it to a file.

Defaults:
  - SREC: demo/cal3.srec
  - year: 1962
  - server: http://127.0.0.1:5000
  - output: printer/output.txt

This script uses the web wrapper endpoints created in tools/webdp8.py:
  POST /loader  (multipart file upload)
  PUT  /switch  (json {"val": <decimal>})
  GET  /trace   (params: start, cycles)
  GET  /output/printer

"""

import argparse
import os
import re
import sys
import time

try:
    import requests
except Exception as e:
    print("This script requires the 'requests' package. Install with: pip install requests", file=sys.stderr)
    raise


def upload_srec(server_url, srec_path):
    url = server_url.rstrip('/') + '/loader'
    with open(srec_path, 'rb') as f:
        files = {'file': (os.path.basename(srec_path), f)}
        resp = requests.post(url, files=files, timeout=30)
    resp.raise_for_status()
    return resp.json()


def set_switch(server_url, year):
    url = server_url.rstrip('/') + '/switch'
    resp = requests.put(url, json={'val': int(year)}, timeout=5)
    resp.raise_for_status()
    return resp.json()


def run_until_halt(server_url, start='0100', cycles=1024, max_iter=20, delay=0.01):
    url = server_url.rstrip('/') + '/trace'
    for i in range(max_iter):
        print(f"Trace iter {i+1}/{max_iter}...", flush=True)
        resp = requests.get(url, params={'start': start, 'cycles': cycles}, timeout=60)
        resp.raise_for_status()
        j = resp.json()
        halted = bool(j.get('halted'))
        steps = len(j.get('steps', []))
        print(f" -> halted={halted} steps={steps}")
        if halted:
            return j
        time.sleep(delay)
    raise RuntimeError(f"No HALT after {max_iter} iterations (start={start}, cycles={cycles})")


def fetch_printer(server_url):
    url = server_url.rstrip('/') + '/output/printer'
    resp = requests.get(url, timeout=10)
    resp.raise_for_status()
    j = resp.json()
    text = j.get('text', '')
    # strip simple ANSI SGR sequences
    clean = re.sub(r"\x1b\[[0-9;]*m", "", text)
    return clean


def main(argv=None):
    p = argparse.ArgumentParser(description='Run demo/cal3 via webdp8 and save printer output')
    p.add_argument('--srec', default='demo/cal3.srec', help='path to cal3.srec')
    p.add_argument('--year', type=int, default=1962, help='decimal year to place in the PDP-8 switch (S)')
    p.add_argument('--month', type=int, default=10, help='decimal month to write into MONTH_IN (1-12)')
    p.add_argument('--server', default='http://127.0.0.1:5000', help='webdp8 server base URL')
    p.add_argument('--out', default='printer/output.txt', help='output file to save printer text')
    p.add_argument('--start', default='0200', help='entry point to trace (octal string)')
    p.add_argument('--cycles', type=int, default=1024, help='cycles per trace chunk')
    p.add_argument('--max-iter', type=int, default=20, help='max trace chunks to run')
    p.add_argument('--raw', action='store_true', help='store the full printer output without collapsing to one line')
    args = p.parse_args(argv)

    if not os.path.exists(args.srec):
        print(f"SREC file not found: {args.srec}", file=sys.stderr)
        return 2

    print(f"Uploading SREC {args.srec} to {args.server}...")
    try:
        up = upload_srec(args.server, args.srec)
    except Exception as e:
        print("Failed to upload SREC:", e, file=sys.stderr)
        return 3
    print("Upload response start:", up.get('start'))

    print(f"Setting PDP-8 switch S to decimal {args.year}...")
    try:
        sw = set_switch(args.server, args.year)
    except Exception as e:
        print("Failed to set switch:", e, file=sys.stderr)
        return 4
    print("Switch set response:", sw)

    # write month into MONTH_IN (octal 2001)
    month_addr = '02001'
    print(f"Writing month {args.month} to MONTH_IN ({month_addr})...")
    try:
        resp = requests.put(args.server.rstrip('/') + '/mem', json={"addr": month_addr, "val": int(args.month)}, timeout=5)
        resp.raise_for_status()
        print("MONTH_IN write response:", resp.json())
    except Exception as e:
        print("Failed to write MONTH_IN:", e, file=sys.stderr)
        return 5

    print("Running until HALT (chunked)...")
    try:
        trace = run_until_halt(args.server, start=args.start, cycles=args.cycles, max_iter=args.max_iter)
    except Exception as e:
        print("Run failed:", e, file=sys.stderr)
        return 5

    print("Fetching printer output...")
    try:
        printer_output = fetch_printer(args.server)
    except Exception as e:
        print("Failed to fetch printer output:", e, file=sys.stderr)
        return 6

    if args.raw:
        payload = printer_output.rstrip('\r\n')
        if not payload.strip():
            print("No printer output captured", file=sys.stderr)
            return 6
        to_save = payload
    else:
        # Normalise to the most recent non-empty line to avoid stale data
        segments = [seg.strip() for seg in re.split(r'[\r\n]+', printer_output) if seg.strip()]
        final_line = segments[-1] if segments else printer_output.strip()

        if not final_line:
            print("No printer output captured", file=sys.stderr)
            return 6
        to_save = final_line

    # Save output
    os.makedirs(os.path.dirname(args.out) or '.', exist_ok=True)
    with open(args.out, 'w', encoding='utf-8') as f:
        f.write(to_save)
        if not to_save.endswith("\n"):
            f.write("\n")

    print(f"Printer output written to: {args.out}\n---BEGIN OUTPUT---\n")
    print(to_save)
    print("\n---END OUTPUT---")
    return 0


if __name__ == '__main__':
    sys.exit(main())

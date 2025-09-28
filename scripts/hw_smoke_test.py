#!/usr/bin/env python3
"""
hw_smoke_test.py

Automated hardware smoke test helper. Usage:
  python3 scripts/hw_smoke_test.py <serial_port> [out_file]

This script runs `download_sd.py` to fetch `logs.jsonl`, then validates the file
(ensures it's JSON lines, counts records, prints first/last timestamps and a small
health summary). Exits with code 0 on success, non-zero on failure.
"""
import sys
import subprocess
import json
import os
from datetime import datetime

DOWNLOAD_SCRIPT = os.path.join(os.path.dirname(__file__), 'download_sd.py')


def read_jsonl(path):
    records = []
    with open(path, 'r') as f:
        for i, line in enumerate(f):
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except Exception as e:
                raise ValueError(f"Invalid JSON on line {i+1}: {e}")
            records.append(obj)
    return records


def summary(records):
    if not records:
        return "No records"
    times = []
    wifi_ok = mqtt_ok = dht_ok = pm_ok = 0
    for r in records:
        ts = r.get('timestamp')
        if ts:
            times.append(ts)
        if r.get('wifi_ok'): wifi_ok += 1
        if r.get('mqtt_ok'): mqtt_ok += 1
        if r.get('dht_ok'): dht_ok += 1
        if r.get('pm_ok'): pm_ok += 1
    first = times[0] if times else 'N/A'
    last = times[-1] if times else 'N/A'
    return {
        'count': len(records),
        'first_timestamp': first,
        'last_timestamp': last,
        'wifi_ok': wifi_ok,
        'mqtt_ok': mqtt_ok,
        'dht_ok': dht_ok,
        'pm_ok': pm_ok,
    }


def main():
    if len(sys.argv) < 2:
        print("Usage: hw_smoke_test.py <serial_port> [out_file]")
        return 2
    port = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) >= 3 else 'logs.jsonl'

    print(f"Running serial download from {port} -> {out}")
    # run download_sd.py
    cp = subprocess.run([sys.executable, DOWNLOAD_SCRIPT, port, out])
    if cp.returncode != 0:
        print("download_sd.py failed")
        return 3
    if not os.path.exists(out):
        print(f"Expected output file {out} not found")
        return 4
    try:
        recs = read_jsonl(out)
    except Exception as e:
        print(f"Failed validating {out}: {e}")
        return 5
    s = summary(recs)
    print("Smoke test summary:")
    print(json.dumps(s, indent=2))
    # Basic success criteria: at least 1 record and timestamps present
    if s.get('count', 0) < 1:
        print("No records found: failing smoke test")
        return 6
    print("Smoke test passed")
    return 0


if __name__ == '__main__':
    sys.exit(main())

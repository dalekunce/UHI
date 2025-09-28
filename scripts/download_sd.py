#!/usr/bin/env python3
"""
download_sd.py

Host helper to download /logs.jsonl from the ESP32 over Serial.

Usage:
  python3 scripts/download_sd.py <serial_port> [out_file]

Example (macOS):
  python3 scripts/download_sd.py /dev/tty.SLAB_USBtoUART logs.jsonl

The firmware expects a single ASCII 'D' character to trigger the dump and
wraps the stream between lines:
  ===BEGIN_LOGS===
  ...file contents...
  ===END_LOGS===

If pyserial is not installed the script will print an install hint.
"""

import sys
import time

OUT_BEGIN = b"===BEGIN_LOGS===\n"
OUT_END = b"===END_LOGS===\n"

try:
    import serial
except Exception:
    print("pyserial is required. Install with: pip3 install pyserial")
    sys.exit(2)


def download(port, out_path, baud=115200, timeout=10):
    print(f"Opening {port} @ {baud}bps")
    ser = serial.Serial(port, baud, timeout=1)
    # flush any existing
    ser.reset_input_buffer()
    ser.reset_output_buffer()
    time.sleep(0.1)
    # send trigger
    ser.write(b'D')
    ser.flush()

    start_time = time.time()
    buf = bytearray()
    in_stream = False
    end_deadline = None
    while True:
        if time.time() - start_time > timeout and not in_stream:
            print("Timed out waiting for device to respond")
            ser.close()
            return False
        chunk = ser.read(256)
        if not chunk:
            # if we've started streaming, allow some time for completion
            if in_stream:
                if end_deadline is None:
                    end_deadline = time.time() + 2
                elif time.time() > end_deadline:
                    break
            else:
                continue
        buf.extend(chunk)
        if not in_stream:
            # search for begin marker
            ib = buf.find(OUT_BEGIN)
            if ib != -1:
                in_stream = True
                # drop everything before begin marker
                buf = buf[ib + len(OUT_BEGIN):]
        else:
            # check for end marker
            ie = buf.find(OUT_END)
            if ie != -1:
                # write up to the marker
                with open(out_path, 'wb') as f:
                    f.write(buf[:ie])
                ser.close()
                print(f"Saved logs to {out_path}")
                return True
    # fallback
    if in_stream and buf:
        with open(out_path, 'wb') as f:
            f.write(buf)
        ser.close()
        print(f"Saved logs to {out_path} (no end marker found)")
        return True
    ser.close()
    print("No data saved")
    return False


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: download_sd.py <serial_port> [out_file]")
        sys.exit(1)
    port = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) >= 3 else 'logs.jsonl'
    ok = download(port, out)
    sys.exit(0 if ok else 2)

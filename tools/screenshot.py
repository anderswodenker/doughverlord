#!/usr/bin/env python3
"""Holt einen Screenshot vom Geraet: schickt 'shot' ueber USB, dekodiert das
Base64-RGB565 und schreibt ein PNG. Aufruf: tools/screenshot.py out.png
Braucht pyserial und Pillow (liegen im PlatformIO-Python)."""
import base64, sys, time
import serial
from PIL import Image

port = serial.Serial('/dev/ttyACM0', 115200, timeout=0.05)
port.dtr = True

def grab():
    # Erst alles roh einsammeln, dann parsen: zeilenweises Lesen ist zu
    # langsam, der USB-CDC des S3 verwirft Bytes, wenn der Host nicht nachkommt.
    port.reset_input_buffer()
    port.write(b"shot\n")
    data = bytearray()
    t0 = time.time()
    while time.time() - t0 < 30:
        data += port.read(65536)
        if b"[shot] end" in data:
            break
    text = data.decode('ascii', 'replace')
    if '[shot] begin' not in text:
        return None
    head = text[text.index('[shot] begin'):].split('\n', 1)
    _, _, w, h, _ = head[0].split()
    body = head[1].split('[shot] end')[0]
    b64 = ''.join(l for l in body.split('\n') if l and not l.startswith('['))
    try:
        raw = base64.b64decode(b64)
    except Exception:
        return (int(w), int(h), None)
    return (int(w), int(h), raw)

t0 = time.time()
for attempt in range(3):
    got = grab()
    if got and got[2] is not None and len(got[2]) == got[0] * got[1] * 2:
        break
    print(f"Versuch {attempt + 1} unvollstaendig, nochmal", file=sys.stderr)
else:
    sys.exit("Screenshot fehlgeschlagen")
w, h, raw = got
img = Image.frombytes('RGB', (w, h), raw, 'raw', 'BGR;16')
img.save(sys.argv[1] if len(sys.argv) > 1 else 'shot.png')
print(f"{w}x{h}, {len(raw)} Bytes, {time.time()-t0:.1f}s")

# FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
"""Generate a four-second, 30 fps uncompressed AVI using only the standard library."""
import struct
import sys
from pathlib import Path

def chunk(tag, body):
    return tag + struct.pack("<I", len(body)) + body + bytes(len(body) % 2)

def group(tag, chunks):
    return chunk(b"LIST", tag + b"".join(chunks))

def make(path):
    w, h, fps, n = 320, 180, 30, 120
    size = w * h * 3
    head = chunk(b"avih", struct.pack("<14I", 1000000//fps, size*fps, 0, 16, n, 0, 1, size, w, h, 0, 0, 0, 0))
    stream = chunk(b"strh", struct.pack("<4s4sIHH8I4h", b"vids", b"DIB ", 0, 0, 0, 0, 1, fps, 0, n, size, 0xffffffff, 0, 0, 0, w, h))
    fmt = chunk(b"strf", struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, size, 0, 0, 0, 0))
    frames, index, offset = [], [], 4
    for k in range(n):
        pos = k * 3 % (w-40)
        data = bytearray(bytes([45, 30, 18]) * (w*h))
        for y in range(h//2, h//2+30):
            begin = (y*w+pos)*3
            data[begin:begin+120] = bytes([140, 230, 90]) * 40
        item = chunk(b"00db", data)
        frames.append(item)
        index.append(struct.pack("<4sIII", b"00db", 16, offset, size))
        offset += len(item)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(chunk(b"RIFF", b"AVI " + group(b"hdrl", [head, group(b"strl", [stream, fmt])]) + group(b"INFO", [chunk(b"IART", b"Faris Alsari / FMSGaming - Famous Gaming\0"), chunk(b"INAM", b"FrameBridge motion test\0")]) + group(b"movi", frames) + chunk(b"idx1", b"".join(index))))

if __name__ == "__main__":
    make(Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).parent/"results"/"motion-30.avi")


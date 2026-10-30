#!/usr/bin/env python3
"""Check a CR2 against what the soup module assumes in camera.

The module reads the first 256 KB of the CR2 (FILM_HDR), finds the IFD0
preview (compression 6, StripOffsets/StripByteCounts), and needs the JPEG
markers up to SOF0 inside that window to size the develop. The preview
itself is then read separately, so only its start has to fit.

usage: cr2info.py FILE.CR2 [...]
"""
import struct
import sys

FILM_HDR = 256 * 1024
TAGS = {0x100: "width", 0x101: "height", 0x103: "compression",
        0x111: "strip_offset", 0x117: "strip_bytes", 0x10F: "make", 0x110: "model"}


def ifd0(data):
    if data[:2] != b"II":
        raise ValueError("not little-endian TIFF")
    off = struct.unpack_from("<I", data, 4)[0]
    n = struct.unpack_from("<H", data, off)[0]
    out = {"ifd0_offset": off, "ifd0_entries": n}
    for i in range(n):
        tag, typ, cnt, val = struct.unpack_from("<HHII", data, off + 2 + 12 * i)
        if tag in (0x10F, 0x110):  # ASCII, stored at val when longer than 4 bytes
            raw = data[val:val + cnt] if cnt > 4 else struct.pack("<I", val)[:cnt]
            out[TAGS[tag]] = raw.split(b"\0")[0].decode("ascii", "replace")
        elif tag in TAGS:
            out[TAGS[tag]] = val & 0xFFFF if typ == 3 else val
    return out


def sof(data, start):
    """JPEG SOF0 width/height and the offset where it was found."""
    k = start + 2
    while k + 9 < len(data):
        if data[k] != 0xFF:
            k += 1
            continue
        while k < len(data) and data[k] == 0xFF:
            k += 1
        m = data[k]
        k += 1
        if m in (0xD8, 0xD9) or 0xD0 <= m <= 0xD7:
            continue
        seglen = struct.unpack_from(">H", data, k)[0]
        if m in (0xC0, 0xC1):
            h, w = struct.unpack_from(">HH", data, k + 3)
            return w, h, k
        k += seglen
    return None


def main():
    for path in sys.argv[1:]:
        with open(path, "rb") as f:
            data = f.read()
        info = ifd0(data)
        so, sb = info.get("strip_offset", 0), info.get("strip_bytes", 0)
        s = sof(data, so) if so else None
        soi = data[so:so + 2] == b"\xff\xd8"
        print(f"== {path}  ({len(data) / 1e6:.1f} MB)")
        print(f"   camera: {info.get('make')} / {info.get('model')}")
        print(f"   IFD0 at {info['ifd0_offset']}, {info['ifd0_entries']} entries, "
              f"compression {info.get('compression')} (module needs 6)")
        print(f"   preview: {info.get('width')}x{info.get('height')} (TIFF tags), "
              f"offset {so}, {sb / 1e6:.2f} MB, starts with SOI: {soi}")
        if s:
            w, h, at = s
            print(f"   JPEG SOF0: {w}x{h}, half = {w // 2}x{h // 2}; SOF0 at byte {at} "
                  f"-> inside first 256 KB: {at + 9 < FILM_HDR}")
        else:
            print("   JPEG SOF0 not found")
        ok = (info.get("compression") == 6 and so + 2 < FILM_HDR and s and s[2] + 9 < FILM_HDR)
        print(f"   module assumptions hold: {'YES' if ok else 'NO'}")


if __name__ == "__main__":
    main()

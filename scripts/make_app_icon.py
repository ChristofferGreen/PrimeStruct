#!/usr/bin/env python3
"""Draws the notepad app icon and writes it as an .icns (macOS only: uses sips and iconutil).

Usage: scripts/make_app_icon.py <output.icns>

The picture is a macOS-style rounded square holding a sheet of paper with a red
header, binding rings and a few lines of colour-coded "code". Everything is
rasterised here with signed distance fields (standard library only), so the icon
is reproducible and needs no image editor.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

SIZE = 1024


def rounded_distance(px, py, x0, y0, x1, y1, r):
    """Signed distance (negative inside) from a point to a rounded rectangle."""
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    hx, hy = (x1 - x0) / 2.0 - r, (y1 - y0) / 2.0 - r
    dx, dy = abs(px - cx) - hx, abs(py - cy) - hy
    ox, oy = max(dx, 0.0), max(dy, 0.0)
    return (ox * ox + oy * oy) ** 0.5 + min(max(dx, dy), 0.0) - r


def hex_color(value):
    return ((value >> 16) & 255, (value >> 8) & 255, value & 255)


def mix(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


class Canvas:
    def __init__(self):
        self.pixels = [[(0.0, 0.0, 0.0, 0.0)] * SIZE for _ in range(SIZE)]

    def shape(self, x0, y0, x1, y1, r, color, alpha=1.0, grow=0.0):
        """Composites a rounded rectangle; `color` is (r,g,b) or f(y) -> (r,g,b)."""
        pad = int(grow) + 2
        for y in range(max(0, int(y0) - pad), min(SIZE, int(y1) + pad)):
            row = self.pixels[y]
            rgb = color(y) if callable(color) else color
            for x in range(max(0, int(x0) - pad), min(SIZE, int(x1) + pad)):
                d = rounded_distance(x + 0.5, y + 0.5, x0, y0, x1, y1, r) - grow
                cover = min(max(0.5 - d, 0.0), 1.0) * alpha
                if cover <= 0.0:
                    continue
                br, bg, bb, ba = row[x]
                out_a = cover + ba * (1.0 - cover)
                row[x] = (
                    (rgb[0] * cover + br * ba * (1.0 - cover)) / out_a,
                    (rgb[1] * cover + bg * ba * (1.0 - cover)) / out_a,
                    (rgb[2] * cover + bb * ba * (1.0 - cover)) / out_a,
                    out_a,
                )

    def png(self, path):
        raw = bytearray()
        for row in self.pixels:
            raw.append(0)
            for r, g, b, a in row:
                raw += bytes((int(r + 0.5), int(g + 0.5), int(b + 0.5), int(a * 255 + 0.5)))

        def chunk(tag, data):
            body = tag + data
            return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

        with open(path, "wb") as f:
            f.write(b"\x89PNG\r\n\x1a\n")
            f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)))
            f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
            f.write(chunk(b"IEND", b""))


def draw():
    c = Canvas()
    top, bottom = hex_color(0x6FA0FF), hex_color(0x2D4FD6)
    # Body: the standard macOS icon grid (824 pt square in 1024) with a soft shadow.
    c.shape(100, 118, 924, 942, 185, (0, 0, 0), alpha=0.25, grow=10)
    c.shape(100, 100, 924, 924, 185, lambda y: mix(top, bottom, (y - 100) / 824.0))
    # Paper with a shadow.
    c.shape(262, 238, 762, 838, 40, (10, 20, 60), alpha=0.28, grow=12)
    c.shape(262, 214, 762, 814, 40, (252, 252, 250))
    # Header band: only its upper corners are round, so cover the lower ones with a plain strip.
    c.shape(262, 214, 762, 314, 40, hex_color(0xEF5A5A))
    c.shape(262, 280, 762, 314, 0, hex_color(0xEF5A5A))
    # Binding rings.
    for x in (372, 512, 652):
        c.shape(x - 22, 192, x + 22, 252, 22, hex_color(0x1B2554))
        c.shape(x - 11, 205, x + 11, 232, 11, hex_color(0xF6F6F6))
    # Lines of "code": a keyword, a string, a number, a type, a comment.
    rows = [
        [(0xC9569A, 312, 392), (0x2F3A5C, 410, 560)],
        [(0x2F3A5C, 340, 430), (0x3FA04F, 448, 700)],
        [(0x3C9FD0, 312, 420), (0xD9822B, 438, 520), (0x2F3A5C, 538, 640)],
        [(0x8A8F98, 312, 690)],
        [(0xC9569A, 312, 372), (0x3C9FD0, 390, 500), (0x2F3A5C, 518, 600)],
    ]
    for i, row in enumerate(rows):
        y = 372 + i * 84
        for color, x0, x1 in row:
            c.shape(x0, y, x1, y + 36, 18, hex_color(color))
    return c


def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 64
    out = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="appicon")
    try:
        master = os.path.join(work, "icon_1024.png")
        draw().png(master)
        iconset = os.path.join(work, "AppIcon.iconset")
        os.makedirs(iconset)
        for size in (16, 32, 128, 256, 512):
            for scale in (1, 2):
                pixels = size * scale
                name = "icon_%dx%d%s.png" % (size, size, "@2x" if scale == 2 else "")
                target = os.path.join(iconset, name)
                if pixels == SIZE:
                    shutil.copy(master, target)
                else:
                    subprocess.run(["sips", "-z", str(pixels), str(pixels), master, "--out", target],
                                   check=True, stdout=subprocess.DEVNULL)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        subprocess.run(["iconutil", "-c", "icns", iconset, "-o", out], check=True)
        shutil.copy(master, os.path.splitext(out)[0] + ".png")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())

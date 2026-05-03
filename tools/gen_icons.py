#!/usr/bin/env python3
"""
Rasterize the Glide sugar-glider mark to PNGs at common icon sizes.
Pure stdlib (struct + zlib): no PIL / cairo / inkscape required.

Output goes to assets/icons/ — both extensions then copy from there.
"""
import os
import struct
import zlib

# Path coordinates from assets/glide-logo.svg (viewBox 0..256).
PATH = [
    (128, 56), (152, 88), (220, 104), (200, 132), (156, 140), (168, 196),
    (128, 216), (88, 196), (100, 140), (56, 132), (36, 104), (104, 88),
]
# Central stripe rectangle (defines the eye axis).
STRIPE = [(124, 70), (132, 70), (132, 210), (124, 210)]
EYES = [(116, 86, 3.5), (140, 86, 3.5)]

BLUE = (0x3B, 0x5B, 0xDB)
DARK = (0x1E, 0x2A, 0x5E)

def point_in_poly(x, y, poly):
    n = len(poly)
    inside = False
    j = n - 1
    for i in range(n):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if ((yi > y) != (yj > y)) and (x < (xj - xi) * (y - yi) / (yj - yi + 1e-9) + xi):
            inside = not inside
        j = i
    return inside

def render_rgba(size):
    """Return a flat RGBA bytearray of `size`x`size`."""
    out = bytearray(size * size * 4)
    scale = 256.0 / size
    for py in range(size):
        for px in range(size):
            # Sample at pixel center, in original 256-space.
            x = (px + 0.5) * scale
            y = (py + 0.5) * scale
            i = (py * size + px) * 4
            color = None
            # Eyes win over stripe / body.
            for (cx, cy, r) in EYES:
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    color = DARK
                    break
            if color is None and point_in_poly(x, y, STRIPE):
                color = DARK
            if color is None and point_in_poly(x, y, PATH):
                color = BLUE
            if color is None:
                # Transparent background.
                out[i + 3] = 0
                continue
            out[i + 0] = color[0]
            out[i + 1] = color[1]
            out[i + 2] = color[2]
            out[i + 3] = 255
    return out

def write_png(path, size):
    rgba = render_rgba(size)
    raw = bytearray()
    for y in range(size):
        raw.append(0)  # filter type: None
        raw.extend(rgba[y * size * 4 : (y + 1) * size * 4])
    compressed = zlib.compress(bytes(raw), 9)

    def chunk(typ, data):
        return (struct.pack(">I", len(data))
                + typ + data
                + struct.pack(">I", zlib.crc32(typ + data) & 0xFFFFFFFF))

    sig = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0)  # 8-bit RGBA
    with open(path, "wb") as f:
        f.write(sig)
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", compressed))
        f.write(chunk(b"IEND", b""))
    print(f"  {path}  {size}x{size}")

def main():
    out_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets", "icons")
    os.makedirs(out_dir, exist_ok=True)
    for s in (16, 32, 48, 64, 128, 256):
        write_png(os.path.join(out_dir, f"glide-{s}.png"), s)
    print("done")

if __name__ == "__main__":
    main()

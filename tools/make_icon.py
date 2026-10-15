"""Generates resources/icon.png and resources/icon.ico (run: python tools/make_icon.py, needs Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw

SIZE = 1024
OUT = Path(__file__).resolve().parent.parent / "resources"


def gradient(size, left, right):
    img = Image.new("RGB", (size, size))
    px = img.load()
    for x in range(size):
        t = x / (size - 1)
        color = tuple(round(a + (b - a) * t) for a, b in zip(left, right))
        for y in range(size):
            px[x, y] = color
    return img


def arrow(draw, y, direction):
    """A horizontal arrow (shaft + head) centred vertically on y."""
    x0, x1, shaft, head, hl = 250, 774, 46, 118, 170
    if direction > 0:
        draw.rectangle([x0, y - shaft // 2, x1 - hl + 20, y + shaft // 2], fill="white")
        draw.polygon([(x1, y), (x1 - hl, y - head), (x1 - hl, y + head)], fill="white")
    else:
        draw.rectangle([x0 + hl - 20, y - shaft // 2, x1, y + shaft // 2], fill="white")
        draw.polygon([(x0, y), (x0 + hl, y - head), (x0 + hl, y + head)], fill="white")


def main():
    OUT.mkdir(exist_ok=True)
    base = gradient(SIZE, (99, 102, 241), (168, 85, 247)).convert("RGBA")
    glyph = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(glyph)
    arrow(d, 390, +1)
    arrow(d, 634, -1)
    base.alpha_composite(glyph)

    mask = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, SIZE - 1, SIZE - 1], radius=224, fill=255)
    base.putalpha(mask)

    base.resize((256, 256), Image.LANCZOS).save(OUT / "icon.png")
    base.save(OUT / "icon.ico", sizes=[(s, s) for s in (16, 24, 32, 48, 64, 128, 256)])


if __name__ == "__main__":
    main()

"""Generate the compact two-colour LCD logo from the canonical PNG asset."""

from pathlib import Path

from PIL import Image


PROJECT_DIR = Path(__file__).resolve().parents[1]
SOURCE = PROJECT_DIR / "data" / "casklogicmark.png"
OUTPUT = PROJECT_DIR / "include" / "DisplayLogo.h"
WIDTH = 80
HEIGHT = 80
NAVY = (23, 50, 77)
TEAL = (47, 143, 157)


def distance(left: tuple[int, int, int], right: tuple[int, int, int]) -> int:
    return sum((a - b) ** 2 for a, b in zip(left, right))


image = Image.open(SOURCE).convert("RGBA")
bounds = image.getbbox()
if bounds is None:
    raise RuntimeError("The CaskLogic logo is empty")
image = image.crop(bounds)
image.thumbnail((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
canvas = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
canvas.alpha_composite(image, ((WIDTH - image.width) // 2, (HEIGHT - image.height) // 2))

navy_mask = bytearray((WIDTH * HEIGHT + 7) // 8)
teal_mask = bytearray((WIDTH * HEIGHT + 7) // 8)
for y in range(HEIGHT):
    for x in range(WIDTH):
        red, green, blue, alpha = canvas.getpixel((x, y))
        if alpha < 64:
            continue
        index = y * WIDTH + x
        target = teal_mask if distance((red, green, blue), TEAL) < distance((red, green, blue), NAVY) else navy_mask
        target[index // 8] |= 1 << (7 - index % 8)


def array(name: str, data: bytearray) -> str:
    rows = []
    for offset in range(0, len(data), 16):
        rows.append(", ".join(f"0x{value:02X}" for value in data[offset : offset + 16]))
    return f"inline constexpr uint8_t {name}[] PROGMEM = {{\n  " + ",\n  ".join(rows) + "\n};\n"


generated = """// Generated from data/casklogicmark.png. Do not edit manually.
#pragma once

#include <Arduino.h>

inline constexpr uint8_t DISPLAY_LOGO_WIDTH = 80;
inline constexpr uint8_t DISPLAY_LOGO_HEIGHT = 80;
"""
generated += array("DISPLAY_LOGO_NAVY", navy_mask)
generated += array("DISPLAY_LOGO_TEAL", teal_mask)
OUTPUT.write_text(generated, encoding="utf-8", newline="\n")

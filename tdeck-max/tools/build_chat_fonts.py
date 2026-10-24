"""Pack native 1-bit chat fonts (ASCII and Latin-1: à è é ì ò ù ...); keep the original splash/fonts untouched."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
for name, size, line_height in [('body', 13, 15), ('meta', 11, 13), ('title', 16, 20)]:
    font = ImageFont.truetype(str(ROOT / 'assets/fonts/LiberationSans-Bold.ttf'), size)
    data, glyphs = [], []
    for code in range(32, 256):
        char = chr(code)
        if 127 <= code < 160 or code == 0xAD:  # C1 controls and the soft hyphen: no ink, no advance
            glyphs.append((len(data), 0, 0, 0, 0, 0))
            continue
        if code == 0xA0:  # no-break space draws as a space
            char = ' '
        x, y, right, bottom = font.getbbox(char, anchor='ls')
        width, height = right - x, bottom - y
        image = Image.new('L', (max(width, 1), max(height, 1)), 255)
        ImageDraw.Draw(image).text((-x, -y), char, font=font, fill=0, anchor='ls')
        offset = len(data)
        bits = [int(pixel < 160) for pixel in image.get_flattened_data()] if width * height else []
        bits += [0] * (-len(bits) % 8)
        data += [sum(bits[i + j] << (7 - j) for j in range(8)) for i in range(0, len(bits), 8)]
        glyphs.append((offset, width, height, math.ceil(font.getlength(char)), x, y))
    packed = ','.join('{' + ','.join(map(str, glyph)) + '}' for glyph in glyphs)
    (ROOT / f'src/ui/chat_{name}_font.h').write_text(
        '#pragma once\n#include <Adafruit_GFX.h>\n'
        f'namespace mm_chat_{name} {{\n'
        'const uint8_t bitmap[] PROGMEM={' + ','.join(map(str, data)) + '};\n'
        'const GFXglyph glyphs[] PROGMEM={' + packed + '};\n'
        'const GFXfont Text PROGMEM={(uint8_t*)bitmap,(GFXglyph*)glyphs,32,255,'
        + str(line_height) + '};\n}\n')

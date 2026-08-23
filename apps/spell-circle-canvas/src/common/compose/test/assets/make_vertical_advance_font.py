#!/usr/bin/env python3
"""Generates VerticalAdvance.ttf — the instrument for the substitution gate.

A code-point substitution is admitted only when the replacement advances
the pen exactly as the original does ALONG THE AXIS THE RUN STEPS ON: a
level run steps by the horizontal advance, an upright column by the
vertical one. Proving the gate reads the right axis needs a face where the
two axes DISAGREE about a pair, which no ordinary text font provides:
letters in a Latin face all share one vertical advance, and CJK faces make
both axes one em.

The font: units/em 1000, glyphs for ".notdef", space and A B C, each a
plain filled rectangle in its own place so a substitution changes pixels.
Its metrics are the whole point:

    glyph   horizontal   vertical
      A          500        1000
      B          800        1000     equal DOWN a column, not along a line
      C          500         700     equal ALONG a line, not down a column

So a run gated on the wrong axis reaches the opposite verdict on both
pairs, and the four renders A->B and A->C in each orientation pin the gate
to the axis its run actually advances on. `vhea`/`vmtx` carry the vertical
advances; without them HarfBuzz would hand every glyph one fallback
advance and B and C would be indistinguishable down a column.

No license question arises: the geometry here is authored by this script
(public domain, CC0).

Regenerate (requires fonttools):

    python3 make_vertical_advance_font.py

and commit the refreshed VerticalAdvance.ttf alongside.
"""

import os

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

UPEM = 1000
ASCENT, DESCENT = 800, -200

# glyph: (horizontal advance, vertical advance, ink box)
LETTERS = {
    "A": (500, 1000, (60, 0, 440, 700)),
    "B": (800, 1000, (60, 0, 740, 400)),
    "C": (500, 700, (60, 300, 440, 700)),
}


def bar_glyph(box):
    x_min, y_min, x_max, y_max = box
    pen = TTGlyphPen(None)
    pen.moveTo((x_min, y_min))
    pen.lineTo((x_max, y_min))
    pen.lineTo((x_max, y_max))
    pen.lineTo((x_min, y_max))
    pen.closePath()
    return pen.glyph()


def main():
    order = [".notdef", "space", *sorted(LETTERS)]
    fb = FontBuilder(UPEM, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap({0x20: "space", **{ord(ch): ch for ch in LETTERS}})

    glyphs = {
        ".notdef": bar_glyph((50, 0, 450, 700)),
        "space": TTGlyphPen(None).glyph(),
    }
    horizontal = {".notdef": (500, 50), "space": (400, 0)}
    # (advance, top side bearing) — the bearing places the ink under the
    # glyph's vertical origin the way the side bearing places it after the
    # horizontal one.
    vertical = {".notdef": (1000, 100), "space": (1000, 0)}
    for letter, (advance, vertical_advance, box) in LETTERS.items():
        glyphs[letter] = bar_glyph(box)
        horizontal[letter] = (advance, box[0])
        vertical[letter] = (vertical_advance, ASCENT - box[3])
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(horizontal)
    fb.setupHorizontalHeader(ascent=ASCENT, descent=DESCENT)
    fb.setupVerticalMetrics(vertical)
    fb.setupVerticalHeader(ascent=UPEM // 2, descent=-(UPEM // 2), lineGap=0)
    fb.setupNameTable(
        {
            "familyName": "Vertical Advance Test",
            "styleName": "Regular",
            "psName": "VerticalAdvanceTest-Regular",
            "licenseDescription": "Generated test asset; public domain (CC0).",
        }
    )
    fb.setupOS2(
        sTypoAscender=ASCENT,
        sTypoDescender=DESCENT,
        usWinAscent=ASCENT,
        usWinDescent=-DESCENT,
    )
    fb.setupPost()

    out = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "VerticalAdvance.ttf"
    )
    fb.save(out)
    print(f"wrote {out} ({os.path.getsize(out)} bytes)")


if __name__ == "__main__":
    main()

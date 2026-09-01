#!/usr/bin/env python3
"""Generates VerticalFeatures.ttf — the instrument for vertical OpenType
features.

A vertical setting asks the face for more than rotated forms: substitutions
that swap in a vertical shape, alternates that recentre punctuation on the
column axis, kana forms cut for a column, metrics that tighten a pair. Which
of those the shaper applies on its own and which one must ask for is a fact
about the shaper, and no ordinary face lets a test see it: a real CJK font
carries the same substitution under several tags, so a glyph that changed
says nothing about which feature changed it.

This font gives every feature its own visible consequence and shares none:

    A          'vert' → A.vert            the vertical form of a letter
    B          'vrt2' → B.vrt2            a letter ONLY the rotation set turns
    S          'vkna' → S.vkna            the small-kana form
    P          'valt' shifts the ink 200 units down the column, advance kept
    Q          'vpal' shortens the advance by 300 units
    T          'vhal' halves the advance
    R R        'vkrn' shortens the pair's first advance by 200 units

'vert' covers A and not B, the way a real face's rotation set is the wider
of the two: a run that turned B is a run 'vrt2' ran on, and one that turned
only A is the shaper's own doing.

So a shaped run names the feature that ran: a glyph id says which
substitution applied, a position says whether an alternate did, and an
advance says whether metrics did. Every letter is a plain filled rectangle
in its own place, so a substitution also changes pixels.

`vhea`/`vmtx` carry the vertical advances; without them HarfBuzz hands every
glyph one fallback advance and the metrics features have nothing to alter.

No license question arises: the geometry here is authored by this script
(public domain, CC0).

Regenerate (requires fonttools):

    python3 make_vertical_features_font.py

and commit the refreshed VerticalFeatures.ttf alongside.
"""

import os

from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

UPEM = 1000
ASCENT, DESCENT = 800, -200

# glyph: (horizontal advance, vertical advance, ink box)
LETTERS = {
    "A": (1000, 1000, (100, 0, 900, 300)),
    "A.vert": (1000, 1000, (100, 300, 900, 600)),
    "B": (1000, 1000, (100, 0, 900, 300)),
    "B.vrt2": (1000, 1000, (100, 600, 900, 900)),
    "S": (1000, 1000, (100, 0, 500, 400)),
    "S.vkna": (1000, 1000, (500, 0, 900, 400)),
    "P": (1000, 1000, (300, 200, 700, 600)),
    "Q": (1000, 1000, (200, 200, 800, 600)),
    "T": (1000, 1000, (350, 0, 650, 800)),
    "R": (1000, 1000, (400, 100, 600, 700)),
}

# The substitutions and adjustments each feature makes, one consequence per
# feature so a shaped run names which one ran.
FEATURES = """
languagesystem DFLT dflt;
languagesystem latn dflt;

feature vert {
    sub A by A.vert;
} vert;

feature vrt2 {
    sub B by B.vrt2;
} vrt2;

feature vkna {
    sub S by S.vkna;
} vkna;

feature valt {
    pos P <0 -200 0 0>;
} valt;

feature vpal {
    pos Q <0 0 0 -300>;
} vpal;

feature vhal {
    pos T <0 0 0 -500>;
} vhal;

feature vkrn {
    pos R R <0 0 0 -200>;
} vkrn;
"""


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
    order = [".notdef", "space", *LETTERS]
    fb = FontBuilder(UPEM, isTTF=True)
    fb.setupGlyphOrder(order)
    # Only the base letters are reachable from text; the alternates are
    # reached exclusively through the features that substitute them, which
    # is what makes a substituted glyph id proof that a feature ran.
    fb.setupCharacterMap(
        {
            0x20: "space",
            **{ord(name): name for name in LETTERS if "." not in name},
        }
    )

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
            "familyName": "Vertical Features Test",
            "styleName": "Regular",
            "psName": "VerticalFeaturesTest-Regular",
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
    addOpenTypeFeaturesFromString(fb.font, FEATURES)

    out = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "VerticalFeatures.ttf"
    )
    fb.save(out)
    print(f"wrote {out} ({os.path.getsize(out)} bytes)")


if __name__ == "__main__":
    main()

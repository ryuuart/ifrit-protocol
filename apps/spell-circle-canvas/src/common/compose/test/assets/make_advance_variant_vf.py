#!/usr/bin/env python3
"""Generates AdvanceVariant.ttf — the §35.3 test instrument.

`ComposeVariationDrive.AdvanceVariantAxisIsRefused` needs a variable face
whose wght axis MOVES GLYPH ADVANCES (most text VFs do; the macOS system
UI face declares no wght at all, so the test skipped on every machine).
Rather than committing a third-party font binary, this script builds a
tiny purpose-built VF the way SigilImage's test assets are committed:
generated once, checked in beside the test, regenerable from this file.

The font: units/em 1000, glyphs for ".notdef", space, and the letters of
the test string "WEIGHT" (W E I G H T), each a plain filled rectangle.
One axis, wght 100..900 (default 400), two masters:

    wght 100:  advance 500, bar 100..400 x 0..700
    wght 900:  advance 900, bar 100..800 x 0..700

Advances interpolate 500 -> 900 across the axis, so
FontContext::axisIsAdvanceInvariant(face, "wght") answers FALSE — the
advance-VARIANT axis the refusal path needs. No license question arises:
the geometry here is authored by this script (public domain, CC0).

Regenerate (requires fonttools):

    python3 make_advance_variant_vf.py

and commit the refreshed AdvanceVariant.ttf alongside.
"""

import os

from fontTools.designspaceLib import AxisDescriptor, DesignSpaceDocument, SourceDescriptor
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools import varLib

UPEM = 1000
LETTERS = "WEIGHT"  # the exact string the test shapes
GLYPH_ORDER = [".notdef", "space"] + sorted(set(LETTERS))


def bar_glyph(x_min, x_max, y_min=0, y_max=700):
    pen = TTGlyphPen(None)
    pen.moveTo((x_min, y_min))
    pen.lineTo((x_max, y_min))
    pen.lineTo((x_max, y_max))
    pen.lineTo((x_min, y_max))
    pen.closePath()
    return pen.glyph()


def master(advance, bar_right):
    fb = FontBuilder(UPEM, isTTF=True)
    fb.setupGlyphOrder(GLYPH_ORDER)
    fb.setupCharacterMap({0x20: "space", **{ord(ch): ch for ch in set(LETTERS)}})
    glyphs = {".notdef": bar_glyph(50, 450, 0, 700), "space": TTGlyphPen(None).glyph()}
    metrics = {".notdef": (500, 50), "space": (400, 0)}
    for ch in set(LETTERS):
        glyphs[ch] = bar_glyph(100, bar_right)
        metrics[ch] = (advance, 100)
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupNameTable(
        {
            "familyName": "Advance Variant Test",
            "styleName": "Regular",
            "psName": "AdvanceVariantTest-Regular",
            "licenseDescription": "Generated test asset; public domain (CC0).",
        }
    )
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
    fb.setupPost()
    return fb.font


def main():
    doc = DesignSpaceDocument()
    axis = AxisDescriptor()
    axis.minimum, axis.default, axis.maximum = 100, 400, 900
    axis.name, axis.tag = "Weight", "wght"
    doc.addAxis(axis)

    light = SourceDescriptor()
    light.font = master(advance=500, bar_right=400)
    light.location = {"Weight": 100}
    doc.addSource(light)

    # The DEFAULT master must exist at the default location for varLib.
    default = SourceDescriptor()
    default.font = master(advance=650, bar_right=550)
    default.location = {"Weight": 400}
    default.copyLib = default.copyInfo = True
    doc.addSource(default)

    heavy = SourceDescriptor()
    heavy.font = master(advance=900, bar_right=800)
    heavy.location = {"Weight": 900}
    doc.addSource(heavy)

    vf, _, _ = varLib.build(doc)
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "AdvanceVariant.ttf")
    vf.save(out)
    print(f"wrote {out} ({os.path.getsize(out)} bytes)")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Builds the instrument faces every test binary in this tree shapes with.

An instrument face is a font with EXACTLY the property one test case has to
ask about and nothing else: a known advance, a ligature, an axis that moves
advances beside one that does not, a script's coverage, a mark that carries
no advance. A case that asks such a question of whatever face the machine
happens to have installed is a case that answers differently per machine and
skips on the machines that answer nothing, so every one of them shapes a
face from this directory instead.

The geometry is rectangles and triangles authored here, so no license
question arises (public domain, CC0). Each face is a few kilobytes.

    Sans.ttf         known advances for letters, punctuation and the space,
                     x-height 500, cap height 700, an `ffi` ligature under
                     `liga`, proportional digits that `tnum` equalises
    Variable.ttf     wght and wdth that MOVE advances, GRAD that does not
                     and moves ink instead
    Optical.ttf      an A and a V whose diagonals lean past their advances,
                     over an `n` for the reference an optical kerner reads
    Marks.ttf        Latin bases and the whole combining-diacritics block,
                     every mark zero-advance and separately encoded
    Arabic.ttf       the letters of a right-to-left sentence, and lam+alef
                     ligated under `rlig`
    Devanagari.ttf   the letters of a conjunct-forming word, virama included
    Cuneiform.ttf    four characters beyond the basic plane
    HanSans.ttf      one Han character and one kana, as one family
    HanSerif.ttf     the same coverage as a SECOND family, so a fallback
                     resolver has two faces to choose between

Regenerate (requires fonttools) and commit what it writes:

    python3 make_instruments.py

Check the committed faces still carry what the cases read off them:

    python3 make_instruments.py --self-test
"""

import argparse
import os
import sys

from fontTools import varLib
from fontTools.designspaceLib import (
    AxisDescriptor,
    DesignSpaceDocument,
    SourceDescriptor,
)
from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

UPEM = 1000
ASCENT, DESCENT = 800, -200
CAP_HEIGHT, X_HEIGHT = 700, 500

HERE = os.path.dirname(os.path.abspath(__file__))

# The `head` timestamp every face is stamped with — 2000-01-01 in the epoch
# a font counts from. Fixed rather than read off the clock so that
# regenerating a face whose glyphs did not move rewrites the same bytes.
FIXED_TIMESTAMP = 3029529600

# The combining marks a stacked base carries. The whole block, so a test may
# stack any of them without asking which ones this generator remembered.
COMBINING = range(0x0300, 0x0370)


# ---------------------------------------------------------------------------
# Outlines


def polygon(*points):
    """A closed contour through `points`, as a TrueType glyph."""
    pen = TTGlyphPen(None)
    pen.moveTo(points[0])
    for point in points[1:]:
        pen.lineTo(point)
    pen.closePath()
    return pen.glyph()


def bar(x_min, x_max, y_min=0, y_max=CAP_HEIGHT):
    """A filled rectangle — a letter whose only property is its extent."""
    return polygon((x_min, y_min), (x_max, y_min), (x_max, y_max), (x_min, y_max))


def blank():
    """No contour at all — a space."""
    return TTGlyphPen(None).glyph()


# ---------------------------------------------------------------------------
# Assembly


def glyph_name(code_point):
    """A name for the glyph at `code_point`, unique and readable in a dump."""
    if 0x41 <= code_point <= 0x5A or 0x61 <= code_point <= 0x7A:
        return chr(code_point)
    if 0x30 <= code_point <= 0x39:
        return "digit" + chr(code_point)
    if code_point == 0x20:
        return "space"
    return "uni%04X" % code_point if code_point <= 0xFFFF else "u%05X" % code_point


def build(family, glyphs, metrics, character_map, features=None, upem=UPEM):
    """A static TrueType face over `glyphs`, with `metrics` as (advance, lsb)."""
    order = [".notdef"] + [name for name in glyphs if name != ".notdef"]
    builder = FontBuilder(upem, isTTF=True)
    builder.setupGlyphOrder(order)
    builder.setupCharacterMap(character_map)
    complete = {".notdef": bar(50, 450, 0, CAP_HEIGHT), **glyphs}
    builder.setupGlyf(complete)
    builder.setupHorizontalMetrics({".notdef": (500, 50), **metrics})
    builder.setupHorizontalHeader(ascent=ASCENT, descent=DESCENT)
    builder.setupNameTable(
        {
            "familyName": family,
            "styleName": "Regular",
            "psName": family.replace(" ", "") + "-Regular",
            "licenseDescription": "Generated test instrument; public domain (CC0).",
        }
    )
    builder.setupOS2(
        sTypoAscender=ASCENT,
        sTypoDescender=DESCENT,
        usWinAscent=ASCENT,
        usWinDescent=-DESCENT,
        sxHeight=X_HEIGHT,
        sCapHeight=CAP_HEIGHT,
    )
    builder.setupPost()
    if features:
        addOpenTypeFeaturesFromString(builder.font, features)
    return builder.font


def save(font, name):
    # A font builder stamps the clock into `head`, which would make every
    # regeneration a diff against a binary whose glyphs did not move. These
    # are committed files, so the stamp is fixed and a rerun that changes
    # nothing writes nothing.
    font["head"].created = font["head"].modified = FIXED_TIMESTAMP
    path = os.path.join(HERE, name)
    font.save(path)
    return path


# ---------------------------------------------------------------------------
# Sans — advances, a ligature, and figures a feature equalises

LIGATURE_ADVANCE = 1500  # f + f + i set as one glyph
LETTER_ADVANCE = 600
SPACE_ADVANCE = 300
PUNCTUATION_ADVANCE = 400

# The lowercase letters whose ink reaches the descent, so an underline
# that skips ink has something to skip and a line's descent is a letter's.
DESCENDING = "gjpqy"
TABULAR_ADVANCE = 600

# Every ASCII mark that is not a letter, a digit or the space, and the few
# beyond ASCII that prose set through this face reaches: the no-break space
# (blank, on the space's advance), the soft hyphen, the hyphen, the dashes,
# the curly quotes and the ellipsis. Each sits on PUNCTUATION_ADVANCE, so a
# sentence's width is a sum a case can write down, and none is a letter, so
# a word boundary is still where the breaker puts it.
PUNCTUATION = (
    list(range(0x21, 0x30))
    + list(range(0x3A, 0x41))
    + list(range(0x5B, 0x61))
    + list(range(0x7B, 0x7F))
    + [0x00AD, 0x2010, 0x2011, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2026]
)


def digit_advance(digit):
    """A DIFFERENT advance per figure, so a face's default figures are
    proportional and the feature that equalises them has something to do."""
    return 460 + 20 * digit


def sans():
    glyphs = {"space": blank()}
    metrics = {"space": (SPACE_ADVANCE, 0)}
    character_map = {0x20: "space"}

    for code_point in list(range(0x41, 0x5B)) + list(range(0x61, 0x7B)) + [0x00DF]:
        name = glyph_name(code_point)
        top = CAP_HEIGHT if code_point < 0x5B else X_HEIGHT
        bottom = DESCENT if chr(code_point) in DESCENDING else 0
        glyphs[name] = bar(100, 500, bottom, top)
        metrics[name] = (LETTER_ADVANCE, 100)
        character_map[code_point] = name

    for digit in range(10):
        name = glyph_name(0x30 + digit)
        advance = digit_advance(digit)
        glyphs[name] = bar(80, advance - 80, 0, CAP_HEIGHT)
        metrics[name] = (advance, 80)
        character_map[0x30 + digit] = name
        # The tabular figure: the same ink on one shared advance.
        glyphs[name + ".tnum"] = bar(80, TABULAR_ADVANCE - 80, 0, CAP_HEIGHT)
        metrics[name + ".tnum"] = (TABULAR_ADVANCE, 80)

    glyphs["uni00A0"] = blank()
    metrics["uni00A0"] = (SPACE_ADVANCE, 0)
    character_map[0x00A0] = "uni00A0"
    for code_point in PUNCTUATION:
        name = glyph_name(code_point)
        glyphs[name] = bar(100, PUNCTUATION_ADVANCE - 100, 0, X_HEIGHT // 2)
        metrics[name] = (PUNCTUATION_ADVANCE, 100)
        character_map[code_point] = name

    glyphs["f_f_i"] = bar(100, LIGATURE_ADVANCE - 100, 0, CAP_HEIGHT)
    metrics["f_f_i"] = (LIGATURE_ADVANCE, 100)

    tabular = "\n".join(
        "    sub %s by %s.tnum;" % (glyph_name(0x30 + d), glyph_name(0x30 + d))
        for d in range(10)
    )
    features = (
        "feature liga {\n"
        "    sub f f i by f_f_i;\n"
        "} liga;\n"
        "feature tnum {\n" + tabular + "\n} tnum;\n"
    )
    return build("Sigil Instrument Sans", glyphs, metrics, character_map, features)


# ---------------------------------------------------------------------------
# Variable — two axes that move advances and one that refuses to

WGHT_RANGE = (100.0, 400.0, 900.0)
WDTH_RANGE = (50.0, 100.0, 200.0)
GRAD_RANGE = (-100.0, 0.0, 150.0)

# What each master sets: the letter advance, and how far the ink reaches.
# GRAD's two masters hold the advance and move the ink alone, which is the
# whole property the substitution gate probes for.
VARIABLE_MASTERS = [
    # (location, letter advance, ink right edge)
    ({"Weight": 100, "Width": 100, "Grade": 0}, 500, 380),
    ({"Weight": 400, "Width": 100, "Grade": 0}, 600, 500),
    ({"Weight": 900, "Width": 100, "Grade": 0}, 800, 700),
    ({"Weight": 400, "Width": 50, "Grade": 0}, 400, 320),
    ({"Weight": 400, "Width": 200, "Grade": 0}, 1000, 860),
    ({"Weight": 400, "Width": 100, "Grade": -100}, 600, 300),
    ({"Weight": 400, "Width": 100, "Grade": 150}, 600, 780),
]


def variable_master(advance, ink_right):
    glyphs = {"space": blank()}
    metrics = {"space": (SPACE_ADVANCE, 0)}
    character_map = {0x20: "space"}
    code_points = (
        list(range(0x41, 0x5B)) + list(range(0x61, 0x7B)) + list(range(0x30, 0x3A))
    )
    for code_point in code_points:
        name = glyph_name(code_point)
        glyphs[name] = bar(100, ink_right, 0, CAP_HEIGHT)
        metrics[name] = (advance, 100)
        character_map[code_point] = name
    return build("Sigil Instrument Variable", glyphs, metrics, character_map)


def variable():
    document = DesignSpaceDocument()
    for name, tag, (low, default, high) in (
        ("Weight", "wght", WGHT_RANGE),
        ("Width", "wdth", WDTH_RANGE),
        ("Grade", "GRAD", GRAD_RANGE),
    ):
        axis = AxisDescriptor()
        axis.name, axis.tag = name, tag
        axis.minimum, axis.default, axis.maximum = low, default, high
        document.addAxis(axis)

    default_location = {"Weight": 400, "Width": 100, "Grade": 0}
    for location, advance, ink_right in VARIABLE_MASTERS:
        source = SourceDescriptor()
        source.font = variable_master(advance, ink_right)
        source.location = location
        if location == default_location:
            source.copyLib = source.copyInfo = True
        document.addSource(source)

    font, _, _ = varLib.build(document)
    return font


# ---------------------------------------------------------------------------
# Optical — a pair whose outlines leave more white than the face's own even
# pair, so measuring the outlines closes it


def optical():
    glyphs = {
        "space": blank(),
        # The reference an optical kerner reads: what this face calls an
        # even pair is what its own n leaves beside itself.
        "n": bar(80, 520, 0, X_HEIGHT),
        "o": bar(100, 500, 0, X_HEIGHT),
        # Diagonals that lean past each other: set adjacent, A's apex stands
        # clear of V's mouth over every band they share.
        "A": polygon((40, 0), (560, 0), (300, CAP_HEIGHT)),
        "V": polygon((40, CAP_HEIGHT), (560, CAP_HEIGHT), (300, 0)),
    }
    metrics = {
        "space": (SPACE_ADVANCE, 0),
        "n": (LETTER_ADVANCE, 80),
        "o": (LETTER_ADVANCE, 100),
        "A": (LETTER_ADVANCE, 40),
        "V": (LETTER_ADVANCE, 40),
    }
    character_map = {0x20: "space", 0x6E: "n", 0x6F: "o", 0x41: "A", 0x56: "V"}
    return build("Sigil Instrument Optical", glyphs, metrics, character_map)


# ---------------------------------------------------------------------------
# Marks — bases and a whole block of zero-advance combining marks


def marks():
    glyphs = {"space": blank()}
    metrics = {"space": (SPACE_ADVANCE, 0)}
    character_map = {0x20: "space"}

    bases = list(range(0x41, 0x5B)) + list(range(0x61, 0x7B)) + [0x01A0]
    for code_point in bases:
        name = glyph_name(code_point)
        top = CAP_HEIGHT if code_point < 0x5B or code_point > 0xFF else X_HEIGHT
        glyphs[name] = bar(100, 500, 0, top)
        metrics[name] = (LETTER_ADVANCE, 100)
        character_map[code_point] = name

    # Every mark carries NO advance, so a base with any number of them set on
    # it measures exactly as wide as the base alone. Nothing composes them
    # into the base either: a mark stays its own glyph, which is what a case
    # about clusters needs to see.
    for index, code_point in enumerate(COMBINING):
        name = glyph_name(code_point)
        low = CAP_HEIGHT + 20 * (index % 5)
        glyphs[name] = bar(150, 450, low, low + 15)
        metrics[name] = (0, 150)
        character_map[code_point] = name
    return build("Sigil Instrument Marks", glyphs, metrics, character_map)


# ---------------------------------------------------------------------------
# Arabic — a right-to-left sentence's letters, and the one mandatory ligature

ARABIC_SENTENCE = "العربية تكتب من اليمين إلى اليسار"
LAM, ALEF = 0x0644, 0x0627


def arabic():
    glyphs = {"space": blank()}
    metrics = {"space": (SPACE_ADVANCE, 0)}
    character_map = {0x20: "space"}
    letters = sorted({ord(ch) for ch in ARABIC_SENTENCE if ch != " "} | {LAM, ALEF})
    for code_point in letters:
        name = glyph_name(code_point)
        glyphs[name] = bar(100, 500, 0, X_HEIGHT)
        metrics[name] = (LETTER_ADVANCE, 100)
        character_map[code_point] = name

    glyphs["lam_alef"] = bar(100, 700, 0, CAP_HEIGHT)
    metrics["lam_alef"] = (800, 100)
    features = (
        "languagesystem DFLT dflt;\n"
        "languagesystem arab dflt;\n"
        "feature rlig {\n"
        "    sub %s %s by lam_alef;\n"
        "} rlig;\n" % (glyph_name(LAM), glyph_name(ALEF))
    )
    return build("Sigil Instrument Arabic", glyphs, metrics, character_map, features)


# ---------------------------------------------------------------------------
# Devanagari — a word whose virama fuses two letters into one cluster

DEVANAGARI_TEXT = "नमस्ते दुनिया"


def devanagari():
    glyphs = {"space": blank()}
    metrics = {"space": (SPACE_ADVANCE, 0)}
    character_map = {0x20: "space"}
    for code_point in sorted({ord(ch) for ch in DEVANAGARI_TEXT if ch != " "}):
        name = glyph_name(code_point)
        glyphs[name] = bar(100, 500, 0, X_HEIGHT)
        metrics[name] = (LETTER_ADVANCE, 100)
        character_map[code_point] = name
    return build("Sigil Instrument Devanagari", glyphs, metrics, character_map)


# ---------------------------------------------------------------------------
# Cuneiform — four characters beyond the basic plane, so every one of them is
# a surrogate pair in UTF-16

CUNEIFORM = [0x12000, 0x12031, 0x12038, 0x1204D]


def cuneiform():
    glyphs = {"space": blank()}
    metrics = {"space": (SPACE_ADVANCE, 0)}
    character_map = {0x20: "space"}
    for code_point in CUNEIFORM:
        name = glyph_name(code_point)
        glyphs[name] = bar(100, 500, 0, CAP_HEIGHT)
        metrics[name] = (LETTER_ADVANCE, 100)
        character_map[code_point] = name
    return build("Sigil Instrument Cuneiform", glyphs, metrics, character_map)


# ---------------------------------------------------------------------------
# Two Han faces — the same coverage under two family names, so a fallback
# resolver asked twice has two distinct faces to hand back

HAN, KANA = 0x4E2D, 0x3042


def han(family, ink_right):
    glyphs = {"space": blank()}
    metrics = {"space": (SPACE_ADVANCE, 0)}
    character_map = {0x20: "space"}
    for code_point in (HAN, KANA):
        name = glyph_name(code_point)
        glyphs[name] = bar(60, ink_right, 0, ASCENT)
        metrics[name] = (1000, 60)
        character_map[code_point] = name
    return build(family, glyphs, metrics, character_map)


# ---------------------------------------------------------------------------

FACES = {
    "Sans.ttf": sans,
    "Variable.ttf": variable,
    "Optical.ttf": optical,
    "Marks.ttf": marks,
    "Arabic.ttf": arabic,
    "Devanagari.ttf": devanagari,
    "Cuneiform.ttf": cuneiform,
    "HanSans.ttf": lambda: han("Sigil Instrument Han Sans", 940),
    "HanSerif.ttf": lambda: han("Sigil Instrument Han Serif", 880),
}


# ---------------------------------------------------------------------------
# The self-test: every property a case reads off these faces, asserted
# against the committed binaries rather than against the code above


def failures():
    """Every property that does not hold, as a list of sentences."""
    broken = []

    def check(condition, sentence):
        if not condition:
            broken.append(sentence)

    def face(name):
        return TTFont(os.path.join(HERE, name))

    for name in FACES:
        check(
            os.path.exists(os.path.join(HERE, name)),
            "%s is missing — run this script with no arguments" % name,
        )
    if broken:
        return broken

    sans_face = face("Sans.ttf")
    advances = sans_face["hmtx"].metrics
    cmap = sans_face.getBestCmap()
    check(sans_face["head"].unitsPerEm == UPEM, "Sans is not 1000 units per em")
    check(
        sans_face["OS/2"].sxHeight == X_HEIGHT
        and sans_face["OS/2"].sCapHeight == CAP_HEIGHT,
        "Sans no longer declares the x-height and cap height a case reads",
    )
    check(
        advances[cmap[0x41]][0] == LETTER_ADVANCE
        and advances[cmap[0x00DF]][0] == LETTER_ADVANCE,
        "Sans letters no longer share one known advance",
    )
    sans_glyf = sans_face["glyf"]
    check(
        all(sans_glyf[glyph_name(ord(ch))].yMin == DESCENT for ch in DESCENDING)
        and sans_glyf["n"].yMin == 0,
        "Sans no longer descends on exactly the letters that descend",
    )
    check(
        all(code_point in cmap for code_point in PUNCTUATION)
        and {advances[cmap[code_point]][0] for code_point in PUNCTUATION}
        == {PUNCTUATION_ADVANCE},
        "Sans punctuation no longer shares one known advance",
    )
    check(
        advances[cmap[0x00A0]][0] == SPACE_ADVANCE,
        "Sans's no-break space no longer matches its space",
    )
    figures = {advances[cmap[0x30 + d]][0] for d in range(10)}
    check(len(figures) == 10, "Sans figures are no longer proportional")
    tabular = {advances[cmap[0x30 + d] + ".tnum"][0] for d in range(10)}
    check(tabular == {TABULAR_ADVANCE}, "Sans tabular figures do not share an advance")
    sans_features = {
        record.FeatureTag
        for record in sans_face["GSUB"].table.FeatureList.FeatureRecord
    }
    check("liga" in sans_features, "Sans has no liga feature")
    check("tnum" in sans_features, "Sans has no tnum feature")
    check("f_f_i" in sans_face.getGlyphOrder(), "Sans has no ffi ligature glyph")

    variable_face = face("Variable.ttf")
    axes = {axis.axisTag: axis for axis in variable_face["fvar"].axes}
    check(
        set(axes) == {"wght", "wdth", "GRAD"},
        "Variable no longer declares wght, wdth and GRAD",
    )
    if set(axes) == {"wght", "wdth", "GRAD"}:
        check(
            axes["wght"].minValue == WGHT_RANGE[0]
            and axes["wght"].maxValue == WGHT_RANGE[2],
            "Variable's wght range moved",
        )
        check(
            axes["GRAD"].minValue == GRAD_RANGE[0]
            and axes["GRAD"].maxValue == GRAD_RANGE[2],
            "Variable's GRAD range moved",
        )
    # The advance deltas: wght and wdth must move them, GRAD must not.
    hvar = variable_face.get("HVAR")
    check(hvar is not None, "Variable carries no HVAR, so advances cannot vary")
    moved = {"wght": False, "wdth": False, "GRAD": False}
    if hvar is not None:
        store = hvar.table.VarStore
        regions = store.VarRegionList.Region
        for data in store.VarData:
            for region_index, deltas in zip(
                data.VarRegionIndex, zip(*data.Item) if data.Item else []
            ):
                region = regions[region_index]
                if not any(delta != 0 for delta in deltas):
                    continue
                for tag, axis in (
                    region.VarRegionAxis.items()
                    if isinstance(region.VarRegionAxis, dict)
                    else zip(
                        [axis.axisTag for axis in variable_face["fvar"].axes],
                        region.VarRegionAxis,
                    )
                ):
                    if axis.StartCoord != 0 or axis.EndCoord != 0:
                        moved[tag] = True
        check(moved["wght"], "Variable's wght no longer moves advances")
        check(moved["wdth"], "Variable's wdth no longer moves advances")
        check(
            not moved["GRAD"],
            "Variable's GRAD moves advances — the gate will refuse it",
        )
    # …and GRAD must still move the ink, or a case cannot see it work.
    gvar = variable_face["gvar"]
    grad_ink = False
    for deltas in gvar.variations.get(variable_face.getBestCmap()[0x57], []):
        low, high = (
            deltas.axes.get("GRAD", (0, 0, 0))[0],
            deltas.axes.get("GRAD", (0, 0, 0))[2],
        )
        if (low or high) and any(
            delta not in (None, (0, 0)) for delta in deltas.coordinates
        ):
            grad_ink = True
    check(grad_ink, "Variable's GRAD no longer moves any outline")

    optical_face = face("Optical.ttf")
    optical_cmap = optical_face.getBestCmap()
    check(
        all(ord(ch) in optical_cmap for ch in "nAVo"),
        "Optical no longer covers the reference n, the A/V pair and a lone o",
    )
    glyf = optical_face["glyf"]
    a_bounds = glyf["A"]
    check(
        a_bounds.numberOfContours == 1 and a_bounds.xMax > LETTER_ADVANCE - 100,
        "Optical's A no longer leans past its advance",
    )

    marks_face = face("Marks.ttf")
    marks_cmap = marks_face.getBestCmap()
    marks_advances = marks_face["hmtx"].metrics
    check(
        all(code_point in marks_cmap for code_point in COMBINING),
        "Marks no longer covers the whole combining block",
    )
    check(
        all(marks_advances[marks_cmap[code_point]][0] == 0 for code_point in COMBINING),
        "a combining mark in Marks carries an advance",
    )
    check(0x01A0 in marks_cmap, "Marks lost the horned O a stacked sample uses")
    check(
        "GSUB" not in marks_face, "Marks gained a GSUB, which may compose a mark away"
    )

    arabic_face = face("Arabic.ttf")
    arabic_cmap = arabic_face.getBestCmap()
    check(
        all(ord(ch) in arabic_cmap for ch in ARABIC_SENTENCE if ch != " "),
        "Arabic no longer covers its own sentence",
    )
    check(
        "lam_alef" in arabic_face.getGlyphOrder(), "Arabic lost the lam-alef ligature"
    )

    devanagari_cmap = face("Devanagari.ttf").getBestCmap()
    check(
        all(ord(ch) in devanagari_cmap for ch in DEVANAGARI_TEXT if ch != " "),
        "Devanagari no longer covers its own word",
    )
    check(0x094D in devanagari_cmap, "Devanagari lost the virama")

    cuneiform_cmap = face("Cuneiform.ttf").getBestCmap()
    check(
        all(code_point in cuneiform_cmap for code_point in CUNEIFORM),
        "Cuneiform no longer covers the four characters a case shapes",
    )

    sans_ttf, serif_ttf = face("HanSans.ttf"), face("HanSerif.ttf")
    for han_face, label in ((sans_ttf, "HanSans"), (serif_ttf, "HanSerif")):
        han_cmap = han_face.getBestCmap()
        check(HAN in han_cmap, "%s no longer covers the shared Han character" % label)
        check(KANA in han_cmap, "%s no longer covers the kana" % label)
    check(
        sans_ttf["name"].getDebugName(1) != serif_ttf["name"].getDebugName(1),
        "the two Han faces share a family name, so a resolver cannot tell them apart",
    )
    check(
        HAN not in sans_face.getBestCmap(),
        "Sans covers the Han character, so it can no longer stand as the face that "
        "sends a fallback resolver looking",
    )
    check(
        KANA not in sans_face.getBestCmap(),
        "Sans covers the kana, so it can no longer stand as the primary a fallback "
        "resolver is asked about",
    )
    return broken


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="check the committed faces instead of rewriting them",
    )
    arguments = parser.parse_args()

    if not arguments.self_test:
        for name, make in FACES.items():
            path = save(make(), name)
            print("wrote %s (%d bytes)" % (path, os.path.getsize(path)))

    broken = failures()
    for sentence in broken:
        print("FAIL: " + sentence, file=sys.stderr)
    if broken:
        return 1
    print("%d instrument faces carry what the cases read off them" % len(FACES))
    return 0


if __name__ == "__main__":
    sys.exit(main())

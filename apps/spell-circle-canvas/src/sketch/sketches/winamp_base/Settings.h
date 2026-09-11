#pragma once

// Construction data and drawing primitives owned by this study.

#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/core/Instances.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Kinetic.h>
#include <sigilcompose/kit/Marquee.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/compute/Noise.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Edges.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilmotion/values/Keyframes.h>
#include <sigilmotion/values/Time.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilweave/layout/ParagraphLayout.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace field = sigil::material::field;
namespace mskia = sigil::material::skia;
namespace motion = sigil::motion;
namespace path = sigil::geometry::path;
namespace patterns = sigil::material::pattern;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace wa {

// ---------------------------------------------------------------------------
// Scale. Native skin px -> canvas px.

constexpr float kScale = 3.0f;
constexpr float n(float v) { return v * kScale; }

/** The shadow tone: the complement spelling of `mskia::scale()`, because a
 * bevel is authored as "how much darker" rather than as a surviving fraction.
 */
inline SkColor4f dark(SkColor4f c, float k) { return mskia::scale(c, 1 - k); }

// ---------------------------------------------------------------------------
// Palette — sampled from the extracted BMPs, or quoted from the skin's own
// plain-text config files. Nothing here is remembered.

constexpr SkColor4f kBody = hexColor(0x343453);     // MAIN.BMP body base
constexpr SkColor4f kBodyTop = hexColor(0x3B3B5A);  // the unit ramp, top-lit
constexpr SkColor4f kBodyBot = hexColor(0x2E2E48);  // ... to shadowed
constexpr SkColor4f kLcd =
    hexColor(0x131320);  // LCD screen (sampled
                         // #181829, dropped four levels so
                         // the #182129 unlit ghost reads)
constexpr SkColor4f kBezel = hexColor(0x161622);  // LCD bezel outer ring
constexpr SkColor4f kUnlit =
    hexColor(0x182129);  // VISCOLOR color 1, "grey for
                         // dots" — byte-identical to the
                         // sampled NUMBERS.BMP background
constexpr SkColor4f kGreen = hexColor(0x00F800);    // NUMBERS.BMP digit green
constexpr SkColor4f kTitle = hexColor(0x25253A);    // TITLEBAR.BMP base
constexpr SkColor4f kGold = hexColor(0xA99865);     // wordmark, focused
constexpr SkColor4f kGoldDim = hexColor(0x7A7A94);  // wordmark, unfocused
constexpr SkColor4f kBtnHi = hexColor(0xEFFFFF);    // CBUTTONS bevel highlight
constexpr SkColor4f kBtnFace = hexColor(0x97A8B9);  // CBUTTONS steel-blue face
constexpr SkColor4f kBtnLo = hexColor(0x4A5A6B);    // CBUTTONS bevel shadow
constexpr SkColor4f kGlyph = hexColor(0x1E2833);   // the ink on a transport key
constexpr SkColor4f kGraph = hexColor(0x1B1A2C);   // EQMAIN graph screen navy
constexpr SkColor4f kGrid = hexColor(0x3A3A55);    // EQMAIN dashed gridline
constexpr SkColor4f kEqTop = hexColor(0x2A9A16);   // fader track, green
constexpr SkColor4f kEqMid = hexColor(0xA6C731);   // ... yellow-gold
constexpr SkColor4f kEqBot = hexColor(0xC5431B);   // ... red
constexpr SkColor4f kPlBg = hexColor(0x000000);    // PLEDIT.TXT NormalBG
constexpr SkColor4f kPlText = hexColor(0x00FF00);  // PLEDIT.TXT Normal
constexpr SkColor4f kPlNow = hexColor(0xFFFFFF);   // PLEDIT.TXT Current
constexpr SkColor4f kPlSel = hexColor(0x0000C6);   // PLEDIT.TXT SelectedBG
constexpr SkColor4f kDesk = hexColor(0x008080);  // Windows 9x/2000 default teal
constexpr SkColor4f kPeak = hexColor(0x969696);  // VISCOLOR 23, peak-hold dots

/** VISCOLOR.TXT colors 2..17, bottom of spectrum -> top, quoted verbatim.
 *  The same family the EQ fader tracks are sampled from. */
constexpr std::array<SkColor4f, 16> kVis = {
    hexColor(0x188408), hexColor(0x299400), hexColor(0x319C08),
    hexColor(0x39B510), hexColor(0x32BE10), hexColor(0x29CE10),
    hexColor(0x94DE21), hexColor(0xBDDE29), hexColor(0xD6B521),
    hexColor(0xDEA518), hexColor(0xC67B08), hexColor(0xD67300),
    hexColor(0xD66600), hexColor(0xD65A00), hexColor(0xCE2910),
    hexColor(0xEF3110)};

// ---------------------------------------------------------------------------
// Type. Almost nothing in classic Winamp is live text: TEXT.BMP is a fixed
// 5x6 px character cell and NUMBERS.BMP a 9x13 one, both baked pixel art.
// The ONE place with a real proportional font is the playlist (PLEDIT.TXT
// says Font=Arial, the CSS says 9px/0.5px tracking) — so that contrast is
// the thing to preserve, and everything else is a deliberate monospace
// approximation at the real cell size.

inline sk_sp<SkTypeface> mono() {
  return weave::ports::face({"Menlo", "Monaco"}, SkFontStyle::kNormal_Weight);
}
inline sk_sp<SkTypeface> monoBold() {
  return weave::ports::face({"Menlo", "Monaco"}, SkFontStyle::kBold_Weight);
}
inline sk_sp<SkTypeface> arial() {
  return weave::ports::face({"Arial", "Helvetica"},
                            SkFontStyle::kNormal_Weight);
}

inline sigil::weave::TextStyle type(const sk_sp<SkTypeface>& tf, float size,
                                    SkColor4f color, float track = 0,
                                    float condense = 1.0f) {
  return weave::textStyle({.face = tf,
                           .size = size,
                           .color = color,
                           .track = track,
                           .condense = condense});
}

inline Element t(const char* s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}
inline Element t(const std::string& s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

// ---------------------------------------------------------------------------
// Geometry vocabulary. ZERO corner radius anywhere — that is the whole
// visual argument of a 2003 hardware skeuomorph against a 2005 gel button.

/** Absolute placement in NATIVE skin coordinates, local to the window. */
inline Element at(Element e, float x, float y, float w, float h) {
  return kit::at(std::move(e), n(x), n(y), n(w), n(h));
}

/** The raised bevel: 1 native px light on the top and left, 1 native px
 *  dark on the bottom and right, inside the node's own edge. Twenty-odd
 *  controls across the three windows wear exactly this.
 *
 *  RAISED AND SUNKEN ARE ONE VALUE and not two drawings — the same two
 *  tones on the far edges instead of the near ones, which is what a
 *  well, a trough and a pressed button have always been. The kit's skin
 *  token set carries that as one bool, so this file states the skin's
 *  tones and nothing about how a bevel is drawn.
 *
 *  It goes in `.overlay()`: over the fill, under the content and the
 *  children. A bevel in `.background()` is painted and then covered by
 *  the surface it was meant to sit on. */
inline Element& raised(Element& e, SkColor4f hi = kBtnHi, SkColor4f lo = kBtnLo,
                       float w = 1.0f) {
  return kit::bevelled(e, kit::bevels::skin(hi, lo, n(w)));
}
/** The same pair the other way up. Every LCD well, trough and list frame
 *  in the skin. */
inline Element& sunken(Element& e,
                       SkColor4f hi = mskia::withAlpha(hexColor(0x5C5C86),
                                                       0.9f),
                       SkColor4f lo = hexColor(0x101018), float w = 1.0f) {
  return kit::bevelled(e, kit::bevels::skin(hi, lo, n(w), true));
}
/** THE WELL EDGE the three list frames share — a fainter light and a
 *  deeper shadow than a button's, which is what a hole in the body is. */
inline const SkColor4f kWellHi = mskia::withAlpha(hexColor(0x585880), 0.7f);
inline const SkColor4f kWellLo = hexColor(0x0E0E18);

/** Right/left/up-pointing triangles for the transport glyphs, as outlines
 *  so the node IS the shape.
 *
 *  A TRANSPORT GLYPH SPANS ITS WHOLE KEY, so it is not the inscribed
 *  `shapes::polygon(3)` — that one's vertices ride the box's ellipse and
 *  land a quarter of the width in. It is a drawing that is a function of
 *  the direction and of nothing else, so the direction is the key it
 *  settles on. */
inline Shape tri(int dir) {  // 0 right 1 left 2 up
  return keyedShape(dir, [dir](SkSize s) {
    const float w = s.width(), h = s.height();
    SkPathBuilder b;
    if (dir == 0) {
      b.moveTo(0, 0);
      b.lineTo(w, h * 0.5f);
      b.lineTo(0, h);
    } else if (dir == 1) {
      b.moveTo(w, 0);
      b.lineTo(0, h * 0.5f);
      b.lineTo(w, h);
    } else {
      b.moveTo(w * 0.5f, 0);
      b.lineTo(w, h);
      b.lineTo(0, h);
    }
    b.close();
    return b.detach();
  });
}

/** The scroll-arrow triangles: up or down. */
inline Shape upDown(bool up) {
  return keyedShape(up, [up](SkSize s) {
    const float w = s.width(), h = s.height();
    SkPathBuilder b;
    if (up) {
      b.moveTo(w * 0.5f, 0);
      b.lineTo(w, h);
      b.lineTo(0, h);
    } else {
      b.moveTo(0, 0);
      b.lineTo(w, 0);
      b.lineTo(w * 0.5f, h);
    }
    b.close();
    return b.detach();
  });
}

/** The Nullsoft lightning bolt baked into MAIN.BMP's bottom-right corner —
 *  the about/easter-egg hitzone at native 253,91,13,15. One drawing, keyed
 *  on its own name so the node settles between describes. */
inline Shape bolt() {
  return keyedShape(std::string_view("nullsoft-bolt"), [](SkSize s) {
    const float w = s.width(), h = s.height();
    static const float p[7][2] = {
        {0.62f, 0.00f}, {0.05f, 0.56f}, {0.40f, 0.56f}, {0.24f, 1.00f},
        {0.95f, 0.40f}, {0.55f, 0.40f}, {0.92f, 0.00f}};
    SkPathBuilder b;
    b.moveTo(p[0][0] * w, p[0][1] * h);
    for (int i = 1; i < 7; ++i) b.lineTo(p[i][0] * w, p[i][1] * h);
    b.close();
    return b.detach();
  });
}

}  // namespace wa

// ===========================================================================

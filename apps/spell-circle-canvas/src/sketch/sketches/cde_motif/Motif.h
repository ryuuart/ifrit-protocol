#pragma once

#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/PixelStyles.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/kit/Chrome.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/kit/Sprites.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilcore/reconcile/Environment.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Page.h>
#include <sigilsketch/kit/Scrollbar.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace measure = sigil::measure;
namespace test = sigil::compose::test;
namespace environment = sigil::core::environment;
namespace motion = sigil::motion;
namespace arrange = sigil::geometry::arrange;
namespace shapes = sigil::geometry::shapes;
namespace pattern = sigil::material::pattern;
namespace weave = sigil::weave;

using namespace sigil::compose;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace cde {

// ===========================================================================
// 1. THE ALGORITHM.  lib/Xm/ColorP.h constants, lib/Xm/Color.c arithmetic.
// ===========================================================================

constexpr int XmMAX_SHORT = 65535;
constexpr int XmCOLOR_PERCENTILE = XmMAX_SHORT / 100;  // = 655, INTEGER div

constexpr double XmRED_LUMINOSITY = 0.30;
constexpr double XmGREEN_LUMINOSITY = 0.59;
constexpr double XmBLUE_LUMINOSITY = 0.11;

constexpr int XmINTENSITY_FACTOR = 75;
constexpr int XmLIGHT_FACTOR = 0;  // yes, zero: `light` is dead code
constexpr int XmLUMINOSITY_FACTOR = 25;

constexpr int XmCOLOR_LITE_SEL_FACTOR = 15;
constexpr int XmCOLOR_LITE_BS_FACTOR = 40;
constexpr int XmCOLOR_LITE_TS_FACTOR = 20;
constexpr int XmCOLOR_DARK_SEL_FACTOR = 15;
constexpr int XmCOLOR_DARK_BS_FACTOR = 30;
constexpr int XmCOLOR_DARK_TS_FACTOR = 50;
constexpr int XmCOLOR_LO_SEL_FACTOR = 15;
constexpr int XmCOLOR_LO_BS_FACTOR = 60;
constexpr int XmCOLOR_LO_TS_FACTOR = 50;
constexpr int XmCOLOR_HI_SEL_FACTOR = 15;
constexpr int XmCOLOR_HI_BS_FACTOR = 40;
constexpr int XmCOLOR_HI_TS_FACTOR = 60;

constexpr int XmCOLOR_LITE_THRESHOLD = 93 * XmCOLOR_PERCENTILE;  // 60915
constexpr int XmCOLOR_DARK_THRESHOLD = 20 * XmCOLOR_PERCENTILE;  // 13100
constexpr int XmFOREGROUND_THRESHOLD = 70 * XmCOLOR_PERCENTILE;  // 45850

/** C's integer division: truncate toward ZERO.
 *
 *  Spelled out rather than written `a / b` because it is the single most
 *  load-bearing line in this file. The medium model's bottom-shadow
 *  factor is `60 + brightness * (40 - 60) / 65535` — the interpolation
 *  term is NEGATIVE — and a language with floor division gets a
 *  different answer for 291 of the 291 medium colours CDE ships. The
 *  static_assert is the guard: C++ has guaranteed truncation toward zero
 *  since C++11, but a port to anything else must re-derive it. */
constexpr int cdiv(int a, int b) {
  const int q = (a < 0 ? -a : a) / (b < 0 ? -b : b);
  return ((a < 0) != (b < 0)) ? -q : q;
}
static_assert(cdiv(-7, 2) == -3, "truncate toward zero, not floor");
static_assert(cdiv(46442 * (XmCOLOR_HI_BS_FACTOR - XmCOLOR_LO_BS_FACTOR),
                   XmMAX_SHORT) == -14,
              "Crimson set 2: f_bs must land on 46, not 45");

struct Rgb {
  int r = 0, g = 0, b = 0;  // 16-bit, 0..65535
  bool operator==(const Rgb&) const = default;
};

enum class Branch { Dark, Medium, Lite };

struct Derived {
  Rgb bg, fg, ts, bs, sel;
  int brightness = 0;
  Branch branch = Branch::Medium;
  int fSel = 0, fBs = 0, fTs = 0;
};

/** lib/Xm/Color.c, Brightness(). The float casts are the source's own —
 *  its comment reads "The casting nonsense below is to try to control
 *  the point at the truncation occurs." */
inline int brightness(Rgb c) {
  const int intensity = cdiv(c.r + c.g + c.b, 3);
  const int luminosity = (int)((XmRED_LUMINOSITY * (double)c.r) +
                               (XmGREEN_LUMINOSITY * (double)c.g) +
                               (XmBLUE_LUMINOSITY * (double)c.b));
  const int maxp = std::max({c.r, c.g, c.b});
  const int minp = std::min({c.r, c.g, c.b});
  const int light = cdiv(minp + maxp, 2);  // weighted 0 — computed, discarded
  return cdiv(intensity * XmINTENSITY_FACTOR + light * XmLIGHT_FACTOR +
                  luminosity * XmLUMINOSITY_FACTOR,
              100);
}

inline int darken(int v, int f) { return v - cdiv(v * f, 100); }
inline int lighten(int v, int f) {
  return v + cdiv(f * (XmMAX_SHORT - v), 100);
}
inline Rgb darken(Rgb c, int f) {
  return {darken(c.r, f), darken(c.g, f), darken(c.b, f)};
}
inline Rgb lighten(Rgb c, int f) {
  return {lighten(c.r, f), lighten(c.g, f), lighten(c.b, f)};
}

/** lib/Xm/Color.c, CalculateColorsRGB() and its three branch functions. */
inline Derived calculate(Rgb bg) {
  Derived d;
  d.bg = bg;
  const int B = brightness(bg);
  d.brightness = B;
  d.fg = (B > XmFOREGROUND_THRESHOLD)
             ? Rgb{0, 0, 0}
             : Rgb{XmMAX_SHORT, XmMAX_SHORT, XmMAX_SHORT};
  if (B < XmCOLOR_DARK_THRESHOLD) {
    // DARK: everything gets LIGHTER, the "bottom" shadow included.
    d.branch = Branch::Dark;
    d.fSel = XmCOLOR_DARK_SEL_FACTOR;
    d.fBs = XmCOLOR_DARK_BS_FACTOR;
    d.fTs = XmCOLOR_DARK_TS_FACTOR;
    d.sel = lighten(bg, d.fSel);
    d.bs = lighten(bg, d.fBs);
    d.ts = lighten(bg, d.fTs);
  } else if (B > XmCOLOR_LITE_THRESHOLD) {
    // LITE: everything gets DARKER, the "top" shadow included — which is
    // why a near-white CDE text field is etched, not highlighted.
    d.branch = Branch::Lite;
    d.fSel = XmCOLOR_LITE_SEL_FACTOR;
    d.fBs = XmCOLOR_LITE_BS_FACTOR;
    d.fTs = XmCOLOR_LITE_TS_FACTOR;
    d.sel = darken(bg, d.fSel);
    d.bs = darken(bg, d.fBs);
    d.ts = darken(bg, d.fTs);
  } else {
    // MEDIUM: each factor interpolates across the brightness range FIRST,
    // then applies. cdiv() on the bs term is the whole finding.
    d.branch = Branch::Medium;
    d.fSel =
        XmCOLOR_LO_SEL_FACTOR +
        cdiv(B * (XmCOLOR_HI_SEL_FACTOR - XmCOLOR_LO_SEL_FACTOR), XmMAX_SHORT);
    d.fBs =
        XmCOLOR_LO_BS_FACTOR +
        cdiv(B * (XmCOLOR_HI_BS_FACTOR - XmCOLOR_LO_BS_FACTOR), XmMAX_SHORT);
    d.fTs =
        XmCOLOR_LO_TS_FACTOR +
        cdiv(B * (XmCOLOR_HI_TS_FACTOR - XmCOLOR_LO_TS_FACTOR), XmMAX_SHORT);
    d.sel = darken(bg, d.fSel);
    d.bs = darken(bg, d.fBs);
    d.ts = lighten(bg, d.fTs);
  }
  return d;
}

/** Display truncation: X takes the TOP EIGHT BITS. Not round(v / 257) —
 *  the difference is an off-by-one on about a third of the values. */
constexpr SkColor4f toSk(Rgb c) noexcept {
  return {(float)((uint32_t)c.r >> 8u) / 255.0f,
          (float)((uint32_t)c.g >> 8u) / 255.0f,
          (float)((uint32_t)c.b >> 8u) / 255.0f, 1.0f};
}
constexpr uint32_t toHex(Rgb c) {
  return ((uint32_t)c.r >> 8u) << 16u | ((uint32_t)c.g >> 8u) << 8u |
         ((uint32_t)c.b >> 8u);
}
constexpr Rgb from8(uint32_t rgb) noexcept {
  return {(int)(((rgb >> 16u) & 0xffu) << 8u),
          (int)(((rgb >> 8u) & 0xffu) << 8u), (int)((rgb & 0xffu) << 8u)};
}
constexpr SkColor4f C(uint32_t rgb) noexcept { return toSk(from8(rgb)); }

// ===========================================================================
// 2. THE PALETTES — the complete themes, verbatim from the shipped .dp
//    files. Eight lines of `#RRRRGGGGBBBB`; that is the entire theme.
//    Note the low bytes on several entries (Default line 2 is
//    `99 1b 99 fe`): the format carries real 16 bits and CDE's designers
//    used it, which is why these run through the algorithm at 16 bits
//    and truncate to 8 only at the very end.
// ===========================================================================

struct Palette {
  const char* name;
  std::array<Rgb, 8> set;
};

constexpr Rgb dp(uint32_t rr, uint32_t gg, uint32_t bb) {
  return {(int)rr, (int)gg, (int)bb};
}

// cde/programs/palettes/Default.dp
constexpr Palette kDefault = {
    "Default",
    {dp(0xed00, 0xa800, 0x7000), dp(0x9900, 0x991b, 0x99fe),
     dp(0x8955, 0x9808, 0xaa00), dp(0x6800, 0x6f00, 0x8200),
     dp(0xc600, 0xb2d2, 0xa87e), dp(0x4900, 0x9200, 0xa700),
     dp(0xb700, 0x8700, 0x8d00), dp(0x938e, 0xab73, 0xbf00)}};

// cde/programs/palettes/Crimson.dp — the palette in the Solaris 10
// screenshot, matched byte for byte off its Front Panel background.
constexpr Palette kCrimson = {
    "Crimson",
    {dp(0xb200, 0x4d00, 0x7a00), dp(0xae00, 0xb200, 0xc300),
     dp(0x7100, 0x8b00, 0xa500), dp(0xff00, 0xf700, 0xe900),
     dp(0x68ce, 0xa600, 0xa0e6), dp(0x8d40, 0xad03, 0xc700),
     dp(0xd300, 0x9700, 0x9800), dp(0x9788, 0x93a3, 0xb500)}};

// cde/programs/palettes/Black.dp — eight lines of the literal X11 colour
// NAME `Black`, the one shipped palette that is not hex, and the only
// one that reaches the DARK branch.
constexpr Palette kBlack = {
    "Black",
    {dp(0, 0, 0), dp(0, 0, 0), dp(0, 0, 0), dp(0, 0, 0), dp(0, 0, 0),
     dp(0, 0, 0), dp(0, 0, 0), dp(0, 0, 0)}};

// cde/programs/palettes/Summer.dp — line 4 is #FFFFFF exactly, the
// cleanest demonstration of the LITE branch there is.
constexpr Palette kSummer = {
    "Summer",
    {dp(0xff00, 0x5600, 0x6415), dp(0xff00, 0xd600, 0x9000),
     dp(0x8200, 0x9f2a, 0xff00), dp(0xff00, 0xff00, 0xff00),
     dp(0x8600, 0xeac4, 0xe700), dp(0xb800, 0xc891, 0xff00),
     dp(0x6400, 0xc100, 0xff00), dp(0xbcf0, 0xc575, 0xdb00)}};

const std::array<const Palette*, 4> kPalettes = {&kDefault, &kCrimson, &kBlack,
                                                 &kSummer};

/** CDE's FIXED grey ramp for icon artwork [SRC, XPM colour tables]. These
 *  are NOT algorithm output — running calculate() on #949494 gives
 *  #CFCFCF/#4D4D4D, not the #BDBDBD/#636363 the XPM files carry. They are
 *  the artists' frozen reference values, so they are quoted, not derived.
 *  Only 1, 2, 7 and 8 appear in the icons that were read; 3–6 are [EST]
 *  at even steps. */
constexpr std::array<uint32_t, 8> kIconGray = {0xDEDEDE, 0xBDBDBD, 0x9C9C9C,
                                               0x7B7B7B, 0x5A5A5A, 0x424242,
                                               0x393939, 0x212121};
/** iconColor1..8, fixed: black, white, red, green, blue, yellow, cyan,
 *  magenta [SRC]. */
constexpr std::array<uint32_t, 8> kIconColor = {0x000000, 0xFFFFFF, 0xFF0000,
                                                0x00FF00, 0x0000FF, 0xFFFF00,
                                                0x00FFFF, 0xFF00FF};

// ===========================================================================
// 3. THE DERIVED THEME — 8 colour sets x 5 colours, as VALUES, reached
//    through the inherited-value channel.
//
//    A CDE widget is drawn in whatever set the window around it was
//    assigned, and no widget picks its own. That is an ambient value, so
//    it is one: `core::environment::Provide<ColorSet>` opens the scope a piece
//    of chrome is drawn in, and every bevel, label, stipple and icon under it
//    reads the same set without being handed it. Threading a set by argument
//    through six levels is how a window ends up drawn in three of them by
//    accident, and one window in three sets is the one mistake a study of this
//    derivation cannot afford.
//
//    Values, not bound Outputs. A bound colour is declared volatility,
//    and volatility is per NODE and binary: "this bevel changes every
//    three seconds" would be priced exactly as "this bevel repaints at
//    60 Hz", across a desktop's worth of bevels, to save the re-describe
//    on the one frame in a hundred and eighty where the palette moves.
// ===========================================================================

struct ColorSet {
  SkColor4f bg{}, fg{}, ts{}, bs{}, sel{};
  bool operator==(const ColorSet&) const = default;

  static ColorSet of(Rgb background) {
    const Derived d = calculate(background);
    return {toSk(d.bg), toSk(d.fg), toSk(d.ts), toSk(d.bs), toSk(d.sel)};
  }
};

struct Theme {
  std::array<ColorSet, 8> s;
  void load(const Palette& p) {
    for (int i = 0; i < 8; ++i) s[(size_t)i] = ColorSet::of(p.set[(size_t)i]);
  }
  const ColorSet& operator[](int i) const { return s[(size_t)(i - 1)]; }
  ColorSet& operator[](int i) { return s[(size_t)(i - 1)]; }
  bool operator==(const Theme&) const = default;
};

/** The set this piece of chrome is being drawn in. Handed nothing; reads
 *  the scope it was composed inside. */
inline ColorSet ambient() { return environment::inheritedOr(ColorSet{}); }

/** A SURFACE WEARING @p s: grounded in the set's background and INKED in
 *  its foreground, which is what every plane in a CDE session is. The ink
 *  is the half a widget used to have to hand to each of its labels — the
 *  set is one decision about a surface and everything on it, so it is
 *  stated where the surface is. */
inline Element surface(const ColorSet& s) { return box().fill(s.bg).ink(s.fg); }

// ===========================================================================
// 4. THE BEVEL — lib/Xm/Draw.c, DrawSimpleShadow().
// ===========================================================================

/** The bevel of the AMBIENT colour set — top shadow over bottom shadow.
 *  Handed nothing: a bevel is drawn in the set of the window it is in.
 *
 *  XmeDrawShadows IS the kit's Motif token set. `sunken` is XmSHADOW_IN
 *  and is literally the two colours swapped, which is one bool there;
 *  `etched` is XmSHADOW_ETCHED_IN, two passes at half the thickness with
 *  the second inverted, which is the token set's inner ring. The mitre
 *  the two off corners step on is the corner token: DrawSimpleShadow's
 *  TOP runs to `x + w - i - 1` and its LEFT starts at `y + T`, so the
 *  top-left corner is square and the two off corners fall on the
 *  diagonal, and the etched pass hands each of those corner pixels to
 *  the far band so the groove closes.
 *
 *  Antialiasing is OFF and the marks land on whole pixels, which is the
 *  token set's own doing: every coordinate a 1993 toolkit computed was an
 *  integer and AA can only soften it. */
inline kit::Bevel bevel(float T, bool sunken, bool etched) {
  const ColorSet s = ambient();
  return etched ? kit::bevels::motifEtched(s.ts, s.bs, T, sunken)
                : kit::bevels::motif(s.ts, s.bs, T, sunken);
}
/** A bevel drawn in the FOREGROUND on both faces — the flat outline Motif
 *  puts inside a maximise box, which is a shadow with one colour. */
inline kit::Bevel bevelFg(float T) {
  const SkColor4f fg = ambient().fg;
  return kit::bevels::motif(fg, fg, T);
}

/** XmeDrawHighlight: a square ring of `highlightThickness` inside the
 *  widget's own edge. No mitre, no shading — it is not a shadow, which is
 *  why it is a plain stroke where the bevel above is a token set.
 *
 *  highlightThickness 2 [MEAS], in the widget's foreground — Motif's
 *  default highlightColor. Only the focused widget shows one. The ring is
 *  drawn UNSMOOTHED, which the general stroke carries: a smoothed edge on
 *  an axis-aligned 2 px ring reads as a blur rather than as a line. */
inline PathFormat highlight(float T) {
  return PathFormat{.width = T,
                    .strokeFill = Fill::color(ambient().fg),
                    .align = PathFormat::Align::Inner,
                    .antiAlias = false};
}

/** Motif's insensitive treatment: the label is painted through a 50%
 *  checkerboard, `stipple(x, y) = (x + y) & 1`. Reproduced the other way
 *  round — the background painted over the complementary half — which is
 *  the identical result and lets the colour stay bound.
 *
 *  patterns::checker(1, on, off) is this pattern and is not usable here:
 *  a Pattern BAKES its colours into the tile, so a themed stipple would
 *  need one Pattern per palette per colour. The kit's stipple is a mask
 *  the colour is laid THROUGH, which is one lattice for all of them. */
inline styles::Stipple stipple() { return styles::stipple(ambient().bg); }

// ===========================================================================
// 5. THE BACKDROP — cde/programs/backdrops/PinStripe.pm, 28 x 52, 2 colours.
// ===========================================================================

/** All 52 rows of the shipped XPM reduce to this. Verified back against
 *  the file: 1456/1456 pixels agree.
 *
 *  Base is a 50% checkerboard, DARK where (x + y) is odd; four "pin" rows
 *  at y = 0, 13, 26, 39 are entirely LIGHT except for two dark pixels
 *  each, on a staggered half-drop. Period 14 in x, 13 in y. In 1993 a
 *  texture was a pixmap and a gradient was a dither, and this is why the
 *  CDE root window is a faintly-structured mid-tone rather than flat. */
inline pattern::Program pinStripeTile(SkColor4f light, SkColor4f dark) {
  return [light, dark](SkCanvas& c, SkSize, uint32_t) {
    SkPaint p;
    p.setAntiAlias(false);
    p.setColor(light, nullptr);
    c.drawRect(SkRect::MakeWH(28, 52), p);
    p.setColor(dark, nullptr);
    for (int y = 0; y < 52; ++y) {
      const bool pin = (y == 0 || y == 13 || y == 26 || y == 39);
      for (int x = 0; x < 28; ++x) {
        bool isDark;
        if (pin)
          isDark =
              (y == 0 || y == 26) ? (x == 13 || x == 27) : (x == 6 || x == 20);
        else
          isDark = ((unsigned)(x + y) & 1u) == 1u;
        if (isDark) c.drawRect(SkRect::MakeXYWH((float)x, (float)y, 1, 1), p);
      }
    }
  };
}

// ===========================================================================
// 6. TYPE.  One face, one size. CDE has no type hierarchy at all: the
//    title bar, the menus, the file names and the dialog buttons are all
//    the same 13 px run [MEAS]. That restraint is the reference.
// ===========================================================================

inline sk_sp<SkTypeface> uiFace() {
  return weave::ports::face({"Helvetica", "Arial"});
}

constexpr float kType = 13.0f;  // [MEAS] ink boxes: cap height 9-10 px
/** THE SUBSTITUTION'S OWN CORRECTION, measured rather than guessed. An X
 *  core bitmap face has integer advances and is about a pixel wider per
 *  glyph than Helvetica at the same nominal size, so a title set in the
 *  stand-in comes out short: the reference's "File Manager - root" spans
 *  128 px of its 1152 and the same string in Helvetica 13 spans 111. The
 *  difference over that run's inter-glyph gaps is this number, and it
 *  puts the desktop back at the reference's density without changing the
 *  size the ink boxes were measured at. */
constexpr float kTrack = 0.95f;

/** THE UI REGISTER, as the partial a screen declares at its root: one
 *  face, one size, HARD EDGES, and NO COLOUR — a colour set's foreground
 *  is the ink of whatever wears the set, so the register is the desktop's
 *  and the colour is the subtree's. `aliased` selects 1-bit rasterisation
 *  and is part of the shape-cache key, so a run carries it through shaping
 *  — which is the whole of what a 1995 X11 desktop's type looks like, and
 *  the one property this reconstruction cannot approximate. */
inline sigil::weave::Type uiType() {
  return {.face = uiFace(),
          .size = kType,
          .track = kTrack,
          .aliased = true,
          .antiAlias = false};
}

/** ONE RUN OF UI TYPE IN THE INK IN FORCE — a window title, a menu item, a
 *  file name: each is just its own words. */
inline Element label(std::string_view t) { return text(toUtf8(t)).shrink(0); }

/** The same at another size, and in a colour of its own where the run is
 *  not in the set's: a calendar page's month over its day, the one figure a
 *  proof row fails on. */
inline Element label(std::string_view t, float size,
                     std::optional<SkColor4f> c = std::nullopt) {
  return text(toUtf8(t)).font({.size = size, .color = c}).shrink(0);
}

/** One run with Motif's mnemonic underline on exactly one character. Two
 *  spans and a `Decoration{Kind::kUnderline}` at thickness 1, which is what
 *  Motif's underline is; `skipInk` off, because Motif's does not break for
 *  a descender.
 *
 *  THE UNDERLINED CHARACTER IS TOLD ITS COLOUR: a decoration carries one of
 *  its own rather than taking the ink the glyphs are painted in, so that
 *  span is a whole style and inherits nothing. The two either side do. */
inline Element mnemonicLabel(std::string_view t, SkColor4f c, int mnemonic) {
  if (mnemonic < 0 || mnemonic >= (int)t.size()) return label(t);
  sigil::weave::Type coloured = uiType();
  coloured.color = c;
  sigil::weave::TextStyle under = weave::textStyle(coloured);
  sigil::weave::Decoration d;
  d.kind = sigil::weave::Decoration::Kind::kUnderline;
  d.skipInk = false;
  d.thickness = 1;
  d.color = c.toSkColor();
  under.paint.addDecoration(d);
  return text(weave::rich()
                  .add(toUtf8(t.substr(0, (size_t)mnemonic)))
                  .add(toUtf8(t.substr((size_t)mnemonic, 1)), under)
                  .add(toUtf8(t.substr((size_t)mnemonic + 1))))
      .shrink(0);
}

// ===========================================================================
// 7. WIDGETS.  Not one takes a colour set as an argument: each reads
//    `ambient()` — the set the window it was composed inside opened with
//    `environment::Provide<ColorSet>` — so a bevel, a label, a stipple and an
//    icon four levels down are all in the same set without anyone
//    passing it down. A window that wants two puts the second scope
//    around the subtree that wears it, which is how the File Manager
//    below has set 1 on its frame and set 5 on its body.
// ===========================================================================

/** XmPushButton. Armed is XmSHADOW_IN *and* the background swapped to the
 *  derived select colour — which is the only place `select` shows up in
 *  ordinary use besides a list selection. */
inline Element pushButton(std::string_view t, bool armed = false,
                          bool defaulted = false, bool insensitive = false) {
  const ColorSet s = ambient();
  Element inner = box()
                      .fill(armed ? s.sel : s.bg)
                      .ink(s.fg)
                      .overlay(bevel(2, armed, false))
                      .padding(2)
                      .alignItems(Align::Center)
                      .justify(Justify::Center)
                      .height(Dimension(25))
                      .child(box()
                                 .padding(6, 2)
                                 .alignItems(Align::Center)
                                 .justify(Justify::Center)
                                 .child(label(t)));
  if (insensitive) inner.foreground(stipple());
  Element ring = box().padding(2).child(std::move(inner));
  if (defaulted) ring.overlay(bevel(1, true, false));
  return ring;
}

/** XmTextField: XmSHADOW_IN at T = 2 over colour set 4 — the only set
 *  that is ever near-white, and therefore the only one that ever takes
 *  the LITE branch. */
inline Element textField(std::string_view t, float w, bool caret = false,
                         const ch::Output<float>* caretOut = nullptr) {
  const ColorSet s = ambient();
  Element inner = box()
                      .row()
                      .alignItems(Align::Center)
                      .grow(1)
                      .padding(3, 0)
                      .child(label(t));
  if (caret && caretOut)
    inner.child(box()
                    .width(Dimension(1))
                    .height(Dimension(13))
                    .fill(s.fg)
                    .opacity(motion::bind(caretOut).quantize(2)));
  Element field = surface(s)
                      .overlay(bevel(2, true, false))
                      .padding(2)
                      .height(Dimension(24))
                      .row()
                      .alignItems(Align::Center)
                      .child(std::move(inner));
  if (!caret) return field.width(Dimension(w));
  // The focused widget carries XmeDrawHighlight's ring OUTSIDE its
  // shadow: four plain rectangles of highlightThickness, no mitre.
  return box()
      .width(Dimension(w))
      .padding(2)
      .overlay(highlight(2))
      .child(std::move(field).grow(1));
}

// ---------------------------------------------------------------------------
// Icon artwork. Drawn from CDE's own symbolic-colour vocabulary — the
// characters below are the same names a real CDE .pm colour table uses,
// which is exactly why a CDE icon re-colours with the theme and a Win95
// icon does not.
//
//   B background   T topShadowColor   S bottomShadowColor   L selectColor
//   F foreground   1..8 iconGray1..8 (FIXED)   k w r g u y c m iconColor
//
// The grid, the palette and the greedy rectangle merge are the kit's
// `kit::pixelMap`; what belongs here is the VOCABULARY — which character
// means which colour, and that five of the twenty-two move with the theme
// while the rest never do.
// ---------------------------------------------------------------------------

/** The characters an icon grid is written in, in palette order: the blank,
 *  the five symbolic colours a theme moves, the fixed eight-step gray ramp
 *  and the eight fixed icon colours. */
constexpr std::string_view kIconChars = " BTSLF12345678kwrguycm";

/** The palette those characters name, resolved against the ambient colour
 *  set — so the same grid is a different sprite under a different theme,
 *  which is the whole reason CDE icons re-colour and Win95 icons do not. */
inline kit::Sprite iconArt(const std::vector<std::string>& rows) {
  const ColorSet s = ambient();
  std::vector<SkColor4f> colours{{0, 0, 0, 0}, s.bg, s.ts, s.bs, s.sel, s.fg};
  for (uint32_t gray : kIconGray) colours.push_back(C(gray));
  for (uint32_t colour : kIconColor) colours.push_back(C(colour));
  return kit::pixelMap(rows, {kIconChars, colours}).value_or(kit::Sprite{});
}

inline Element art(const std::vector<std::string>& rows, float cell) {
  return kit::pixelSprite(iconArt(rows), {.cell = cell});
}

// The icon set. 24 x 24 grids at cell 2 => the 48 x 48 that Fpclock.l.pm's
// own header ("48 48 6 1") establishes as the Front Panel icon size.

inline std::vector<std::string> icoHome() {
  return {"                        ", "                        ",
          "           88           ", "          8118          ",
          "         811118         ", "        81111118        ",
          "       8111111118       ", "      811111111118      ",
          "     81111111111118     ", "    8111111111111118    ",
          "   811111111111111118   ", "  88888888888888888888  ",
          "   822222222222222228   ", "   82wwww22222wwww28    ",
          "   82wkkw22222wkkw28    ", "   82wwww22222wwww28    ",
          "   822222222222222228   ", "   822228LLLL8222228    ",
          "   822228LLLL8222228    ", "   822228LLLL8222228    ",
          "   822228LLLL8222228    ", "   88888888888888888    ",
          "                        ", "                        "};
}

inline std::vector<std::string> icoEditor() {
  return {"                        ", "                     22 ",
          "  1111111111111     288 ", "  1wwwwwwwwwww1    2882 ",
          "  1wkkkkkkkkw1    2882  ", "  1wwwwwwwwwww1  28822  ",
          "  1wkkkkkkkkw1  28822   ", "  1wwwwwwwwwww1 8822    ",
          "  1wkkkkkkkw1  8822     ", "  1wwwwwwwwww1 822      ",
          "  1wkkkkkkkw1 822       ", "  1wwwwwwwwww1822       ",
          "  1wkkkkkkkw18822       ", "  1wwwwwwwww18822       ",
          "  1wkkkkkkkw1822        ", "  1wwwwwwwwww122        ",
          "  1wkkkkkkkw112         ", "  1wwwwwwwwww11         ",
          "  1wkkkkkkkkw1          ", "  1wwwwwwwwwww1         ",
          "  1111111111111         ", "                        ",
          "                        ", "                        "};
}

inline std::vector<std::string> icoMail() {
  return {"                        ", "                        ",
          "                        ", "  TTTTTTTTTTTTTTTTTTTT  ",
          "  T1111111111111111118  ", "  T8111111111111111818  ",
          "  T18111111111111181 8  ", "  T118111111111118111 8 ",
          "  T11181111111181111118 ", "  T111181111118111111 8 ",
          "  T11111811118111111118 ", "  T111111811811111111 8 ",
          "  T11111118811111111118 ", "  T1111111111111111118  ",
          "  T1111111111111111118  ", "  T1111111111111111118  ",
          "  T1111111111111111118  ", "  T1111111111111111118  ",
          "  T1111111111111111118  ", "  88888888888888888888  ",
          "                        ", "                        ",
          "                        ", "                        "};
}

inline std::vector<std::string> icoPrinter() {
  return {"                        ", "                        ",
          "      TTTTTTTTTTTT      ", "      TwwwwwwwwwwS      ",
          "      TwkkkkkkkkwS      ", "      TwwwwwwwwwwS      ",
          "      TwkkkkkkkkwS      ", "      TwwwwwwwwwwS      ",
          "   TTTTTTTTTTTTTTTTTT   ", "   T2222222222222222S   ",
          "   T2222222222222222S   ", "   T22g222222222222 S   ",
          "   T2222222222222222S   ", "   T2222222222222222S   ",
          "   88888888888888888S   ", "     TTTTTTTTTTTTTT     ",
          "     TwwwwwwwwwwwwS     ", "     TwkkkkkkkkkkwS     ",
          "     TwwwwwwwwwwwwS     ", "     88888888888888     ",
          "                        ", "                        ",
          "                        ", "                        "};
}

inline std::vector<std::string> icoStyle() {
  return {"                        ", "        TTTTTTTT        ",
          "      TT22222222TT      ", "     T2222222222 2S     ",
          "    T222rr2222gg222S    ", "   T2222rr2222gg2222S   ",
          "   T22222222222222 2S   ", "  T2222222222222222 S   ",
          "  T22yy2222222222uu2S   ", "  T22yy2222222222uu2S   ",
          "  T2222222222222222 S   ", "  T2222222222222222 S   ",
          "  T22222mm2222cc2222S   ", "   S2222mm2222cc222S    ",
          "   S222222222222222S    ", "    SS22222222222SS     ",
          "      SSSS2222SSS       ", "         SSSS           ",
          "            kkk         ", "             kkk        ",
          "              kk        ", "                        ",
          "                        ", "                        "};
}

inline std::vector<std::string> icoApps() {
  return {"                        ", "  TTTTTTTTTTTTTTTTTTTT  ",
          "  T222222222222222222S  ", "  T2TTTTTT22TTTTTT222S  ",
          "  T2Twwww822Twwww8222S  ", "  T2Twwww822Twwww8222S  ",
          "  T2T8888822T88888222S  ", "  T222222222222222222S  ",
          "  T2TTTTTT22TTTTTT222S  ", "  T2Tuuuu822Trrrr8222S  ",
          "  T2Tuuuu822Trrrr8222S  ", "  T2T8888822T88888222S  ",
          "  T222222222222222222S  ", "  T2TTTTTT22TTTTTT222S  ",
          "  T2Tgggg822Tyyyy8222S  ", "  T2Tgggg822Tyyyy8222S  ",
          "  T2T8888822T88888222S  ", "  T222222222222222222S  ",
          "  8888888888888888888S  ", "                        ",
          "                        ", "                        ",
          "                        ", "                        "};
}

inline std::vector<std::string> icoHelp() {
  return {"                        ", "                        ",
          "    TTTTTTTTTTTTTTTT    ", "    TwwwwwwSwwwwwwwS    ",
          "    TwwwwwwSwwwwwwwS    ", "    Twkkk22Sw22kkk2S    ",
          "    Tw22222Sw222222S    ", "    Twkkkk2Sw2kkkk2S    ",
          "    Tw22222Sw222222S    ", "    Twkkk22Sw22kkk2S    ",
          "    Tw22222Sw222222S    ", "    Twkkkk2Sw2kkkk2S    ",
          "    Tw22222Sw222222S    ", "    TwwwwwwSwwwwwwwS    ",
          "    88888888888888888   ", "        TTTTTTTT        ",
          "       T22kkkk22S       ", "       T22k22k22S       ",
          "       T2222k222S       ", "       T222k2222S       ",
          "       T2222222 S       ", "       T222k2222S       ",
          "       88888888888      ", "                        "};
}

inline std::vector<std::string> icoTrash() {
  return {"                        ", "         888888         ",
          "        81111118        ", "   TTTTTTTTTTTTTTTTTT   ",
          "   T1111111111111111S   ", "   88888888888888888S   ",
          "                        ", "     TTTTTTTTTTTTTT     ",
          "     T12111211121 1S    ", "     T12111211121 1S    ",
          "     T12111211121 1S    ", "     T12111211121 1S    ",
          "     T12111211121 1S    ", "     T12111211121 1S    ",
          "     T12111211121 1S    ", "     T12111211121 1S    ",
          "     T12111211121 1S    ", "     T111111111111S     ",
          "      T1111111111S      ", "      888888888888      ",
          "                        ", "                        ",
          "                        ", "                        "};
}

inline std::vector<std::string> icoLock() {
  return {"            ", "    2222    ", "   28  82   ", "   2    2   ",
          "   2    2   ", "  TTTTTTTT  ", "  T222222S  ", "  T22kk22S  ",
          "  T22kk22S  ", "  T222222S  ", "  88888888  ", "            "};
}

inline std::vector<std::string> icoExit() {
  return {"            ", "  TTTTTTTT  ", "  T222222S  ", "  T2wwww2S  ",
          "  T2w22w2S  ", "  T2w22w2S  ", "  T2wwww2S  ", "  T222222S  ",
          "  88888888  ", "     kk     ", "    kkkk    ", "            "};
}

/** The File Manager's folder icon — a directory cell, drawn once and
 *  repeated eighteen times. */
inline std::vector<std::string> icoFolder() {
  return {"                ", "  TTTTTT        ", "  T2222TTTTTTT  ",
          "  T22222222222S ", "  T22222222222S ", "  T22222222222S ",
          "  T22222222222S ", "  T22222222222S ", "  T22222222222S ",
          "  T22222222222S ", "  T22222222222S ", "  T22222222222S ",
          "  888888888888S ", "                ", "                ",
          "                "};
}

}  // namespace cde

// ===========================================================================

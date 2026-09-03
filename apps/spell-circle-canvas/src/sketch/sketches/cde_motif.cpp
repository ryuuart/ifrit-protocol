// cde_motif.cpp — the Common Desktop Environment 1.0 on OSF/Motif 2.1
// (1993–1995), at Sun's canonical workstation resolution 1152 x 900, 1:1.
// The Workspace: the Front Panel, the File Manager, a torn-off menu, the
// Style Manager's Color dialog, and a derivation strip that shows the
// algorithm running.
//
// THIS IS A STUDY OF A *SYSTEM* RATHER THAN A PICTURE, and that is the
// whole reason CDE was chosen. A CDE theme is
// EIGHT BACKGROUND COLOURS in a 40-byte `.dp` file and nothing else;
// Motif's XmGetColors derives the foreground, the top shadow, the bottom
// shadow and the select colour for each of them at runtime, by a
// published function, on the user's machine. Change the eight numbers and
// the whole screen re-derives. So: NOTHING derived is authored here. The
// function is implemented (cde::calculate, below) and every one of the
// 40 live colours on this canvas is its output.
//
// (Mac OS 8 "Platinum" is the obvious neighbouring subject and does not
// work: Apple's HIG describes Platinum entirely in prose — no ramps, no
// bevel widths, no pixel patterns. The "published specification" it is
// usually credited with is not there. Motif's is, in source, and that is
// why this subject and not that one.)
//
// SOURCES — [SRC] read out of shipped source, [MEAS] pixel-sampled,
// [CALC] computed from the algorithm, [EST] estimated.
//
//   · THE ALGORITHM. Open Motif R2.1.30, lib/Xm/Color.c + lib/Xm/ColorP.h
//     (The Open Group's own mirror,
//     opengroup.org/infosrv/openmotif/R2.1.30/motif/lib/Xm/). Every
//     constant below is a verbatim #define; Brightness() and
//     CalculateColorsFor{Light,Dark,Medium}Background() are transcribed
//     line for line from that file.
//   · THE BEVEL. lib/Xm/Draw.c, DrawSimpleShadow / XmeDrawShadows /
//     XmeDrawHighlight — the segment table in motifShadowPaths() is that
//     function, index arithmetic included.
//   · THE PALETTES. All 41 shipped cde/programs/palettes/*.dp from
//     github.com/cdesktopenv/cde. Default / Crimson / Black / Summer are
//     quoted verbatim as 16-bit X colour specs.
//   · THE FRONT PANEL. cde/programs/types/dtwm.fp.src — "a full
//     definition for the default front panel". The control list, its
//     order, its subpanels and DISPLAY_CONTROL_LABELS False all come
//     from that file.
//   · THE BACKDROP. cde/programs/backdrops/PinStripe.pm, "28 52 2 1",
//     drawn in bottomShadowColor + topShadowColor. All 52 rows were read
//     and reduced to the four-line rule in pinStripeTile(); the rule was
//     then checked back against the file — 1456 of 1456 pixels agree.
//   · THE SCREENSHOTS. commons.wikimedia.org Category:Common Desktop
//     Environment — CDE_running_on_Solaris_10.png (1024x768, palette
//     Crimson, which is how the algorithm was verified) and
//     CDE_Debian_Workspace_1.png (1600x900, Default). Every [MEAS]
//     number — the 6 px window frame, the 23 px title bar, the 31 px
//     menu bar, the front panel's 953x86 and its cell table — is
//     sampled from those two images.
//
// RECONSTRUCTED, and flagged as such: the icon ARTWORK. CDE's 48x48
// icons are hand-drawn XPMs by HP's User Interaction Design Group. The
// ones here are drawn from the same symbolic-colour vocabulary the real
// XPM colour tables use (`s background`, `s topShadowColor`,
// `s bottomShadowColor`, `s selectColor`, plus the FIXED iconGray1..8
// ramp), so they re-colour with the palette the way a real CDE icon
// does — but they are not pixel-identical to HP's. The typeface is also
// a substitution: CDE's UI font is the X11 BITMAP face
// `-dt-interface system-medium-r-normal-*-*-100-*`, ~13 px with a 9–10 px
// cap height [MEAS]; Helvetica at 13 px stands in for it.
//
// FOUR THINGS THE ALGORITHM TEACHES THAT NO RECONSTRUCTION FROM MEMORY
// WOULD GET:
//
//  · C'S INTEGER DIVISION IS LOAD-BEARING. The medium-model bottom-shadow
//    factor interpolates DOWNWARD (LO_BS 60 -> HI_BS 40), so its term
//    `brightness * (40 - 60) / 65535` is NEGATIVE, and C truncates toward
//    zero where Python/Ruby/Haskell floor. Run over every shipped
//    palette, 291 of 291 medium colours get a DIFFERENT bottom shadow
//    under floor division, by up to 3/255 per channel. That is #5D6069
//    (right) against #5F616B (wrong) on the Front Panel — exactly the
//    class of error that makes a reconstruction look subtly off with
//    nothing to point at. cdiv() below is written out, and there is a
//    static_assert pinning the semantic.
//  · CDE'S DESIGNERS NEVER USED THE DARK BRANCH, and used the light
//    branch five times in 328 shipped colours — all five the near-white
//    text field (Crimson, DarkGold, Delphinium, Desert, Summer). The
//    light branch darkens ALL THREE derived colours, the "top" shadow
//    included, which is why a CDE entry field reads as ETCHED rather than
//    sunken: Crimson's #FFF7E9 field has a #CCC5BA top shadow and a
//    #99948B bottom one, two greys and no white anywhere. If your
//    near-white field has a white highlight on it you took the wrong
//    branch. (Black.dp is the only shipped palette that reaches the dark
//    branch; it is in the cycle for exactly that reason.)
//  · THE ARITHMETIC IS DEFINED ON GAMMA-ENCODED INTEGERS AND DOING IT
//    "PROPERLY" DESTROYS IT. The same factors applied in linear light
//    move the bottom shadow by up to 0x39 — #5D6069 becomes #838794,
//    a hard engraved edge becoming a washed-out mid-grey. Colour here
//    carries no space with it, so a value that is display-encoded ON
//    PURPOSE and a value nobody thought about are the same bytes, and
//    nothing but this comment tells them apart.
//  · SUNKEN IS NOT A DIFFERENT DRAWING. XmeDrawShadows implements
//    XmSHADOW_IN by swapping the two GCs and calling the same function.
//    One boolean covers every Motif widget state, and MotifShadow below
//    is built that way.
//
// VERIFICATION. calculate() reproduces, to the byte, the four values
// sampled off the Solaris screenshot: Front Panel ts #DCDEE5, bs
// #5D6069, active title bar #B24D7A — and, on a background that appears
// in NO shipped palette, the workspace-switch button #63639C -> ts
// #B7B7D1 / bs #2F2F4A. The derivation strip draws all of them, with the
// verdict computed from the two values rather than written by hand.
//
// And the render is verified against the [MEAS] band tables too, by
// sampling the capture. Window frame, going in from the left edge:
// DCADC2 DCADC2 | B24D7A B24D7A B24D7A | 57253B — 2 px top shadow, 3 px
// band, 1 px inverted inner bevel. Vertically: frame 6,
// title bar 23 (1 ts + 21 body + 1 bs), menu bar 31 (1 + 29 + 1), then
// the client area. Front Panel from its top edge: DCDEE5 DCDEE5 |
// AEB2C3 AEB2C3 | DCDEE5 DCDEE5 — the raised T=2 shell containing a
// raised T=2 box, 2 px apart. Every band is an exact number of device
// pixels with no fractional coverage anywhere.
//
// ---------------------------------------------------------------------
// WHAT DRAWING IT TEACHES, THAT THE MEASUREMENTS DO NOT
//
//  · A BOUND `Fill` IS THE WRONG TRADE FOR A COLOUR THAT CHANGES EVERY
//    FEW SECONDS. Bound, a palette change is forty assignments and no
//    re-describe — but `isAnimated()` is declared per NODE and is
//    BINARY, so every node carrying one of those colours is
//    content-volatile forever and paints live on every frame, which here
//    is most of the desktop. As values reached through `env::Provide`,
//    almost everything prunes and picture-caches, at the cost of one
//    heavier frame — a re-describe the reconciler patches — when the
//    palette actually turns. Nothing is wrong with the binding; it just
//    cannot say "this repaints when the theme changes" as distinct from
//    "this repaints at 60 Hz".
//  · A WIDGET DOES NOT CHOOSE ITS OWN COLOUR SET, so it should not be
//    handed one. The set is a property of the window a widget was
//    composed inside, which is what an inherited value is:
//    `core::env::Provide<ColorSet>` opens the scope and every bevel, label,
//    stipple and icon four levels down reads the same one. Threading a
//    set by argument is how a window ends up drawn in three of them, and
//    a study of this derivation cannot afford that mistake.
//  · A 2 px BAND LANDS EXACTLY ON THE DEVICE PIXEL GRID. At contentScale
//    1 with integer rects and AA off, every sampled band above is one
//    flat colour with no 254/255 edge anywhere. Yoga's resolved rects
//    are integers when the inputs are, `.padding(6)` is exact, and
//    inset(5, …)'s path-op offset survives it.
//  · EDGING IS ASKED FOR. `ShapingStyle::aliased` (compose spells it
//    `Type::aliased`) selects hard-edged rasterisation and is part of the
//    shape-cache key, so every one of the ~80 UI runs on this canvas goes
//    through the text engine and comes out 1-bit, the way an X core font
//    does. Motif's mnemonic underline goes with it: two spans and a
//    `Decoration{Kind::kUnderline}` underline exactly one character,
//    which is what Motif's is.
//  · ZERO outline() CALLS, and it is worth saying. Motif has no shaped
//    nodes: every silhouette is the default rect, so the rule that a
//    shaped node can never prune costs this artefact exactly nothing.
//    The only three corners() in the file are inside the clock icon,
//    which is artwork, not chrome.
//  · `.background()` IS NOT WHERE A BEVEL GOES. It paints BENEATH the
//    fill (the CSS box-shadow ordering), so a bevel put there is drawn
//    and then covered by the surface it was meant to sit on, leaving a
//    flat slab of colour. `.overlay()` is the slot that means "over the
//    fill, under content and children", which is exactly a Motif
//    shadow's layer. Every bevel in this file uses it.
//
// EDIT THESE FIRST
//   kPalettes        — the shipped .dp files this cycles through. Every
//                      colour on the canvas is derived from their eight
//                      backgrounds and nothing else; add one and the whole
//                      desktop re-themes.
//   the 3.0 s in update() — how long each palette holds. The declared
//                      capture moment is chosen against it: Default holds
//                      [12, 15) and 13.5 s is dead centre, so the plate is
//                      never caught mid-switch.
//   the sweep's rate  — how fast the derivation strip walks the grey ramp
//                      that crosses both branch thresholds.
//   kType / kTrack    — the UI register. kType is the measured size; kTrack
//                      is the correction that puts a Helvetica run at the
//                      X core face's own width.

#include <include/core/SkBitmap.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilmotion/bind/Bind.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilcore/reconcile/Env.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilmaterial/kit/Patterns.h>
#include <sigilcompose/brush/Adaptors.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilcompose/testing/Checks.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace measure = sigil::measure;
namespace test = sigil::compose::test;
namespace env = sigil::core::env;
namespace motion = sigil::motion;
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
//    it is one: `core::env::Provide<ColorSet>` opens the scope a piece of
//    chrome is drawn in, and every bevel, label, stipple and icon under
//    it reads the same set without being handed it. Threading a set by
//    argument through six levels is how a window ends up drawn in three
//    of them by accident, and one window in three sets is the one
//    mistake a study of this derivation cannot afford.
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
inline ColorSet ambient() { return env::inheritedOr(ColorSet{}); }

// ===========================================================================
// 4. THE BEVEL — lib/Xm/Draw.c, DrawSimpleShadow(), segment for segment.
// ===========================================================================

/** X11's XDrawSegments includes BOTH endpoints and draws one pixel wide,
 *  so a segment is the half-open rect [min, max+1). */
inline void seg(SkPathBuilder& p, int x0, int y0, int x1, int y1) {
  const float l = (float)std::min(x0, x1), t = (float)std::min(y0, y1);
  const float r = (float)(std::max(x0, x1) + 1),
              b = (float)(std::max(y0, y1) + 1);
  p.addRect(SkRect::MakeLTRB(l, t, r, b));
}

/** DrawSimpleShadow(x, y, w, h, T, cor), verbatim.
 *
 *  The mitre FALLS OUT OF THE ARITHMETIC and is the entire corner
 *  treatment: TOP's right end is `x + w - i - 1`, so at T=2 the top row
 *  runs full width and the second row stops one pixel short. LEFT starts
 *  at `y + T`, not `y + i`, so the top-LEFT corner is entirely top-shadow
 *  and square — only the two off-corners step. A reconstruction that
 *  strokes a rectangle misses both. */
inline void motifShadowPaths(SkPathBuilder& topP, SkPathBuilder& botP, int x,
                             int y, int w, int h, int T, int cor) {
  T = std::min({T, w / 2, h / 2});
  if (T <= 0) return;
  for (int i = 0; i < T; ++i) {
    seg(topP, x, y + i, x + w - i - 1, y + i);                  // TOP
    seg(topP, x + i, y + T, x + i, y + h - i - 1);              // LEFT
    seg(botP, x + i + (cor ? 0 : 1), y + h - i - 1, x + w - 1,  // BOTTOM
        y + h - i - 1);
    seg(botP, x + w - i - 1, y + i + 1 - cor, x + w - i - 1,
        y + h - 1);  // RIGHT
  }
}

/** The Motif shadow as a comparable DecorationScheme, holding the two
 *  Output<Fill>s so a palette change repaints it without a re-describe.
 *
 *  `sunken` is XmSHADOW_IN and it is LITERALLY A SWAP of the two colours —
 *  XmeDrawShadows does exactly that and then calls the same function.
 *  `etched` is XmSHADOW_ETCHED_IN: two passes at T/2 with cor=1, the
 *  second inverted, which is the only place in a CDE session that branch
 *  is visible (menu separators).
 *
 *  Antialiasing is OFF. Every coordinate here is an integer on the device
 *  pixel lattice at contentScale 1; AA can only soften it. */
struct MotifShadow {
  float thickness = 2;
  bool sunken = false;
  bool etched = false;
  SkColor4f top{1, 1, 1, 1}, bottom{0, 0, 0, 1};

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    const SkRect b = ctx.outline.getBounds();
    const int x = (int)std::lround(b.left()), y = (int)std::lround(b.top());
    const int w = (int)std::lround(b.width()), h = (int)std::lround(b.height());
    if (w <= 0 || h <= 0) return;
    SkColor4f ct = top, cb = bottom;
    if (sunken) std::swap(ct, cb);
    const int T = (int)thickness;
    SkPathBuilder pt, pb;
    if (etched && T != 1) {
      motifShadowPaths(pt, pb, x, y, w, h, T / 2, 1);
      motifShadowPaths(pb, pt, x + T / 2, y + T / 2, w - T, h - T, T / 2, 1);
    } else {
      motifShadowPaths(pt, pb, x, y, w, h, T, 0);
    }
    SkPaint p;
    p.setAntiAlias(false);
    p.setColor(ct, nullptr);
    canvas.drawPath(pt.detach(), p);
    p.setColor(cb, nullptr);
    canvas.drawPath(pb.detach(), p);
  }
  bool operator==(const MotifShadow&) const = default;
};

/** The bevel of the AMBIENT colour set — top shadow over bottom shadow.
 *  Handed nothing: a bevel is drawn in the set of the window it is in. */
inline MotifShadow bevel(float T, bool sunken, bool etched) {
  const ColorSet s = ambient();
  return MotifShadow{T, sunken, etched, s.ts, s.bs};
}
/** A bevel drawn in the FOREGROUND on both faces — the flat outline Motif
 *  puts inside a maximise box, which is a shadow with one colour. */
inline MotifShadow bevelFg(float T) {
  const ColorSet s = ambient();
  return MotifShadow{T, false, false, s.fg, s.fg};
}

/** XmeDrawHighlight: four plain rectangles, a square ring of
 *  `highlightThickness`. No mitre, no shading — it is not a shadow. */
struct MotifHighlight {
  float thickness = 2;
  SkColor4f color{0, 0, 0, 1};
  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    const SkRect b = ctx.outline.getBounds();
    const float t = thickness;
    SkPaint p;
    p.setAntiAlias(false);
    p.setColor(color, nullptr);
    canvas.drawRect(SkRect::MakeLTRB(b.left(), b.top(), b.right(), b.top() + t),
                    p);
    canvas.drawRect(
        SkRect::MakeLTRB(b.left(), b.bottom() - t, b.right(), b.bottom()), p);
    canvas.drawRect(
        SkRect::MakeLTRB(b.left(), b.top() + t, b.left() + t, b.bottom() - t),
        p);
    canvas.drawRect(
        SkRect::MakeLTRB(b.right() - t, b.top() + t, b.right(), b.bottom() - t),
        p);
  }
  bool operator==(const MotifHighlight&) const = default;
};

/** highlightThickness 2 [MEAS], in the widget's foreground — Motif's
 *  default highlightColor. Only the focused widget shows one. */
inline MotifHighlight highlight(float T) {
  return MotifHighlight{T, ambient().fg};
}

/** Motif's insensitive treatment: the label is painted through a 50%
 *  checkerboard, `stipple(x, y) = (x + y) & 1`. Reproduced the other way
 *  round — the background painted over the complementary half — which is
 *  the identical result and lets the colour stay bound.
 *
 *  patterns::checker(1, on, off) is this pattern exactly, and it is not
 *  usable here: a Pattern BAKES its colours into the tile, so a themed
 *  stipple would need one Pattern per palette per colour. A 2x2 mask
 *  image plus SkColorFilters::Blend(colour, kSrcIn) is one tile for all
 *  of them, tinted where it is drawn. */
inline sk_sp<SkImage> checkerMask() {
  static sk_sp<SkImage> mask = [] {
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(2, 2));
    bm.eraseColor(SK_ColorTRANSPARENT);
    *bm.getAddr32(0, 0) = 0xFFFFFFFFu;
    *bm.getAddr32(1, 1) = 0xFFFFFFFFu;
    bm.setImmutable();
    return bm.asImage();
  }();
  return mask;
}

struct MotifStipple {
  SkColor4f color{1, 1, 1, 1};
  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    SkPaint p;
    p.setAntiAlias(false);
    p.setShader(
        checkerMask()->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat,
                                  SkSamplingOptions(SkFilterMode::kNearest)));
    p.setColorFilter(
        SkColorFilters::Blend(color.toSkColor(), SkBlendMode::kSrcIn));
    canvas.drawRect(ctx.outline.getBounds(), p);
  }
  bool operator==(const MotifStipple&) const = default;
};

inline MotifStipple stipple() { return MotifStipple{ambient().bg}; }

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
  static sk_sp<SkTypeface> f = weave::ports::pickTypeface({"Helvetica", "Arial"});
  return f;
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

/** The UI register: one face, one size, HARD EDGES. `aliased` selects
 *  1-bit rasterisation and is part of the shape-cache key, so a run
 *  carries it through shaping — which is the whole of what a 1995 X11
 *  desktop's type looks like, and the one property this reconstruction
 *  cannot approximate. */
inline sigil::weave::TextStyle type(SkColor4f c, float size = kType) {
  return weave::textStyle({.face = uiFace(),
                               .size = size,
                               .color = c,
                               .track = kTrack,
                               .aliased = true,
                               .antiAlias = false});
}

/** One run of UI type, with Motif's mnemonic underline on exactly one
 *  character when asked. Two spans and a `Decoration{Kind::kUnderline}`
 *  at thickness 1, which is what Motif's underline is; `skipInk` off,
 *  because Motif's does not break for a descender. */
inline Element label(std::string_view t, SkColor4f c, float size = kType,
                     int mnemonic = -1) {
  const sigil::weave::TextStyle plain = type(c, size);
  if (mnemonic < 0 || mnemonic >= (int)t.size())
    return text(toU8(t), plain).shrink(0);
  sigil::weave::TextStyle under = plain;
  sigil::weave::Decoration d;
  d.kind = sigil::weave::Decoration::Kind::kUnderline;
  d.skipInk = false;
  d.thickness = 1;
  d.color = c.toSkColor();
  under.paint.addDecoration(d);
  return text(rich(plain)
                  .add(toU8(t.substr(0, (size_t)mnemonic)))
                  .add(toU8(t.substr((size_t)mnemonic, 1)), under)
                  .add(toU8(t.substr((size_t)mnemonic + 1))))
      .shrink(0);
}

// ===========================================================================
// 7. WIDGETS.  Not one takes a colour set as an argument: each reads
//    `ambient()` — the set the window it was composed inside opened with
//    `env::Provide<ColorSet>` — so a bevel, a label, a stipple and an
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
  Element lab = label(t, s.fg);
  Element inner = box()
                      .fill(armed ? s.sel : s.bg)
                      .overlay(bevel(2, armed, false))
                      .padding(2)
                      .alignItems(Align::Center)
                      .justify(Justify::Center)
                      .height(Dim(25))
                      .child(box()
                                 .padding(6, 2)
                                 .alignItems(Align::Center)
                                 .justify(Justify::Center)
                                 .child(std::move(lab)));
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
                      .child(label(t, s.fg));
  if (caret && caretOut)
    inner.child(box().width(Dim(1)).height(Dim(13)).fill(s.fg).opacity(
        motion::bind(caretOut).quantize(2)));
  Element field = box()
                      .fill(s.bg)
                      .overlay(bevel(2, true, false))
                      .padding(2)
                      .height(Dim(24))
                      .row()
                      .alignItems(Align::Center)
                      .child(std::move(inner));
  if (!caret) return field.width(Dim(w));
  // The focused widget carries XmeDrawHighlight's ring OUTSIDE its
  // shadow: four plain rectangles of highlightThickness, no mitre.
  return box()
      .width(Dim(w))
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
// Rendered as greedy merged rectangles, so a 24x24 grid becomes ~20-30
// boxes rather than 576. Symbolic characters get a bound Fill; the fixed
// ramp gets a constant one, so most of an icon is statically cached and
// only the symbolic runs repaint on a theme change.
// ---------------------------------------------------------------------------

inline std::optional<SkColor4f> fixedColor(char ch) {
  if (ch >= '1' && ch <= '8') return C(kIconGray[(size_t)(ch - '1')]);
  switch (ch) {
    case 'k':
      return C(kIconColor[0]);
    case 'w':
      return C(kIconColor[1]);
    case 'r':
      return C(kIconColor[2]);
    case 'g':
      return C(kIconColor[3]);
    case 'u':
      return C(kIconColor[4]);
    case 'y':
      return C(kIconColor[5]);
    case 'c':
      return C(kIconColor[6]);
    case 'm':
      return C(kIconColor[7]);
    default:
      return std::nullopt;
  }
}

inline std::optional<SkColor4f> symbolic(char c, const ColorSet& s) {
  switch (c) {
    case 'B':
      return s.bg;
    case 'T':
      return s.ts;
    case 'S':
      return s.bs;
    case 'L':
      return s.sel;
    case 'F':
      return s.fg;
    default:
      return std::nullopt;
  }
}

/** Greedy rectangle merge over an ASCII colour grid. */
inline Element art(const std::vector<std::string>& rows, float cell) {
  const ColorSet s = ambient();
  const int h = (int)rows.size();
  const int w = h ? (int)rows[0].size() : 0;
  Element root =
      stack().width(Dim((float)w * cell)).height(Dim((float)h * cell));
  std::vector<char> done((size_t)(w * h), 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const size_t idx = (size_t)y * (size_t)w + (size_t)x;
      if (done[idx]) continue;
      const char ch = rows[(size_t)y][(size_t)x];
      if (ch == ' ' || ch == '.') {
        done[idx] = 1;
        continue;
      }
      int rw = 1;
      while (x + rw < w && rows[(size_t)y][(size_t)x + (size_t)rw] == ch &&
             !done[(size_t)y * (size_t)w + (size_t)x + (size_t)rw])
        ++rw;
      int rh = 1;
      while (y + rh < h) {
        bool ok = true;
        for (int k = 0; k < rw; ++k)
          if (rows[(size_t)y + (size_t)rh][(size_t)x + (size_t)k] != ch ||
              done[((size_t)y + (size_t)rh) * (size_t)w + (size_t)x +
                   (size_t)k]) {
            ok = false;
            break;
          }
        if (!ok) break;
        ++rh;
      }
      for (int j = 0; j < rh; ++j)
        for (int k = 0; k < rw; ++k)
          done[((size_t)y + (size_t)j) * (size_t)w + (size_t)x + (size_t)k] = 1;
      Element cellBox = box()
                            .left(Dim((float)x * cell))
                            .top(Dim((float)y * cell))
                            .width(Dim((float)rw * cell))
                            .height(Dim((float)rh * cell));
      if (auto fc = fixedColor(ch))
        cellBox.fill(*fc);
      else if (auto sym = symbolic(ch, s))
        cellBox.fill(*sym);
      root.child(std::move(cellBox));
    }
  }
  return root;
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

struct CdeMotifSketch : sketch::Sketch {
  using Set = cde::ColorSet;

  cde::Theme theme;
  int paletteIndex = 0;
  double nextSwitch = 0.0;

  // The one thing on this canvas allowed to move smoothly, because it is
  // showing a FUNCTION rather than a desktop.
  ch::Output<float> sweep{0.0f};     // 0..1, the grey ramp under the strip
  ch::Output<float> clockT{0.42f};   // fraction of an hour; the minute hand
  ch::Output<float> busy{0.0f};      // the TYPE busy blinker, ~2 Hz
  ch::Output<float> caret{0.0f};     // the text field's I-beam, ~1 Hz
  ch::Output<float> subpanel{0.0f};  // the Text Editor subpanel's wipe

  std::array<Pattern, 4> backdrops;
  bool backdropsBuilt = false;

  // -------------------------------------------------------------------------

  /** THE VERIFICATION, as claims rather than as printed lines.
   *
   *  Four bytes sampled off a photograph of a running CDE desktop, plus
   *  the one branch that is easy to get backwards. The verdict is never
   *  written by hand — `check()` computes it from the two values — and
   *  the table is DRAWN, on the derivation strip, because a study whose
   *  subject is an algorithm should carry its own proof in the picture
   *  rather than in a console nobody captures. */
  static measure::Table derivation() {
    struct Case {
      const char* what;
      uint32_t bg, ts, bs;
    };
    const Case cases[] = {
        {"Front Panel", 0xAEB2C3, 0xDCDEE5, 0x5D6069},
        {"switch btn", 0x63639C, 0xB7B7D1, 0x2F2F4A},
    };
    auto hexOf = [](cde::Rgb c) {
      char buf[16];
      std::snprintf(buf, sizeof buf, "#%06X", cde::toHex(c));
      return std::string(buf);
    };
    measure::Table t;
    for (const Case& c : cases) {
      const cde::Derived d = cde::calculate(cde::from8(c.bg));
      char want[16];
      std::snprintf(want, sizeof want, "#%06X", c.ts);
      t.add(measure::check(std::string(c.what) + " ts", want, hexOf(d.ts)));
      std::snprintf(want, sizeof want, "#%06X", c.bs);
      t.add(measure::check(std::string(c.what) + " bs", want, hexOf(d.bs)));
    }
    // The five LITE colours CDE ships are all colour-set 4, and the top
    // shadow comes out DARKER than the background on every one — which is
    // the branch a reimplementation gets backwards.
    const cde::Derived lite = cde::calculate(cde::from8(0xFFF7E9));
    t.add(measure::check("#FFF7E9 takes LITE", lite.branch == cde::Branch::Lite));
    t.add(measure::check("LITE ts darker than bg", lite.ts.r < lite.bg.r));
    return t;
  }

  // -------------------------------------------------------------------------
  // Window chrome.

  /** The 6 px Motif window frame [MEAS]: a raised outer bevel of 2, a 3 px
   *  band, and a 1 px INNER bevel that is inverted — you are looking at
   *  the inside face of a ridge. The inner one goes through
   *  inset(5, …), which is that helper's exact use case ("the
   *  same bevel again, five pixels in"). */
  Element windowFrame(Element content) {
    const Set s = cde::ambient();
    return box()
        .fill(s.bg)
        .overlay(cde::bevel(2, false, false))
        .overlay(inset(5, cde::bevel(1, true, false)))
        .padding(6)
        .column()
        .child(std::move(content));
  }

  /** The title bar: 23 px, shadowThickness 1, with the window-menu button
   *  at the left and minimise/maximise at the right [MEAS]. The label is
   *  centred in the derived foreground — white on both #B24D7A and
   *  #EDA870, because both sets fall under the 70% threshold. */
  Element titleBar(std::string_view t, bool active) {
    const Set s = cde::ambient();
    auto furniture = [&](Element glyph) {
      return box()
          .width(Dim(20))
          .height(Dim(19))
          .fill(s.bg)
          .overlay(cde::bevel(1, false, false))
          .alignItems(Align::Center)
          .justify(Justify::Center)
          .child(std::move(glyph));
    };
    Element menuGlyph = box().width(Dim(12)).height(Dim(4)).fill(s.fg);
    Element minGlyph = box().width(Dim(5)).height(Dim(5)).fill(s.fg);
    Element maxGlyph =
        box().width(Dim(11)).height(Dim(11)).overlay(cde::bevelFg(1));
    return box()
        .height(Dim(23))
        .fill(s.bg)
        .overlay(cde::bevel(1, false, false))
        .row()
        .alignItems(Align::Center)
        .padding(2, 1)
        .child(furniture(std::move(menuGlyph)))
        .child(box()
                   .grow(1)
                   .alignItems(Align::Center)
                   .justify(Justify::Center)
                   .child(cde::label(t, s.fg)))
        .child(furniture(std::move(minGlyph)))
        .child(box().width(Dim(2)))
        .child(furniture(std::move(maxGlyph)));
  }

  /** The 31 px menu bar [MEAS], in colour set 6, with Motif's mnemonic
   *  underline on exactly one character of every label. */
  Element menuBar(const std::vector<std::string>& items, int rightFrom) {
    const Set s = cde::ambient();
    Element bar = box()
                      .height(Dim(31))
                      .fill(s.bg)
                      .overlay(cde::bevel(1, false, false))
                      .row()
                      .alignItems(Align::Center)
                      .padding(10, 1);
    for (int i = 0; i < (int)items.size(); ++i) {
      if (i == rightFrom) bar.child(box().grow(1));
      bar.child(box().padding(8, 4).child(
          cde::label(items[(size_t)i], s.fg, cde::kType, 0)));
    }
    return bar;
  }

  // -------------------------------------------------------------------------
  // The File Manager. Frame and title in set 1, menu bar and client area
  // in set 5, the path field in set 4 and the scrollbar in set 3 — four
  // scopes opened around four subtrees, and no widget below them told
  // which it is in.

  Element fileManager() {
    // THE WINDOW'S COLOUR SETS, and the scope each one covers. The frame
    // and the title bar are set 1; the menu bar and the client area are
    // ONE set together, which is what the reference shows — a CDE window
    // is chrome plus a body, not three bands of three colours. Only the
    // path field is its own (set 4 is the near-white text set, and the
    // only one that ever takes the LITE branch), and the scrollbar is
    // set 3 because a scrollbar is dtwm's, not the application's.
    env::Provide<cde::ColorSet> chrome(theme[1]);
    Element title = titleBar("File Manager - user", true);

    Element window;
    {
      env::Provide<cde::ColorSet> body(theme[5]);
      const Set& c5 = cde::ambient();

      // The reference holds 31 folders in a pane this size. They flow
      // and wrap, the way an XmContainer lays icons out, rather than
      // sitting in a hand-counted grid.
      static const char* kNames[31] = {
          "bin",    "boot",     "cdrom",  "dev",   "devices",    "etc",
          "export", "home",     "kernel", "lib",   "mnt",        "net",
          "opt",    "platform", "proc",   "sbin",  "system",     "tmp",
          "usr",    "var",      "vol",    "xfn",   "lost+found", "core",
          "mail",   "news",     "share",  "spool", "local",      "adm",
          "log"};

      Element grid = box().row().wrapLines().gap(4).padding(8, 8);
      for (const char* name : kNames)
        grid.child(box()
                       .width(Dim(70))
                       .column()
                       .alignItems(Align::Center)
                       .gap(2)
                       .child(cde::art(cde::icoFolder(), 2.0f))
                       .child(cde::label(name, c5.fg)));

      // The scrollbar: a sunken trough with a raised slider, and a
      // STEPPER at each end — CDE puts an arrow box top and bottom, and
      // without them the trough reads as a plain groove.
      Element scrollbar;
      {
        env::Provide<cde::ColorSet> bar(theme[3]);
        const Set& c3 = cde::ambient();
        auto stepper = [&](bool up) {
          return box()
              .width(Dim(15))
              .height(Dim(15))
              .shrink(0)
              .fill(c3.bg)
              .overlay(cde::bevel(2, false, false))
              .alignItems(Align::Center)
              .justify(Justify::Center)
              .child(box()
                         .width(Dim(9))
                         .height(Dim(7))
                         .shape(shapes::polygon(3, up ? 0.0f : 180.0f))
                         .fill(c3.fg));
        };
        scrollbar = box()
                        .width(Dim(19))
                        .column()
                        .padding(2)
                        .gap(2)
                        .fill(c3.bg)
                        .overlay(cde::bevel(2, true, false))
                        .child(stepper(true))
                        .child(box().grow(1).column().child(
                            box().height(Dim(150)).fill(c3.bg).overlay(
                                cde::bevel(2, false, false))))
                        .child(stepper(false));
      }

      Element pathRow = box()
                            .row()
                            .alignItems(Align::Center)
                            .padding(8, 6)
                            .gap(8)
                            .child(cde::label("Path:", c5.fg));
      {
        env::Provide<cde::ColorSet> field(theme[4]);
        pathRow.child(cde::textField("/export/home/user", 420, true, &caret));
      }

      Element client =
          box()
              .grow(1)
              .fill(c5.bg)
              .column()
              .child(std::move(pathRow))
              // The icon pane is an XmScrolledWindow: XmSHADOW_IN at
              // T = 2, which is why a CDE file view reads as a well and
              // not as a sheet of colour.
              .child(box()
                         .grow(1)
                         .margin(6, 0, 6, 0)
                         .row()
                         .overlay(cde::bevel(2, true, false))
                         .padding(2)
                         .child(box().grow(1).clip().child(std::move(grid)))
                         .child(std::move(scrollbar)))
              .child(box().height(Dim(2)).margin(2, 3).overlay(
                  cde::bevel(2, true, true)))
              .child(box()
                         .height(Dim(22))
                         .row()
                         .alignItems(Align::Center)
                         .padding(8, 2)
                         .child(cde::label("31 Items  11 Hidden", c5.fg)));

      window = box()
                   .grow(1)
                   .column()
                   .child(std::move(title))
                   .child(menuBar({"File", "Selected", "View", "Help"}, 3))
                   .child(std::move(client));
    }
    return windowFrame(std::move(window));
  }

  // -------------------------------------------------------------------------
  // The Style Manager's Color dialog. The self-documenting piece: the
  // eight numbers of the theme, on screen, in the theme.

  Element colorDialog() {
    const Set& c1 = theme[1];
    const Set& c2 = theme[2];  // dtsession's primary set — unstyled widgets
    const Set& c6 = theme[6];  // list panes

    Element list = box()
                       .fill(c6.bg)
                       .overlay(cde::bevel(2, true, false))
                       .padding(2)
                       .grow(1)
                       .column();
    for (int i = 0; i < (int)cde::kPalettes.size(); ++i) {
      const bool current = i == paletteIndex;
      Element rowBox =
          box()
              .row()
              .alignItems(Align::Center)
              .height(Dim(20))
              .padding(6, 0)
              .child(cde::label(cde::kPalettes[(size_t)i]->name, c6.fg));
      if (current) rowBox.fill(c6.sel);
      list.child(std::move(rowBox));
    }
    for (const char* n : {"Cabernet", "Charcoal", "Delphinium", "Desert",
                          "GrayScale", "Lilac", "Neptune", "Olive"})
      list.child(box()
                     .row()
                     .alignItems(Align::Center)
                     .height(Dim(20))
                     .padding(6, 0)
                     .child(cde::label(n, c6.fg)));

    // XmScrollBar: a sunken trough in the workspace set with a raised
    // slider, 15 px of trough plus 2 px of shadow either side [MEAS].
    auto scrollBar = [&](const Set& t, float sliderFrac, float sliderTop) {
      return box()
          .width(Dim(19))
          .fill(t.bg)
          .overlay(cde::bevel(2, true, false))
          .padding(2)
          .column()
          .child(box().height(Dim(sliderTop)))
          .child(box()
                     .height(pct(sliderFrac))
                     .fill(t.bg)
                     .overlay(cde::bevel(2, false, false)));
    };

    Element listPane = box()
                           .row()
                           .width(Dim(170))
                           .child(std::move(list))
                           .child(scrollBar(theme[3], 34, 0));

    // The eight colour-set swatches, 4 x 2. This IS the palette file.
    Element swatches = box().column().gap(6);
    for (int r = 0; r < 2; ++r) {
      Element rr = box().row().gap(6);
      for (int c = 0; c < 4; ++c) {
        const int idx = r * 4 + c + 1;
        rr.child(box()
                     .width(Dim(50))
                     .height(Dim(42))
                     .fill(theme[idx].bg)
                     .overlay(cde::bevel(2, false, false)));
      }
      swatches.child(std::move(rr));
    }

    Element buttons = box()
                          .row()
                          .gap(10)
                          .justify(Justify::SpaceBetween)
                          .child(cde::pushButton("OK", false, true))
                          .child(cde::pushButton("Add..."))
                          .child(cde::pushButton("Delete", false, false, true))
                          .child(cde::pushButton("Help"));

    Element body =
        box()
            .fill(c2.bg)
            .grow(1)
            .column()
            .padding(10)
            .gap(10)
            .child(box()
                       .row()
                       .gap(12)
                       .grow(1)
                       .child(box()
                                  .column()
                                  .gap(4)
                                  .child(cde::label("Palettes", c2.fg))
                                  .child(std::move(listPane)))
                       .child(box()
                                  .column()
                                  .gap(4)
                                  .child(cde::label("Color Sets", c2.fg))
                                  .child(std::move(swatches))
                                  .child(box().height(Dim(6)))
                                  .child(cde::label("Number of Colors:", c2.fg))
                                  .child(cde::label("  High Color  (8 sets)",
                                                    c2.fg))))
            .child(box().height(Dim(2)).overlay(cde::bevel(2, false, true)))
            .child(std::move(buttons));

    return windowFrame(box()
                           .grow(1)
                           .column()
                           .child(titleBar("Style Manager - Color", false))
                           .child(std::move(body)));
  }

  // -------------------------------------------------------------------------
  // A torn-off menu, posted over the desktop. Set 6, shadowThickness 2.

  Element postedMenu() {
    const Set& s = theme[6];
    auto item = [&](std::string_view t, bool cascade, bool insensitive) {
      Element row = box()
                        .row()
                        .alignItems(Align::Center)
                        .height(Dim(24))
                        .padding(14, 0)
                        .child(cde::label(t, s.fg))
                        .child(box().grow(1));
      if (cascade)
        row.child(box().width(Dim(9)).height(Dim(9)).fill(s.bg).overlay(
            cde::bevel(2, false, false)));
      if (insensitive) row.foreground(cde::stipple());
      return row;
    };
    // XmSHADOW_ETCHED_IN at T = 2 — a two-pass etched shadow, and the only
    // place in a CDE session where that branch of XmeDrawShadows shows.
    auto separator = [&] {
      return box().height(Dim(2)).margin(3).overlay(cde::bevel(2, true, true));
    };
    // The tear-off "perforation" Motif puts at the top of a posted menu.
    Element tearOff =
        box().height(Dim(9)).margin(3).overlay(cde::bevel(2, true, true));

    return box()
        .fill(s.bg)
        .overlay(cde::bevel(2, false, false))
        .padding(2)
        .width(Dim(214))
        .column()
        .child(std::move(tearOff))
        .child(item("New...", false, false))
        .child(item("Open", true, false))
        .child(separator())
        .child(item("Print", false, true))
        .child(item("Properties...", false, false))
        .child(separator())
        .child(item("Close", false, false));
  }

  // -------------------------------------------------------------------------
  // The derivation strip. Not a CDE widget and it does not pretend to be:
  // a plain Motif frame with a label, showing the function running over a
  // grey ramp that crosses both thresholds.

  Element derivationStrip() {
    const Set& s = theme[2];
    const int v = std::clamp((int)std::lround(sweep.value() * 255.0f), 0, 255);
    const cde::Rgb bgv =
        cde::from8((uint32_t)v << 16u | (uint32_t)v << 8u | (uint32_t)v);
    const cde::Derived d = cde::calculate(bgv);

    auto swatch = [&](const char* name, cde::Rgb c) {
      char hex[16];
      std::snprintf(hex, sizeof(hex), "#%06X", cde::toHex(c));
      return box()
          .column()
          .gap(3)
          .alignItems(Align::Center)
          .child(box()
                     .width(Dim(70))
                     .height(Dim(36))
                     .fill(cde::toSk(c))
                     .overlay(cde::bevel(2, false, false)))
          .child(cde::label(name, s.fg))
          .child(cde::label(hex, s.fg));
    };

    const char* branch = d.branch == cde::Branch::Dark   ? "DARK"
                         : d.branch == cde::Branch::Lite ? "LITE"
                                                         : "MEDIUM";
    char line[128];
    std::snprintf(line, sizeof(line),
                  "B = %5d      branch %-6s      f = (%d, %d, %d)",
                  d.brightness, branch, d.fSel, d.fBs, d.fTs);

    Element proof = box().column().gap(1).justify(Justify::Center);
    for (const measure::Check& c : derivation().rows)
      proof.child(cde::label(c.line(22, 9), c.pass ? s.fg : s.bs, 10));

    return box()
        .fill(s.bg)
        .overlay(cde::bevel(2, false, false))
        .padding(2)
        .column()
        .child(box().padding(8, 6).child(
            cde::label("XmGetColors( bg ) - live", s.fg, cde::kType, 0)))
        .child(box()
                   .row()
                   .gap(18)
                   .padding(8, 0)
                   .child(box()
                              .row()
                              .gap(6)
                              .child(swatch("bg", d.bg))
                              .child(swatch("topShadow", d.ts))
                              .child(swatch("botShadow", d.bs))
                              .child(swatch("select", d.sel))
                              .child(swatch("foreground", d.fg)))
                   // THE PROOF, on the plate: the sampled bytes this
                   // algorithm has to reproduce, each with its verdict
                   // computed from the two values rather than written.
                   .child(std::move(proof)))
        .child(box().padding(8, 8).child(cde::label(line, s.fg)));
  }

  // -------------------------------------------------------------------------
  // The Front Panel. dtwm.fp.src's control list, in POSITION_HINTS order,
  // with DISPLAY_CONTROL_LABELS False — which is why it is only 86 px tall.

  /** The end handles: a 1-px alternating bottomShadow/topShadow texture,
   *  period 2 in y [MEAS at x = 60, y = 700..761, perfect alternation].
   *  The only texture on the panel. */
  Element handle() {
    const Set s = cde::ambient();
    Element h = box().width(Dim(18)).column();
    for (int i = 0; i < 31; ++i)
      h.child(box().height(Dim(1)).fill(((unsigned)i & 1u) ? s.ts : s.bs));
    return box()
        .width(Dim(18))
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .child(std::move(h));
  }

  Element panelSeparator() {
    return box().width(Dim(2)).column().child(
        box().grow(1).overlay(cde::bevel(2, true, true)));
  }

  /** A Front Panel control: a 48 x 48 icon, 4 px either side, with the
   *  small chevron above it that marks a subpanel [SRC: Text Editor,
   *  Printer, Applications and Help have subpanels]. */
  Element control(Element icon, float w, bool subpanelArrow) {
    const Set s = cde::ambient();
    Element chev = box()
                       .height(Dim(10))
                       .alignItems(Align::Center)
                       .justify(Justify::Center);
    if (subpanelArrow) {
      Element up = box().column().alignItems(Align::Center);
      for (int i = 0; i < 4; ++i)
        up.child(
            box().width(Dim((float)(1 + i * 2))).height(Dim(1)).fill(s.fg));
      chev.child(std::move(up));
    }
    return box()
        .width(Dim(w))
        .column()
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .child(std::move(chev))
        .child(std::move(icon));
  }

  /** Fpclock.l.pm plus dtwm's live hands. No second hand, and it STEPS
   *  ONCE A MINUTE — there is no smooth motion anywhere in CDE, so the
   *  step is declared with bind().quantize(61) rather than left to
   *  emerge from the frame rate. */
  Element clockIcon() {
    const Set s = cde::ambient();
    Element face = stack().width(Dim(48)).height(Dim(48));
    face.child(box().inset(0).corners({24}).fill(s.bg).foreground(
        PathFormat{.width = 2,
                   .strokeFill = Fill::color(cde::C(cde::kIconGray[6])),
                   .align = PathFormat::Align::Inner}));
    face.child(box().inset(4).corners({20}).fill(cde::C(cde::kIconGray[0])));
    // Twelve ticks, placed by arithmetic.
    for (int i = 0; i < 12; ++i) {
      const float a = (float)i * 30.0f * (float)M_PI / 180.0f;
      const float r = 18.0f;
      const float cx = 24.0f + std::sin(a) * r;
      const float cy = 24.0f - std::cos(a) * r;
      const float sz = (i % 3 == 0) ? 4.0f : 2.0f;
      face.child(box()
                     .left(Dim(cx - sz * 0.5f))
                     .top(Dim(cy - sz * 0.5f))
                     .width(Dim(sz))
                     .height(Dim(sz))
                     .fill(cde::C(cde::kIconColor[0])));
    }
    // Hour hand, ~60% radius; minute hand, full radius. Both quantised to
    // the minute: 61 levels across [0,1] is a 6-degree step.
    face.child(box()
                   .left(Dim(23))
                   .top(Dim(13))
                   .width(Dim(3))
                   .height(Dim(11))
                   .fill(cde::C(cde::kIconColor[0]))
                   .transformOrigin(0.5f, 1.0f)
                   .rotate(motion::bind(&clockT).quantize(61).scale(30).offset(300)));
    face.child(box()
                   .left(Dim(23))
                   .top(Dim(6))
                   .width(Dim(2))
                   .height(Dim(18))
                   .fill(cde::C(cde::kIconColor[0]))
                   .transformOrigin(0.5f, 1.0f)
                   .rotate(motion::bind(&clockT).quantize(61).scale(360)));
    face.child(box()
                   .left(Dim(22))
                   .top(Dim(22))
                   .width(Dim(4))
                   .height(Dim(4))
                   .corners({2})
                   .fill(cde::C(cde::kIconColor[0])));
    return face;
  }

  /** FpCM, the date control — a calendar page. */
  Element dateIcon() {
    const Set s = cde::ambient();
    return stack()
        .width(Dim(48))
        .height(Dim(48))
        .child(box()
                   .inset(3, 2, 3, 2)
                   .fill(cde::C(cde::kIconColor[1]))
                   .overlay(cde::bevel(2, false, false)))
        .child(box()
                   .left(Dim(5))
                   .top(Dim(4))
                   .width(Dim(38))
                   .height(Dim(13))
                   .fill(s.sel)
                   .alignItems(Align::Center)
                   .justify(Justify::Center)
                   .child(cde::label("Jul", s.fg, 11)))
        .child(box()
                   .left(Dim(5))
                   .top(Dim(18))
                   .width(Dim(38))
                   .height(Dim(24))
                   .alignItems(Align::Center)
                   .justify(Justify::Center)
                   .child(cde::label("22", cde::C(cde::kIconColor[0]), 19)));
  }

  /** The SWITCH: dtwm.fp.src gives it NUMBER_OF_ROWS 2, plus Lock, the
   *  `TYPE busy` blinker, a blank and Exit. Workspace n's button is filled
   *  with colour set 3, 8, 6 and 7 respectively [MEAS] — the single most
   *  visible use of the palette on the screen. */
  Element workspaceSwitch() {
    static const char* kNames[4] = {"One", "Two", "Three", "Four"};
    static const int kSets[4] = {3, 8, 6, 7};
    Element gridEl = box().column().gap(3);
    for (int r = 0; r < 2; ++r) {
      Element rr = box().row().gap(3);
      for (int c = 0; c < 2; ++c) {
        const int i = r * 2 + c;
        const Set& ws = theme[kSets[i]];
        rr.child(box()
                     .width(Dim(129))
                     .height(Dim(22))
                     .fill(ws.bg)
                     .overlay(cde::bevel(2, false, false))
                     .row()
                     .alignItems(Align::Center)
                     .padding(7, 0)
                     .child(cde::label(kNames[i], ws.fg)));
      }
      gridEl.child(std::move(rr));
    }
    Element left = box()
                       .column()
                       .gap(4)
                       .alignItems(Align::Center)
                       .justify(Justify::Center)
                       .width(Dim(26))
                       .child(cde::art(cde::icoLock(), 2.0f))
                       .child(box()
                                  .width(Dim(10))
                                  .height(Dim(10))
                                  .fill(cde::C(0x00C000))
                                  .overlay(cde::bevel(1, true, false))
                                  .opacity(motion::bind(&busy).quantize(2)));
    Element right = box()
                        .width(Dim(26))
                        .alignItems(Align::Center)
                        .justify(Justify::Center)
                        .child(cde::art(cde::icoExit(), 2.0f));
    return box()
        .width(Dim(324))
        .row()
        .alignItems(Align::Center)
        .justify(Justify::Center)
        .gap(3)
        .child(std::move(left))
        .child(std::move(gridEl))
        .child(std::move(right));
  }

  Element frontPanel() {
    const Set& s = theme[2];  // dtsession: the primary colour set

    Element rowEl = box().row().alignItems(Align::Center).height(Dim(74));
    rowEl.child(handle());
    rowEl.child(panelSeparator());
    rowEl.child(control(clockIcon(), 58, false));
    rowEl.child(panelSeparator());
    rowEl.child(control(dateIcon(), 56, false));
    rowEl.child(panelSeparator());
    rowEl.child(control(cde::art(cde::icoHome(), 2.0f), 56, false));
    rowEl.child(panelSeparator());
    rowEl.child(control(cde::art(cde::icoEditor(), 2.0f), 56, true));
    rowEl.child(panelSeparator());
    rowEl.child(control(cde::art(cde::icoMail(), 2.0f), 54, false));
    rowEl.child(panelSeparator());
    rowEl.child(workspaceSwitch());
    rowEl.child(panelSeparator());
    rowEl.child(control(cde::art(cde::icoPrinter(), 2.0f), 56, true));
    rowEl.child(panelSeparator());
    rowEl.child(control(cde::art(cde::icoStyle(), 2.0f), 54, false));
    rowEl.child(panelSeparator());
    rowEl.child(control(cde::art(cde::icoApps(), 2.0f), 58, true));
    rowEl.child(panelSeparator());
    rowEl.child(control(cde::art(cde::icoHelp(), 2.0f), 56, true));
    rowEl.child(panelSeparator());
    rowEl.child(control(cde::art(cde::icoTrash(), 2.0f), 56, false));
    rowEl.child(panelSeparator());
    rowEl.child(handle());

    // A raised T = 2 shell containing a raised T = 2 box, 2 px apart
    // [MEAS: ts/ts/bg/bg/ts/ts going in from the top edge].
    return box()
        .fill(s.bg)
        .overlay(cde::bevel(2, false, false))
        .padding(4)
        .child(box()
                   .grow(1)
                   .fill(s.bg)
                   .overlay(cde::bevel(2, false, false))
                   .padding(2)
                   .child(std::move(rowEl)));
  }

  /** A subpanel, sliding up out of the Front Panel. dtwm.fp.src gives
   *  subpanels to TextEditor (PersApps), Printer (PersPrinters),
   *  Applications and Help (HelpSubpanel) [SRC]; this is Help's.
   *  Element::wipe at 90 degrees is the reveal — and it covers the
   *  node's decorations, which is exactly what a bevelled panel needs
   *  and what trim()/scaleY could not have given. */
  Element helpSubpanel() {
    const Set& s = theme[2];
    Element col = box()
                      .fill(s.bg)
                      .overlay(cde::bevel(2, false, false))
                      .padding(4)
                      .column()
                      .alignItems(Align::Center)
                      .gap(4)
                      .child(cde::label("Help", s.fg))
                      .child(box()
                                 .row()
                                 .gap(6)
                                 .child(cde::art(cde::icoHelp(), 1.4f))
                                 .child(cde::art(cde::icoApps(), 1.4f)))
                      .child(box().height(Dim(6)).width(Dim(60)).overlay(
                          cde::bevel(2, true, true)));
    return col.mask(by::edge(270.0f, &subpanel));  // 270 = from the BOTTOM
  }

  // -------------------------------------------------------------------------

  /** dtwm iconifies a window ONTO THE ROOT WINDOW as a bevelled square
   *  with the window's title beneath it — an XmSHADOW_OUT box in the
   *  primary colour set, which is why iconified windows change colour
   *  with the theme along with everything else. */
  Element iconifiedWindow(std::string_view title,
                          const std::vector<std::string>& pixmap) {
    const Set& s = theme[2];
    return box()
        .column()
        .alignItems(Align::Center)
        .gap(2)
        .child(box()
                   .width(Dim(64))
                   .height(Dim(64))
                   .fill(s.bg)
                   .overlay(cde::bevel(2, false, false))
                   .alignItems(Align::Center)
                   .justify(Justify::Center)
                   .child(cde::art(pixmap, 2.0f)))
        .child(box()
                   .fill(s.bg)
                   .overlay(cde::bevel(1, false, false))
                   .padding(4, 1)
                   .child(cde::label(title, s.fg)));
  }

  Element describe(sketch::SketchContext& ctx) {
    Element root = stack().width(Dim(1152)).height(Dim(900));

    // 1. The root window: PinStripe, tiled, in colour set 3's shadows.
    //    ONE 28 x 52 pixmap that dtwm tiles — not instanced, not
    //    randomised per repeat; the lattice is part of how it looks.
    root.child(box()
                   .inset(0)
                   .fill(backdrops[(size_t)paletteIndex].material())
                   .cache(Cache::Texture));

    // 2. The File Manager.
    root.child(fileManager().left(Dim(40)).top(Dim(48)).width(Dim(600)).height(
        Dim(452)));

    // 3. The Style Manager's Color dialog.
    root.child(colorDialog().left(Dim(664)).top(Dim(96)).width(Dim(452)).height(
        Dim(356)));

    // 4. A posted (torn-off) menu.
    root.child(postedMenu().left(Dim(700)).top(Dim(506)));

    // 5. The derivation strip — the only smooth motion on the canvas.
    root.child(slot("derivation").left(Dim(40)).top(Dim(536)));

    // 6. The Help subpanel, wiping up out of the panel. The Help control
    //    sits at x = 912..968, so the subpanel is centred on it and its
    //    bottom edge meets the panel's top.
    root.child(
        helpSubpanel().left(Dim(865)).top(Dim(700)).width(Dim(150)).height(
            Dim(106)));

    // 6b. Iconified windows on the root, where dtwm parks them.
    root.child(box()
                   .left(Dim(48))
                   .top(Dim(690))
                   .row()
                   .gap(34)
                   .child(iconifiedWindow("Terminal", cde::icoEditor()))
                   .child(iconifiedWindow("Mailer", cde::icoMail()))
                   .child(iconifiedWindow("Print Manager", cde::icoPrinter())));

    // 7. The Front Panel, bottom-centred. 960 wide => 948 of content,
    //    which is exactly the measured control list.
    root.child(frontPanel().left(Dim(96)).top(Dim(806)).width(Dim(960)).height(
        Dim(86)));

    (void)ctx;
    return root;
  }

  // -------------------------------------------------------------------------

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1152, 900);
    ctx.background(cde::C(0x000000));
    // The still has to name its moment: palettes snap every 3 s over
    // {Default, Crimson, Black, Summer}, and an undeclared capture can land
    // on a snap or on Black, the all-black degenerate palette. Default —
    // the shipped canonical, loaded at setup — holds [12, 15); 13.5 s is
    // dead centre, with the derivation strip visibly mid-sweep.
    ctx.captureAt(13.5);

    theme.load(*cde::kPalettes[0]);

    if (!backdropsBuilt) {
      for (size_t i = 0; i < cde::kPalettes.size(); ++i) {
        const cde::Derived d = cde::calculate(cde::kPalettes[i]->set[2]);
        // NEAREST, and it is the whole reason the backdrop reads. This
        // is a 1993 PIXMAP: two colours, one pixel each, at full contrast
        // between the set's top and bottom shadows — the widest pair the
        // derivation produces. Sampled linearly the 1 px dither smears
        // into a flat mid-tone and the lattice disappears, which is the
        // opposite of what a dither is for.
        backdrops[i] =
            Pattern::tile({28, 52},
                          cde::pinStripeTile(cde::toSk(d.ts), cde::toSk(d.bs)))
                .sampling(SkSamplingOptions(SkFilterMode::kNearest));
      }
      backdropsBuilt = true;
    }

    // The clock: 60x, so a minute passes every second and the hand
    // visibly steps.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      clockT = (float)std::fmod(t / 60.0 + 0.42, 1.0);
      busy = (std::fmod(t, 0.5) < 0.25) ? 1.0f : 0.0f;
      caret = (std::fmod(t, 1.0) < 0.5) ? 1.0f : 0.0f;
      // The subpanel posts and unposts on a 12 s cycle, in step with the
      // palette loop.
      const double c = std::fmod(t, 12.0);
      const double up =
          c < 1.0 ? c
                  : (c < 10.5 ? 1.0 : std::max(0.0, 1.0 - (c - 10.5) / 0.7));
      subpanel = (float)std::clamp(up, 0.0, 1.0);
      // The sweep: 0 -> 255 over 8 s, held 1 s at each end.
      const double u = std::fmod(t, 10.0);
      sweep = (float)(u < 1.0 ? 0.0 : (u < 9.0 ? (u - 1.0) / 8.0 : 1.0));
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("derivation", derivationStrip());
  }

  int lastSweepByte = -1;

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // Beat one: the palette cycles every 3 s. SNAP, do not crossfade —
    // CDE's colour server re-allocates the cells and every window
    // repaints on the next expose; a 300 ms lerp across a desktop would
    // be the single most anachronistic thing you could add.
    if (elapsed >= nextSwitch) {
      nextSwitch = std::floor(elapsed / 3.0) * 3.0 + 3.0;
      paletteIndex =
          ((int)std::floor(elapsed / 3.0)) % (int)cde::kPalettes.size();
      theme.load(*cde::kPalettes[(size_t)paletteIndex]);
      // A palette change is a re-describe, which is what the inherited
      // channel makes it: the reconciler patches the nodes whose props
      // moved and leaves the rest, where a bound colour would have made
      // every node on the desktop volatile for ever to save this.
      ctx.composer.render(describe(ctx));
    }

    const int b = std::clamp((int)std::lround(sweep.value() * 255.0f), 0, 255);
    if (b != lastSweepByte) {
      lastSweepByte = b;
      ctx.composer.renderSlot("derivation", derivationStrip());
    }
  }
};

SIGIL_SKETCH(
    CdeMotifSketch, "Study \xc2\xb7 Screens",
    "CDE 1.0 on OSF/Motif 2.1 (1995) \xe2\x80\x94 XmGetColors reproduced "
    "byte-exact")

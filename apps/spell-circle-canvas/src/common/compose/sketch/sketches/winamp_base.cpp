// winamp_base.cpp — Winamp 2.91, Nullsoft's default "Base" (Classic) skin:
// the docked Main + Equalizer + Playlist trio on a Windows-98 teal desktop.
// Design/programming: Justin Frankel / Nullsoft, shipped 2003.
//
// SCALE = 3. Every number below is in NATIVE skin pixels and goes through
// n() — divide any canvas number by 3 to recover the real 2003 asset pixel.
// The Main and EQ windows are the native 275x116 unit; the Playlist is
// resized to 400x377 (+5 width segments of 25, +9 height segments of 29 —
// an ordinary real-world resize). Canvas 1320x1947 = 1200x1827 of content
// plus a flat 60 px desktop margin on all four sides.
//
// REFERENCE (fetched, unzipped and pixel-sampled — not remembered):
//   · base-2.91.wsz, the literal shipped skin package, mirrored MIT at
//     raw.githubusercontent.com/captbaritone/webamp/master/packages/webamp/
//     assets/skins/base-2.91.wsz — MAIN.BMP, TITLEBAR.BMP, CBUTTONS.BMP,
//     EQMAIN.BMP, PLEDIT.BMP, SHUFREP.BMP, VOLUME.BMP, BALANCE.BMP,
//     NUMBERS.BMP, TEXT.BMP, plus VISCOLOR.TXT and PLEDIT.TXT (both quoted
//     verbatim below) and a REGION.TXT that is entirely commented out —
//     Base uses no window-shape mask, every window is a plain rectangle.
//   · archive.org/details/base-2.91 ("Winamp Classic: Justin Frankel;
//     Nullsoft"); skins.webamp.org/skin/5e4f10275dcb1fb211d4a8b4f1bda236.
//   · captbaritone/webamp (MIT) read directly: js/constants.ts gives
//     WINDOW_WIDTH=275, WINDOW_HEIGHT=116, TRACK_HEIGHT=13,
//     WINDOW_RESIZE_SEGMENT_WIDTH=25, _HEIGHT=29 and the real band set
//     [60,170,310,600,1000,3000,6000,12000,14000,16000]; js/skinSprites.ts
//     gives the crop rects (TEXT.BMP's character cell is a fixed 5x6 px,
//     MAIN_VOLUME_BACKGROUND is 68x420 = 28 stacked 15 px frames);
//     css/{main,equalizer,playlist}-window.css is the fixed pixel position
//     of every control — classic skins reskin bitmaps only and cannot move
//     a button, so that stylesheet IS the original SDK's layout.
//   · MainVolume.tsx: `const sprite = Math.round(percent * 28);` — the
//     28-frame quantised volume slider, implemented here as quantisation.
//   · eeggs.com + TITLEBAR.BMP itself: "IT REALLY WHIPS THE LLAMA'S ASS!"
//     is a real baked title-bar state, not a joke I invented.
//
// THIS IS A SKIN, so the original is bitmaps. Everything here is generated:
// the brushed body is Material::linearUnit + patterns::grain(stretch),
// the bevels are shapes::onEdges/shapes::inset stroke pairs, the LEDs are
// an instancing Atlas+Pool, the fader tracks are one shared 3-stop
// Material, the title-bar grip is a rotated patterns::stripes tile.
//
// FIVE THINGS THE RENDER TAUGHT THAT THE MEASUREMENTS COULD NOT:
//  · patterns::grain(freq=6.0) is unusable at any scale — freq is
//    features-per-PIXEL, so 6.0 with stretch=6 asks for 36 cycles/px in y
//    and returns aliasing hash. Brushed aluminium at x3 wants
//    freq≈0.075 with stretch=5: uFreq = (0.015, 0.375), i.e. features ~65
//    px long and ~3 px apart. The elongation is what reads as brushed; the
//    frequency only decides whether you can see it at all, and `contrast`
//    is the real volume knob (0.26 here — 0.35 was already "textured
//    plastic" rather than "metal you have to look for").
//  · the bevel really does do half the work. With the grain and without
//    the 1 px highlight/shadow pair the panels read as flat lilac card.
//  · nothing in this UI may have a corner radius — one .corners() left in
//    by habit and the whole argument collapses into 2005 Aqua.
//  · the LCD sells itself with the UNLIT ghost: "88:88" in VISCOLOR's
//    documented off-segment grey under the live digits is what turns a
//    green number into a segment display. But the sampled screen colour
//    (#181829) and the sampled unlit grey (#182129) are FOUR levels apart,
//    so on the literal pair the ghost is invisible. The screen here is
//    #131320 — the one palette value in this file that is not the sampled
//    one, and it is flagged rather than quietly corrected.
//  · a fixed-cell bitmap font has to be sized from a MEASURED advance, not
//    an assumed one. `pix(cellN)` derives its point size from the
//    substituted face's real advance/em, probed once with ctx.measure() in
//    setup(); guessing "Menlo is 0.602 em" (it is 0.603) is the kind of
//    error that only shows up as digits overflowing a 9 px cell.

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Pattern.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <sigilweave/Paragraph.h>
#include <sigilweave/ParagraphLayout.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace wa {

// ---------------------------------------------------------------------------
// Scale. Native skin px -> canvas px.

constexpr float kScale = 3.0f;
constexpr float n(float v) { return v * kScale; }

constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f, a};
}
inline SkColor4f lift(SkColor4f c, float k) {
  return {std::min(1.0f, c.fR + k), std::min(1.0f, c.fG + k),
          std::min(1.0f, c.fB + k), c.fA};
}
inline SkColor4f dark(SkColor4f c, float k) {
  return {c.fR * (1 - k), c.fG * (1 - k), c.fB * (1 - k), c.fA};
}
inline SkColor4f fade(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, a}; }

// ---------------------------------------------------------------------------
// Palette — sampled from the extracted BMPs, or quoted from the skin's own
// plain-text config files. Nothing here is remembered.

constexpr SkColor4f kBody = C(0x343453);     // MAIN.BMP body base
constexpr SkColor4f kBodyTop = C(0x3B3B5A);  // the unit ramp, top-lit
constexpr SkColor4f kBodyBot = C(0x2E2E48);  // ... to shadowed
constexpr SkColor4f kLcd = C(0x131320);      // LCD screen (sampled
                                             // #181829, dropped four levels so
                                             // the #182129 unlit ghost reads)
constexpr SkColor4f kBezel = C(0x161622);    // LCD bezel outer ring
constexpr SkColor4f kUnlit = C(0x182129);    // VISCOLOR color 1, "grey for
                                             // dots" — byte-identical to the
                                             // sampled NUMBERS.BMP background
constexpr SkColor4f kGreen = C(0x00F800);    // NUMBERS.BMP digit green
constexpr SkColor4f kTitle = C(0x25253A);    // TITLEBAR.BMP base
constexpr SkColor4f kGold = C(0xA99865);     // wordmark, focused
constexpr SkColor4f kGoldDim = C(0x7A7A94);  // wordmark, unfocused
constexpr SkColor4f kBtnHi = C(0xEFFFFF);    // CBUTTONS bevel highlight
constexpr SkColor4f kBtnFace = C(0x97A8B9);  // CBUTTONS steel-blue face
constexpr SkColor4f kBtnLo = C(0x4A5A6B);    // CBUTTONS bevel shadow
constexpr SkColor4f kGlyph = C(0x1E2833);    // the ink on a transport key
constexpr SkColor4f kGraph = C(0x1B1A2C);    // EQMAIN graph screen navy
constexpr SkColor4f kGrid = C(0x3A3A55);     // EQMAIN dashed gridline
constexpr SkColor4f kEqTop = C(0x2A9A16);    // fader track, green
constexpr SkColor4f kEqMid = C(0xA6C731);    // ... yellow-gold
constexpr SkColor4f kEqBot = C(0xC5431B);    // ... red
constexpr SkColor4f kPlBg = C(0x000000);     // PLEDIT.TXT NormalBG
constexpr SkColor4f kPlText = C(0x00FF00);   // PLEDIT.TXT Normal
constexpr SkColor4f kPlNow = C(0xFFFFFF);    // PLEDIT.TXT Current
constexpr SkColor4f kPlSel = C(0x0000C6);    // PLEDIT.TXT SelectedBG
constexpr SkColor4f kDesk = C(0x008080);     // Windows 9x/2000 default teal
constexpr SkColor4f kPeak = C(0x969696);     // VISCOLOR 23, peak-hold dots

/** VISCOLOR.TXT colors 2..17, bottom of spectrum -> top, quoted verbatim.
 *  The same family the EQ fader tracks are sampled from. */
constexpr std::array<SkColor4f, 16> kVis = {
    C(0x188408), C(0x299400), C(0x319C08), C(0x39B510),
    C(0x32BE10), C(0x29CE10), C(0x94DE21), C(0xBDDE29),
    C(0xD6B521), C(0xDEA518), C(0xC67B08), C(0xD67300),
    C(0xD66600), C(0xD65A00), C(0xCE2910), C(0xEF3110)};

// ---------------------------------------------------------------------------
// Type. Almost nothing in classic Winamp is live text: TEXT.BMP is a fixed
// 5x6 px character cell and NUMBERS.BMP a 9x13 one, both baked pixel art.
// The ONE place with a real proportional font is the playlist (PLEDIT.TXT
// says Font=Arial, the CSS says 9px/0.5px tracking) — so that contrast is
// the thing to preserve, and everything else is a deliberate monospace
// approximation at the real cell size.

inline sk_sp<SkTypeface> face(const char *family, int weight,
                              const char *fallback = nullptr) {
  auto mgr = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      family, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                          SkFontStyle::kUpright_Slant));
  if (!f && fallback)
    f = mgr->matchFamilyStyle(
        fallback, SkFontStyle(weight, SkFontStyle::kNormal_Width,
                              SkFontStyle::kUpright_Slant));
  if (!f)
    f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Normal());
  return f;
}
inline const sk_sp<SkTypeface> &mono() {
  static sk_sp<SkTypeface> f =
      face("Menlo", SkFontStyle::kNormal_Weight, "Monaco");
  return f;
}
inline const sk_sp<SkTypeface> &monoBold() {
  static sk_sp<SkTypeface> f =
      face("Menlo", SkFontStyle::kBold_Weight, "Monaco");
  return f;
}
inline const sk_sp<SkTypeface> &arial() {
  static sk_sp<SkTypeface> f =
      face("Arial", SkFontStyle::kNormal_Weight, "Helvetica");
  return f;
}

inline sigil::weave::TextStyle type(const sk_sp<SkTypeface> &tf, float size,
                                    SkColor4f color, float track = 0,
                                    float condense = 1.0f) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = tf;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = track;
  s.shaping.scaleX = condense;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** The measured advance/em of the substituted monospace faces. Probed once
 *  with compose::measure() in setup() rather than assumed — the whole point
 *  of a fixed-cell bitmap font is that the cell is exact, and "Menlo is
 *  0.602 em" was wrong enough to make the LCD digits overflow their well. */
inline float &monoEm() {
  static float v = 0.602f;
  return v;
}
inline float &boldEm() {
  static float v = 0.602f;
  return v;
}

/** TEXT.BMP's 5x6 native cell, approximated: a monospace sized so its
 *  advance plus tracking lands on exactly `cellN` NATIVE px. Uppercase only
 *  — the real font has no lowercase glyphs at all. */
inline sigil::weave::TextStyle pix(float cellN, SkColor4f c,
                                   bool bold = false, float trackN = 0.0f) {
  const float em = bold ? boldEm() : monoEm();
  return type(bold ? monoBold() : mono(), n(cellN - trackN) / em, c,
              n(trackN), 1.0f);
}
inline Element t(const char *s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}
inline Element t(const std::string &s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

// ---------------------------------------------------------------------------
// Geometry vocabulary. ZERO corner radius anywhere — that is the whole
// visual argument of a 2003 hardware skeuomorph against a 2005 gel button.

/** Absolute placement in NATIVE skin coordinates, local to the window. */
inline Element at(Element e, float x, float y, float w, float h) {
  e.absolute()
      .left(Dim(n(x)))
      .top(Dim(n(y)))
      .width(Dim(n(w)))
      .height(Dim(n(h)));
  return e;
}

/** The raised bevel: 1 native px light top/left, 1 native px dark
 *  bottom/right, inset. Twenty-odd controls across three windows wear
 *  exactly this, which is the point — if the idiom generalises it is a
 *  primitive, not a one-skin hack. */
inline Element &raised(Element &e, SkColor4f hi = kBtnHi,
                       SkColor4f lo = kBtnLo, float w = 1.0f) {
  e.foreground(shapes::onEdges(
      shapes::Edge::Top | shapes::Edge::Left,
      util::stroke(n(w), Fill::color(hi), PathFormat::Align::Inner)));
  e.foreground(shapes::onEdges(
      shapes::Edge::Bottom | shapes::Edge::Right,
      util::stroke(n(w), Fill::color(lo), PathFormat::Align::Inner)));
  return e;
}
/** The sunken bevel — the same pair with the light swapped to the far
 *  edges. Every LCD well, trough and list frame in the skin. */
inline Element &sunken(Element &e, SkColor4f hi = fade(C(0x5C5C86), 0.9f),
                       SkColor4f lo = C(0x101018), float w = 1.0f) {
  e.foreground(shapes::onEdges(
      shapes::Edge::Top | shapes::Edge::Left,
      util::stroke(n(w), Fill::color(lo), PathFormat::Align::Inner)));
  e.foreground(shapes::onEdges(
      shapes::Edge::Bottom | shapes::Edge::Right,
      util::stroke(n(w), Fill::color(hi), PathFormat::Align::Inner)));
  return e;
}

/** Right/left/up-pointing triangles for the transport glyphs, as outlines
 *  so the node IS the shape. */
inline std::function<SkPath(SkSize)> tri(int dir) { // 0 right 1 left 2 up
  return [dir](SkSize s) {
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
  };
}

/** The scroll-arrow triangles: up or down. */
inline std::function<SkPath(SkSize)> upDown(bool up) {
  return [up](SkSize s) {
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
  };
}

/** The Nullsoft lightning bolt baked into MAIN.BMP's bottom-right corner —
 *  the about/easter-egg hitzone at native 253,91,13,15. */
inline std::function<SkPath(SkSize)> bolt() {
  return [](SkSize s) {
    const float w = s.width(), h = s.height();
    static const float p[7][2] = {{0.62f, 0.00f}, {0.05f, 0.56f},
                                  {0.40f, 0.56f}, {0.24f, 1.00f},
                                  {0.95f, 0.40f}, {0.55f, 0.40f},
                                  {0.92f, 0.00f}};
    SkPathBuilder b;
    b.moveTo(p[0][0] * w, p[0][1] * h);
    for (int i = 1; i < 7; ++i)
      b.lineTo(p[i][0] * w, p[i][1] * h);
    b.close();
    return b.detach();
  };
}

} // namespace wa

// ===========================================================================

struct WinampBase : sigil::compose::sketch::Sketch {
  using Out = ch::Output<float>;

  // ---- THE bound outputs. Every idle motion is declared; only discrete
  // state (the digits, the 28-frame sliders, the track list) re-describes.
  Out playPos{0.42f};   // [0,1] through the current track — drives the seek
                        // thumb in px, the elapsed underlay's scaleX and the
                        // MM:SS readout, from ONE source.
  Out marqueePhase{0};  // px, wraps
  Out volFrame{0};      // 0..27, a HARD integer — round(pct * 28)
  Out balFrame{0};
  std::array<Out, 11> gain{}; // preamp + 10 bands, [-1,1]; drives the fader
                              // thumbs AND the response graph
  Out graphDraw{0};
  Out llama{0}, llamaPop{1}; // the title-bar easter egg
  Out glint{0};              // clutter-bar specular sweep
  Out led{0};                // play-status LED
  std::array<Out, 25> rowIn{}; // playlist row reveal, in bands of four

  // ---- generated materials, held so their identity prunes ----
  Material steel, deskMat, lcdMat, faderTrack, graphMat;
  Pattern gripTile, visDots, graphGrid;

  // ---- instancing: the spectrum analyser LEDs and the playlist rows ----
  static constexpr int kCols = 19; // 19 bars x (3 px bar + 1 px gap) = 76
  static constexpr int kRows = 16; // VISCOLOR gives exactly 16 ramp stops
  std::shared_ptr<instancing::Atlas> ledAtlas;
  std::shared_ptr<instancing::Pool> ledPool;
  std::shared_ptr<instancing::Atlas> rowAtlas;
  std::shared_ptr<instancing::Pool> rowPool;
  std::array<float, kCols> colLevel{}, colPeak{};
  double lastRoll = -1.0;

  // ---- discrete state, the describe path ----
  int volSprite = -1, balSprite = -1;
  int shownSec = -1;
  int nowPlaying = 8, selected = 13;
  double elapsedNow = 0;

  // paragraph identities held so the playlist prunes across slot renders
  std::vector<std::shared_ptr<sigil::weave::Paragraph>> rowPara;

  float marqueeW = 1;

  // =========================================================================
  // Data — a plausible 2003 playlist. Track 1 is the real one Winamp
  // shipped with.

  struct Track {
    const char *title;
    const char *time;
    int seconds;
  };
  static const std::array<Track, 25> &tracks() {
    static const std::array<Track, 25> v = {{
        {"DJ Mike Llama - Llama Whippin' Intro", "0:05", 5},
        {"Nine Inch Nails - The Perfect Drug", "5:15", 315},
        {"The Prodigy - Breathe", "5:35", 335},
        {"Fatboy Slim - Right Here, Right Now", "6:27", 387},
        {"Massive Attack - Teardrop", "5:29", 329},
        {"Portishead - Glory Box", "5:07", 307},
        {"Underworld - Born Slippy .NUXX", "9:44", 584},
        {"The Chemical Brothers - Block Rockin' Beats", "5:15", 315},
        {"Daft Punk - Around The World", "7:09", 429},
        {"Aphex Twin - Windowlicker", "6:07", 367},
        {"Orbital - Halcyon On And On", "9:27", 567},
        {"Leftfield - Phat Planet", "5:33", 333},
        {"Air - Sexy Boy", "4:58", 298},
        {"Boards Of Canada - Roygbiv", "2:31", 151},
        {"Autechre - Gantz Graf", "5:07", 307},
        {"Squarepusher - My Red Hot Car", "4:36", 276},
        {"Photek - Ni Ten Ichi Ryu (Two Swords)", "6:03", 363},
        {"Roni Size / Reprazent - Brown Paper Bag", "5:23", 323},
        {"Goldie - Inner City Life", "7:49", 469},
        {"LTJ Bukem - Horizons", "8:44", 524},
        {"Amon Tobin - Bricolage", "4:23", 263},
        {"DJ Shadow - Midnight In A Perfect World", "4:57", 297},
        {"Mr. Oizo - Flat Beat", "3:47", 227},
        {"Basement Jaxx - Where's Your Head At", "5:37", 337},
        {"Groove Armada - At The River", "3:24", 204},
    }};
    return v;
  }

  /** The real stock "Rock" curve: boosted bass and treble, recessed mids.
   *  [0] is the preamp. */
  static const std::array<float, 11> &rockPreset() {
    static const std::array<float, 11> v = {0.18f,  0.62f,  0.44f, -0.35f,
                                            -0.55f, -0.15f, 0.30f, 0.60f,
                                            0.72f,  0.72f,  0.70f};
    return v;
  }

  static std::string mmss(int seconds) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%d:%02d", seconds / 60, seconds % 60);
    return buf;
  }
  /** The readout is four fixed 9x13 cells, so a one-digit minute leaves the
   *  tens cell dark rather than shifting the run. */
  static std::string mmssCells(int seconds) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%2d:%02d", (seconds / 60) % 100,
                  seconds % 60);
    return buf;
  }

  // =========================================================================
  // Materials

  void buildMaterials() {
    using namespace wa;
    // THE brushed body. One recipe, three windows, two sizes — the unit
    // square is what makes 275x116 and 400x377 share it. The anisotropic
    // grain is the whole "brushed, not painted" argument: stretch divides
    // the frequency in x and multiplies it in y, so the field becomes long
    // horizontal streaks rather than speckle.
    steel = Material::blend(
        {{Material::linearUnit({0, 0}, {0, 1},
                               {{0.0f, kBodyTop}, {1.0f, kBodyBot}}),
          SkBlendMode::kSrcOver},
         {patterns::grain(0.075f, 2, 7.0f, 0.26f, 5.0f),
          SkBlendMode::kOverlay}});

    // The desktop: flat teal plus ONE low-octave dither, baked once.
    deskMat = Material::blend({{Material::solid(kDesk), SkBlendMode::kSrcOver},
                               {patterns::grain(0.45f, 1, 3.0f, 0.055f, 1.0f),
                                SkBlendMode::kOverlay}});

    // CRT glass: the flat screen colour plus a soft off-centre catch-light.
    lcdMat = Material::blend(
        {{Material::solid(kLcd), SkBlendMode::kSrcOver},
         {Material::radialUnit({0.28f, 0.22f}, 1.25f,
                               {{0.0f, C(0x2A2A46, 0.75f)},
                                {1.0f, C(0x2A2A46, 0.0f)}}),
          SkBlendMode::kSrcOver}});

    // ONE fader-track value shared by all eleven faders (preamp + 10 bands).
    faderTrack = Material::linearUnit(
        {0, 0}, {0, 1},
        {{0.0f, kEqTop}, {0.46f, kEqMid}, {1.0f, kEqBot}});

    graphMat = Material::solid(kGraph);

    // The title-bar grip: horizontal hairlines, as a rotated stripe tile.
    gripTile = patterns::stripes(n(1), n(1), C(0x4C4C74)).rotate(90.0f);
    // The visualiser well's baked dot grid (MAIN.BMP paints these under the
    // bars, in VISCOLOR's own "grey for dots").
    visDots = patterns::halftone(n(2), n(0.5f), kUnlit, false);
    // The EQ graph's dashed rules.
    graphGrid = Pattern::tile({n(4), n(4)}, [](SkCanvas &c, SkSize, uint32_t) {
      SkPaint p;
      p.setColor4f(kGrid, nullptr);
      c.drawRect(SkRect::MakeWH(n(2), n(1)), p);
    });
  }

  // =========================================================================
  // Small parts

  /** A transport / menu key: the two-stroke bevel over a steel-blue face
   *  with a vertical ramp, zero radius, content centred. */
  Element key(float x, float y, float w, float h, Element glyph) {
    using namespace wa;
    Element e = at(box(), x, y, w, h);
    e.fill(Material::linearUnit({0, 0}, {0, 1},
                                {{0.0f, lift(kBtnFace, 0.10f)},
                                 {0.55f, kBtnFace},
                                 {1.0f, dark(kBtnFace, 0.22f)}}));
    raised(e);
    // The second keyline, one native px in — shapes::inset is literally
    // "the same bevel again, N px further in", which is Winamp's doubled
    // button edge without a second element.
    e.foreground(shapes::inset(
        n(1), shapes::onEdges(shapes::Edge::Bottom | shapes::Edge::Right,
                              util::stroke(n(1),
                                           Fill::color(fade(kBtnLo, 0.45f)),
                                           PathFormat::Align::Inner))));
    e.child(glyph);
    return e;
  }

  /** A glyph part inside a key, in native px local to the key. */
  static Element part(float x, float y, float w, float h,
                      std::function<SkPath(SkSize)> shape = {}) {
    using namespace wa;
    Element e = at(box(), x, y, w, h).fill(kGlyph);
    if (shape)
      e.outline(std::move(shape));
    return e;
  }

  /** A tiny beveled text button — ON / AUTO / PRESETS / ADD / REM / … */
  Element textKey(float x, float y, float w, float h, const char *label,
                  float cell = 4.0f) {
    using namespace wa;
    Element e = key(x, y, w, h, box());
    e.justify(Justify::Center).alignItems(Align::Center);
    e.child(t(label, pix(cell, C(0x121A24))));
    return e;
  }

  // ---- title bar ----------------------------------------------------------

  /** The title bar. The wordmark is baked pixel-art logotype in the real
   *  TITLEBAR.BMP; this is a deliberate tight-monospace approximation in
   *  the sampled gold, cross-faded with the real "IT REALLY WHIPS THE
   *  LLAMA'S ASS!" state that the same bitmap carries. */
  Element titleBar(float wN, const char *label, bool wide,
                   bool hasMin = true, float hN = 14.0f) {
    using namespace wa;
    Element bar = at(box(), 0, 0, wN, hN);
    bar.fill(Material::linearUnit({0, 0}, {0, 1},
                                  {{0.0f, lift(kTitle, 0.06f)},
                                   {1.0f, dark(kTitle, 0.25f)}}));
    raised(bar, fade(C(0x5A5A82), 0.85f), C(0x101018));

    // grip hairlines either side of the wordmark
    const float gripW = wide ? 100.0f : 52.0f;
    const float gy = (hN - 7.0f) * 0.5f;
    bar.child(at(box(), 24, gy, gripW, 7).fill(gripTile.material()));
    bar.child(at(box(), wN - 24 - gripW, gy, gripW, 7).fill(gripTile.material()));

    // the wordmark, and the easter egg crossfaded over it
    Element mark = at(box(), 0, (hN - 8) * 0.5f, wN, 8)
                       .justify(Justify::Center)
                       .alignItems(Align::Center);
    mark.child(t(label, pix(6.6f, kGold, true, 1.7f))
                   .opacity(bind(&llama).invert()));
    bar.child(mark);
    Element egg = at(box(), 0, (hN - 8) * 0.5f, wN, 8)
                      .justify(Justify::Center)
                      .alignItems(Align::Center);
    egg.child(t("IT REALLY WHIPS THE LLAMA'S ASS!",
                pix(5.2f, kGold, true, 0.7f))
                  .opacity(&llama)
                  .scale(&llamaPop));
    bar.child(egg);

    // window buttons, native 9x9, right-aligned exactly as the SDK pins them
    auto wbtn = [&](float x, const char *g) {
      Element b = at(box(), x, (hN - 9) * 0.5f, 9, 9)
                      .fill(dark(kTitle, 0.35f))
                      .justify(Justify::Center)
                      .alignItems(Align::Center);
      raised(b, fade(C(0x5A5A82), 0.8f), C(0x0E0E16));
      b.child(t(g, pix(3.6f, kGold)));
      return b;
    };
    if (!wide)
      bar.child(wbtn(6, "-")); // the option/context menu, native 6,3,9,9
    if (hasMin)
      bar.child(wbtn(wN - 31, "_"));
    bar.child(wbtn(wN - 21, "="));
    bar.child(wbtn(wN - 11, "x"));
    return bar;
  }

  // ---- main window --------------------------------------------------------

  Element mainWindow() {
    using namespace wa;
    Element w = box().width(Dim(n(275))).height(Dim(n(116)));
    // The brushed body, on its own leaf so the bake is a texture and the
    // window's live children never drag the grain shader back per frame.
    w.child(box().absolute().inset(0).fill(steel).cache(Cache::Texture));
    raised(w, fade(C(0x585880), 0.7f), C(0x0E0E18));
    w.child(titleBar(275, "WINAMP", false));

    // ---- the big display well (native 0,22 .. 275,57) -------------------
    Element well = at(box(), 0, 21, 275, 37).fill(lcdMat);
    sunken(well);
    w.child(well);

    // clutter bar O A I D V — its own dark strip, running past the well
    Element clutter = at(box(), 10, 22, 8, 43).fill(C(0x101020));
    sunken(clutter, fade(C(0x4A4A70), 0.7f), C(0x080810));
    static const char *cl[5] = {"O", "A", "I", "D", "V"};
    static const float cy[5] = {3, 11, 18, 25, 33};
    static const float cht[5] = {8, 7, 7, 8, 7};
    for (int i = 0; i < 5; ++i)
      clutter.child(at(box(), 0, cy[i], 8, cht[i])
                        .justify(Justify::Center)
                        .alignItems(Align::Center)
                        .child(t(cl[i], pix(3.4f, C(0x8E8EB4)))));
    // the specular glint that sweeps the stack once every 5 s
    clutter.child(at(box(), 0, 0, 8, 6)
                      .fill(C(0xCFE4FF, 0.55f))
                      .blend(SkBlendMode::kPlus)
                      .translateY(bind(&glint).to(-n(6), n(43)))
                      .opacity(bind(&glint)
                                   .offset(-0.5f)
                                   .scale(2.0f)
                                   .invert()
                                   .clamp(0.0f, 0.75f)));
    w.child(clutter);

    // play-status LED (native 26,28,9,9)
    Element status = at(box(), 26, 28, 9, 9);
    status.child(at(box(), 0, 1, 3, 7).fill(kGreen).opacity(&led));
    status.child(at(box(), 4, 2, 5, 5).fill(kGreen).opacity(&led)
                     .outline(tri(0)));
    w.child(status);

    // MM:SS — the four NUMBERS.BMP cells sit at native x 48/60 and 78/90,
    // 9x13 each. The ghost "88:88" behind them, in VISCOLOR's documented
    // off-segment grey, is what turns green numerals into a display.
    Element clock = at(box(), 45, 26, 54, 13);
    clock.child(at(box(), 0, 0, 54, 13)
                    .justify(Justify::Center)
                    .alignItems(Align::Center)
                    .child(t("88:88", pix(10, kUnlit))));
    clock.child(box().absolute().inset(0).child(slot("time")));
    w.child(clock);

    // the scrolling track title (TEXT.BMP, 5x6 cell) in its own sunken box
    Element titleWell = at(box(), 109, 22, 158, 11).fill(C(0x101020));
    sunken(titleWell, fade(C(0x4A4A70), 0.5f), C(0x08080E));
    Element title = at(box(), 2, 2, 154, 7).clip();
    title.child(util::marquee(
        t(marqueeText(), pix(5, C(0x00E000))), marqueeW, &marqueePhase, n(40)));
    titleWell.child(title);
    w.child(titleWell);

    // kbps / kHz readouts — each a small bordered window with its unit
    // printed outside it, exactly as MAIN.BMP bakes them.
    auto readout = [&](float x, float wN, const char *v) {
      Element e = at(box(), x, 41, wN, 9).fill(C(0x101020));
      sunken(e, fade(C(0x4A4A70), 0.5f), C(0x08080E));
      e.child(at(box(), 1, 2, wN - 2, 6)
                  .justify(Justify::End)
                  .alignItems(Align::Center)
                  .child(t(v, pix(4.6f, C(0x00E000)))));
      return e;
    };
    w.child(readout(111, 17, "192"));
    w.child(at(box(), 130, 43, 20, 6)
                .alignItems(Align::Center)
                .child(t("kbps", pix(4, C(0x6E6E9A)))));
    w.child(readout(154, 13, "44"));
    w.child(at(box(), 169, 43, 18, 6)
                .alignItems(Align::Center)
                .child(t("kHz", pix(4, C(0x6E6E9A)))));
    w.child(at(box(), 212, 41, 28, 12)
                .justify(Justify::Center)
                .alignItems(Align::Center)
                .child(t("MONO", pix(4.4f, C(0x3A3A5C)))));
    w.child(at(box(), 240, 41, 29, 12)
                .justify(Justify::Center)
                .alignItems(Align::Center)
                .child(t("STEREO", pix(4.0f, C(0x00E000)))));

    // ---- the spectrum analyser well (native 24,43,76,16) ---------------
    Element vis = at(box(), 24, 43, 76, 16).fill(C(0x000000));
    sunken(vis, fade(C(0x4A4A70), 0.6f), C(0x08080E));
    vis.child(box().absolute().inset(0).fill(visDots.material()));
    // ONE atlas stamp for 19x16 LED segments plus 19 peak-hold dots.
    vis.child(box().absolute().inset(0).child(
        instancing::instances(ledAtlas, ledPool, instancing::Mode::Live,
                              SkBlendMode::kPlus)));
    w.child(vis);

    // The power-on tic: two 60 ms blinks before the display settles lit.
    // A shutter over the whole well, keyframed with easeNone so each step
    // is a hard cut — old displays do not fade in.
    w.child(at(box(), 0, 21, 275, 37)
                .fill(C(0x090911))
                .opacity(withKeyframes<float>({{0ms, 1.0f},
                                               {300ms, 1.0f},
                                               {310ms, 0.0f},
                                               {360ms, 0.0f},
                                               {370ms, 1.0f},
                                               {420ms, 1.0f},
                                               {430ms, 0.0f}},
                                              &ch::easeNone)));

    // ---- volume / balance / EQ+PL toggles -------------------------------
    w.child(box().absolute()
                .left(Dim(n(107)))
                .top(Dim(n(57)))
                .width(Dim(n(108)))
                .height(Dim(n(13)))
                .child(slot("sliders")));

    w.child(eqPlToggle());

    // ---- position / seek bar (native 16,72,248,10) ----------------------
    Element pos = at(box(), 16, 72, 248, 10).fill(C(0x14141F));
    sunken(pos, fade(C(0x4A4A70), 0.7f), C(0x08080E));
    // ONE Output, three consumers: the elapsed underlay's scaleX …
    pos.child(at(box(), 1, 1, 246, 8)
                  .fill(C(0x24243A))
                  .transformOrigin(0, 0.5f)
                  .scaleX(&playPos));
    // … and the thumb, in pixels.
    Element thumb = at(box(), 1, 0, 29, 10)
                        .fill(Material::linearUnit(
                            {0, 0}, {0, 1},
                            {{0.0f, lift(kBtnFace, 0.12f)},
                             {1.0f, dark(kBtnFace, 0.28f)}}))
                        .translateX(bind(&playPos).to(0, n(248 - 31)));
    raised(thumb);
    thumb.child(at(box(), 13, 2, 1, 6).fill(fade(kBtnLo, 0.8f)));
    thumb.child(at(box(), 15, 2, 1, 6).fill(fade(kBtnHi, 0.7f)));
    pos.child(thumb);
    w.child(pos);

    // ---- the six transport keys + shuffle / repeat ----------------------
    w.child(transportRow());

    // the baked Nullsoft bolt, bottom right
    w.child(at(box(), 253, 91, 13, 15)
                .outline(bolt())
                .fill(Material::linearUnit({0, 0}, {0, 1},
                                           {{0.0f, C(0xFFD24A)},
                                            {1.0f, C(0xC05C08)}})));
    return w;
  }

  Element eqPlToggle() {
    using namespace wa;
    Element g = at(box(), 219, 58, 46, 12);
    auto tog = [&](float x, const char *lbl, bool on) {
      Element e = key(x, 0, 23, 12, box());
      e.row().alignItems(Align::Center).padding(n(2), 0, 0, 0);
      e.child(box().width(Dim(n(3))).height(Dim(n(3)))
                  .fill(on ? wa::kGreen : C(0x3C4A58)));
      e.child(box().width(Dim(n(1.5f))));
      e.child(t(lbl, pix(4.2f, C(0x121A24))));
      return e;
    };
    g.child(tog(0, "EQ", true));
    g.child(tog(23, "PL", true));
    return g;
  }

  Element transportRow() {
    using namespace wa;
    Element r = at(box(), 0, 88, 275, 28);

    // prev |<<
    Element prev = box();
    prev.child(part(4, 5, 2, 8));
    prev.child(part(7, 5, 6, 8, tri(1)));
    prev.child(part(13, 5, 6, 8, tri(1)));
    r.child(key(16, 0, 23, 18, prev));
    // play >
    Element play = box();
    play.child(part(8, 4, 8, 10, tri(0)));
    r.child(key(39, 0, 23, 18, play));
    // pause ||
    Element pause = box();
    pause.child(part(8, 5, 3, 8));
    pause.child(part(13, 5, 3, 8));
    r.child(key(62, 0, 23, 18, pause));
    // stop []
    Element stop = box();
    stop.child(part(8, 5, 8, 8));
    r.child(key(85, 0, 23, 18, stop));
    // next >>|
    Element next = box();
    next.child(part(4, 5, 6, 8, tri(0)));
    next.child(part(10, 5, 6, 8, tri(0)));
    next.child(part(17, 5, 2, 8));
    r.child(key(108, 0, 22, 18, next));
    // eject
    Element eject = box();
    eject.child(part(7, 3, 8, 5, tri(2)));
    eject.child(part(7, 9, 8, 3));
    r.child(key(136, 1, 22, 16, eject));

    // SHUFFLE (47x15) and REP (28x15) — SHUFREP.BMP; the real CSS lets
    // these two sum to 75 native inside a 74-wide container, a 1 px
    // rounding slop in the ORIGINAL layout, kept.
    Element shuf = key(164, 1, 47, 15, box());
    shuf.row().alignItems(Align::Center).padding(n(3), 0, 0, 0);
    shuf.child(box().width(Dim(n(3))).height(Dim(n(3))).fill(C(0x3C4A58)));
    shuf.child(box().width(Dim(n(2))));
    shuf.child(t("SHUFFLE", pix(4.6f, C(0x121A24))));
    r.child(shuf);

    Element rep = key(211, 1, 28, 15, box());
    rep.row().alignItems(Align::Center).padding(n(3), 0, 0, 0);
    rep.child(box().width(Dim(n(3))).height(Dim(n(3))).fill(wa::kGreen));
    rep.child(box().width(Dim(n(2))));
    rep.child(t("REP", pix(4.6f, C(0x121A24))));
    r.child(rep);

    // the "hardware self-test" light sweep — a single 150 ms pass over the
    // six keys with NO easing, entirely as mount keyframes.
    r.child(at(box(), 0, 0, 8, 18)
                .fill(C(0xE8F4FF, 0.55f))
                .blend(SkBlendMode::kPlus)
                .translateX(withKeyframes<float>(
                    {{600ms, n(10)}, {750ms, n(162)}}, &ch::easeNone))
                .opacity(withKeyframes<float>({{590ms, 0.0f},
                                               {600ms, 1.0f},
                                               {735ms, 1.0f},
                                               {750ms, 0.0f}},
                                              &ch::easeNone)));
    return r;
  }

  // ---- the 28-frame sliders (the describe path — see the report) ---------

  /** VOLUME.BMP is 28 stacked 15 px frames and the player picks one with
   *  `round(percent * 28)`: the bar's COLOUR is the level. So this is a
   *  discrete frame swap, re-described when the frame index changes —
   *  never a lerped fill. */
  Element sliders(int vol, int bal) {
    using namespace wa;
    Element g = box();

    const SkColor4f volColor =
        kVis[(size_t)std::clamp((vol * 15) / 27, 0, 15)];
    Element track = at(box(), 0, 0, 68, 13).fill(C(0x1B1B2C));
    sunken(track, fade(C(0x4A4A70), 0.6f), C(0x0A0A12));
    track.child(at(box(), 1, 3, 66, 3).fill(dark(volColor, 0.45f)));
    track.child(at(box(), 1, 6, 66, 3).fill(volColor));
    track.child(at(box(), 1, 9, 66, 2).fill(dark(volColor, 0.65f)));
    Element vt = at(box(), 0, 1, 14, 11)
                     .fill(Material::linearUnit(
                         {0, 0}, {0, 1},
                         {{0.0f, lift(kBtnFace, 0.12f)},
                          {1.0f, dark(kBtnFace, 0.30f)}}))
                     .translateX(n((68.0f - 14.0f) * (float)vol / 27.0f));
    raised(vt);
    vt.child(at(box(), 6, 2, 1, 7).fill(fade(kBtnLo, 0.85f)));
    track.child(vt);
    g.child(track);

    // BALANCE.BMP, same 28-frame mechanism, centre-detented.
    const int b = std::abs(bal - 14);
    const SkColor4f balColor = kVis[(size_t)std::clamp((b * 15) / 14, 0, 15)];
    Element btr = at(box(), 70, 0, 38, 13).fill(C(0x1B1B2C));
    sunken(btr, fade(C(0x4A4A70), 0.6f), C(0x0A0A12));
    btr.child(at(box(), 1, 4, 36, 5).fill(dark(balColor, 0.55f)));
    btr.child(at(box(), 18, 1, 2, 11).fill(fade(balColor, 0.9f)));
    Element bt = at(box(), 0, 1, 14, 11)
                     .fill(Material::linearUnit(
                         {0, 0}, {0, 1},
                         {{0.0f, lift(kBtnFace, 0.12f)},
                          {1.0f, dark(kBtnFace, 0.30f)}}))
                     .translateX(n((38.0f - 14.0f) * (float)bal / 27.0f));
    raised(bt);
    bt.child(at(box(), 6, 2, 1, 7).fill(fade(kBtnLo, 0.85f)));
    btr.child(bt);
    g.child(btr);
    return g;
  }

  std::string marqueeText() {
    const Track &tr = tracks()[(size_t)nowPlaying];
    std::string s = std::to_string(nowPlaying + 1);
    s += ". ";
    s += tr.title;
    s += "  ***  ";
    for (char &c : s)
      c = (char)std::toupper((unsigned char)c); // TEXT.BMP has no lowercase
    return s;
  }

  Element timeReadout(int seconds) {
    using namespace wa;
    return box()
        .justify(Justify::Center)
        .alignItems(Align::Center)
        .child(t(mmssCells(seconds), wa::pix(10, wa::kGreen)));
  }

  // ---- equalizer window ---------------------------------------------------

  Element eqWindow() {
    using namespace wa;
    Element w = box().width(Dim(n(275))).height(Dim(n(116)));
    w.child(box().absolute().inset(0).fill(steel).cache(Cache::Texture));
    raised(w, fade(C(0x585880), 0.7f), C(0x0E0E18));
    w.child(titleBar(275, "WINAMP EQUALIZER", false, false));

    // ON / AUTO / PRESETS
    Element on = key(14, 18, 26, 12, box());
    on.row().alignItems(Align::Center).padding(n(2), 0, 0, 0);
    on.child(box().width(Dim(n(3))).height(Dim(n(3))).fill(wa::kGreen));
    on.child(box().width(Dim(n(2))));
    on.child(t("ON", pix(4.4f, C(0x121A24))));
    w.child(on);

    Element autoB = key(40, 18, 32, 12, box());
    autoB.row().alignItems(Align::Center).padding(n(2), 0, 0, 0);
    autoB.child(box().width(Dim(n(3))).height(Dim(n(3))).fill(C(0x3C4A58)));
    autoB.child(box().width(Dim(n(2))));
    autoB.child(t("AUTO", pix(4.4f, C(0x121A24))));
    w.child(autoB);

    w.child(textKey(217, 18, 44, 12, "PRESETS", 4.4f));

    // the response graph (native 86,17,113,19) — its curve is the SAME 10
    // Outputs the faders below ride, so the two widgets can never disagree.
    Element graph = at(box(), 86, 17, 113, 19).fill(graphMat);
    sunken(graph, fade(C(0x4A4A70), 0.6f), C(0x08080E));
    graph.child(box().absolute().inset(0).fill(graphGrid.material()));
    graph.child(eqCurve().absolute().inset(0).cache(Cache::None));
    w.child(graph);

    // the eleven faders: preamp at native x21, bands on an 18 px pitch
    // from x78 — the real, non-skinnable positions.
    for (int i = 0; i < 11; ++i) {
      const float x = i == 0 ? 21.0f : 78.0f + 18.0f * (float)(i - 1);
      Element trough = at(box(), x, 38, 14, 63).fill(C(0x14141F));
      sunken(trough, fade(C(0x4A4A70), 0.55f), C(0x08080E));
      trough.child(at(box(), 2, 1, 10, 61).fill(faderTrack));
      // thumb 11x11, travel 0..52 native. bind() turns the [-1,1] gain
      // straight into pixels — no second Output in slider units.
      Element th = at(box(), 1, 0, 12, 11)
                       .fill(Material::linearUnit(
                           {0, 0}, {0, 1},
                           {{0.0f, lift(kBtnFace, 0.14f)},
                            {1.0f, dark(kBtnFace, 0.32f)}}))
                       .translateY(
                           bind(&gain[(size_t)i]).from(-1.0f, 1.0f).to(n(52),
                                                                       n(0)));
      raised(th);
      th.child(at(box(), 2, 5, 8, 1).fill(fade(kBtnLo, 0.85f)));
      trough.child(th);
      w.child(trough);
    }

    // +12dB / 0dB / -12dB, baked pixel art in the real EQMAIN.BMP, printed
    // here in the gap between the preamp and the first band.
    static const char *db[3] = {"+12DB", "+0DB", "-12DB"};
    static const float dby[3] = {38, 62, 89};
    const SkColor4f dbc[3] = {dark(kEqTop, 0.15f), dark(kEqMid, 0.28f),
                              dark(kEqBot, 0.10f)};
    for (int i = 0; i < 3; ++i)
      w.child(at(box(), 38, dby[i], 38, 7)
                  .justify(Justify::End)
                  .alignItems(Align::Center)
                  .child(t(db[i], pix(3.6f, dbc[i]))));

    // PREAMP + the ten band captions, tight against the fader feet.
    w.child(at(box(), 3, 104, 30, 7)
                .alignItems(Align::Center)
                .child(t("PREAMP", pix(3.6f, C(0x8E8EB4)))));
    static const char *bands[10] = {"60",  "170", "310", "600", "1K",
                                    "3K",  "6K",  "12K", "14K", "16K"};
    for (int i = 0; i < 10; ++i)
      w.child(at(box(), 76.0f + 18.0f * (float)i, 104, 18, 7)
                  .justify(Justify::Center)
                  .alignItems(Align::Center)
                  .child(t(bands[i], pix(3.6f, C(0x8E8EB4)))));
    return w;
  }

  /** The response curve. It has to read the ELEVEN live Outputs at paint
   *  time and reveal itself over 300 ms, and outline() resolves at layout,
   *  so this drops to a custom leaf — see the gap list. */
  Element eqCurve() {
    return custom([this](SkCanvas &canvas, const PaintContext &ctx) {
      const float w = ctx.size.width(), h = ctx.size.height();
      const float mid = h * 0.5f;
      SkPaint zero;
      zero.setColor4f(wa::fade(wa::kGrid, 0.9f), nullptr);
      canvas.drawRect(SkRect::MakeXYWH(0, mid - wa::n(0.5f), w, wa::n(1)),
                      zero);

      const int kN = 10;
      std::array<SkPoint, kN> pts{};
      for (int i = 0; i < kN; ++i) {
        const float g = gain[(size_t)(i + 1)].value();
        pts[(size_t)i] = {w * ((float)i + 0.5f) / (float)kN,
                          mid - g * (h * 0.42f)};
      }
      SkPathBuilder b;
      b.moveTo(0, pts[0].fY);
      // Catmull-Rom through the ten band values, so the graph is one curve
      // rather than a polyline of segments.
      const int kSteps = 96;
      for (int s = 0; s <= kSteps; ++s) {
        const float u = (float)s / (float)kSteps * (float)(kN - 1);
        const int i = std::clamp((int)u, 0, kN - 2);
        const float f = u - (float)i;
        auto P = [&](int k) {
          return pts[(size_t)std::clamp(k, 0, kN - 1)];
        };
        const SkPoint p0 = P(i - 1), p1 = P(i), p2 = P(i + 1), p3 = P(i + 2);
        const float y =
            0.5f * ((2 * p1.fY) + (-p0.fY + p2.fY) * f +
                    (2 * p0.fY - 5 * p1.fY + 4 * p2.fY - p3.fY) * f * f +
                    (-p0.fY + 3 * p1.fY - 3 * p2.fY + p3.fY) * f * f * f);
        const float x =
            0.5f * ((2 * p1.fX) + (-p0.fX + p2.fX) * f +
                    (2 * p0.fX - 5 * p1.fX + 4 * p2.fX - p3.fX) * f * f +
                    (-p0.fX + 3 * p1.fX - 3 * p2.fX + p3.fX) * f * f * f);
        if (s == 0)
          b.moveTo(0, y);
        b.lineTo(x, y);
        if (s == kSteps)
          b.lineTo(w, y);
      }
      SkPath path = b.detach();
      // hand-rolled trim: the reveal window over a LIVE path.
      const float draw = std::clamp(graphDraw.value(), 0.0f, 1.0f);
      canvas.save();
      canvas.clipRect(SkRect::MakeWH(w * draw, h));
      SkPaint p;
      p.setAntiAlias(true);
      p.setStyle(SkPaint::kStroke_Style);
      p.setStrokeWidth(wa::n(1));
      p.setColor4f(wa::C(0x1AE81A), nullptr);
      canvas.drawPath(path, p);
      p.setStrokeWidth(wa::n(2.5f));
      p.setColor4f(wa::C(0x1AE81A, 0.20f), nullptr);
      canvas.drawPath(path, p);
      canvas.restore();
    });
  }

  // ---- playlist window ----------------------------------------------------

  Element playlistWindow() {
    using namespace wa;
    const float W = 400, H = 377;
    Element w = box().width(Dim(n(W))).height(Dim(n(H)));
    w.child(box().absolute().inset(0).fill(steel).cache(Cache::Texture));
    raised(w, fade(C(0x585880), 0.7f), C(0x0E0E18));
    w.child(titleBar(W, "WINAMP PLAYLIST", true, false, 20.0f));

    // the list well: left rail 12, right rail 20, 25 rows of 13 px
    Element list = at(box(), 12, 20, W - 32, 319).fill(kPlBg);
    sunken(list, fade(C(0x4A4A70), 0.6f), C(0x06060A));
    // row backgrounds: one atlas stamp, three tint states.
    list.child(box().absolute().inset(0).child(
        instancing::instances(rowAtlas, rowPool, instancing::Mode::Live)));
    list.child(box().absolute().inset(0).clip().child(slot("tracks")));
    w.child(list);

    // the scrollbar rail and its two arrow buttons
    Element rail = at(box(), W - 20, 20, 20, 319).fill(C(0x1A1A2A));
    sunken(rail, fade(C(0x4A4A70), 0.5f), C(0x0A0A12));
    w.child(rail);
    Element grip = at(box(), W - 19, 24, 18, 36).fill(kBtnFace);
    raised(grip);
    w.child(grip);
    auto arrow = [&](float y, bool up) {
      Element e = key(W - 19, y, 18, 14, box());
      e.child(part(6, up ? 5 : 4, 6, 5, upDown(up)));
      return e;
    };
    w.child(arrow(310, true));
    w.child(arrow(324, false));

    // ---- the bottom control strip (native y 339..377) -------------------
    Element bottom = at(box(), 0, 339, W, 38);
    bottom.child(box().absolute().inset(0).fill(steel).cache(Cache::Texture));
    bottom.child(at(box(), 0, 0, W, 1).fill(fade(C(0x585880), 0.6f)));

    // ADD / REM / SEL / MISC, bottom-anchored 6 native px above the sill.
    // (The brief's absolute y for this row lands above the strip; the
    // bottom-anchored reading is what the real window does.)
    static const char *menus[4] = {"ADD", "REM", "SEL", "MISC"};
    for (int i = 0; i < 4; ++i)
      bottom.child(textKey(14 + 29 * (float)i, 14, 22, 18, menus[i], 4.0f));
    Element opts = key(W - 44, 14, 22, 18, box());
    opts.column().justify(Justify::Center).alignItems(Align::Center);
    opts.child(t("LIST", pix(3.8f, C(0x121A24))));
    opts.child(t("OPTS", pix(3.8f, C(0x121A24))));
    bottom.child(opts);

    // running-time readout
    bottom.child(at(box(), 132, 13, 62, 7)
                     .alignItems(Align::Center)
                     .child(t(runningTime(), pix(4.0f, kPlText))));

    // the mini transport dock
    Element dock = at(box(), 132, 22, 62, 12).fill(C(0x12121E));
    sunken(dock, fade(C(0x4A4A70), 0.55f), C(0x08080E));
    for (int i = 0; i < 5; ++i) {
      Element g = box();
      if (i == 0) {
        g.child(part(2, 3, 1, 5));
        g.child(part(4, 3, 4, 5, tri(1)));
      } else if (i == 1) {
        g.child(part(3, 2, 5, 6, tri(0)));
      } else if (i == 2) {
        g.child(part(3, 3, 2, 5));
        g.child(part(6, 3, 2, 5));
      } else if (i == 3) {
        g.child(part(3, 3, 5, 5));
      } else {
        g.child(part(3, 3, 4, 5, tri(0)));
        g.child(part(8, 3, 1, 5));
      }
      Element b = at(box(), 2 + 12 * (float)i, 1, 11, 10)
                      .fill(fade(kBtnFace, 0.9f));
      raised(b, fade(kBtnHi, 0.8f), kBtnLo);
      b.child(g);
      dock.child(b);
    }
    bottom.child(dock);

    // the preview-visualiser swatch (default checkerboard art)
    Element sw = at(box(), W - 88, 20, 38, 14).fill(C(0x000000));
    sunken(sw, fade(C(0x4A4A70), 0.5f), C(0x08080E));
    sw.child(box().absolute().inset(0).fill(
        patterns::checker(n(2), C(0x2B2B44), C(0x14141F)).material()));
    bottom.child(sw);
    w.child(bottom);
    return w;
  }

  std::string runningTime() {
    int total = 0;
    for (const Track &tr : tracks())
      total += tr.seconds;
    return mmss((int)(playPos.value() * tracks()[(size_t)nowPlaying].seconds)) +
           "/" + mmss(total);
  }

  /** The list rows. Twenty-five rows of REAL text — the one place classic
   *  Winamp uses a proportional font — with SigilWeave doing the ellipsis
   *  the real CSS asks for. Backgrounds come from the instancing pool
   *  underneath; the Pool has no text lane, so the rows themselves stay
   *  elements. */
  Element trackList() {
    using namespace wa;
    Element col = box().column();
    const float rowH = 13, listW = 368;
    for (int i = 0; i < 25; ++i) {
      const Track &tr = tracks()[(size_t)i];
      const SkColor4f ink = i == nowPlaying ? kPlNow : kPlText;
      Element r = box()
                      .height(Dim(n(rowH)))
                      .row()
                      .alignItems(Align::Center)
                      .padding(n(3), 0, n(3), 0);
      // PLEDIT.TXT Font=Arial, CSS font-size 9px + 0.5px tracking.
      auto st = type(arial(), n(9) * 0.78f, ink, n(0.5f));
      r.child(ellipsized(i, std::to_string(i + 1) + ". " + tr.title, st,
                         n(listW - 40)));
      r.child(box().grow(1));
      r.child(t(tr.time, type(arial(), n(9) * 0.78f, ink, n(0.5f))));
      // rows reveal in bands of four (24 rows at an even stagger reads as
      // 24 animations; batching reads as a list populating)
      r.opacity(&rowIn[(size_t)i]);
      r.translateY(bind(&rowIn[(size_t)i]).invert().scale(n(2)));
      col.child(r);
    }
    return col;
  }

  /** One clipped, ellipsised row title. The paragraph is held so pointer
   *  identity keeps the shaping cache warm across slot re-renders. */
  Element ellipsized(int idx, const std::string &s,
                     const sigil::weave::TextStyle &st, float w) {
    sigil::weave::ParagraphBuilder b(st);
    b.addText(toU8(s));
    auto p = std::make_shared<sigil::weave::Paragraph>(b.build());
    if ((int)rowPara.size() <= idx)
      rowPara.resize((size_t)idx + 1);
    rowPara[(size_t)idx] = p;
    sigil::weave::ParagraphLayoutOptions o;
    o.overflow.ellipsis = u"…";
    o.overflow.maxLines = 1;
    return text(p, o).width(Dim(w)).shrink(0);
  }

  // =========================================================================

  Element describe() {
    using namespace wa;
    Element root = stack().width(Dim(1320)).height(Dim(1947));

    // the desktop: flat teal + one baked dither pass
    root.child(box()
                   .absolute()
                   .inset(0)
                   .fill(deskMat)
                   .cache(Cache::Texture));

    // Main — pops in at its final position, scale 0.9 -> 1 on outBack.
    root.child(mainWindow()
                   .absolute()
                   .left(Dim(60))
                   .top(Dim(60))
                   .transformOrigin(0.5f, 0.5f)
                   .scale(withFrom(0.9f, 1.0f, {200ms, ease::outBack(), 100ms}))
                   .opacity(withKeyframes<float>(
                       {{0ms, 0.0f}, {99ms, 0.0f}, {100ms, 1.0f}},
                       &ch::easeNone)));

    // Equalizer — docking snap from 60 px above, the same outBack value.
    root.child(eqWindow()
                   .absolute()
                   .left(Dim(60))
                   .top(Dim(408))
                   .translateY(withFrom(-60.0f, 0.0f,
                                        {250ms, ease::outBack(), 900ms}))
                   .opacity(withKeyframes<float>(
                       {{0ms, 0.0f}, {899ms, 0.0f}, {900ms, 1.0f}},
                       &ch::easeNone)));

    // Playlist — same snap, 1.25 s later.
    root.child(playlistWindow()
                   .absolute()
                   .left(Dim(60))
                   .top(Dim(756))
                   .translateY(withFrom(-60.0f, 0.0f,
                                        {250ms, ease::outBack(), 2150ms}))
                   .opacity(withKeyframes<float>(
                       {{0ms, 0.0f}, {2149ms, 0.0f}, {2150ms, 1.0f}},
                       &ch::easeNone)));
    return root;
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    using namespace wa;
    ctx.canvas(1320, 1947);
    ctx.background(kDesk);
    buildMaterials();

    // The substituted monospace faces, measured rather than assumed.
    {
      auto probe = [&](const sk_sp<SkTypeface> &tf) {
        const float w =
            ctx.measure(text(u8"MMMMMMMMMM", type(tf, 100.0f, kGreen)))
                .width();
        return w > 1.0f ? w / 1000.0f : 0.602f;
      };
      wa::monoEm() = probe(mono());
      wa::boldEm() = probe(monoBold());
    }

    // --- the LED atlas: ONE cell, a 3x1 native quad, tinted per instance.
    ledAtlas = std::make_shared<instancing::Atlas>(2.0f);
    ledAtlas->cell(box().fill(SkColor4f{1, 1, 1, 1}), {n(3), n(1)});
    ledPool = std::make_shared<instancing::Pool>();
    ledPool->resize((size_t)(kCols * kRows + kCols));
    {
      auto pos = ledPool->positions();
      auto tint = ledPool->tints();
      for (int c = 0; c < kCols; ++c)
        for (int r = 0; r < kRows; ++r) {
          const size_t i = (size_t)(c * kRows + r);
          pos[i] = {n(4.0f * (float)c + 1.5f),
                    n((float)(kRows - 1 - r) + 0.5f)};
          tint[i] = fade(kVis[(size_t)r], 0.0f);
        }
      for (int c = 0; c < kCols; ++c) {
        const size_t i = (size_t)(kCols * kRows + c);
        pos[i] = {n(4.0f * (float)c + 1.5f), n(0.5f)};
        tint[i] = fade(kPeak, 0.0f);
      }
    }

    // --- playlist row backgrounds: three tint states, one stamp.
    rowAtlas = std::make_shared<instancing::Atlas>(1.0f);
    rowAtlas->cell(box().fill(SkColor4f{1, 1, 1, 1}), {n(368), n(13)});
    rowPool = std::make_shared<instancing::Pool>();
    rowPool->resize(25);
    {
      auto pos = rowPool->positions();
      for (int i = 0; i < 25; ++i)
        pos[(size_t)i] = {n(184), n(13.0f * (float)i + 6.5f)};
    }

    // --- the marquee wants its content width measured once.
    marqueeW = ctx.measure(t(marqueeText(), pix(5, C(0x00E000)))).width();
    if (marqueeW < 1)
      marqueeW = n(300);

    // --- one steppable drives every idle loop.
    ctx.ticker.add([this](double dt) {
      step(dt);
      return true;
    });

    ctx.composer.render(describe());
    pushSlots(ctx, true);
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    elapsedNow = elapsed;
    pushSlots(ctx, false);
  }

  /** The three discrete-state slots. Everything continuous is bound; these
   *  three are text content and a 28-frame sprite index, neither of which
   *  a PropValue can carry. */
  void pushSlots(sketch::SketchContext &ctx, bool force) {
    const int sec =
        (int)(playPos.value() * (float)tracks()[(size_t)nowPlaying].seconds);
    if (force || sec != shownSec) {
      shownSec = sec;
      ctx.composer.renderSlot("time", timeReadout(sec));
    }
    const int v = (int)std::lround(volFrame.value());
    const int b = (int)std::lround(balFrame.value());
    if (force || v != volSprite || b != balSprite) {
      volSprite = v;
      balSprite = b;
      ctx.composer.renderSlot("sliders", sliders(v, b));
    }
    static int lastNow = -1, lastSel = -1;
    if (force || nowPlaying != lastNow || selected != lastSel) {
      lastNow = nowPlaying;
      lastSel = selected;
      ctx.composer.renderSlot("tracks", trackList());
    }
  }

  // =========================================================================
  // The frame loop. Nothing here re-describes.

  static float hash(int a, int b) {
    uint32_t h = (uint32_t)(a * 374761393 + b * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffffff) / (float)0xffffff;
  }

  void step(double dt) {
    using namespace wa;
    const double t = (elapsedNow += dt);

    // ---- playback position: ONE Output, three consumers ----------------
    const float len = (float)tracks()[(size_t)nowPlaying].seconds;
    playPos = std::fmod(0.42f + (float)std::max(0.0, t - 0.45) / len, 1.0f);

    // ---- play LED, from 0.45 s -----------------------------------------
    led = t > 0.45 ? 1.0f : 0.0f;

    // ---- marquee: the title holds through the boot sequence, then runs
    // at the real ~30 native px/s with a 1 s pause at the loop seam ------
    {
      const float span = marqueeW + n(40);
      const double period = (double)(span / n(30)) + 1.0;
      const double u = std::fmod(std::max(0.0, t - 4.0), period);
      const double run = (double)(span / n(30));
      marqueePhase = u < run ? -(float)(u * n(30)) : -span;
    }

    // ---- the eleven EQ gains: staggered elastic to the Rock preset -----
    for (int i = 0; i < 11; ++i) {
      const double start = 1.15 + 0.04 * (double)i;
      const float target = rockPreset()[(size_t)i];
      float v = 0.0f;
      if (t > start) {
        const float u = (float)std::min(1.0, (t - start) / 0.70);
        v = target * ch::easeOutElastic(u, 1.0f, 0.4f);
        if (t > start + 0.70) {
          // "live audio": an 8 Hz quantised wobble on the settled preset
          const int k = (int)std::floor((t - start) * 8.0);
          v = target + (hash(i * 7 + 3, k) - 0.5f) * 0.055f;
        }
      }
      gain[(size_t)i] = std::clamp(v, -1.0f, 1.0f);
    }
    graphDraw = (float)std::clamp((t - 1.85) / 0.30, 0.0, 1.0);

    // ---- the 28-frame sliders: quantisation, not a lerp -----------------
    {
      const float pctV = t < 3.5 ? 0.0f : 0.78f;
      const float pctB = t < 3.5 ? 0.5f : 0.5f;
      volFrame = std::max(0.0f, std::round(pctV * 28.0f) - 1.0f);
      balFrame = std::max(0.0f, std::round(pctB * 28.0f) - 1.0f);
    }

    // ---- the analyser: hard 12 Hz steps, no easing anywhere ------------
    if (t >= 3.2) {
      const double stepT = std::floor(t * 12.0);
      if (stepT != lastRoll) {
        lastRoll = stepT;
        for (int c = 0; c < kCols; ++c) {
          const float shape =
              0.30f + 0.70f * std::sin(3.14159f * (float)c / (float)kCols);
          const float a = hash(c, (int)stepT);
          const float bb = hash(c * 3 + 11, (int)stepT / 3);
          const float lvl = (0.18f + 0.82f * a * (0.35f + 0.65f * bb)) * shape;
          colLevel[(size_t)c] = lvl * (float)kRows;
          colPeak[(size_t)c] = std::max(colPeak[(size_t)c] - 0.35f,
                                        colLevel[(size_t)c]);
        }
      }
    }
    {
      auto tint = ledPool->tints();
      auto pos = ledPool->positions();
      for (int c = 0; c < kCols; ++c) {
        const int lit = (int)colLevel[(size_t)c];
        for (int r = 0; r < kRows; ++r) {
          const size_t i = (size_t)(c * kRows + r);
          tint[i] = fade(kVis[(size_t)r], r < lit ? 1.0f : 0.0f);
        }
        const size_t pi = (size_t)(kCols * kRows + c);
        const float pk = colPeak[(size_t)c];
        pos[pi] = {n(4.0f * (float)c + 1.5f),
                   n((float)kRows - std::clamp(pk, 0.0f, (float)kRows) + 0.5f)};
        tint[pi] = fade(kPeak, pk > 0.6f ? 0.85f : 0.0f);
      }
    }

    // ---- playlist rows: reveal in bands of four ------------------------
    {
      auto tint = rowPool->tints();
      for (int i = 0; i < 25; ++i) {
        const double start = 2.4 + 0.13 * (double)(i / 4);
        const float u = (float)std::clamp((t - start) / 0.13, 0.0, 1.0);
        rowIn[(size_t)i] = u * u;
        SkColor4f c = i == selected ? kPlSel
                      : i == nowPlaying
                          ? SkColor4f{1, 1, 1, 0.10f}
                          : SkColor4f{0, 0, 0, 0};
        c.fA *= u;
        tint[(size_t)i] = c;
      }
    }

    // ---- the clutter-bar glint, once every 5 s -------------------------
    glint = (float)std::fmod(t, 5.0) < 0.2
                ? (float)(std::fmod(t, 5.0) / 0.2)
                : 0.0f;

    // ---- the LLAMA easter egg, every ~9 s for 800 ms -------------------
    {
      const double u = std::fmod(t + 2.0, 9.0);
      if (u < 0.8) {
        const float f = (float)(u / 0.8);
        llama = f < 0.12f ? f / 0.12f : (f > 0.88f ? (1.0f - f) / 0.12f : 1.0f);
        llamaPop = 0.85f + 0.15f * ch::easeOutBounce(std::min(1.0f, f * 4.0f),
                                                     1.70158f);
      } else {
        llama = 0.0f;
        llamaPop = 1.0f;
      }
    }
  }
};

SIGIL_SKETCH(WinampBase)

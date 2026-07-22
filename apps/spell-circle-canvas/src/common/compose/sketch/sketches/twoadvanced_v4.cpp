// twoadvanced_v4.cpp — 2Advanced Studios, v4 "Prophecy" (2003–2006),
// the post-splash Home/Index interface (flashindex.swf, a 970×655 stage).
// Creative direction / design / UI / Flash animation: Eric Jordan.
//
// REFERENCE (everything measured, not remembered):
//   · http://web.archive.org/web/20040610062326/http://2advanced.com/flashindex.htm
//     — the live 2004 HTML shell: rails 12×780, stage 970×655, footer
//       970×110 tiled by a 970×1 strip. This sketch is EXACTLY ×2 of that
//       frame: 1940×1560. Halve any number here to recover the 2004 pixel.
//   · .../images/{leftsidepanel,rightsidepanel,sitebackground,sitefooter,
//     splash_*}.gif — the real production GIFs, pixel-sampled: chrome
//     maroon #571119, page ramp #4A100F→#0A0000, logo cyan #7BDAD6.
//   · .../flashindex.swf (543,340 B, CWS/zlib SWF6) — decompressed and
//     `strings`-mined. It yields the component-kit class names this file
//     rebuilds as functions — FSingleBevelPanelClass, FDoubleBevelPanelClass,
//     FBevelledPanelClass — the nav taxonomy company/services/portfolio/
//     accolades/experimental/equipment/contact, and the embedded faces
//     Arial, Arial Black, Helvetica 95 Black, Helvetica CondensedBlack.
//   · .../mp3Info.xml — the 2004 playlist; the 2006 one (DESERT TRANCE /
//     NEVERMIND / SYNERGY / EXHALE) is off the FWA press screenshot,
//     https://thefwa.com/cases/2advanced-v4-prophecy (900×575 of the live
//     interface, news items dated 01.30.06 / 04.05.06).
//   · https://v4prophecy.2advanced.com/ — 2Advanced's own Ruffle
//     restoration of this exact movie.
//
// THE PALETTE IS NOT CYAN/ORANGE. The genre's reputation says orange;
// neither the 2004 GIFs nor the 2006 screenshot contain ANY. It is
// cyan-teal (#7BDAD6 / #01D0D5 / #579797) against oxblood (#571119 /
// #4A100F), with blood red (#700000) — not orange — on the CTAs. Do not
// "correct" this from memory.
//
// The MAINFRAME hero was a real Cinema 4D composite (the site's own press
// copy name-checks Maxon). There is no 3D here: flat SDF shapes, one
// radial portal, an opacity-ramped skyline, three dome silhouettes with
// gloss rim-light, a mirrored blurred reflection under a hard horizon
// hairline, and a blurred kPlus bloom copy over the whole stack. §10's
// claim holds — stacking ORDER does the work, not any one layer.

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Decorations.h>
#include <sigilcompose/Instances.h>
#include <sigilcompose/Kinetic.h>
#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Material.h>
#include <sigilcompose/Pattern.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Sdf.h>
#include <sigilcompose/Shapes.h>
#include <sigilcompose/Util.h>

#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkFontMgr.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace tav {

// ---------------------------------------------------------------------------
// Palette — §4, every value sampled from an artefact.

constexpr SkColor4f C(uint32_t rgb, float a = 1.0f) {
  return {(float)((rgb >> 16) & 0xff) / 255.0f,
          (float)((rgb >> 8) & 0xff) / 255.0f, (float)(rgb & 0xff) / 255.0f, a};
}

constexpr SkColor4f kBgTop = C(0x4A100F);   // page gradient, top
constexpr SkColor4f kBgMid = C(0x1A0001);
constexpr SkColor4f kBgBot = C(0x0A0000);   // …faded to near-black
constexpr SkColor4f kChrome = C(0x571119);  // THE chrome maroon
constexpr SkColor4f kChromeHi = C(0x6A1B21);
constexpr SkColor4f kD1 = C(0x180707);      // footer-dock HUD darks
constexpr SkColor4f kD2 = C(0x260909);
constexpr SkColor4f kD3 = C(0x370C0D);
constexpr SkColor4f kD4 = C(0x400E0F);
constexpr SkColor4f kD5 = C(0x4C1010);
constexpr SkColor4f kCyan = C(0x7BDAD6);    // logo wordmark core
constexpr SkColor4f kCyanRing = C(0x95C9CC);
constexpr SkColor4f kDust = C(0x8D7777);    // the dusty-rose third neutral
constexpr SkColor4f kDustDim = C(0x735757);
constexpr SkColor4f kTealBar = C(0x2C7B80); // status-bar segment
constexpr SkColor4f kGlow = C(0x01D0D5);    // MAINFRAME portal core
constexpr SkColor4f kPanel = C(0x579797);   // monitor-panel body
constexpr SkColor4f kPanelHi = C(0x84B8B6);
constexpr SkColor4f kPanelSh = C(0x3C8282);
constexpr SkColor4f kCta = C(0x700000);     // LAUNCH / ARCHIVES core
constexpr SkColor4f kCtaHi = C(0xB27E82);
constexpr SkColor4f kNear = C(0xF3F3F3);
constexpr SkColor4f kBody = C(0xC9DEDD);
constexpr SkColor4f kDate = C(0x1C4040);
constexpr SkColor4f kHeadDim = C(0xB8A0A0);

inline SkColor4f lift(SkColor4f c, float k) {
  return {std::min(1.0f, c.fR + k), std::min(1.0f, c.fG + k),
          std::min(1.0f, c.fB + k), c.fA};
}
inline SkColor4f dark(SkColor4f c, float k) {
  return {c.fR * (1 - k), c.fG * (1 - k), c.fB * (1 - k), c.fA};
}
inline SkColor4f fade(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, a}; }

// ---------------------------------------------------------------------------
// Type — the SWF's embedded faces, substituted with the nearest the
// platform ships. Helvetica CondensedBlack is the whole chrome voice.

inline sk_sp<SkTypeface> face(const char *family, int weight, int width,
                              const char *fallbackFamily = nullptr) {
  auto mgr = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      family, SkFontStyle(weight, width, SkFontStyle::kUpright_Slant));
  if (!f && fallbackFamily)
    f = mgr->matchFamilyStyle(
        fallbackFamily,
        SkFontStyle(weight, width, SkFontStyle::kUpright_Slant));
  if (!f)
    f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Bold());
  return f;
}

inline const sk_sp<SkTypeface> &condBlack() {
  static sk_sp<SkTypeface> f =
      face("Helvetica Neue", SkFontStyle::kBlack_Weight,
           SkFontStyle::kCondensed_Width, "Avenir Next Condensed");
  return f;
}
inline const sk_sp<SkTypeface> &blackFace() {
  static sk_sp<SkTypeface> f =
      face("Arial Black", SkFontStyle::kBlack_Weight,
           SkFontStyle::kNormal_Width, "Helvetica Neue");
  return f;
}
inline const sk_sp<SkTypeface> &arial() {
  static sk_sp<SkTypeface> f = face("Arial", SkFontStyle::kNormal_Weight,
                                    SkFontStyle::kNormal_Width, "Helvetica");
  return f;
}

/** Tracking authored in Illustrator units (1/1000 em), as §6 states it. */
inline sigil::weave::TextStyle type(const sk_sp<SkTypeface> &tf, float size,
                                    SkColor4f color, float trackUnits = 0,
                                    float condense = 1.0f) {
  sigil::weave::TextStyle s;
  s.shaping.typeface = tf;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = size * trackUnits / 1000.0f;
  s.shaping.scaleX = condense;
  s.paint.foreground.setColor4f(color, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

inline sigil::weave::TextStyle micro(float size, SkColor4f c, float tr = 200) {
  return type(condBlack(), size, c, tr, 0.92f);
}
inline sigil::weave::TextStyle label(float size, SkColor4f c, float tr = 100) {
  return type(condBlack(), size, c, tr, 0.88f);
}
inline sigil::weave::TextStyle heavy(float size, SkColor4f c, float tr = 40) {
  return type(blackFace(), size, c, tr, 0.94f);
}
inline sigil::weave::TextStyle prose(float size, SkColor4f c) {
  return type(arial(), size, c, 0);
}

inline Element t(const char *s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

// ---------------------------------------------------------------------------
// Geometry vocabulary — chamfers, not radii (§5).

enum Cut : uint8_t { kTL = 1, kTR = 2, kBR = 4, kBL = 8 };

/** The angle-cut rect: a 45° cut removing a cut×cut triangle from each
 *  selected corner. Reused at every nesting depth — status-bar seam, CTA
 *  buttons, panel end-caps, readout windows, transport keys, dropdowns. */
inline std::function<SkPath(SkSize)> chamfer(float cut, uint8_t mask) {
  return [cut, mask](SkSize s) {
    const float w = s.width(), h = s.height();
    const float c = std::min({cut, w * 0.5f, h * 0.5f});
    SkPathBuilder b;
    if (mask & kTL)
      b.moveTo(c, 0);
    else
      b.moveTo(0, 0);
    if (mask & kTR) {
      b.lineTo(w - c, 0);
      b.lineTo(w, c);
    } else {
      b.lineTo(w, 0);
    }
    if (mask & kBR) {
      b.lineTo(w, h - c);
      b.lineTo(w - c, h);
    } else {
      b.lineTo(w, h);
    }
    if (mask & kBL) {
      b.lineTo(c, h);
      b.lineTo(0, h - c);
    } else {
      b.lineTo(0, h);
    }
    if (mask & kTL)
      b.lineTo(0, c);
    b.close();
    return b.detach();
  };
}

/** An OPEN hairline across the node — the trim() reveal primitive: a
 *  stroked open outline draws itself on when trim's end ramps 0→1. */
inline std::function<SkPath(SkSize)> ray(float dirX, float dirY) {
  return [dirX, dirY](SkSize s) {
    SkPathBuilder b;
    b.moveTo(dirX < 0 ? s.width() : 0, dirY < 0 ? s.height() : 0);
    b.lineTo(dirX < 0 ? 0 : s.width(), dirY < 0 ? 0 : s.height());
    return b.detach();
  };
}

// ---------------------------------------------------------------------------
// Decoration vocabulary — the hairline kit the whole idiom runs on. All
// value schemes, so a static bracketed panel prunes with no memo.

/** Corner brackets: two legs, a gap from the edge. Cyan on the audio
 *  module, oxblood on the footer dock, everywhere in between. */
struct Brackets {
  SkColor4f color = kCyan;
  float leg = 18, thickness = 3, gap = 4;
  uint8_t mask = 0xF;

  bool operator==(const Brackets &) const = default;
  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(false);
    p.setColor4f(color, nullptr);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(thickness);
    const float w = ctx.size.width(), h = ctx.size.height();
    const float o = gap + thickness * 0.5f;
    auto L = [&](float x, float y, float sx, float sy) {
      SkPathBuilder b;
      b.moveTo(x + sx * leg, y);
      b.lineTo(x, y);
      b.lineTo(x, y + sy * leg);
      c.drawPath(b.detach(), p);
    };
    if (mask & kTL) L(o, o, 1, 1);
    if (mask & kTR) L(w - o, o, -1, 1);
    if (mask & kBR) L(w - o, h - o, -1, -1);
    if (mask & kBL) L(o, h - o, 1, -1);
  }
};

/** The inner half of FDoubleBevelPanelClass: a second, fainter bevel
 *  nested `inset` px inside the first. A FOREGROUND, so it rides above
 *  the panel's own content — which is what makes it read as a frame. */
struct InsetBevel {
  SkColor4f hi{1, 1, 1, 0.18f}, lo{0, 0, 0, 0.35f};
  float inset = 6, hiW = 2, loW = 1;

  bool operator==(const InsetBevel &) const = default;
  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const SkRect r = SkRect::MakeWH(ctx.size.width(), ctx.size.height())
                         .makeInset(inset, inset);
    if (r.isEmpty())
      return;
    SkPaint p;
    p.setAntiAlias(false);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(hiW);
    p.setColor4f(hi, nullptr);
    SkPathBuilder a;
    a.moveTo(r.fLeft, r.fBottom);
    a.lineTo(r.fLeft, r.fTop);
    a.lineTo(r.fRight, r.fTop);
    c.drawPath(a.detach(), p);
    p.setStrokeWidth(loW);
    p.setColor4f(lo, nullptr);
    SkPathBuilder b;
    b.moveTo(r.fRight, r.fTop);
    b.lineTo(r.fRight, r.fBottom);
    b.lineTo(r.fLeft, r.fBottom);
    c.drawPath(b.detach(), p);
  }
};

/** The readout nobody reads: a rail of ticks along one edge, every Nth
 *  one long. Denser than any single element on the page. */
struct TickRail {
  SkColor4f color = fade(kCyan, 0.5f);
  float spacing = 8, shortLen = 4, longLen = 9, thickness = 1;
  int major = 4;
  bool vertical = false, farSide = false;

  bool operator==(const TickRail &) const = default;
  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(false);
    p.setColor4f(color, nullptr);
    const float w = ctx.size.width(), h = ctx.size.height();
    const float run = vertical ? h : w;
    int i = 0;
    for (float d = spacing * 0.5f; d < run; d += spacing, ++i) {
      const float len = (i % major == 0) ? longLen : shortLen;
      if (vertical)
        c.drawRect(SkRect::MakeXYWH(farSide ? w - len : 0, d, len, thickness),
                   p);
      else
        c.drawRect(SkRect::MakeXYWH(d, farSide ? h - len : 0, thickness, len),
                   p);
    }
  }
};

/** The rail flare tick read off leftsidepanel.gif: a near-vertical
 *  hairline ending in a small flag, with a soft highlight travelling down
 *  it every 6 s (§9's idle list). The rail is 24 px at ×2, so the 90 px
 *  flare runs 8° off VERTICAL — the only way that length fits the asset. */
struct RailFlares {
  SkColor4f color = C(0x99AAAA);
  float period = 6.0f, phase = 0.0f;

  bool operator==(const RailFlares &) const = default;
  bool animated() const { return true; }
  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const float w = ctx.size.width(), h = ctx.size.height();
    const float ys[3] = {h * 0.167f, h * 0.5f, h * 0.833f};
    const float len = 90.0f, lean = 12.6f;
    const double tt = ctx.elapsedSeconds + phase;
    const float scan = (float)std::fmod(tt, (double)period) / period;
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(2);
    for (int i = 0; i < 3; ++i) {
      const float y0 = ys[i] - len * 0.5f;
      const float x0 = w * 0.5f - lean * 0.5f;
      SkPathBuilder b;
      b.moveTo(x0, y0);
      b.lineTo(x0 + lean, y0 + len);
      const SkPath path = b.detach();
      p.setColor4f(fade(color, 0.55f), nullptr);
      c.drawPath(path, p);
      SkPaint f;
      f.setAntiAlias(true);
      f.setColor4f(fade(color, 0.8f), nullptr);
      c.drawRect(SkRect::MakeXYWH(x0 + lean - 3, y0 + len, 6, 3), f);
      SkPaint g;
      g.setAntiAlias(true);
      g.setStyle(SkPaint::kStroke_Style);
      g.setStrokeWidth(2);
      g.setColor4f(fade(kCyan, 0.9f * (1.0f - std::abs(scan - 0.5f) * 2)),
                   nullptr);
      SkPathBuilder hb;
      hb.moveTo(x0 + lean * scan, y0 + len * scan);
      const float s2 = std::min(1.0f, scan + 0.16f);
      hb.lineTo(x0 + lean * s2, y0 + len * s2);
      c.drawPath(hb.detach(), g);
    }
  }
};

/** Scanline veil — the CRT this idiom was shot on. */
struct Scanlines {
  SkColor4f color{0, 0, 0, 0.20f};
  float period = 4, on = 2;

  bool operator==(const Scanlines &) const = default;
  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPaint p;
    p.setColor4f(color, nullptr);
    for (float y = 0; y < ctx.size.height(); y += period)
      c.drawRect(SkRect::MakeXYWH(0, y, ctx.size.width(), on), p);
  }
};

// ---------------------------------------------------------------------------

inline Element place(Element e, float x, float y, float w, float h) {
  e.absolute().left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h));
  return e;
}

// ---------------------------------------------------------------------------
// The two panel CLASSES the SWF's symbol table names, as reusable
// functions — which is what they were in 2Advanced's own component kit.

/** FSingleBevelPanelClass: base fill + a 3 px lifted top/left highlight
 *  inset 1 px and a 2 px darkened bottom/right shadow. Two strokes, no
 *  filter — crisp at any scale, and shapes::onEdges() follows the
 *  CHAMFERED outline wherever the panel has one. */
inline Element singleBevel(Element e, SkColor4f base) {
  e.fill(base);
  e.foreground(shapes::onEdges(
      shapes::Edge::Top | shapes::Edge::Left,
      util::stroke(3, Fill::color(lift(base, 0.10f)),
                   PathFormat::Align::Inner)));
  e.foreground(shapes::onEdges(
      shapes::Edge::Bottom | shapes::Edge::Right,
      util::stroke(2, Fill::color(dark(base, 0.45f)),
                   PathFormat::Align::Inner)));
  return e;
}

/** FDoubleBevelPanelClass: the same pair, plus a fainter one `inset` px
 *  in. MAINFRAME, FEATURE SYSTEM and PRESS UPDATES wear this; everything
 *  smaller is single. */
inline Element doubleBevel(Element e, SkColor4f base, float inset = 6) {
  e = singleBevel(std::move(e), base);
  e.foreground(InsetBevel{fade(lift(base, 0.16f), 0.8f),
                          fade(dark(base, 0.55f), 0.85f), inset, 2, 1});
  return e;
}

} // namespace tav

// ===========================================================================

struct TwoAdvancedV4 : sigil::compose::sketch::Sketch {
  // --- bound outputs: every idle motion is DECLARED, none re-describes ---
  ch::Output<float> stripePan{0.0f};    // hazard-stripe conveyor
  ch::Output<float> portalGlow{54.0f};  // MAINFRAME portal glow radius
  ch::Output<float> pressScroll{0.0f};  // PRESS UPDATES auto-scroll
  ch::Output<float> vuLeft{0.4f}, vuRight{0.6f};
  std::array<ch::Output<float>, 21> dot{}; // 7 clusters × 3 dots
  std::array<ch::Output<float>, 3> gauge{};
  std::array<ch::Output<float>, 3> gaugeAlpha{};

  // --- generated materials, HELD so their identity prunes across renders ---
  Pattern hazard;         // baked 45° stripe tile — the STATIC reuse path
  Pattern hatchA, hatchB; // footer-dock crosshatch (two passes = crosshatch)
  Pattern dither;         // teal readout-box dither
  Material grain;         // page-background film grain (luminance, not RGB)
  Material spectrum, stripesLive, waterStreaks;

  // --- instancing: the footer dock's chevron tick array ---
  std::shared_ptr<instancing::Atlas> dockAtlas;
  std::shared_ptr<instancing::Pool> dockPool;

  int bootPct = 0;
  bool booted = false;

  // =========================================================================
  // Live materials (SkSL).

  /** The audio spectrum: a bar field whose heights are hashed per column
   *  per TIME STEP. uTime arrives quantized at 10 Hz (quantizeTime), so
   *  the readout is deliberately chunky — the era's digital feel, and the
   *  P3R sea rule applied to a waveform. */
  static sk_sp<SkRuntimeEffect> spectrumFx() {
    static const sk_sp<SkRuntimeEffect> fx = [] {
      auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
        uniform float2 uResolution;
        uniform float  uTime;
        uniform float  uBars;
        uniform float4 uHot;
        uniform float4 uCool;
        float h(float a, float b) {
          return fract(sin(a * 12.9898 + b * 78.233) * 43758.5453);
        }
        half4 main(float2 xy) {
          float bw = uResolution.x / uBars;
          float i  = floor(xy.x / bw);
          float fx = fract(xy.x / bw);
          float gy = step(fract(xy.y / (uResolution.y / 5.0)), 0.06);
          float4 acc = float4(0.012, 0.075, 0.082, 1.0);
          acc = mix(acc, float4(0.04, 0.19, 0.20, 1.0), gy);
          float gx = step(fract(xy.x / (bw * 4.0)), 0.03);
          acc = mix(acc, float4(0.04, 0.19, 0.20, 1.0), gx);
          if (fx > 0.72) return half4(half3(acc.rgb), 1.0);
          float k  = floor(uTime * 10.0);
          float n  = h(i, k);
          float n2 = h(i * 3.1 + 7.0, k * 0.7 + 5.0);
          float amp = 0.14 + 0.80 * n * (0.35 + 0.65 * n2);
          amp *= 0.55 + 0.55 * sin(i / uBars * 3.14159);
          float top = uResolution.y * (1.0 - clamp(amp, 0.0, 1.0));
          float on  = step(top, xy.y);
          float g = clamp((xy.y - top) / max(uResolution.y - top, 1.0),
                          0.0, 1.0);
          acc = mix(acc, mix(uHot, uCool, g), on);
          float cap = step(abs(xy.y - (top - 3.0)), 1.5);
          acc = mix(acc, float4(1.0, 1.0, 1.0, 1.0), cap * 0.7);
          return half4(half3(acc.rgb), 1.0);
        }
      )"));
      if (!e)
        SkDebugf("twoadvanced spectrum: %s\n", err.c_str());
      return e;
    }();
    return fx;
  }

  /** The diagonal hazard stripe as a LIVE material so every header bar can
   *  run its slow conveyor pan (§9: 20 px per 8 s) off ONE bound uniform.
   *  This exact value is reused by the nav bar and four panel headers. */
  static sk_sp<SkRuntimeEffect> stripeFx() {
    static const sk_sp<SkRuntimeEffect> fx = [] {
      auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
        uniform float  uPan;
        uniform float  uOn;
        uniform float  uPeriod;
        uniform float4 uColor;
        uniform float4 uBase;
        half4 main(float2 xy) {
          float d = (xy.x + xy.y) * 0.70710678 - uPan;
          float f = mod(d, uPeriod);
          float a = clamp(smoothstep(0.0, 1.2, uOn - f), 0.0, 1.0);
          float4 c = mix(uBase, uColor, a);
          return half4(half3(c.rgb), 1.0);
        }
      )"));
      if (!e)
        SkDebugf("twoadvanced stripe: %s\n", err.c_str());
      return e;
    }();
    return fx;
  }

  /** Luminance film grain. patterns::grain() is the library's own answer
   *  to this and is exactly right in principle (equal channels, so an
   *  overlay reads as LIGHT rather than hue-shifting the oxblood ramp) —
   *  but it segfaults the moment its material paints (reported), so this
   *  is the same idea hand-rolled: one value-noise octave pair, no
   *  uniform-bounded loop. */
  static sk_sp<SkRuntimeEffect> grainFx() {
    static const sk_sp<SkRuntimeEffect> fx = [] {
      auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
        uniform float uFreq;
        uniform float uSeed;
        float hash21(float2 p) {
          p = fract(p * float2(123.34, 456.21) + uSeed);
          p += dot(p, p + 45.32);
          return fract(p.x * p.y);
        }
        float vnoise(float2 p) {
          float2 i = floor(p);
          float2 f = fract(p);
          float2 u = f * f * (3.0 - 2.0 * f);
          return mix(mix(hash21(i), hash21(i + float2(1, 0)), u.x),
                     mix(hash21(i + float2(0, 1)),
                         hash21(i + float2(1, 1)), u.x), u.y);
        }
        half4 main(float2 pos) {
          float2 q = pos * uFreq;
          float v = 0.5 * vnoise(q) + 0.3 * vnoise(q * 2.0) +
                    0.2 * vnoise(q * 4.0);
          return half4(half3(v), 1.0);
        }
      )"));
      if (!e)
        SkDebugf("twoadvanced grain: %s\n", err.c_str());
      return e;
    }();
    return fx;
  }

  /** Horizontal streak water, dark teal-black, drifting slowly. */
  static sk_sp<SkRuntimeEffect> waterFx() {
    static const sk_sp<SkRuntimeEffect> fx = [] {
      auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
        uniform float2 uResolution;
        uniform float  uTime;
        half4 main(float2 xy) {
          float v = xy.y / max(uResolution.y, 1.0);
          float s = sin(xy.y * 0.85 + sin(xy.x * 0.011 + uTime * 0.45) * 2.4);
          float band = smoothstep(0.55, 1.0, s) * (0.22 + 0.78 * v);
          float3 base = mix(float3(0.000, 0.078, 0.082),
                            float3(0.004, 0.122, 0.129), v);
          float3 c = base + float3(0.02, 0.15, 0.16) * band;
          return half4(half3(c), 1.0);
        }
      )"));
      if (!e)
        SkDebugf("twoadvanced water: %s\n", err.c_str());
      return e;
    }();
    return fx;
  }

  // =========================================================================
  // Small parts.

  Element tickDots(int cluster, SkColor4f c = tav::kCyan) {
    Element r = box().row().gap(4).alignItems(Align::Center);
    for (int i = 0; i < 3; ++i)
      r.child(box().width(5).height(5).fill(c).opacity(
          &dot[(size_t)(cluster * 3 + i)]));
    return r;
  }

  /** The two-weight panel header: heavy half + regular half over the
   *  hazard-stripe ground, with a flavour line, a tick cluster and a tick
   *  rail. Four panels wear this identically. */
  Element panelHeader(const char *boldHalf, const char *restHalf,
                      const char *flavor, int cluster, float w) {
    using namespace tav;
    return box().width(Dim(w)).height(28).row().alignItems(Align::Center)
        .padding(10, 0)
        .fill(stripesLive)
        .foreground(shapes::onEdges(
            shapes::Edge::Bottom,
            util::stroke(1, Fill::color(fade(kCyan, 0.35f)),
                         PathFormat::Align::Inner)))
        .child(t(boldHalf, heavy(17, kNear, 40)))
        .child(t(restHalf, type(arial(), 15, kHeadDim, 40, 0.95f)))
        .child(box().width(12))
        .child(box().width(1).height(12).fill(fade(kCyan, 0.4f)))
        .child(box().width(10))
        .child(t(flavor, micro(11, fade(kDust, 1.0f), 260)))
        .child(box().grow(1))
        .child(tickDots(cluster))
        .child(box().width(8))
        .child(box().width(34).height(10).foreground(TickRail{
            fade(kCyan, 0.55f), 4, 3, 7, 1, 3, false, true}));
  }

  /** CTA: chamfered, blood-red ramp in UNIT space (the button's height is
   *  a layout outcome, not a number I wrote down), gloss band on top. */
  Element cta(const char *lbl, float w = 116, float h = 34,
              SkColor4f hairline = tav::kNear) {
    using namespace tav;
    return box().width(Dim(w)).height(Dim(h))
        .outline(chamfer(9, kTL | kBR))
        .fill(Material::linearUnit({0, 0}, {0, 1},
                                   {{0.0f, kCtaHi},
                                    {0.42f, kCta},
                                    {1.0f, C(0x3A0000)}}))
        .stroke(util::stroke(1, Fill::color(kChrome), PathFormat::Align::Outer))
        .foreground(styles::gloss(fade(kCtaHi, 0.55f), h * 0.30f,
                                  {0, -h * 0.26f}, 0.62f, 0.30f))
        .foreground(util::stroke(1, Fill::color(fade(hairline, 0.45f)),
                                 PathFormat::Align::Inner))
        .row().justify(Justify::Center).alignItems(Align::Center)
        .child(t(lbl, label(15, kNear, 110)));
  }

  /** A dark readout window: chamfered, inset-bevelled, bracketed. */
  Element readout(float w, float h, SkColor4f ground = tav::C(0x1B0708)) {
    using namespace tav;
    return box().width(Dim(w)).height(Dim(h))
        .outline(chamfer(7, kTR | kBL))
        .fill(ground)
        .foreground(InsetBevel{fade(kChromeHi, 0.6f), {0, 0, 0, 0.5f}, 0, 1, 1})
        .foreground(Brackets{fade(kCyan, 0.55f), 8, 2, 3, 0xF});
  }

  /** A radar wedge: shapes::sector, rotation BOUND. Every gauge on the
   *  page is this with a different bezel. */
  Element radarSweep(int i, SkColor4f tint, float inner = 0.30f) {
    return box().absolute().inset(0)
        .outline(shapes::sector(-100, 78, inner))
        .fill(Material::linearUnit({0, 0}, {1, 1},
                                   {{0.0f, tav::fade(tint, 0.85f)},
                                    {1.0f, tav::fade(tint, 0.05f)}}))
        .rotate(&gauge[(size_t)i])
        .opacity(&gaugeAlpha[(size_t)i]);
  }

  // =========================================================================
  // Regions.

  Element statusBar() {
    using namespace tav;
    // Two segments meeting on a DIAGONAL seam, not a vertical edge —
    // the teal leads, the maroon follows 80 ms later (§9 beat 5).
    Element teal =
        singleBevel(box().absolute().left(Dim(0)).top(Dim(0)).width(560)
                        .height(40)
                        .outline(chamfer(40, kBR))
                        .row().alignItems(Align::Center).padding(10, 0).gap(8),
                    kTealBar)
            .translateY(withFrom(-46.0f, 0.0f,
                                 {380ms, &ch::easeOutQuint, 1450ms}))
            .child(box().width(22).height(22).corners({5})
                       .fill(Material::radialUnit({0.5f, 0.42f}, 1.15f,
                                                  {{0.0f, kCyanRing},
                                                   {0.55f, kTealBar},
                                                   {1.0f, C(0x0C2A2C)}}))
                       .stroke(util::stroke(1, Fill::color(fade(kCyan, 0.7f)),
                                            PathFormat::Align::Inner))
                       .justify(Justify::Center).alignItems(Align::Center)
                       .child(box().width(9).height(9).corners({5})
                                  .stroke(util::stroke(
                                      2, Fill::color(kCyan)))))
            .child(t("SYSTEM ONLINE", micro(12, kNear, 240)))
            .child(box().width(1).height(14).fill(fade(kCyan, 0.4f)))
            .child(t("SECTOR 04 / PROPHECY", micro(12, fade(kCyan, 0.85f), 240)))
            .child(box().grow(1))
            .child(box().width(90).height(12).foreground(TickRail{
                fade(kNear, 0.45f), 6, 3, 8, 1, 3, false, true}))
            .child(box().width(46));

    Element maroon =
        singleBevel(box().absolute().left(Dim(548)).top(Dim(0))
                        .width(Dim(1892.0f - 548.0f)).height(40)
                        .outline(chamfer(40, kTL))
                        .row().alignItems(Align::Center)
                        .padding(58, 0, 14, 0).gap(10),
                    kChrome)
            .translateY(withFrom(-46.0f, 0.0f,
                                 {380ms, &ch::easeOutQuint, 1530ms}))
            .child(t("2ADVANCED STUDIOS", micro(12, kDust, 260)))
            .child(box().width(1).height(14).fill(fade(kDust, 0.4f)))
            .child(t("PROGRESSIVE DESIGN TECHNOLOGY", micro(12, kDustDim, 260)))
            .child(box().grow(1))
            .child(t("LAT 33.6189 N   LON 117.9298 W", micro(11, kDustDim, 200)))
            .child(box().width(1).height(14).fill(fade(kDust, 0.4f)))
            .child(t("V4.PROPHECY", heavy(14, kNear, 80)));

    return box().absolute().left(Dim(0)).top(Dim(0)).width(1892).height(40)
        .child(maroon)
        .child(teal);
  }

  Element audioModule() {
    using namespace tav;
    static const char *tracks[4] = {"01  DESERT TRANCE", "02  NEVERMIND",
                                    "03  SYNERGY", "04  EXHALE"};
    static const char *times[4] = {"4:12", "5:08", "3:41", "3:55"};
    Element list = box().column().width(268).gap(2);
    for (int i = 0; i < 4; ++i) {
      const bool sel = i == 2;
      list.child(
          box().height(23).row().alignItems(Align::Center).padding(6, 0).gap(6)
              .fill(sel ? kChromeHi : fade(C(0x2A0A0C), 0.85f))
              .foreground(shapes::onEdges(
                  shapes::Edge::Left,
                  util::stroke(2,
                               Fill::color(sel ? kCyan : fade(kDust, 0.35f)),
                               PathFormat::Align::Inner)))
              .child(t(sel ? "\xe2\x96\xb8" : " ", micro(11, kCyan, 0)))
              .child(t(tracks[i], type(blackFace(), 13,
                                       sel ? kNear : kHeadDim, 60, 0.92f)))
              .child(box().grow(1))
              .child(t(times[i], micro(11, sel ? kCyan : kDustDim, 120))));
    }

    Element scope =
        box().grow(1).height(100)
            .outline(chamfer(8, kTR | kBL))
            .fill(spectrum)
            .foreground(Scanlines{{0, 0, 0, 0.16f}, 3, 1})
            .foreground(Brackets{fade(kCyan, 0.6f), 10, 2, 3, 0xF})
            .foreground(util::stroke(1, Fill::color(fade(kCyan, 0.35f)),
                                     PathFormat::Align::Inner));

    auto key = [&](const char *glyph, bool hot) {
      return box().width(38).height(22)
          .outline(chamfer(6, kTL | kBR))
          .fill(Material::linearUnit({0, 0}, {0, 1},
                                     {{0.0f, hot ? kCtaHi : C(0x5A2226)},
                                      {0.5f, hot ? kCta : C(0x3A0F12)},
                                      {1.0f, C(0x240607)}}))
          .stroke(util::stroke(1, Fill::color(fade(kDust, 0.35f)),
                               PathFormat::Align::Inner))
          .justify(Justify::Center).alignItems(Align::Center)
          .child(t(glyph, micro(11, hot ? kNear : kDust, 0)));
    };
    auto meter = [&](float w, const ch::Output<float> *bind, SkColor4f c) {
      return box().width(Dim(w)).height(6).fill(C(0x1B0708))
          .child(box().absolute().left(Dim(0)).top(Dim(0)).width(Dim(w))
                     .height(6).fill(c).scale(bind).transformOrigin(0, 0.5f));
    };

    Element panel =
        singleBevel(box().width(596).height(174).column().padding(9).gap(6),
                    C(0x3E1013));
    panel.key("audio")
        .foreground(Brackets{kCyan, 18, 3, 4, kTL | kTR})
        .foreground(TickRail{fade(kDust, 0.45f), 7, 3, 6, 1, 4, false, true})
        .child(box().row().gap(8).height(100).child(list).child(scope))
        .child(box().row().gap(5).alignItems(Align::Center)
                   .child(key("\xe2\x97\x82\xe2\x97\x82", false))
                   .child(key("\xe2\x96\xa0", false))
                   .child(key("\xe2\x96\xb8", true))
                   .child(key("\xe2\x96\xb8\xe2\x96\xb8", false))
                   .child(box().width(8))
                   .child(meter(64, &vuLeft, fade(kCyan, 0.85f)))
                   .child(box().grow(1))
                   .child(t("VOL", micro(10, kDustDim, 200)))
                   .child(meter(56, &vuRight, fade(kCyanRing, 0.8f))))
        .child(box().height(18).row().alignItems(Align::Center).padding(6, 0)
                   .fill(fade(kChrome, 0.9f))
                   .child(t("AUDIO PREFERENCES", micro(11, kDust, 240)))
                   .child(box().grow(1))
                   .child(t("STREAM 128K \xc2\xb7 STEREO",
                            micro(10, kDustDim, 200))));
    return panel;
  }

  Element navBar() {
    using namespace tav;
    // The taxonomy verbatim from the SWF's strings.
    static const char *items[7] = {"COMPANY",   "SERVICES",     "PORTFOLIO",
                                   "ACCOLADES", "EXPERIMENTAL", "EQUIPMENT",
                                   "CONTACT"};
    Element bar =
        singleBevel(box().width(584).height(46).row()
                        .justify(Justify::SpaceEvenly).alignItems(Align::Center)
                        .padding(6, 0),
                    kChrome);
    bar.fill(stripesLive).staggerChildren(40ms); // §9 beat 7
    for (int i = 0; i < 7; ++i) {
      bar.child(box().column().alignItems(Align::Center).gap(3)
                    .translateY(withFrom(16.0f, 0.0f,
                                         {240ms, &ch::easeOutQuint, 2250ms}))
                    .opacity(withFrom(0.0f, 1.0f,
                                      {240ms, &ch::easeOutQuad, 2250ms}))
                    .child(t(items[i], label(13, i == 2 ? kCyan : kNear, 80)))
                    .child(box().width(i == 2 ? 22.0f : 8.0f).height(2)
                               .fill(fade(i == 2 ? kCyan : kDust, 0.75f))));
      if (i < 6)
        bar.child(box().width(1).height(20).fill(fade(C(0x2A0A0C), 0.9f)));
    }
    return bar;
  }

  /** The strip under the nav bar. The real index never left 584×150 of
   *  gradient showing: five chamfered section thumbnails over a dark
   *  readout ground, each with its own bracket set and index number. */
  Element quickLaunch() {
    using namespace tav;
    static const char *secs[5] = {"PORTFOLIO", "SERVICES", "ACCOLADES",
                                  "EXPERIMENTAL", "EQUIPMENT"};
    static const char *nums[5] = {"01", "02", "03", "04", "05"};
    Element chips = box().row().gap(6).height(64);
    for (int i = 0; i < 5; ++i) {
      const float g = 0.28f + 0.16f * (float)i;
      chips.child(
          box().grow(1).column().gap(3)
              .child(box().grow(1)
                         .outline(chamfer(8, kTL | kBR))
                         .fill(Material::linearUnit(
                             {0, 0}, {0, 1},
                             {{0.0f, C(0x06232A)}, {1.0f, C(0x01090B)}}))
                         .stroke(util::stroke(
                             1, Fill::color(fade(kCyan, 0.35f)),
                             PathFormat::Align::Inner))
                         .child(box().absolute().inset(0)
                                    .fill(Material::radialUnit(
                                        {0.5f, 0.92f}, 1.0f,
                                        {{0.0f, fade(kGlow, g)},
                                         {1.0f, fade(kGlow, 0.0f)}})))
                         .child(place(box().fill(C(0x010A0C)), 12, 16, 14, 26))
                         .child(place(box().fill(C(0x02171B)), 30, 8, 20, 34))
                         .child(place(box().fill(C(0x010A0C)), 54, 20, 16, 22))
                         .child(place(box().fill(fade(kGlow, 0.5f)), 0, 41,
                                      200, 1))
                         .foreground(Brackets{fade(kCyan, 0.6f), 7, 1, 2, 0xF})
                         .foreground(Scanlines{{0, 0, 0, 0.24f}, 3, 1})
                         .child(box().absolute().left(Dim(4)).top(Dim(3))
                                    .child(t(nums[i],
                                             micro(9, fade(kCyan, 0.9f), 140)))))
              .child(t(secs[i], micro(9, i == 0 ? kCyan : kDust, 200))));
    }

    Element panel = singleBevel(
        box().width(584).height(144).column().padding(8).gap(6), C(0x3E1013));
    panel.key("quick")
        .translateY(withFrom(40.0f, 0.0f, {380ms, &ch::easeOutQuint, 2450ms}))
        .opacity(withFrom(0.0f, 1.0f, {320ms, &ch::easeOutQuad, 2450ms}))
        .foreground(Brackets{fade(kCyan, 0.55f), 12, 2, 4, kBL | kBR})
        .child(box().row().alignItems(Align::Center).gap(8).height(16)
                   .child(t("QUICK", heavy(13, kNear, 40)))
                   .child(t(" LAUNCH", type(arial(), 12, kHeadDim, 40, 0.95f)))
                   .child(box().width(1).height(11).fill(fade(kCyan, 0.4f)))
                   .child(t("SELECT A SECTOR", micro(10, kDust, 260)))
                   .child(box().grow(1))
                   .child(tickDots(4))
                   .child(box().width(60).height(9).foreground(TickRail{
                       fade(kCyan, 0.5f), 5, 3, 7, 1, 4, false, true})))
        .child(chips)
        .child(box().height(16).row().alignItems(Align::Center).gap(8)
                   .padding(5, 0)
                   .fill(fade(kChrome, 0.85f))
                   .child(t("\xe2\x96\xb8 INDEX", micro(9, kCyan, 200)))
                   .child(box().grow(1).height(1).fill(fade(kDust, 0.3f)))
                   .child(t("05 SECTORS \xc2\xb7 42 PROJECTS",
                            micro(9, kDustDim, 200))));
    return panel;
  }

  Element masthead() {
    using namespace tav;
    Element emblem =
        box().width(78).height(78)
            .background(styles::OuterGlow{fade(kGlow, 0.45f), 14, 0})
            .fill(sdf::material(sdf::circle(),
                                {.fill = {0, 0, 0, 0},
                                 .borderWidth = 4,
                                 .borderColor = kCyan}))
            .justify(Justify::Center).alignItems(Align::Center)
            .child(box().width(50).height(50)
                       .outline(shapes::polygon(6, 0))
                       .stroke(util::stroke(
                           1, Fill::color(fade(kCyanRing, 0.75f))))
                       .justify(Justify::Center).alignItems(Align::Center)
                       .child(t("2", type(blackFace(), 32, kCyan, 0, 0.85f))));

    return box().width(700).height(182).column()
        .translateX(withFrom(320.0f, 0.0f, {420ms, &ch::easeOutQuint, 1850ms}))
        .opacity(withFrom(0.0f, 1.0f, {300ms, &ch::easeOutQuad, 1850ms}))
        .child(box().grow(1).row().alignItems(Align::Center)
                   .padding(26, 0, 8, 0).gap(18)
                   .child(emblem)
                   .child(box().column().gap(6)
                              .child(t("2ADVANCED STUDIOS",
                                       type(blackFace(), 25, kCyan, 80, 0.90f))
                                         .effect(styles::textGlow(
                                             fade(kGlow, 0.55f), 6)))
                              .child(t("PROGRESSIVE DESIGN TECHNOLOGY",
                                       micro(12, kDust, 240)))
                              .child(box().row().gap(6).alignItems(Align::Center)
                                         .child(box().width(30).height(1)
                                                    .fill(fade(kCyan, 0.5f)))
                                         .child(t("EST. 1999 \xc2\xb7 IRVINE CA",
                                                  micro(10, kDustDim, 200)))))
                   .child(box().grow(1))
                   .child(box().column().alignItems(Align::End).gap(4)
                              .child(t("BUILD 4.0.7", micro(10, kDustDim, 200)))
                              .child(t("FLASH 6 REQ.", micro(10, kDustDim, 200)))
                              .child(t("1024\xc3\x97" "768 MIN",
                                       micro(10, kDustDim, 200)))))
        // the glowing 2px cyan divider under the whole masthead panel
        .child(box().height(2).fill(kCyan)
                   .background(styles::OuterGlow{fade(kGlow, 0.55f), 10, 1}));
  }

  // ---- the hero -----------------------------------------------------------

  /** §10: composite ORDER is what sells it. sky → skyline → portal →
   *  ring → figures → water + reflection → horizon hairline.
   *  `still` builds the same scene with no live material and no entrance,
   *  so the bloom duplicate stays provably static and bakes ONCE. */
  Element heroScene(float w, float h, bool still) {
    using namespace tav;
    const float horizon = std::round(h * 0.66f);
    const float cx = w * 0.50f;

    Element scene = stack().width(Dim(w)).height(Dim(h)).clip();

    scene.child(box().absolute().inset(0).fill(Material::linearUnit(
        {0, 0}, {0, 0.66f},
        {{0.0f, C(0x02070A)}, {0.62f, C(0x03181D)}, {1.0f, C(0x073038)}})));

    // the portal's wide halo, behind everything — LOW alpha; the disc
    // itself is small and the exponential SDF glow does the reaching.
    scene.child(place(box().fill(Material::radialUnit(
                          {0.5f, 0.5f}, 1.0f,
                          {{0.00f, fade(kGlow, 0.34f)},
                           {0.30f, fade(kTealBar, 0.16f)},
                           {1.00f, fade(kTealBar, 0.0f)}})),
                      cx - 330, horizon - 420, 660, 620)
                    .blend(SkBlendMode::kPlus));

    // skyline: flat slabs, opacity + tint ramped by |x−cx| (cheap fog)
    static const float bx[13] = {30,  108, 186, 250, 320, 388, 446,
                                 692, 748, 812, 884, 962, 1050};
    static const float bw[13] = {62, 54, 48, 60, 50, 38, 52,
                                 44, 56, 58, 46, 70, 64};
    static const float bh[13] = {120, 168, 92,  146, 200, 110, 74,
                                 86,  158, 118, 190, 96,  140};
    for (int i = 0; i < 13; ++i) {
      const float d = std::abs(bx[i] + bw[i] * 0.5f - cx) / (w * 0.5f);
      const float o = 0.55f + 0.45f * d;
      const SkColor4f tint = {0.008f + 0.028f * (1 - d),
                              0.048f + 0.045f * (1 - d),
                              0.060f + 0.050f * (1 - d), 1.0f};
      Element slab =
          place(box().fill(tint), bx[i], horizon - bh[i], bw[i], bh[i])
              .opacity(o);
      slab.child(box().absolute().left(Dim(bw[i] * 0.25f))
                     .top(Dim(bh[i] * 0.22f)).width(3).height(3)
                     .fill(fade(kGlow, 0.55f)));
      slab.child(box().absolute().left(Dim(bw[i] * 0.62f))
                     .top(Dim(bh[i] * 0.48f)).width(3).height(3)
                     .fill(fade(kGlow, 0.38f)));
      scene.child(slab);
      if (bh[i] > 150)
        scene.child(place(box().fill(fade(tint, o)), bx[i] + bw[i] * 0.5f - 1,
                          horizon - bh[i] - 26, 2, 26));
    }

    // THE portal: one SDF circle. Its box must RESERVE sdf::pad() for the
    // glow — sdf::minBoxFor() is the only honest way to size it, since
    // pad eats into the half-size (a 300 box with glowRadius 54 leaves
    // almost no disc at all). 132 px of visible disc, reaching far.
    sdf::Style ps{.fill = fade(kGlow, 0.95f),
                  .borderWidth = 3,
                  .borderColor = {0.90f, 1.0f, 1.0f, 0.95f},
                  .glowRadius = 54,
                  .glowColor = fade(kGlow, 0.75f)};
    const float pbox = sdf::minBoxFor(ps, 132);
    Material pm = sdf::material(sdf::circle(), ps);
    if (!still)
      pm.uniform("uGlowR", &portalGlow); // ±8 % sine, period 4 s
    Element portal = place(box().fill(pm), cx - pbox * 0.5f,
                           horizon - 108 - pbox * 0.5f, pbox, pbox)
                         .blend(SkBlendMode::kPlus);
    if (!still)
      // the one deliberately bouncy beat: the power core kicking on
      portal.scale(withKeyframes<float>({{2400ms, 0.80f},
                                         {2820ms, 1.06f},
                                         {2980ms, 0.985f},
                                         {3060ms, 1.0f}}));
    scene.child(portal);

    // an orbital ring, trim-revealed with the panel
    Element ring = place(box().outline(shapes::arc(-125, 310))
                             .stroke(util::stroke(
                                 2, Fill::color(fade(kCyanRing, 0.6f)))),
                         cx - 118, horizon - 226, 236, 236);
    if (!still)
      ring.trim(0.0f,
                withFrom(0.0f, 1.0f, {700ms, &ch::easeOutQuint, 2600ms}));
    scene.child(ring);

    // three helmeted figures, backlit: dome silhouette + gloss rim-light
    const float fxs[3] = {cx - 205, cx, cx + 200};
    const float fss[3] = {0.82f, 1.0f, 0.78f};
    for (int i = 0; i < 3; ++i) {
      const float dw = 66 * fss[i], dh = 100 * fss[i];
      Element fig = place(box().outline([](SkSize s) {
                            SkPathBuilder b;
                            b.moveTo(0, s.height());
                            b.lineTo(0, s.width() * 0.5f);
                            b.arcTo(SkRect::MakeWH(s.width(), s.width()), 180,
                                    180, false);
                            b.lineTo(s.width(), s.height());
                            b.close();
                            return b.detach();
                          })
                              .fill(C(0x01080A)),
                          fxs[i] - dw * 0.5f, horizon - dh + 14, dw, dh);
      fig.foreground(styles::gloss(fade(kCyanRing, 0.95f), dw * 0.09f,
                                   {0, -dh * 0.15f}, 0.28f, 0.20f));
      fig.foreground(shapes::onEdges(
          shapes::Edge::Top,
          util::stroke(1.5f, Fill::color(fade(kGlow, 0.85f)),
                       PathFormat::Align::Inner)));
      fig.child(box().absolute().left(Dim(dw * 0.22f)).top(Dim(dh * 0.30f))
                    .width(Dim(dw * 0.56f)).height(3)
                    .fill(fade(kGlow, 0.55f)));
      scene.child(fig);
    }

    // water: streaks + a mirrored, blurred copy of the portal glow
    Element water = place(box().clip(), 0, horizon, w, h - horizon);
    if (still)
      water.fill(Material::linearUnit({0, 0}, {0, 1},
                                      {{0.0f, C(0x001415)},
                                       {1.0f, C(0x011F21)}}));
    else
      water.fill(waterStreaks);
    water.child(box().absolute().left(Dim(cx - 190)).top(Dim(-72))
                    .width(380).height(300)
                    .fill(Material::radialUnit({0.5f, 0.14f}, 1.05f,
                                               {{0.0f, fade(kGlow, 0.75f)},
                                                {0.45f, fade(kTealBar, 0.32f)},
                                                {1.0f, fade(kTealBar, 0.0f)}}))
                    .effect(Effect::filter(
                        SkImageFilters::Blur(14, 26, nullptr)))
                    .opacity(0.45f)
                    .blend(SkBlendMode::kPlus));
    scene.child(water);

    // THE horizon hairline — §10 is right that this sells the reflection
    // more than the blur does.
    scene.child(place(box().fill(fade(kGlow, 0.62f)), 0, horizon - 1, w, 2));
    scene.child(place(box().fill(fade(kCyanRing, 0.16f)), 0, horizon + 3, w, 1));
    return scene;
  }

  Element hero(float w, float h) {
    using namespace tav;
    Element s = stack().width(Dim(w)).height(Dim(h)).clip();
    s.child(heroScene(w, h, false));
    // the atmospheric bloom pass: the same composite, blurred, screened
    // back over itself. Built `still` so it is provably static and the
    // Texture bake is paid once, not per frame.
    s.child(heroScene(w, h, true)
                .effect(Effect::filter(SkImageFilters::Blur(22, 22, nullptr)))
                .opacity(0.34f)
                .blend(SkBlendMode::kPlus)
                .cache(Cache::Texture)
                .bakeScale(0.5f));
    s.child(box().absolute().inset(0).fill(Material::radialUnit(
        {0.5f, 0.5f}, 1.0f,
        {{0.00f, {0, 0, 0, 0}},
         {0.58f, {0, 0, 0, 0.10f}},
         {1.00f, {0, 0, 0, 0.66f}}})));
    s.child(box().absolute().inset(0).foreground(Scanlines{}));
    s.child(box().absolute().inset(0)
                .foreground(Brackets{fade(kCyan, 0.7f), 22, 2, 8, 0xF}));

    auto corner = [&](const char *a, const char *b, float l, float tp,
                      bool end) {
      return box().absolute().column().gap(2)
          .alignItems(end ? Align::End : Align::Start)
          .left(Dim(l)).top(Dim(tp))
          .child(t(a, micro(10, fade(kCyan, 0.8f), 220)))
          .child(t(b, micro(10, fade(kCyanRing, 0.45f), 220)));
    };
    s.child(corner("REND / MAXON C4D R8", "PASS 04 \xc2\xb7 FRM 0142", 20, 18,
                   false));
    s.child(corner("38.2144 N", "121.4944 W", w - 132, 18, true));
    s.child(corner("DEPTH 00.42", "PRESS 1013 HPA", 20, h - 42, false));
    s.child(box().absolute().left(Dim(w - 214)).top(Dim(h - 32))
                .row().gap(6).alignItems(Align::Center)
                .child(box().width(120).height(8).foreground(TickRail{
                    fade(kCyan, 0.6f), 6, 3, 8, 1, 4, false, false}))
                .child(t("SIG 88%", micro(10, fade(kCyan, 0.85f), 200))));
    return s;
  }

  Element mainframe() {
    using namespace tav;
    Element panel = doubleBevel(
        box().width(1180).height(350).column().padding(3), kChrome, 3);
    panel.key("mainframe")
        .translateY(withFrom(70.0f, 0.0f, {520ms, &ch::easeOutQuint, 2400ms}))
        .opacity(withFrom(0.0f, 1.0f, {300ms, &ch::easeOutQuad, 2400ms}))
        .child(panelHeader("MAIN", "FRAME", "PRIMARY VISUAL FEED", 0, 1174))
        .child(box().grow(1).clip().child(hero(1174, 316)));
    return panel;
  }

  // ---- teal monitor panels ------------------------------------------------

  Element monitorBody(float w, float h) {
    using namespace tav;
    return box().width(Dim(w)).height(Dim(h))
        .fill(Material::linearUnit({0, 0}, {0, 1},
                                   {{0.00f, kPanelHi},
                                    {0.15f, kPanel},
                                    {0.88f, kPanel},
                                    {1.00f, kPanelSh}}))
        .foreground(styles::gloss(fade(kPanelHi, 0.5f), 40, {0, -h * 0.34f},
                                  0.72f, 0.28f))
        .foreground(shapes::onEdges(
            shapes::Edge::Top,
            util::stroke(1, Fill::color(fade(C(0xCFEFEC), 0.7f)),
                         PathFormat::Align::Inner)));
  }

  /** A label-over-value pair on a teal panel — the tabular voice. */
  Element specPair(const char *k, const char *v) {
    using namespace tav;
    return box().column().gap(2)
        .child(t(k, micro(9, fade(C(0x123B3D), 0.75f), 260)))
        .child(box().height(1).fill(fade(kDate, 0.28f)))
        .child(t(v, type(blackFace(), 11, C(0x0E3234), 40, 0.92f)));
  }

  Element featureSystem() {
    using namespace tav;
    Element thumb =
        box().width(150).height(150)
            .outline(chamfer(12, kTL | kBR))
            .fill(Material::linearUnit({0, 0}, {0, 1},
                                       {{0.0f, C(0x06232A)},
                                        {1.0f, C(0x011114)}}))
            .stroke(util::stroke(1, Fill::color(fade(C(0x0B3B40), 0.9f)),
                                 PathFormat::Align::Inner))
            .child(box().absolute().inset(0).fill(Material::radialUnit(
                {0.5f, 0.72f}, 0.95f,
                {{0.0f, fade(kGlow, 0.8f)},
                 {0.5f, fade(kTealBar, 0.28f)},
                 {1.0f, fade(kTealBar, 0.0f)}})))
            .child(place(box().fill(C(0x010A0C)), 18, 74, 30, 60))
            .child(place(box().fill(C(0x02171B)), 52, 46, 44, 88))
            .child(place(box().fill(C(0x010A0C)), 100, 62, 34, 72))
            .child(place(box().fill(fade(kGlow, 0.6f)), 0, 108, 150, 1))
            .foreground(Brackets{fade(kCyan, 0.85f), 12, 2, 4, 0xF})
            .foreground(Scanlines{{0, 0, 0, 0.22f}, 3, 1});

    Element copy =
        box().grow(1).column().gap(6)
            .child(box().row().gap(7).alignItems(Align::Center)
                       .child(box().width(9).height(9)
                                  .outline(shapes::polygon(3, 90))
                                  .fill(kDate))
                       .child(t("01.30.06",
                                type(blackFace(), 14, kDate, 40, 0.95f)))
                       .child(box().grow(1).height(1).fill(fade(kDate, 0.35f))))
            .child(t("N.O.-XPLODE TV COMMERCIAL",
                     type(blackFace(), 17, C(0x0E3234), 40, 0.92f)))
            .child(box().grow(1).padding(9)
                       .fill(dither.material())
                       .foreground(util::stroke(
                           1, Fill::color(fade(kPanelSh, 0.9f)),
                           PathFormat::Align::Inner))
                       // the ONE place this interface is not tracked caps
                       .child(t("2Advanced completes a broadcast spot for "
                                "BSN's N.O.-Xplode line \xe2\x80\x94 full CG "
                                "environment, character rig and compositing, "
                                "delivered in nine weeks on a Maxon pipeline "
                                "against a live-action plate.",
                                prose(13, C(0x0B2C2E)))))
            // the spec readout: dense, tabular, and never actually read
            .child(box().row().gap(14)
                       .child(specPair("CLIENT", "BSN / N.O.-XPLODE"))
                       .child(specPair("RUNTIME", "00:30 \xc2\xb7 NTSC"))
                       .child(specPair("TOOLS", "C4D R8 / AE 6.5"))
                       .child(specPair("DELIVERED", "01.24.06")))
            .child(box().row().gap(8).alignItems(Align::Center)
                       .child(t("\xe2\x80\xba VIEW CASE STUDY",
                                micro(11, C(0x123B3D), 220)))
                       .child(box().grow(1))
                       .child(box().width(150).height(6)
                                  .fill(fade(kPanelSh, 0.7f))
                                  .child(box().absolute().left(Dim(0))
                                             .top(Dim(0)).width(112).height(6)
                                             .fill(C(0x0E3234))))
                       .child(t("74%", micro(10, C(0x123B3D), 160))));

    Element bodyArea =
        monitorBody(690, 316).row().padding(11).gap(11)
            .child(thumb)
            .child(copy);
    // the hazard wedge, bottom-left — the STATIC baked-tile pattern path
    bodyArea.child(place(box().outline([](SkSize s) {
                           SkPathBuilder b;
                           b.moveTo(0, 0);
                           b.lineTo(s.width(), s.height());
                           b.lineTo(0, s.height());
                           b.close();
                           return b.detach();
                         })
                             .fill(hazard.material())
                             .opacity(0.45f),
                         0, 316 - 46, 150, 46));
    bodyArea.child(box().absolute().left(Dim(690 - 11 - 116))
                       .top(Dim(316 - 11 - 34))
                       .child(cta("LAUNCH", 116, 34, kPanelSh)));

    Element panel = doubleBevel(
        box().width(696).height(350).column().padding(3), kChrome, 3);
    panel.key("feature")
        .translateX(withFrom(90.0f, 0.0f, {500ms, &ch::easeOutQuint, 2600ms}))
        .opacity(withFrom(0.0f, 1.0f, {300ms, &ch::easeOutQuad, 2600ms}))
        .child(panelHeader("FEATURE", " SYSTEM", "LATEST TRANSMISSION", 1, 690))
        .child(bodyArea);
    return panel;
  }

  Element pressUpdates() {
    using namespace tav;
    struct Entry {
      const char *date, *headline, *body;
    };
    static const Entry entries[6] = {
        {"+ 04.05.06", "PROPHECY PRIME SKIN RELEASED",
         "The v4 desktop suite ships with the new application skin, the "
         "wallpaper set and the FIBERGLASS icon pack."},
        {"+ 03.11.06", "NAMED FWA SITE OF THE MONTH",
         "Prophecy takes the month for interface design and sound "
         "integration."},
        {"+ 01.30.06", "N.O.-XPLODE SPOT NOW AIRING",
         "Nine weeks, a full CG environment, delivered on a Maxon pipeline."},
        {"+ 01.07.06", "EQUIPMENT PAGE UPDATED",
         "New render nodes online; the studio moves to a dual-Xeon farm."},
        {"+ 12.14.05", "EXPERIMENTAL SECTION REOPENS",
         "Six new motion studies posted under the experimental banner."},
        {"+ 11.02.05", "HOLIDAY DESKTOP SET",
         "Three widescreen wallpapers in the Prophecy palette."},
    };

    Element list = box().column().gap(9).translateY(&pressScroll);
    for (const Entry &e : entries)
      list.child(
          box().column().gap(4)
              .child(box().row().gap(7).alignItems(Align::Center)
                         .fill(fade(kPanelSh, 0.55f)).padding(6, 3)
                         .child(t(e.date,
                                  type(blackFace(), 13, kDate, 40, 0.95f)))
                         .child(box().grow(1).height(1)
                                    .fill(fade(kDate, 0.3f)))
                         .child(t("\xe2\x96\xb8", micro(9, kDate, 0))))
              .child(t(e.headline,
                       type(blackFace(), 13, C(0x0E3234), 50, 0.92f)))
              .child(t(e.body, prose(12.5f, C(0x0C2E30)))));

    Element scrollbar =
        box().width(16).column().gap(3)
            .child(box().width(16).height(16).fill(kPanelSh)
                       .justify(Justify::Center).alignItems(Align::Center)
                       .child(t("\xe2\x96\xb4", micro(8, kBody, 0))))
            .child(box().grow(1).fill(fade(kPanelSh, 0.6f))
                       .child(box().absolute().left(Dim(2)).top(Dim(6))
                                  .width(12).height(90)
                                  .fill(Material::linearUnit(
                                      {0, 0}, {1, 0},
                                      {{0.0f, C(0xCFEFEC)}, {1.0f, kPanelHi}}))
                                  .stroke(util::stroke(
                                      1, Fill::color(fade(kDate, 0.4f)),
                                      PathFormat::Align::Inner))))
            .child(box().width(16).height(16).fill(kPanelSh)
                       .justify(Justify::Center).alignItems(Align::Center)
                       .child(t("\xe2\x96\xbe", micro(8, kBody, 0))));

    Element bodyArea =
        monitorBody(690, 376).column().padding(11).gap(9)
            .child(box().grow(1).row().gap(8)
                       .child(box().grow(1).clip().padding(9)
                                  .fill(dither.material())
                                  .foreground(util::stroke(
                                      1, Fill::color(fade(kPanelSh, 0.9f)),
                                      PathFormat::Align::Inner))
                                  .child(list))
                       .child(scrollbar))
            .child(box().row().alignItems(Align::Center).gap(8)
                       .child(t("06 ENTRIES \xc2\xb7 PAGE 1/4",
                                micro(11, C(0x123B3D), 220)))
                       .child(box().grow(1))
                       .child(cta("ARCHIVES", 116, 34, kPanelSh)));

    Element panel = doubleBevel(
        box().width(696).height(410).column().padding(3), kChrome, 3);
    panel.key("press")
        .translateY(withFrom(60.0f, 0.0f, {420ms, &ch::easeOutQuint, 3250ms}))
        .opacity(withFrom(0.0f, 1.0f, {300ms, &ch::easeOutQuad, 3250ms}))
        .child(panelHeader("PRESS", " UPDATES", "STUDIO WIRE", 2, 690))
        .child(bodyArea);
    return panel;
  }

  Element auxiliary() {
    using namespace tav;
    struct Col {
      const char *icon, *title, *l1, *l2, *link;
    };
    static const Col cols[3] = {
        {"\xe2\x96\xa0", "BROADCAST", "Live studio feed, weeknights at",
         "22:00 PST on the Prophecy channel.", "\xe2\x80\xba OFFLINE"},
        {"\xe2\x96\xb2", "DISPATCH", "Monthly transmission to 41,208",
         "subscribers. No spam, ever.", "\xe2\x80\xba SUBSCRIBE"},
        {"\xe2\x97\x86", "DESKTOPS", "Fourteen widescreen wallpapers",
         "in the Prophecy palette.", "\xe2\x80\xba VIEW"},
    };
    Element row = box().width(1180).height(154).row().gap(10).padding(3);
    for (int i = 0; i < 3; ++i) {
      Element col =
          singleBevel(box().grow(1).column().padding(9).gap(5), C(0x3E1013));
      col.foreground(TickRail{fade(kDust, 0.35f), 7, 3, 6, 1, 4, false, true})
          .child(box().row().gap(8).alignItems(Align::Center)
                     .child(box().width(32).height(32)
                                .outline(chamfer(8, kTL | kBR))
                                .fill(Material::linearUnit(
                                    {0, 0}, {0, 1},
                                    {{0.0f, C(0x5A1A20)}, {1.0f, C(0x220608)}}))
                                .stroke(util::stroke(
                                    1, Fill::color(kCta),
                                    PathFormat::Align::Inner))
                                .justify(Justify::Center)
                                .alignItems(Align::Center)
                                .child(t(cols[i].icon, micro(12, kCyan, 0))))
                     .child(box().column().gap(2)
                                .child(t(cols[i].title, heavy(15, kNear, 60)))
                                .child(t("AUXILIARY MODULE",
                                         micro(9, kDustDim, 240))))
                     .child(box().grow(1))
                     .child(tickDots(3 + i, fade(kCyan, 0.9f))))
          .child(box().column().gap(1)
                     .child(t(cols[i].l1, prose(12.5f, kBody)))
                     .child(t(cols[i].l2, prose(12.5f, fade(kBody, 0.7f)))))
          .child(box().grow(1))
          .child(box().row().alignItems(Align::Center).gap(8)
                     .child(t(cols[i].link, micro(11, fade(kCyan, 0.9f), 220)))
                     .child(box().grow(1))
                     .child(box().width(120).height(24)
                                .outline(chamfer(7, kTL | kBR))
                                .fill(Material::linearUnit(
                                    {0, 0}, {0, 1},
                                    {{0.0f, kPanelHi},
                                     {0.5f, kPanel},
                                     {1.0f, kPanelSh}}))
                                .foreground(styles::gloss(
                                    {1, 1, 1, 0.55f}, 8, {0, -7}, 0.60f, 0.30f))
                                .stroke(util::stroke(
                                    1, Fill::color(fade(C(0xCFEFEC), 0.6f)),
                                    PathFormat::Align::Inner))
                                .justify(Justify::Center)
                                .alignItems(Align::Center)
                                .child(t("VIEW", label(12, kDate, 140)))));
      row.child(col);
    }
    row.key("aux")
        .translateY(withFrom(56.0f, 0.0f, {400ms, &ch::easeOutQuint, 3100ms}))
        .opacity(withFrom(0.0f, 1.0f, {300ms, &ch::easeOutQuad, 3100ms}));
    return row;
  }

  /** The band the FWA screenshot leaves under AUXILIARY on the left
   *  column. A 2Advanced page never left 300 px of gradient showing. */
  Element transmissionLog() {
    using namespace tav;
    Element bars = box().row().gap(3).alignItems(Align::End).height(74);
    for (int i = 0; i < 34; ++i) {
      const float v = 0.18f + 0.80f * std::abs(std::sin(i * 0.73f) *
                                               std::cos(i * 0.31f + 1.1f));
      bars.child(box().width(7).height(Dim(74 * v)).shrink(0)
                     .fill(Material::linearUnit({0, 0}, {0, 1},
                                                {{0.0f, fade(kCyan, 0.95f)},
                                                 {1.0f, fade(kTealBar, 0.3f)}})));
    }

    static const char *hexLines[7] = {
        "0x0000  4A 10 0F 57 11 19 6A 1B  21 7B DA D6 95 C9 CC 8D",
        "0x0010  77 77 2C 7B 80 01 D0 D5  57 97 97 84 B8 B6 3C 82",
        "0x0020  82 70 00 00 B2 7E 82 F3  F3 F3 18 07 07 26 09 09",
        "0x0030  37 0C 0D 40 0E 0F 4C 10  10 2A 00 00 1A 00 01 0A",
        "0x0040  46 4C 41 53 48 49 4E 44  45 58 2E 53 57 46 00 00",
        "0x0050  43 57 53 06 EC 45 08 00  78 9C 00 00 00 00 00 00",
        "0x0060  50 52 4F 50 48 45 43 59  20 50 52 49 4D 45 00 00",
    };
    Element hexRows = box().column().gap(2);
    for (int i = 0; i < 7; ++i)
      hexRows.child(t(hexLines[i],
                      type(arial(), 11,
                           fade(kCyanRing, i < 5 ? 0.6f : 0.32f), 60)));

    Element gauges = box().row().gap(10).alignItems(Align::Center);
    for (int i = 0; i < 3; ++i)
      gauges.child(
          box().width(58).height(58)
              .fill(sdf::material(sdf::circle(),
                                  {.fill = C(0x140505),
                                   .borderWidth = 2,
                                   .borderColor = kChromeHi}))
              .justify(Justify::Center).alignItems(Align::Center)
              .child(radarSweep(i, kCyan, 0.34f))
              .child(t(i == 0 ? "A" : (i == 1 ? "B" : "C"),
                       micro(11, fade(kCyan, 0.85f), 120))));

    Element panel = singleBevel(
        box().width(1180).height(236).column().padding(9).gap(7), C(0x3E1013));
    panel.key("txlog")
        .translateY(withFrom(56.0f, 0.0f, {400ms, &ch::easeOutQuint, 3400ms}))
        .opacity(withFrom(0.0f, 1.0f, {320ms, &ch::easeOutQuad, 3400ms}))
        .foreground(Brackets{fade(kCyan, 0.5f), 14, 2, 5, kTR | kBL})
        .child(box().row().alignItems(Align::Center).gap(9).height(20)
                   .child(t("TRANSMISSION", heavy(15, kNear, 40)))
                   .child(t(" LOG", type(arial(), 14, kHeadDim, 40, 0.95f)))
                   .child(box().width(1).height(12).fill(fade(kCyan, 0.4f)))
                   .child(t("BUFFER 04 / 16 \xc2\xb7 UPLINK NOMINAL",
                            micro(11, kDust, 240)))
                   .child(box().grow(1))
                   .child(tickDots(6))
                   .child(box().width(120).height(10).foreground(TickRail{
                       fade(kCyan, 0.5f), 5, 3, 8, 1, 4, false, true})))
        .child(box().row().gap(10).grow(1)
                   .child(box().width(380).padding(7).clip()
                              .fill(C(0x180505))
                              .foreground(InsetBevel{fade(kChromeHi, 0.5f),
                                                     {0, 0, 0, 0.5f}, 0, 1, 1})
                              .foreground(Brackets{fade(kCyan, 0.4f), 9, 2, 3,
                                                   0xF})
                              .child(bars))
                   .child(box().grow(1).padding(8)
                              .fill(C(0x140404))
                              .foreground(InsetBevel{fade(kChromeHi, 0.5f),
                                                     {0, 0, 0, 0.5f}, 0, 1, 1})
                              .child(hexRows))
                   .child(box().column().gap(6).alignItems(Align::Center)
                              .padding(9, 6)
                              .fill(C(0x1B0607))
                              .foreground(InsetBevel{fade(kChromeHi, 0.5f),
                                                     {0, 0, 0, 0.5f}, 0, 1, 1})
                              .child(gauges)
                              .child(t("EQUIPMENT STATUS",
                                       micro(10, kDustDim, 220)))));
    return panel;
  }

  Element subSystem() {
    using namespace tav;
    auto chip = [&](const char *glyph) {
      return box().width(40).height(40).corners({20})
          .fill(Material::linearUnit({0, 0}, {0, 1},
                                     {{0.0f, C(0x6A1B21)},
                                      {1.0f, C(0x220608)}}))
          .stroke(util::stroke(1, Fill::color(fade(kDust, 0.5f)),
                               PathFormat::Align::Inner))
          .justify(Justify::Center).alignItems(Align::Center)
          .child(t(glyph, heavy(15, kCyan, 0)));
    };
    auto selector = [&](const char *lbl, const char *value) {
      return box().row().gap(8).alignItems(Align::Center)
          .child(box().width(46).height(34)
                     .outline(chamfer(8, kTL | kBR))
                     .fill(Material::radialUnit({0.5f, 0.76f}, 1.1f,
                                                {{0.0f, C(0x0A4148)},
                                                 {1.0f, C(0x010D10)}}))
                     .stroke(util::stroke(1, Fill::color(fade(kCyan, 0.5f)),
                                          PathFormat::Align::Inner)))
          .child(box().column().gap(1)
                     .child(t(lbl, micro(10, kDustDim, 240)))
                     .child(box().row().gap(5).alignItems(Align::Center)
                                .child(t("\xe2\x96\xb8", micro(9, kCyan, 0)))
                                .child(t(value, label(13, kNear, 90)))
                                .child(t("\xe2\x96\xbe", micro(9, kDust, 0)))));
    };

    Element row =
        singleBevel(box().width(1892).height(100).row()
                        .alignItems(Align::Center).padding(14, 0).gap(18),
                    C(0x2E0B0D));
    row.key("subsys")
        .background(styles::Overlay{hazard.material(), SkBlendMode::kSrcOver,
                                    0.16f})
        .opacity(withFrom(0.0f, 1.0f, {400ms, &ch::easeOutQuad, 3650ms}))
        .foreground(TickRail{fade(kDust, 0.35f), 9, 4, 8, 1, 4, false, false})
        .child(t("SUB", heavy(15, kNear, 40)))
        .child(t("SYSTEM", type(arial(), 14, kHeadDim, 40, 0.95f)))
        .child(box().width(1).height(30).fill(fade(kDust, 0.35f)))
        .child(t("PARTNERS:", micro(11, kDust, 240)))
        .child(chip("A"))
        .child(chip("M"))
        .child(box().width(1).height(30).fill(fade(kDust, 0.35f)))
        .child(selector("DESKTOPS", "'FIBERGLASS'"))
        .child(box().width(1).height(30).fill(fade(kDust, 0.35f)))
        .child(selector("APP SKINS", "'PROPHECY PRIME'"))
        .child(box().grow(1))
        .child(box().column().alignItems(Align::End).gap(3)
                   .child(t("BANDWIDTH  \xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa0"
                            "\xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa1\xe2\x96\xa1",
                            micro(11, fade(kCyan, 0.85f), 200)))
                   .child(t("UPTIME 118:24:07", micro(10, kDustDim, 200))))
        .child(box().width(70).height(40).foreground(TickRail{
            fade(kCyan, 0.45f), 6, 4, 10, 1, 3, false, true}));
    return row;
  }

  Element legalStrip() {
    using namespace tav;
    return box().width(1892).height(90).column().padding(6, 8)
        .alignItems(Align::Center).gap(4)
        .key("legal")
        .opacity(withFrom(0.0f, 1.0f, {400ms, &ch::easeOutQuad, 3750ms}))
        .child(box().alignSelf(Align::Stretch).row().alignItems(Align::Center)
                   .child(box().row().gap(6).alignItems(Align::Center)
                              .child(box().width(60).height(1)
                                         .fill(fade(kDust, 0.35f)))
                              .child(t("SITE REQUIRES MACROMEDIA FLASH "
                                       "PLAYER 6",
                                       micro(10, kDustDim, 200))))
                   .child(box().grow(1))
                   .child(box().row().gap(7).alignItems(Align::Center)
                              .child(t("ARCHIVED VERSIONS:",
                                       micro(11, kDust, 240)))
                              .child(box().height(24).padding(8, 0)
                                         .outline(chamfer(7, kTL | kBR))
                                         .fill(C(0x2A0A0C))
                                         .stroke(util::stroke(
                                             1,
                                             Fill::color(fade(kDust, 0.45f)),
                                             PathFormat::Align::Inner))
                                         .row().gap(6)
                                         .alignItems(Align::Center)
                                         .child(t("\xe2\x96\xb8",
                                                  micro(9, kCyan, 0)))
                                         .child(t("V3 'EXPANSIONS'",
                                                  label(12, kNear, 90)))
                                         .child(t("\xe2\x96\xbe",
                                                  micro(9, kDust, 0))))))
        .child(box().grow(1))
        .child(t("COPYRIGHT (C) 2003 2ADVANCED STUDIOS, LLC.  ALL RIGHTS "
                 "RESERVED.",
                 micro(12, kDust, 240)))
        .child(box().row().gap(10).alignItems(Align::Center)
                   .child(t("LEGAL", micro(11, kDustDim, 200)))
                   .child(box().width(1).height(10).fill(fade(kDust, 0.35f)))
                   .child(t("PRIVACY POLICY", micro(11, kDustDim, 200)))
                   .child(box().width(1).height(10).fill(fade(kDust, 0.35f)))
                   .child(t("SITE MAP", micro(11, kDustDim, 200))));
  }

  /** The 970×110 sitefooter.gif, ×2 — deliberately MONOCHROME oxblood,
   *  no accent colour anywhere (the real asset saves colour for the SWF's
   *  live states). Crosshatch dither, bracketed windows, chevrons, and the
   *  three-circle gauge cluster in its dark bezel. */
  Element footerDock() {
    using namespace tav;
    Element strip =
        box().width(1892).height(220)
            .fill(Material::blend(
                {{Material::linearUnit({0, 0}, {0, 1},
                                       {{0.0f, kD5}, {0.45f, kD2},
                                        {1.0f, kD1}}),
                  SkBlendMode::kSrcOver},
                 {hatchA.material(), SkBlendMode::kSrcOver},
                 {hatchB.material(), SkBlendMode::kSrcOver}}))
            .row().alignItems(Align::Center).padding(14, 12).gap(12)
            .key("dock")
            .opacity(withFrom(0.0f, 1.0f, {400ms, &ch::easeOutQuad, 3850ms}))
            .foreground(shapes::onEdges(
                shapes::Edge::Top,
                util::stroke(2, Fill::color(kD5), PathFormat::Align::Inner)));

    auto window = [&](const char *title, const char *a, const char *b,
                      float w) {
      return readout(w, 150, C(0x140404))
          .column().padding(10).gap(5)
          .child(box().row().alignItems(Align::Center).gap(6)
                     .child(t(title, type(blackFace(), 12, kD5, 60, 0.92f)))
                     .child(box().grow(1).height(1).fill(kD4))
                     .child(t("\xc2\xbb", micro(11, kD5, 0))))
          .child(t(a, micro(10, fade(kD5, 0.95f), 220)))
          .child(t(b, micro(10, kD4, 220)))
          .child(box().grow(1))
          .child(box().row().gap(5).alignItems(Align::Center)
                     .child(box().width(58).height(8).foreground(TickRail{
                         kD5, 5, 3, 7, 1, 3, false, false}))
                     .child(box().grow(1))
                     .child(t("v v", micro(10, kD4, 200))));
    };

    strip.child(window("NAVIGATION", "SECTOR / PROPHECY", "NODE 04.11.22", 300));
    strip.child(window("EQUIPMENT", "RENDER FARM  08/08", "STORAGE  4.2 TB",
                       300));
    strip.child(window("DISPATCH", "QUEUE  00114", "LAST  04.05.06", 280));

    // the instanced chevron array — one atlas cell, one stamp
    strip.child(box().width(260).height(150)
                    .outline(chamfer(7, kTR | kBL))
                    .fill(C(0x110303))
                    .foreground(InsetBevel{kD5, {0, 0, 0, 0.6f}, 0, 1, 1})
                    .child(box().absolute().left(Dim(12)).top(Dim(12))
                               .width(236).height(96)
                               .child(instancing::instances(
                                   dockAtlas, dockPool,
                                   instancing::Mode::Data)))
                    .child(box().absolute().left(Dim(12)).top(Dim(122))
                               .child(t("ARRAY 6\xc3\x97" "14 \xc2\xb7 IDLE",
                                        micro(10, fade(kD5, 0.95f), 220)))));

    strip.child(box().grow(1));

    Element cluster =
        box().width(310).height(150)
            .outline(chamfer(9, kTL | kBR))
            .fill(Material::linearUnit({0, 0}, {0, 1},
                                       {{0.0f, kD3}, {1.0f, C(0x0C0202)}}))
            .foreground(InsetBevel{kD5, {0, 0, 0, 0.6f}, 5, 2, 1})
            .foreground(Brackets{kD5, 12, 2, 5, 0xF})
            .row().justify(Justify::Center).alignItems(Align::Center).gap(14);
    for (int i = 0; i < 3; ++i)
      cluster.child(
          box().width(80).height(80)
              .fill(sdf::material(sdf::circle(),
                                  {.fill = C(0x0A0202),
                                   .borderWidth = 3,
                                   .borderColor = kD5}))
              .justify(Justify::Center).alignItems(Align::Center)
              .child(radarSweep(i, C(0x8A2A2C), 0.42f))
              .child(box().absolute().inset(26).corners({16})
                         .fill(fade(kD1, 0.92f))
                         .stroke(util::stroke(1, Fill::color(kD4),
                                              PathFormat::Align::Inner)))
              .child(t(i == 0 ? "01" : (i == 1 ? "02" : "03"),
                       micro(11, kD5, 140))));
    strip.child(cluster);
    return strip;
  }

  Element rail(bool right) {
    using namespace tav;
    return box().absolute().left(Dim(right ? 1916.0f : 0.0f)).top(Dim(0))
        .width(24).height(Dim(1560))
        .fill(Material::linearUnit({0, 0}, {0, 1},
                                   {{0.00f, C(0x6A1B21)},
                                    {0.22f, kChrome},
                                    {0.70f, C(0x2A0708)},
                                    {1.00f, C(0x0A0000)}}))
        .foreground(shapes::onEdges(
            right ? shapes::Edge::Left : shapes::Edge::Right,
            util::stroke(1, Fill::color(fade(C(0x99AAAA), 0.35f)),
                         PathFormat::Align::Inner)))
        .foreground(RailFlares{C(0x99AAAA), 6.0f, right ? 3.0f : 0.0f})
        .cache(Cache::None);
  }

  // ---- boot overlay (§9 beats 1–4) ---------------------------------------

  Element bootOverlay() {
    using namespace tav;
    const float cx = 970, cy = 760;

    auto hair = [&](float x, float y, float w, float h, float dx, float dy,
                    int delayMs) {
      return place(box().outline(ray(dx, dy))
                       .stroke(util::stroke(1.5f, Fill::color(kCyan)))
                       .trim(0.0f,
                             withFrom(0.0f, 1.0f,
                                      {400ms, &ch::easeOutQuint,
                                       std::chrono::milliseconds(delayMs)})),
                   x, y, w, h);
    };

    Element o = stack().absolute().inset(0).zIndex(90);
    o.child(box().absolute().inset(0).fill(C(0x120303))
                .opacity(withKeyframes<float>({{0ms, 1.0f},
                                               {1400ms, 1.0f},
                                               {1560ms, 0.0f}})));
    // 1. the single cyan pixel-dot
    o.child(place(box().fill(kCyan), cx - 3, cy - 3, 6, 6)
                .opacity(withKeyframes<float>({{0ms, 0.0f},
                                               {150ms, 1.0f},
                                               {1350ms, 1.0f},
                                               {1450ms, 0.0f}})));
    // 2. the reticle drawing OUTWARD from it on four trimmed rays
    o.child(hair(cx - 470, cy, 470, 1, -1, 1, 150));
    o.child(hair(cx, cy, 470, 1, 1, 1, 150));
    o.child(hair(cx, cy - 300, 1, 300, 1, -1, 220));
    o.child(hair(cx, cy, 1, 300, 1, 1, 220));
    o.child(place(box().outline(shapes::arc(-90, 359))
                      .stroke(util::stroke(1, Fill::color(fade(kCyan, 0.7f))))
                      .trim(0.0f, withFrom(0.0f, 1.0f,
                                           {500ms, &ch::easeOutQuint, 260ms})),
                  cx - 92, cy - 92, 184, 184));
    // 3. the 0→100 readout (a slot: TEXT, so it cannot be a binding)
    o.child(place(box().column().alignItems(Align::Center).gap(9),
                  cx - 260, cy + 120, 520, 110)
                .opacity(withKeyframes<float>({{520ms, 0.0f},
                                               {620ms, 1.0f},
                                               {1350ms, 1.0f},
                                               {1450ms, 0.0f}}))
                .child(slot("bootpct"))
                .child(box().width(420).height(2).fill(fade(kCyan, 0.18f))
                           .child(box().absolute().inset(0)
                                      .outline(ray(1, 1))
                                      .stroke(util::stroke(
                                          2, Fill::color(kCyan)))
                                      .trim(0.0f,
                                            withFrom(0.0f, 1.0f,
                                                     {800ms, &ch::easeNone,
                                                      550ms}))))
                .child(t("LOADING PROPHECY INTERFACE \xc2\xb7 970\xc3\x97" "655",
                         micro(11, fade(kCyan, 0.6f), 240))));
    // 4. the boot-complete flash
    o.child(box().absolute().inset(0).fill(SkColor4f{1, 1, 1, 1})
                .opacity(withKeyframes<float>({{1330ms, 0.0f},
                                               {1390ms, 0.7f},
                                               {1460ms, 0.0f}}))
                .blend(SkBlendMode::kPlus));
    o.opacity(withKeyframes<float>({{1440ms, 1.0f}, {1480ms, 0.0f}}));
    return o;
  }

  Element bootReadout() {
    using namespace tav;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%03d", bootPct);
    return box().row().alignItems(Align::Baseline).gap(6)
        .child(t(buf, type(blackFace(), 46, kCyan, 40, 0.9f)))
        .child(t("%", type(blackFace(), 20, fade(kCyan, 0.6f), 40, 0.9f)));
  }

  // =========================================================================

  Element describe() {
    using namespace tav;
    Element stage = box().absolute().left(Dim(24)).top(Dim(0)).width(1892)
                        .height(Dim(1530));
    stage.child(statusBar());
    stage.child(place(audioModule(), 0, 48, 596, 174));
    stage.child(place(navBar(), 604, 48, 584, 46));
    stage.child(place(masthead(), 1192, 8, 700, 182));
    stage.child(place(mainframe(), 0, 246, 1180, 350));
    stage.child(place(featureSystem(), 1196, 246, 696, 350));
    stage.child(place(auxiliary(), 0, 616, 1180, 154));
    stage.child(place(transmissionLog(), 0, 790, 1180, 236));
    stage.child(place(pressUpdates(), 1196, 616, 696, 410));
    stage.child(place(subSystem(), 0, 1080, 1892, 100));
    stage.child(place(legalStrip(), 0, 1190, 1892, 90));
    stage.child(place(footerDock(), 0, 1310, 1892, 220));

    // sitebackground.gif is a 1×1600 strip: the oxblood is only the top
    // band, and it is nearly black by a third of the way down. A linear
    // two-stop ramp reads as a flat maroon field and gets the figure/
    // ground backwards — the PANELS are the light thing on this page.
    return stack()
        .fill(Material::linearUnit({0, 0}, {0, 1},
                                   {{0.00f, kBgTop},
                                    {0.13f, C(0x2E0808)},
                                    {0.40f, C(0x150202)},
                                    {1.00f, kBgBot}}))
        .child(box().absolute().inset(0).fill(grain).opacity(0.07f)
                   .blend(SkBlendMode::kOverlay))
        .child(rail(false))
        .child(rail(true))
        .child(stage)
        .child(bootOverlay());
  }

  // =========================================================================

  void setup(sketch::SketchContext &ctx) override {
    using namespace tav;
    ctx.canvas(1940, 1560);
    ctx.background(C(0x0A0000));

    // --- generated materials, built ONCE and HELD (identity = pruning) ---
    hazard = patterns::stripes(6, 10, kChromeHi);
    hazard.rotate(45);
    hatchA = patterns::stripes(1, 7, fade(kD4, 0.5f));
    hatchA.rotate(45);
    hatchB = patterns::stripes(1, 7, fade(kD1, 0.55f));
    hatchB.rotate(-45);
    dither = patterns::checker(1.5f, fade(kPanelSh, 0.28f),
                               fade(kPanelHi, 0.15f));
    // LUMINANCE grain — patterns::grain() would be the right call here
    // (see grainFx()); it crashes, so this is the same recipe by hand.
    grain = Material::sksl(grainFx(), {{"uFreq", 0.9f}, {"uSeed", 4.0f}});

    spectrum = Material::sksl(spectrumFx(), {{"uBars", 32.0f}})
                   .uniform("uHot", kGlow)
                   .uniform("uCool", kTealBar)
                   .quantizeTime(10.0f); // §8: a deliberately chunky readout

    // ONE stripe material value, reused by the nav bar and four panel
    // headers; the pan is a bound uniform, not five redraw loops.
    stripesLive =
        Material::sksl(stripeFx(), {{"uOn", 6.0f}, {"uPeriod", 16.0f}})
            .uniform("uColor", kChromeHi)
            .uniform("uBase", kChrome)
            .uniform("uPan", &stripePan);

    waterStreaks = Material::sksl(waterFx());

    // --- the instanced chevron array in the footer dock ---
    dockAtlas = std::make_shared<instancing::Atlas>(2.0f);
    const int chev = dockAtlas->cell(
        box().outline([](SkSize s) {
              SkPathBuilder b;
              b.moveTo(0, 0);
              b.lineTo(s.width() * 0.62f, s.height() * 0.5f);
              b.lineTo(0, s.height());
              b.lineTo(s.width() * 0.30f, s.height() * 0.5f);
              b.close();
              return b.detach();
            })
            .fill(kD5),
        {12, 10});
    dockPool = std::make_shared<instancing::Pool>();
    instancing::place::grid(*dockPool, 6 * 14, 14, {14, 12}, {0, 0}, {2, 4});
    {
      auto frames = dockPool->frames();
      auto tints = dockPool->tints();
      for (size_t i = 0; i < frames.size(); ++i) {
        frames[i] = chev;
        const float k = 0.35f + 0.65f * (float)((i * 7 + 3) % 11) / 10.0f;
        tints[i] = {1, 1, 1, k};
      }
      dockPool->touch();
    }

    // --- declared idle motion (§9's second list) --------------------------
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      const float s = (float)t;
      stripePan = s * 2.5f;                                // 20 px / 8 s
      portalGlow = 54.0f + 4.4f * std::sin(s * 1.5708f);   // ±8 %, period 4 s
      vuLeft = 0.45f + 0.42f * std::abs(std::sin(s * 3.1f));
      vuRight = 0.40f + 0.45f * std::abs(std::sin(s * 2.3f + 1.1f));

      // seven tick clusters, each blinking its three dots in sequence,
      // phase-offset per panel so they never lock step
      for (int p = 0; p < 7; ++p) {
        const float off = (float)((p * 137) % 800) / 1000.0f;
        const float cyc = std::fmod(s + off, 2.7f);
        for (int i = 0; i < 3; ++i) {
          const bool on = cyc >= i * 0.4f && cyc < i * 0.4f + 0.32f;
          dot[(size_t)(p * 3 + i)] = on ? 1.0f : 0.22f;
        }
      }

      // three gauges round-robin a radar sweep, 1 s each
      const int active = (int)std::fmod((double)s, 3.0);
      for (int i = 0; i < 3; ++i) {
        if (i == active)
          gauge[(size_t)i] = (float)std::fmod(s * 200.0, 360.0);
        gaugeAlpha[(size_t)i] = i == active ? 1.0f : 0.22f;
      }

      // PRESS UPDATES: 2.5 s dwell, then a 600 ms eased step; ~19 s loop
      const float span = 3.1f;
      const float u = std::fmod(std::max(0.0f, s - 4.5f), span * 6.0f);
      const int idx = (int)(u / span);
      float f = (u - (float)idx * span - 2.5f) / 0.6f;
      f = std::clamp(f, 0.0f, 1.0f);
      f = f < 0.5f ? 2 * f * f : 1 - 2 * (1 - f) * (1 - f); // easeInOutQuad
      pressScroll = -62.0f * ((float)idx + f);
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("bootpct", bootReadout());
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    // The DATA path, used for exactly one thing: the boot percentage is
    // TEXT CONTENT, not a paint property, so it cannot be a binding. The
    // slot() keeps the churn local — the rest of the tree is untouched.
    if (booted)
      return;
    const double u = (elapsed - 0.55) / 0.80;
    const int pct = (int)std::lround(std::clamp(u, 0.0, 1.0) * 100.0);
    if (pct == bootPct)
      return;
    bootPct = pct;
    if (pct >= 100)
      booted = true;
    ctx.composer.renderSlot("bootpct", bootReadout());
  }
};

SIGIL_SKETCH(TwoAdvancedV4)

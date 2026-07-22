// twoadvanced_v4.cpp — 2Advanced Studios, v4 "Prophecy" (2003–2006),
// the post-splash Home/Index interface (flashindex.swf, a 970×655 stage).
// Creative direction / design / UI / Flash animation: Eric Jordan.
//
// REFERENCE (everything measured, not remembered):
//   · http://web.archive.org/web/20040610062326/http://2advanced.com/flashindex.htm
//     — the live 2004 HTML shell: rails 12×780, stage 970×655, footer
//       970×110, tiled by a 970×1 strip. This sketch is EXACTLY ×2 of that
//       frame: 1940×1560. Halve any number here to recover the 2004 pixel.
//   · .../images/{leftsidepanel,rightsidepanel,sitebackground,sitefooter,
//     splash_*}.gif — the real production GIFs, pixel-sampled: chrome
//     maroon #571119, page ramp #4A100F→#0A0000, logo cyan #7BDAD6.
//   · .../flashindex.swf (543,340 B, CWS/zlib SWF6) — decompressed and
//     `strings`-mined. It yields the component-kit class names this file
//     rebuilds as functions: FSingleBevelPanelClass, FDoubleBevelPanelClass,
//     FBevelledPanelClass; the nav taxonomy company/services/portfolio/
//     accolades/experimental/equipment/contact; and the embedded faces
//     Arial, Arial Black, Helvetica 95 Black, Helvetica CondensedBlack.
//   · .../mp3Info.xml — the 2004 playlist; the 2006 one (DESERT TRANCE /
//     NEVERMIND / SYNERGY / EXHALE) is off the FWA press screenshot,
//     https://thefwa.com/cases/2advanced-v4-prophecy (900×575, the live
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
// hairline, and a blurred kPlus bloom copy over the whole stack. Stacking
// order does the work.

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
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>

#include <array>
#include <cmath>
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
constexpr SkColor4f kTealBar = C(0x2C7B80); // status bar segment
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
inline SkColor4f dim(SkColor4f c, float k) {
  return {c.fR * (1 - k), c.fG * (1 - k), c.fB * (1 - k), c.fA};
}
inline SkColor4f alpha(SkColor4f c, float a) { return {c.fR, c.fG, c.fB, a}; }

// ---------------------------------------------------------------------------
// Type — the SWF's embedded faces, substituted.
//   Helvetica 95 Black / Helvetica CondensedBlack → the heaviest condensed
//   grotesk macOS ships; Arial / Arial Black are present natively.

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

/** Helvetica CondensedBlack stand-in — the chrome/label voice. */
inline const sk_sp<SkTypeface> &condBlack() {
  static sk_sp<SkTypeface> f =
      face("Helvetica Neue", SkFontStyle::kBlack_Weight,
           SkFontStyle::kCondensed_Width, "Avenir Next Condensed");
  return f;
}
/** Helvetica 95 Black / Arial Black — the headline voice. */
inline const sk_sp<SkTypeface> &black() {
  static sk_sp<SkTypeface> f = face("Arial Black", SkFontStyle::kBlack_Weight,
                                    SkFontStyle::kNormal_Width, "Helvetica Neue");
  return f;
}
/** Arial regular — the ONE sentence-case voice in the whole interface. */
inline const sk_sp<SkTypeface> &arial() {
  static sk_sp<SkTypeface> f = face("Arial", SkFontStyle::kNormal_Weight,
                                    SkFontStyle::kNormal_Width, "Helvetica");
  return f;
}

/** Tracking is authored in Illustrator units (1/1000 em), as §6 states it. */
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
  return type(black(), size, c, tr, 0.94f);
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
 *  selected corner. Reused at every nesting depth — status-bar segments,
 *  CTA buttons, panel end-caps, readout windows, corner wedges. */
inline std::function<SkPath(SkSize)> chamfer(float cut, uint8_t mask) {
  return [cut, mask](SkSize s) {
    const float w = s.width(), h = s.height();
    const float c = std::min({cut, w * 0.5f, h * 0.5f});
    SkPathBuilder b;
    (mask & kTL) ? b.moveTo(c, 0) : b.moveTo(0, 0);
    (mask & kTR) ? (void)b.lineTo(w - c, 0), (void)b.lineTo(w, c)
                 : (void)b.lineTo(w, 0);
    (mask & kBR) ? (void)b.lineTo(w, h - c), (void)b.lineTo(w - c, h)
                 : (void)b.lineTo(w, h);
    (mask & kBL) ? (void)b.lineTo(c, h), (void)b.lineTo(0, h - c)
                 : (void)b.lineTo(0, h);
    if (mask & kTL)
      b.lineTo(0, c);
    b.close();
    return b.detach();
  };
}

/** An open hairline across the node — the trim() reveal primitive.
 *  dirX/dirY pick which way it draws from its anchor. */
inline std::function<SkPath(SkSize)> ray(float dirX, float dirY) {
  return [dirX, dirY](SkSize s) {
    SkPathBuilder b;
    const float x0 = dirX < 0 ? s.width() : 0, y0 = dirY < 0 ? s.height() : 0;
    b.moveTo(x0, y0);
    b.lineTo(dirX < 0 ? 0 : s.width(), dirY < 0 ? 0 : s.height());
    return b.detach();
  };
}

// ---------------------------------------------------------------------------
// Decoration vocabulary — the hairline kit the whole idiom runs on.

/** Corner brackets: two legs, a gap from the edge. Cyan on the audio
 *  module, dark on the readout windows, everywhere in between. */
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
 *  nested `inset` px inside the first. A foreground so it rides above the
 *  panel's own content, like the frame it is. */
struct InsetBevel {
  SkColor4f hi{1, 1, 1, 0.18f}, lo{0, 0, 0, 0.35f};
  float inset = 6, hiW = 2, loW = 1;

  bool operator==(const InsetBevel &) const = default;
  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkRect r = SkRect::MakeWH(ctx.size.width(), ctx.size.height())
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

/** The readout nobody reads: a rail of ticks along one edge, every fourth
 *  one long. The single densest piece of vocabulary on the whole screen. */
struct TickRail {
  SkColor4f color = alpha(kCyan, 0.5f);
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
      if (vertical) {
        const float x = farSide ? w - len : 0;
        c.drawRect(SkRect::MakeXYWH(x, d, len, thickness), p);
      } else {
        const float y = farSide ? h - len : 0;
        c.drawRect(SkRect::MakeXYWH(d, y, thickness, len), p);
      }
    }
  }
};

/** The rail flare tick from leftsidepanel.gif: a near-vertical hairline
 *  ending in a small flag, with a soft highlight travelling down it
 *  every 6 s (§9's idle list). */
struct RailFlares {
  SkColor4f color = C(0x99AAAA);
  float period = 6.0f, phase = 0.0f;

  bool operator==(const RailFlares &) const = default;
  bool animated() const { return true; }
  void paint(SkCanvas &c, const PaintContext &ctx) const {
    const float w = ctx.size.width(), h = ctx.size.height();
    const float ys[3] = {h * 0.167f, h * 0.5f, h * 0.833f};
    const float len = 90.0f;
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(2);
    const double tt = ctx.elapsedSeconds + phase;
    const float scan = (float)std::fmod(tt, (double)period) / period;
    for (int i = 0; i < 3; ++i) {
      const float y0 = ys[i] - len * 0.5f;
      const float lean = len * 0.14f; // ~8° off vertical
      SkPathBuilder b;
      b.moveTo(w * 0.5f - lean * 0.5f, y0);
      b.lineTo(w * 0.5f + lean * 0.5f, y0 + len);
      SkPath path = b.detach();
      p.setColor4f(alpha(color, 0.55f), nullptr);
      c.drawPath(path, p);
      // the flag/notch terminator
      SkPaint f;
      f.setAntiAlias(true);
      f.setColor4f(alpha(color, 0.8f), nullptr);
      c.drawRect(SkRect::MakeXYWH(w * 0.5f + lean * 0.5f - 3, y0 + len, 6, 3),
                 f);
      // the travelling highlight
      const float hy = y0 + len * scan;
      SkPaint g;
      g.setAntiAlias(true);
      g.setStyle(SkPaint::kStroke_Style);
      g.setStrokeWidth(2);
      g.setColor4f(alpha(kCyan, 0.85f * (1.0f - std::abs(scan - 0.5f) * 2)),
                   nullptr);
      SkPathBuilder hb;
      const float f0 = (hy - y0) / len;
      hb.moveTo(w * 0.5f - lean * 0.5f + lean * f0, hy);
      hb.lineTo(w * 0.5f - lean * 0.5f + lean * std::min(1.0f, f0 + 0.14f),
                std::min(y0 + len, hy + len * 0.14f));
      c.drawPath(hb.detach(), g);
    }
  }
};

/** Scanline veil over the hero — the CRT the era shot these on. */
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
// Placement.

inline Element place(Element e, float x, float y, float w, float h) {
  e.absolute().left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h));
  return e;
}

// ---------------------------------------------------------------------------
// The two panel CLASSES the SWF's symbol table names.

/** FSingleBevelPanelClass: base fill + a 3px lifted top/left highlight
 *  inset 1px and a 2px darkened bottom/right shadow. Two strokes, no
 *  filter — crisp at any scale, and shapes::onEdges() follows the
 *  CHAMFERED outline where the panel has one. */
inline Element singleBevel(Element e, SkColor4f base) {
  e.fill(base);
  e.foreground(shapes::onEdges(
      shapes::Edge::Top | shapes::Edge::Left,
      util::stroke(3, Fill::color(lift(base, 0.10f)),
                   PathFormat::Align::Inner)));
  e.foreground(shapes::onEdges(
      shapes::Edge::Bottom | shapes::Edge::Right,
      util::stroke(2, Fill::color(dim(base, 0.45f)),
                   PathFormat::Align::Inner)));
  return e;
}

/** FDoubleBevelPanelClass: the same pair, plus a fainter one 6px in.
 *  MAINFRAME and FEATURE SYSTEM wear this; everything smaller is single. */
inline Element doubleBevel(Element e, SkColor4f base, float inset = 6) {
  e = singleBevel(std::move(e), base);
  e.foreground(InsetBevel{alpha(lift(base, 0.14f), 0.75f),
                          alpha(dim(base, 0.55f), 0.85f), inset, 2, 1});
  return e;
}

} // namespace tav

// ===========================================================================

struct TwoAdvancedV4 : sigil::compose::sketch::Sketch {
  using Sketch::Sketch;

  // --- bound outputs (all motion is declared, none of it re-describes) ---
  ch::Output<float> wavePhase{0};      // audio spectrum uTime driver
  ch::Output<float> stripePan{0};      // hazard-stripe conveyor
  ch::Output<float> portalGlow{54};    // MAINFRAME portal glow radius
  ch::Output<float> pressScroll{0};    // PRESS UPDATES auto-scroll
  ch::Output<float> vuLeft{0.4f}, vuRight{0.6f};
  std::array<ch::Output<float>, 21> dot{}; // 7 panels × 3-dot tick clusters
  std::array<ch::Output<float>, 3> gauge{};

  // --- generated materials held for identity (Pattern/noise bake once) ---
  Pattern hazard;   // baked 45° stripe tile — the STATIC reuse path
  Pattern hatchA, hatchB; // footer-dock crosshatch
  Pattern dither;   // teal readout-box dither
  Material grain;   // page-background film grain
  Material spectrum, stripesLive, waterStreaks;

  // --- instancing: the footer dock's chevron tick array ---
  std::shared_ptr<instancing::Atlas> dockAtlas;
  std::shared_ptr<instancing::Pool> dockPool;

  int bootPct = 0;
  bool booted = false;

  // =========================================================================
  // Live materials (SkSL).

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
          // grid ground: 1px verticals every bar, horizontals every 1/5
          float gy = step(fract(xy.y / (uResolution.y / 5.0)), 0.08);
          float4 acc = float4(0.02, 0.10, 0.11, 1.0) * 1.0;
          acc = mix(acc, float4(0.05, 0.22, 0.23, 1.0), gy);
          if (fx > 0.72) return half4(acc);
          float step0 = floor(uTime);              // uTime arrives quantized
          float n  = h(i, step0);
          float n2 = h(i * 3.1, step0 * 0.7 + 5.0);
          float amp = 0.14 + 0.80 * n * (0.35 + 0.65 * n2);
          // a slow envelope so the middle of the band is loudest
          amp *= 0.55 + 0.55 * sin(i / uBars * 3.14159);
          float top = uResolution.y * (1.0 - clamp(amp, 0.0, 1.0));
          float on  = step(top, xy.y);
          float grad = clamp((xy.y - top) / max(uResolution.y - top, 1.0), 0.0, 1.0);
          float4 bar = mix(uHot, uCool, grad);
          acc = mix(acc, bar, on);
          // the peak-hold cap
          float cap = step(abs(xy.y - (top - 3.0)), 1.5);
          acc = mix(acc, float4(1.0, 1.0, 1.0, 1.0), cap * 0.75);
          return half4(half3(acc.rgb) * acc.a, acc.a);
        }
      )"));
      if (!e)
        SkDebugf("twoadvanced spectrum: %s\n", err.c_str());
      return e;
    }();
    return fx;
  }

  /** The hazard stripe as a LIVE material so the header bars can run their
   *  slow conveyor pan (§9: 20 px per 8 s) without re-describing. ONE
   *  material value reused across the nav bar and every panel header. */
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
          float a = smoothstep(0.0, 1.0, uOn - f) ;
          float4 c = mix(uBase, uColor, clamp(a, 0.0, 1.0));
          return half4(half3(c.rgb) * c.a, c.a);
        }
      )"));
      if (!e)
        SkDebugf("twoadvanced stripe: %s\n", err.c_str());
      return e;
    }();
    return fx;
  }

  /** Horizontal streak water, dark teal-black, drifting. */
  static sk_sp<SkRuntimeEffect> waterFx() {
    static const sk_sp<SkRuntimeEffect> fx = [] {
      auto [e, err] = SkRuntimeEffect::MakeForShader(SkString(R"(
        uniform float2 uResolution;
        uniform float  uTime;
        half4 main(float2 xy) {
          float v = xy.y / max(uResolution.y, 1.0);
          float s = sin(xy.y * 0.9 + sin(xy.x * 0.012 + uTime * 0.5) * 2.2);
          float band = smoothstep(0.55, 1.0, s) * (0.25 + 0.75 * v);
          float3 base = mix(float3(0.000, 0.078, 0.082),
                            float3(0.004, 0.122, 0.129), v);
          float3 c = base + float3(0.02, 0.16, 0.17) * band;
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

  Element tickDots(int panelIndex, SkColor4f c = tav::kCyan) {
    Element r = box().row().gap(4).alignItems(Align::Center);
    for (int i = 0; i < 3; ++i)
      r.child(box().width(5).height(5).fill(c).opacity(
          &dot[(size_t)(panelIndex * 3 + i)]));
    return r;
  }

  /** The two-weight panel header: "MAIN" heavy + "FRAME" regular over the
   *  hazard-stripe ground, with a tick cluster and micro flavor line. */
  Element panelHeader(const char *boldHalf, const char *restHalf,
                      const char *flavor, int panelIndex, float w) {
    using namespace tav;
    Element bar =
        box().width(Dim(w)).height(28).row().alignItems(Align::Center)
            .padding(10, 0)
            .fill(stripesLive)
            .foreground(shapes::onEdges(
                shapes::Edge::Bottom,
                util::stroke(1, Fill::color(alpha(kCyan, 0.35f)),
                             PathFormat::Align::Inner)))
            .child(t(boldHalf, heavy(18, kNear, 40)))
            .child(t(restHalf, type(arial(), 16, kHeadDim, 40, 0.95f)))
            .child(box().width(12))
            .child(box().width(1).height(12).fill(alpha(kCyan, 0.4f)))
            .child(box().width(10))
            .child(t(flavor, micro(11, alpha(kDust, 0.95f), 260)))
            .child(box().grow(1))
            .child(tickDots(panelIndex))
            .child(box().width(8))
            .child(box().width(34).height(10)
                       .foreground(TickRail{alpha(kCyan, 0.55f), 4, 3, 7, 1, 3,
                                            false, true}));
    return bar;
  }

  /** CTA: chamfered, blood-red ramp, gloss band across the top third. */
  Element cta(const char *lbl, float w = 116, float h = 34,
              SkColor4f hairline = tav::kNear) {
    using namespace tav;
    return box().width(Dim(w)).height(Dim(h))
        .outline(chamfer(9, kTL | kBR))
        .fill(Material::linear({0, 0}, {0, h},
                               {{0.0f, kCtaHi}, {0.42f, kCta}, {1.0f, C(0x3A0000)}}))
        .stroke(util::stroke(1, Fill::color(kChrome), PathFormat::Align::Outer))
        .foreground(styles::gloss(alpha(kCtaHi, 0.55f), h * 0.30f,
                                  {0, -h * 0.26f}, 0.62f, 0.30f))
        .foreground(util::stroke(1, Fill::color(alpha(hairline, 0.45f)),
                                 PathFormat::Align::Inner))
        .row().justify(Justify::Center).alignItems(Align::Center)
        .child(t(lbl, label(15, kNear, 110)));
  }

  /** A dark readout window: bracketed, dithered, hairlined. The unit that
   *  makes this idiom read as dense rather than clean. */
  Element readout(float w, float h, SkColor4f ground = tav::C(0x1B0708)) {
    using namespace tav;
    return box().width(Dim(w)).height(Dim(h))
        .outline(chamfer(7, kTR | kBL))
        .fill(ground)
        .foreground(InsetBevel{alpha(kChromeHi, 0.6f), {0, 0, 0, 0.5f}, 0, 1, 1})
        .foreground(Brackets{alpha(kCyan, 0.55f), 8, 2, 3, 0xF});
  }

  // =========================================================================
  // Regions.

  Element statusBar() {
    using namespace tav;
    // Two segments meeting on a DIAGONAL seam, not a vertical edge.
    Element teal =
        singleBevel(box().absolute().left(Dim(0)).top(Dim(0)).width(560)
                        .height(40)
                        .outline(chamfer(40, kBR))
                        .row().alignItems(Align::Center).padding(10, 0).gap(8),
                    kTealBar)
            .translateY(withFrom(-46.0f, 0.0f,
                                 {380ms, &ch::easeOutQuint, 1450ms}))
            .child(box().width(22).height(22).corners({5})
                       .fill(Material::radial({11, 9}, 14,
                                              {{0.0f, kCyanRing},
                                               {0.55f, kTealBar},
                                               {1.0f, C(0x0C2A2C)}}))
                       .stroke(util::stroke(1, Fill::color(alpha(kCyan, 0.7f)),
                                            PathFormat::Align::Inner))
                       .justify(Justify::Center).alignItems(Align::Center)
                       .child(box().width(8).height(8).corners({4})
                                  .stroke(util::stroke(
                                      2, Fill::color(kCyan),
                                      PathFormat::Align::Center))))
            .child(t("SYSTEM ONLINE", micro(12, kNear, 240)))
            .child(box().width(1).height(14).fill(alpha(kCyan, 0.4f)))
            .child(t("SECTOR 04 / PROPHECY", micro(12, alpha(kCyan, 0.8f), 240)))
            .child(box().grow(1))
            .child(box().width(90).height(12)
                       .foreground(TickRail{alpha(kNear, 0.45f), 6, 3, 8, 1, 3,
                                            false, true}))
            .child(box().width(46));

    Element maroon =
        singleBevel(box().absolute().left(Dim(548)).top(Dim(0))
                        .width(Dim(1892.0f - 548.0f)).height(40)
                        .outline(chamfer(40, kTL))
                        .row().alignItems(Align::Center).padding(58, 0, 14, 0)
                        .gap(10),
                    kChrome)
            .translateY(withFrom(-46.0f, 0.0f,
                                 {380ms, &ch::easeOutQuint, 1530ms}))
            .child(t("2ADVANCED STUDIOS", micro(12, alpha(kDust, 1.0f), 260)))
            .child(box().width(1).height(14).fill(alpha(kDust, 0.4f)))
            .child(t("PROGRESSIVE DESIGN TECHNOLOGY", micro(12, kDustDim, 260)))
            .child(box().grow(1))
            .child(t("LAT 33.6189 N   LON 117.9298 W", micro(11, kDustDim, 200)))
            .child(box().width(1).height(14).fill(alpha(kDust, 0.4f)))
            .child(t("V4.PROPHECY", heavy(14, kNear, 80)));

    return box().absolute().left(Dim(0)).top(Dim(0)).width(1892).height(40)
        .child(maroon)
        .child(teal);
  }

  Element audioModule() {
    using namespace tav;
    const char *tracks[4] = {"01  DESERT TRANCE", "02  NEVERMIND",
                             "03  SYNERGY", "04  EXHALE"};
    Element list = box().column().width(268).gap(2);
    for (int i = 0; i < 4; ++i) {
      const bool sel = i == 2;
      Element row =
          box().height(23).row().alignItems(Align::Center).padding(6, 0).gap(6)
              .fill(sel ? kChromeHi : alpha(C(0x2A0A0C), 0.85f))
              .foreground(shapes::onEdges(
                  shapes::Edge::Left,
                  util::stroke(2,
                               Fill::color(sel ? kCyan : alpha(kDust, 0.35f)),
                               PathFormat::Align::Inner)))
              .child(t(sel ? "\xe2\x96\xb8" : " ",
                       micro(11, kCyan, 0)))
              .child(t(tracks[i], type(black(), 13, sel ? kNear : kHeadDim, 60,
                                       0.92f)))
              .child(box().grow(1))
              .child(t(sel ? "3:41" : (i == 0 ? "4:12" : (i == 1 ? "5:08"
                                                                 : "3:55")),
                       micro(11, sel ? kCyan : kDustDim, 120)));
      list.child(row);
    }

    Element scope =
        box().grow(1).height(100)
            .outline(chamfer(8, kTR | kBL))
            .fill(spectrum)
            .foreground(Brackets{alpha(kCyan, 0.6f), 10, 2, 3, 0xF})
            .foreground(Scanlines{{0, 0, 0, 0.16f}, 3, 1})
            .foreground(util::stroke(1, Fill::color(alpha(kCyan, 0.35f)),
                                     PathFormat::Align::Inner));

    auto transport = [&](const char *glyph, bool hot) {
      return box().width(38).height(22)
          .outline(chamfer(6, kTL | kBR))
          .fill(Material::linear({0, 0}, {0, 22},
                                 {{0.0f, hot ? kCtaHi : C(0x5A2226)},
                                  {0.5f, hot ? kCta : C(0x3A0F12)},
                                  {1.0f, C(0x240607)}}))
          .stroke(util::stroke(1, Fill::color(alpha(kDust, 0.35f)),
                               PathFormat::Align::Inner))
          .justify(Justify::Center).alignItems(Align::Center)
          .child(t(glyph, micro(11, hot ? kNear : kDust, 0)));
    };

    Element panel =
        singleBevel(box().width(596).height(174).column().padding(9).gap(6),
                    C(0x33090C));
    panel.key("audio")
        .foreground(Brackets{kCyan, 18, 3, 4, kTL | kTR})
        .foreground(TickRail{alpha(kDust, 0.45f), 7, 3, 6, 1, 4, false, true})
        .child(box().row().gap(8).height(100).child(list).child(scope))
        .child(box().row().gap(5).alignItems(Align::Center)
                   .child(transport("\xe2\x97\x82\xe2\x97\x82", false))
                   .child(transport("\xe2\x96\xa0", false))
                   .child(transport("\xe2\x96\xb8", true))
                   .child(transport("\xe2\x96\xb8\xe2\x96\xb8", false))
                   .child(box().width(8))
                   .child(box().width(64).height(6)
                              .fill(C(0x1B0708))
                              .child(box().absolute().left(Dim(0)).top(Dim(0))
                                         .width(Dim(64)).height(6)
                                         .fill(alpha(kCyan, 0.8f))
                                         .scale(&vuLeft)
                                         .transformOrigin(0, 0.5f)))
                   .child(box().grow(1))
                   .child(t("VOL", micro(10, kDustDim, 200)))
                   .child(box().width(56).height(6).fill(C(0x1B0708))
                              .child(box().absolute().left(Dim(0)).top(Dim(0))
                                         .width(Dim(56)).height(6)
                                         .fill(alpha(kCyanRing, 0.75f))
                                         .scale(&vuRight)
                                         .transformOrigin(0, 0.5f))))
        .child(box().height(18).row().alignItems(Align::Center).padding(6, 0)
                   .fill(alpha(kChrome, 0.9f))
                   .child(t("AUDIO PREFERENCES", micro(11, kDust, 240)))
                   .child(box().grow(1))
                   .child(t("STREAM 128K \xc2\xb7 STEREO",
                            micro(10, kDustDim, 200))));
    return panel;
  }

  Element navBar() {
    using namespace tav;
    const char *items[7] = {"COMPANY",      "SERVICES",  "PORTFOLIO",
                            "ACCOLADES",    "EXPERIMENTAL", "EQUIPMENT",
                            "CONTACT"};
    Element bar =
        singleBevel(box().width(584).height(46).row()
                        .justify(Justify::SpaceEvenly).alignItems(Align::Center)
                        .padding(8, 0),
                    kChrome);
    bar.background(Overlay{stripesLive, SkBlendMode::kSrcOver, 1.0f})
        .staggerChildren(40ms);
    for (int i = 0; i < 7; ++i) {
      Element item =
          box().column().alignItems(Align::Center).gap(3)
              .translateY(withFrom(16.0f, 0.0f,
                                   {240ms, &ch::easeOutQuint, 2250ms}))
              .opacity(withFrom(0.0f, 1.0f, {240ms, &ch::easeOutQuad, 2250ms}))
              .child(t(items[i], label(14, i == 2 ? kCyan : kNear, 90)))
              .child(box().width(i == 2 ? 22.0f : 8.0f).height(2)
                         .fill(alpha(i == 2 ? kCyan : kDust, 0.75f)));
      bar.child(item);
      if (i < 6)
        bar.child(box().width(1).height(20).fill(alpha(C(0x2A0A0C), 0.8f)));
    }
    return bar;
  }

  Element masthead() {
    using namespace tav;
    Element emblem =
        box().width(70).height(70)
            .fill(sdf::material(sdf::circle(),
                                {.fill = {0, 0, 0, 0},
                                 .borderWidth = 4,
                                 .borderColor = kCyan,
                                 .glowRadius = 9,
                                 .glowColor = alpha(kGlow, 0.55f)}))
            .justify(Justify::Center).alignItems(Align::Center)
            .child(box().width(44).height(44)
                       .outline(shapes::polygon(6, 0))
                       .stroke(util::stroke(1, Fill::color(alpha(kCyanRing,
                                                                 0.75f))))
                       .justify(Justify::Center).alignItems(Align::Center)
                       .child(t("2", type(black(), 30, kCyan, 0, 0.85f))));

    return box().width(700).height(182).row().alignItems(Align::Center)
        .padding(30, 0, 8, 0).gap(18)
        .translateX(withFrom(320.0f, 0.0f, {420ms, &ch::easeOutQuint, 1850ms}))
        .opacity(withFrom(0.0f, 1.0f, {300ms, &ch::easeOutQuad, 1850ms}))
        .child(emblem)
        .child(box().column().gap(6)
                   .child(t("2ADVANCED STUDIOS", type(black(), 26, kCyan, 80,
                                                      0.90f))
                              .effect(styles::textGlow(alpha(kGlow, 0.6f), 6)))
                   .child(t("PROGRESSIVE DESIGN TECHNOLOGY",
                            micro(12, kDust, 240)))
                   .child(box().row().gap(6).alignItems(Align::Center)
                              .child(box().width(30).height(1)
                                         .fill(alpha(kCyan, 0.5f)))
                              .child(t("EST. 1999 \xc2\xb7 IRVINE CA",
                                       micro(10, kDustDim, 200)))))
        .child(box().grow(1))
        .child(box().column().alignItems(Align::End).gap(4)
                   .child(t("BUILD 4.0.7", micro(10, kDustDim, 200)))
                   .child(t("FLASH 6 REQ.", micro(10, kDustDim, 200)))
                   .child(t("1024\xc3\x97""768 MIN", micro(10, kDustDim, 200))))
        // the glowing 2px divider under the whole masthead
        .foreground(shapes::onEdges(
            shapes::Edge::Bottom,
            util::stroke(2, Fill::color(kCyan), PathFormat::Align::Inner)))
        .foreground(OuterGlowShim{});
  }

  // A tiny local decoration so the masthead rule glows without a filter.
  struct OuterGlowShim {
    bool operator==(const OuterGlowShim &) const = default;
    void paint(SkCanvas &c, const PaintContext &ctx) const {
      SkPaint p;
      p.setAntiAlias(true);
      p.setColor4f(tav::alpha(tav::kGlow, 0.30f), nullptr);
      p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 5));
      c.drawRect(SkRect::MakeXYWH(0, ctx.size.height() - 6, ctx.size.width(), 5),
                 p);
    }
  };

  // ---- the hero -----------------------------------------------------------

  /** §10: composite ORDER is what sells it. sky → skyline → portal →
   *  figures → water+reflection → horizon hairline → bloom. */
  Element heroScene(float w, float h) {
    using namespace tav;
    const float horizon = h * 0.66f;
    const float cx = w * 0.50f;

    Element scene = stack().width(Dim(w)).height(Dim(h)).clip();

    // sky
    scene.child(box().absolute().inset(0).fill(Material::linear(
        {0, 0}, {0, horizon},
        {{0.0f, C(0x02070A)}, {0.62f, C(0x03181D)}, {1.0f, C(0x073038)}})));

    // portal halo — a wide radial behind everything
    scene.child(place(box().fill(Material::radial(
                          {260, 260}, 260,
                          {{0.0f, alpha(kGlow, 0.55f)},
                           {0.35f, alpha(kTealBar, 0.30f)},
                           {1.0f, alpha(kTealBar, 0.0f)}})),
                      cx - 260, horizon - 380, 520, 520)
                    .blend(SkBlendMode::kPlus));

    // skyline: flat SDF slabs, opacity + tint ramped by |x - cx| (fog)
    const float bx[13] = {40,  120, 196, 262, 330, 396, 452, 690,
                          742, 806, 878, 954, 1042};
    const float bw[13] = {62,  54,  48,  60,  50,  38,  52,  44,
                          56,  58,  46,  70,  64};
    const float bh[13] = {120, 168, 92,  146, 200, 110, 74,  86,
                          158, 118, 190, 96,  140};
    for (int i = 0; i < 13; ++i) {
      const float d = std::abs(bx[i] + bw[i] * 0.5f - cx) / (w * 0.5f);
      const float o = 0.30f + 0.62f * d;
      SkColor4f tint = {0.02f + 0.05f * (1 - d), 0.10f + 0.07f * (1 - d),
                        0.13f + 0.07f * (1 - d), 1.0f};
      Element slab =
          place(box().fill(tint), bx[i], horizon - bh[i], bw[i], bh[i])
              .opacity(o);
      // a couple of lit windows per slab, cyan, tiny
      slab.child(box().absolute().left(Dim(bw[i] * 0.25f))
                     .top(Dim(bh[i] * 0.22f)).width(3).height(3)
                     .fill(alpha(kGlow, 0.5f)));
      slab.child(box().absolute().left(Dim(bw[i] * 0.62f))
                     .top(Dim(bh[i] * 0.48f)).width(3).height(3)
                     .fill(alpha(kGlow, 0.35f)));
      scene.child(slab);
      // the antenna mast on the tall ones
      if (bh[i] > 150)
        scene.child(place(box().fill(alpha(tint, o)), bx[i] + bw[i] * 0.5f - 1,
                          horizon - bh[i] - 26, 2, 26));
    }

    // the portal proper — one SDF circle, glow radius BOUND (idle pulse)
    Element portal =
        place(box().fill(sdf::material(
                  sdf::circle(),
                  {.fill = alpha(kGlow, 0.92f),
                   .borderWidth = 3,
                   .borderColor = {0.85f, 1.0f, 1.0f, 0.9f},
                   .glowRadius = 54,
                   .glowColor = alpha(kGlow, 0.85f)})
                                 .uniform("uGlowR", &portalGlow)),
              cx - 150, horizon - 260, 300, 300)
            .scale(withKeyframes<float>({{2400ms, 0.78f},
                                         {2820ms, 1.06f},
                                         {2980ms, 0.985f},
                                         {3060ms, 1.0f}}))
            .blend(SkBlendMode::kPlus);
    scene.child(portal);

    // an orbital ring around the portal — trim-revealed
    scene.child(place(box().outline(shapes::arc(-120, 300))
                          .stroke(util::stroke(2, Fill::color(alpha(kCyanRing,
                                                                    0.75f))))
                          .trim(0.0f, withFrom(0.0f, 1.0f,
                                               {700ms, &ch::easeOutQuint,
                                                2600ms})),
                      cx - 186, horizon - 296, 372, 372));

    // three helmeted figures, backlit — dome silhouette + gloss rim
    const float fx[3] = {cx - 210, cx, cx + 205};
    const float fs[3] = {0.82f, 1.0f, 0.78f};
    for (int i = 0; i < 3; ++i) {
      const float dw = 66 * fs[i], dh = 96 * fs[i];
      Element fig =
          place(box().outline([](SkSize s) {
                  SkPathBuilder b;
                  const float w2 = s.width(), h2 = s.height();
                  const float r = w2 * 0.5f;
                  b.moveTo(0, h2);
                  b.lineTo(0, r);
                  b.arcTo(SkRect::MakeWH(w2, w2), 180, 180, false);
                  b.lineTo(w2, h2);
                  b.close();
                  return b.detach();
                })
                    .fill(C(0x01080A)),
                fx[i] - dw * 0.5f, horizon - dh + 12, dw, dh);
      fig.foreground(styles::gloss(alpha(kCyanRing, 0.95f), dw * 0.10f,
                                   {0, -dh * 0.16f}, 0.30f, 0.20f));
      fig.foreground(shapes::onEdges(
          shapes::Edge::Top,
          util::stroke(1.5f, Fill::color(alpha(kGlow, 0.8f)),
                       PathFormat::Align::Inner)));
      // visor slit
      fig.child(box().absolute().left(Dim(dw * 0.22f)).top(Dim(dh * 0.30f))
                    .width(Dim(dw * 0.56f)).height(Dim(2.5f))
                    .fill(alpha(kGlow, 0.55f)));
      scene.child(fig);
    }

    // water: streak material + a mirrored, blurred copy of the portal
    Element water =
        place(box().fill(waterStreaks).clip(), 0, horizon, w, h - horizon);
    water.child(box().absolute().left(Dim(cx - 180)).top(Dim(-70))
                    .width(360).height(300)
                    .fill(Material::radial({180, 40}, 190,
                                           {{0.0f, alpha(kGlow, 0.75f)},
                                            {0.45f, alpha(kTealBar, 0.35f)},
                                            {1.0f, alpha(kTealBar, 0.0f)}}))
                    .effect(Effect::filter(SkImageFilters::Blur(14, 26, nullptr)))
                    .opacity(0.42f)
                    .blend(SkBlendMode::kPlus));
    scene.child(water);

    // THE horizon hairline — §10 says put real effort here; it is what
    // makes the reflection read at all.
    scene.child(place(box().fill(alpha(kGlow, 0.60f)), 0, horizon - 1, w, 2));
    scene.child(place(box().fill(alpha(kCyanRing, 0.18f)), 0, horizon + 2, w, 1));

    return scene;
  }

  Element hero(float w, float h) {
    using namespace tav;
    Element stackNode = stack().width(Dim(w)).height(Dim(h)).clip();
    stackNode.child(heroScene(w, h));
    // the bloom pass: the same composite, blurred, screened back over
    stackNode.child(heroScene(w, h)
                        .effect(Effect::filter(SkImageFilters::Blur(20, 20,
                                                                    nullptr)))
                        .opacity(0.45f)
                        .blend(SkBlendMode::kPlus)
                        .cache(Cache::Texture)
                        .bakeScale(0.5f));
    // vignette + scanlines + the readouts nobody reads
    stackNode.child(box().absolute().inset(0).fill(Material::radial(
        {w * 0.5f, h * 0.5f}, w * 0.62f,
        {{0.0f, {0, 0, 0, 0}}, {0.62f, {0, 0, 0, 0.10f}}, {1.0f, {0, 0, 0, 0.62f}}})));
    stackNode.child(box().absolute().inset(0).foreground(Scanlines{}));
    stackNode.child(box().absolute().inset(0).foreground(
        Brackets{alpha(kCyan, 0.7f), 22, 2, 8, 0xF}));

    auto corner = [&](const char *a, const char *b, float l, float tp,
                      bool rightAlign) {
      Element col = box().absolute().column().gap(2)
                        .alignItems(rightAlign ? Align::End : Align::Start)
                        .left(Dim(l)).top(Dim(tp))
                        .child(t(a, micro(10, alpha(kCyan, 0.75f), 220)))
                        .child(t(b, micro(10, alpha(kCyanRing, 0.45f), 220)));
      return col;
    };
    stackNode.child(corner("REND / MAXON C4D R8", "PASS 04 \xc2\xb7 FRM 0142",
                           20, 18, false));
    stackNode.child(corner("38.2144 N", "121.4944 W", w - 128, 18, true));
    stackNode.child(corner("DEPTH 00.42", "PRESS 1013 HPA", 20, h - 40, false));
    stackNode.child(box().absolute().left(Dim(w - 200)).top(Dim(h - 34))
                        .row().gap(6).alignItems(Align::Center)
                        .child(box().width(120).height(8)
                                   .foreground(TickRail{alpha(kCyan, 0.6f), 6,
                                                        3, 8, 1, 4, false,
                                                        false}))
                        .child(t("SIG 88%", micro(10, alpha(kCyan, 0.8f), 200))));
    return stackNode;
  }

  Element mainframe() {
    using namespace tav;
    Element body = box().grow(1).clip();
    body.child(hero(1180 - 6, 350 - 28 - 6));

    Element panel = doubleBevel(
        box().width(1180).height(350).column().padding(3), C(0x2A0A0C), 3);
    panel.key("mainframe")
        .translateY(withFrom(70.0f, 0.0f, {520ms, &ch::easeOutQuint, 2400ms}))
        .opacity(withFrom(0.0f, 1.0f, {300ms, &ch::easeOutQuad, 2400ms}))
        .child(panelHeader("MAIN", "FRAME", "PRIMARY VISUAL FEED", 0, 1174))
        .child(body);
    return panel;
  }

  // ---- teal monitor panels ------------------------------------------------

  Element monitorBody(float w, float h) {
    using namespace tav;
    return box().width(Dim(w)).height(Dim(h))
        .fill(Material::linear({0, 0}, {0, h},
                               {{0.00f, kPanelHi},
                                {0.15f, kPanel},
                                {0.88f, kPanel},
                                {1.00f, kPanelSh}}))
        .foreground(styles::gloss(alpha(kPanelHi, 0.55f), 40, {0, -h * 0.34f},
                                  0.72f, 0.28f))
        .foreground(shapes::onEdges(
            shapes::Edge::Top,
            util::stroke(1, Fill::color(alpha(C(0xCFEFEC), 0.7f)),
                         PathFormat::Align::Inner)));
  }

  Element featureSystem() {
    using namespace tav;
    Element thumb =
        box().width(150).height(150)
            .outline(chamfer(12, kTL | kBR))
            .fill(Material::linear({0, 0}, {0, 150},
                                   {{0.0f, C(0x06232A)}, {1.0f, C(0x011114)}}))
            .stroke(util::stroke(1, Fill::color(alpha(C(0x0B3B40), 0.9f)),
                                 PathFormat::Align::Inner))
            .child(box().absolute().inset(0).fill(Material::radial(
                {75, 108}, 92,
                {{0.0f, alpha(kGlow, 0.8f)},
                 {0.5f, alpha(kTealBar, 0.30f)},
                 {1.0f, alpha(kTealBar, 0.0f)}})))
            .child(place(box().fill(C(0x010A0C)), 18, 74, 30, 60))
            .child(place(box().fill(C(0x02171B)), 52, 46, 44, 88))
            .child(place(box().fill(C(0x010A0C)), 100, 62, 34, 72))
            .child(place(box().fill(alpha(kGlow, 0.55f)), 0, 108, 150, 1))
            .foreground(Brackets{alpha(kCyan, 0.8f), 12, 2, 4, 0xF})
            .foreground(Scanlines{{0, 0, 0, 0.22f}, 3, 1});

    Element copy =
        box().grow(1).column().gap(6)
            .child(box().row().gap(7).alignItems(Align::Center)
                       .child(box().width(9).height(9)
                                  .outline(shapes::polygon(3, 90))
                                  .fill(kDate))
                       .child(t("01.30.06", type(black(), 14, kDate, 40, 0.95f)))
                       .child(box().grow(1).height(1).fill(alpha(kDate, 0.35f))))
            .child(t("N.O.-XPLODE TV COMMERCIAL",
                     type(black(), 17, C(0x0E3234), 40, 0.92f)))
            .child(box().grow(1).padding(9)
                       .fill(dither.material())
                       .foreground(util::stroke(
                           1, Fill::color(alpha(kPanelSh, 0.9f)),
                           PathFormat::Align::Inner))
                       .child(t("2Advanced completes a broadcast spot for BSN's "
                                "N.O.-Xplode line \xe2\x80\x94 full CG "
                                "environment, character rig and compositing "
                                "delivered in nine weeks. Shot on a Maxon "
                                "pipeline against a live-action plate.",
                                prose(13, C(0x0B2C2E)))))
            .child(box().row().gap(8).alignItems(Align::Center)
                       .child(t("\xe2\x80\xba VIEW CASE STUDY",
                                micro(11, C(0x123B3D), 220)))
                       .child(box().grow(1)));

    Element bodyArea =
        monitorBody(696 - 6, 350 - 28 - 6).row().padding(11).gap(11)
            .child(thumb)
            .child(copy);
    // the hazard wedge in the bottom-left corner (the STATIC baked tile)
    bodyArea.child(box().absolute().left(Dim(0))
                       .top(Dim(350.0f - 28 - 6 - 46)).width(150).height(46)
                       .outline([](SkSize s) {
                         SkPathBuilder b;
                         b.moveTo(0, 0);
                         b.lineTo(s.width(), s.height());
                         b.lineTo(0, s.height());
                         b.close();
                         return b.detach();
                       })
                       .fill(hazard.material())
                       .opacity(0.5f));
    bodyArea.child(box().absolute().left(Dim(696.0f - 6 - 11 - 116))
                       .top(Dim(350.0f - 28 - 6 - 11 - 34))
                       .child(cta("LAUNCH", 116, 34, kPanelSh)));

    Element panel = doubleBevel(
        box().width(696).height(350).column().padding(3), C(0x2A0A0C), 3);
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
      const char *date;
      const char *headline;
      const char *body;
    };
    static const Entry entries[6] = {
        {"+ 04.05.06", "PROPHECY PRIME SKIN RELEASED",
         "The v4 desktop suite ships with the new app skin, wallpaper set and "
         "the FIBERGLASS icon pack."},
        {"+ 03.11.06", "2ADVANCED NAMED FWA SITE OF THE MONTH",
         "Prophecy takes the month for interface design and sound integration."},
        {"+ 01.30.06", "N.O.-XPLODE SPOT NOW AIRING",
         "Nine weeks, full CG environment, delivered on a Maxon pipeline."},
        {"+ 01.07.06", "EQUIPMENT PAGE UPDATED",
         "New render nodes online; the studio moves to a dual-Xeon farm."},
        {"+ 12.14.05", "EXPERIMENTAL SECTION REOPENS",
         "Six new motion studies posted under the experimental banner."},
        {"+ 11.02.05", "HOLIDAY DESKTOP SET",
         "Three widescreen wallpapers in the Prophecy palette."},
    };

    Element list = box().column().gap(9).translateY(&pressScroll);
    for (const Entry &e : entries) {
      list.child(
          box().column().gap(4)
              .child(box().row().gap(7).alignItems(Align::Center)
                         .fill(alpha(kPanelSh, 0.55f))
                         .padding(6, 3)
                         .child(t(e.date, type(black(), 13, kDate, 40, 0.95f)))
                         .child(box().grow(1).height(1)
                                    .fill(alpha(kDate, 0.30f)))
                         .child(t("\xe2\x96\xb8", micro(9, kDate, 0))))
              .child(t(e.headline, type(black(), 13, C(0x0E3234), 50, 0.92f)))
              .child(t(e.body, prose(12.5f, C(0x0C2E30)))));
    }

    Element scrollbar =
        box().width(16).column().gap(3)
            .child(box().width(16).height(16).fill(kPanelSh)
                       .justify(Justify::Center).alignItems(Align::Center)
                       .child(t("\xe2\x96\xb4", micro(8, kBody, 0))))
            .child(box().grow(1).fill(alpha(kPanelSh, 0.6f))
                       .child(box().absolute().left(Dim(2)).top(Dim(6))
                                  .width(12).height(90)
                                  .fill(Material::linear(
                                      {0, 0}, {12, 0},
                                      {{0.0f, C(0xCFEFEC)}, {1.0f, kPanelHi}}))
                                  .stroke(util::stroke(
                                      1, Fill::color(alpha(kDate, 0.4f)),
                                      PathFormat::Align::Inner))))
            .child(box().width(16).height(16).fill(kPanelSh)
                       .justify(Justify::Center).alignItems(Align::Center)
                       .child(t("\xe2\x96\xbe", micro(8, kBody, 0))));

    Element window =
        box().grow(1).row().gap(8)
            .child(box().grow(1).clip().padding(9)
                       .fill(dither.material())
                       .foreground(util::stroke(
                           1, Fill::color(alpha(kPanelSh, 0.9f)),
                           PathFormat::Align::Inner))
                       .child(list))
            .child(scrollbar);

    Element bodyArea = monitorBody(696 - 6, 410 - 28 - 6)
                           .column().padding(11).gap(9)
                           .child(window)
                           .child(box().row().alignItems(Align::Center).gap(8)
                                      .child(t("06 ENTRIES \xc2\xb7 PAGE 1/4",
                                               micro(11, C(0x123B3D), 220)))
                                      .child(box().grow(1))
                                      .child(cta("ARCHIVES", 116, 34,
                                                 kPanelSh)));

    Element panel = doubleBevel(
        box().width(696).height(410).column().padding(3), C(0x2A0A0C), 3);
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
        {"\xe2\x96\xa0", "BROADCAST", "Live studio feed, weeknights",
         "22:00 PST on the Prophecy channel.", "\xe2\x80\xba OFFLINE"},
        {"\xe2\x96\xb2", "DISPATCH", "Monthly transmission to 41,208",
         "subscribers. No spam, ever.", "\xe2\x80\xba SUBSCRIBE"},
        {"\xe2\x97\x86", "DESKTOPS", "Fourteen widescreen wallpapers",
         "in the Prophecy palette.", "\xe2\x80\xba VIEW"},
    };
    Element row = box().width(1180).height(154).row().gap(10).padding(3);
    for (int i = 0; i < 3; ++i) {
      Element col =
          singleBevel(box().grow(1).column().padding(9).gap(6), C(0x33090C));
      col.foreground(TickRail{alpha(kDust, 0.35f), 7, 3, 6, 1, 4, false, true})
          .child(box().row().gap(8).alignItems(Align::Center)
                     .child(box().width(32).height(32)
                                .outline(chamfer(8, kTL | kBR))
                                .fill(Material::linear(
                                    {0, 0}, {0, 32},
                                    {{0.0f, C(0x5A1A20)}, {1.0f, C(0x220608)}}))
                                .stroke(util::stroke(
                                    1, Fill::color(alpha(kCta, 1.0f)),
                                    PathFormat::Align::Inner))
                                .justify(Justify::Center)
                                .alignItems(Align::Center)
                                .child(t(cols[i].icon, micro(12, kCyan, 0))))
                     .child(box().column().gap(2)
                                .child(t(cols[i].title, heavy(15, kNear, 60)))
                                .child(t("AUXILIARY MODULE",
                                         micro(9, kDustDim, 240))))
                     .child(box().grow(1))
                     .child(tickDots(3 + i, alpha(kCyan, 0.9f))))
          .child(box().column().gap(1)
                     .child(t(cols[i].l1, prose(12.5f, kBody)))
                     .child(t(cols[i].l2, prose(12.5f, alpha(kBody, 0.7f)))))
          .child(box().grow(1))
          .child(box().row().alignItems(Align::Center).gap(8)
                     .child(t(cols[i].link, micro(11, alpha(kCyan, 0.9f), 220)))
                     .child(box().grow(1))
                     .child(box().width(120).height(24)
                                .outline(chamfer(7, kTL | kBR))
                                .fill(Material::linear(
                                    {0, 0}, {0, 24},
                                    {{0.0f, alpha(kPanelHi, 0.95f)},
                                     {0.5f, alpha(kPanel, 0.95f)},
                                     {1.0f, alpha(kPanelSh, 0.95f)}}))
                                .foreground(styles::gloss(
                                    {1, 1, 1, 0.55f}, 8, {0, -7}, 0.60f, 0.30f))
                                .stroke(util::stroke(
                                    1, Fill::color(alpha(C(0xCFEFEC), 0.6f)),
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

  /** The gap the FWA screenshot leaves under AUXILIARY on the left column:
   *  a transmission-log strip, the densest readout on the page. */
  Element transmissionLog() {
    using namespace tav;
    Element bars = box().row().gap(3).alignItems(Align::End).height(74);
    for (int i = 0; i < 46; ++i) {
      const float v = 0.18f + 0.82f * std::abs(std::sin(i * 0.73f) *
                                               std::cos(i * 0.31f + 1.1f));
      bars.child(box().width(7).height(Dim(74 * v))
                     .fill(Material::linear({0, 0}, {0, 74 * v},
                                            {{0.0f, alpha(kCyan, 0.95f)},
                                             {1.0f, alpha(kTealBar, 0.35f)}})));
    }

    Element hexRows = box().column().gap(2);
    static const char *hexLines[7] = {
        "0x0000  4A 10 0F 57 11 19 6A 1B  21 7B DA D6 95 C9 CC 8D",
        "0x0010  77 77 2C 7B 80 01 D0 D5  57 97 97 84 B8 B6 3C 82",
        "0x0020  82 70 00 00 B2 7E 82 F3  F3 F3 18 07 07 26 09 09",
        "0x0030  37 0C 0D 40 0E 0F 4C 10  10 2A 00 00 1A 00 01 0A",
        "0x0040  46 4C 41 53 48 49 4E 44  45 58 2E 53 57 46 00 00",
        "0x0050  43 57 53 06 EC 45 08 00  78 9C 00 00 00 00 00 00",
        "0x0060  50 52 4F 50 48 45 43 59  20 50 52 49 4D 45 00 00",
    };
    for (int i = 0; i < 7; ++i)
      hexRows.child(t(hexLines[i], type(arial(), 11,
                                        alpha(kCyanRing, i < 5 ? 0.65f : 0.35f),
                                        60)));

    Element gauges = box().row().gap(10).alignItems(Align::Center);
    for (int i = 0; i < 3; ++i) {
      gauges.child(
          box().width(58).height(58)
              .fill(sdf::material(sdf::circle(),
                                  {.fill = C(0x140505),
                                   .borderWidth = 2,
                                   .borderColor = alpha(kChromeHi, 1.0f)}))
              .justify(Justify::Center).alignItems(Align::Center)
              .child(box().absolute().inset(6).corners({26})
                         .fill(Material::sweep({23, 23},
                                               {{0.00f, alpha(kCyan, 0.0f)},
                                                {0.55f, alpha(kCyan, 0.0f)},
                                                {0.93f, alpha(kCyan, 0.55f)},
                                                {1.00f, alpha(kCyan, 0.9f)}}))
                         .rotate(&gauge[(size_t)i]))
              .child(t(i == 0 ? "A" : (i == 1 ? "B" : "C"),
                       micro(11, alpha(kCyan, 0.85f), 120))));
    }

    Element panel = singleBevel(
        box().width(1180).height(236).column().padding(9).gap(7), C(0x33090C));
    panel.key("txlog")
        .translateY(withFrom(56.0f, 0.0f, {400ms, &ch::easeOutQuint, 3400ms}))
        .opacity(withFrom(0.0f, 1.0f, {320ms, &ch::easeOutQuad, 3400ms}))
        .foreground(Brackets{alpha(kCyan, 0.5f), 14, 2, 5, kTR | kBL})
        .child(box().row().alignItems(Align::Center).gap(9).height(20)
                   .child(t("TRANSMISSION", heavy(15, kNear, 40)))
                   .child(t(" LOG", type(arial(), 14, kHeadDim, 40, 0.95f)))
                   .child(box().width(1).height(12).fill(alpha(kCyan, 0.4f)))
                   .child(t("BUFFER 04 / 16 \xc2\xb7 UPLINK NOMINAL",
                            micro(11, kDust, 240)))
                   .child(box().grow(1))
                   .child(tickDots(6))
                   .child(box().width(120).height(10)
                              .foreground(TickRail{alpha(kCyan, 0.5f), 5, 3, 8,
                                                   1, 4, false, true})))
        .child(box().row().gap(10).grow(1)
                   .child(box().width(400).padding(7)
                              .fill(C(0x180505))
                              .foreground(InsetBevel{alpha(kChromeHi, 0.5f),
                                                     {0, 0, 0, 0.5f}, 0, 1, 1})
                              .foreground(Brackets{alpha(kCyan, 0.4f), 9, 2, 3,
                                                   0xF})
                              .child(bars))
                   .child(box().grow(1).padding(8)
                              .fill(C(0x140404))
                              .foreground(InsetBevel{alpha(kChromeHi, 0.5f),
                                                     {0, 0, 0, 0.5f}, 0, 1, 1})
                              .child(hexRows))
                   .child(box().column().gap(6).alignItems(Align::Center)
                              .padding(8, 4)
                              .fill(C(0x1B0607))
                              .foreground(InsetBevel{alpha(kChromeHi, 0.5f),
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
          .fill(Material::linear({0, 0}, {0, 40},
                                 {{0.0f, C(0x6A1B21)}, {1.0f, C(0x220608)}}))
          .stroke(util::stroke(1, Fill::color(alpha(kDust, 0.5f)),
                               PathFormat::Align::Inner))
          .justify(Justify::Center).alignItems(Align::Center)
          .child(t(glyph, heavy(15, kCyan, 0)));
    };
    auto selector = [&](const char *lbl, const char *value) {
      return box().row().gap(8).alignItems(Align::Center)
          .child(box().width(46).height(34)
                     .outline(chamfer(8, kTL | kBR))
                     .fill(Material::linear({0, 0}, {0, 34},
                                            {{0.0f, C(0x05262B)},
                                             {1.0f, C(0x010D10)}}))
                     .stroke(util::stroke(1, Fill::color(alpha(kCyan, 0.5f)),
                                          PathFormat::Align::Inner))
                     .child(box().absolute().inset(0).fill(Material::radial(
                         {23, 26}, 26,
                         {{0.0f, alpha(kGlow, 0.55f)},
                          {1.0f, alpha(kGlow, 0.0f)}}))))
          .child(box().column().gap(1)
                     .child(t(lbl, micro(10, kDustDim, 240)))
                     .child(box().row().gap(5).alignItems(Align::Center)
                                .child(t("\xe2\x96\xb8", micro(9, kCyan, 0)))
                                .child(t(value, label(13, kNear, 90)))
                                .child(t("\xe2\x96\xbe", micro(9, kDust, 0)))));
    };

    Element row =
        singleBevel(box().width(1892).height(100).row().alignItems(Align::Center)
                        .padding(14, 0).gap(20),
                    C(0x2E0B0D));
    row.key("subsys")
        .background(Overlay{hazard.material(), SkBlendMode::kSrcOver, 0.18f})
        .opacity(withFrom(0.0f, 1.0f, {400ms, &ch::easeOutQuad, 3650ms}))
        .foreground(TickRail{alpha(kDust, 0.35f), 9, 4, 8, 1, 4, false, false})
        .child(t("SUB", heavy(15, kNear, 40)))
        .child(t("SYSTEM", type(arial(), 14, kHeadDim, 40, 0.95f)))
        .child(box().width(1).height(30).fill(alpha(kDust, 0.35f)))
        .child(t("PARTNERS:", micro(11, kDust, 240)))
        .child(chip("A"))
        .child(chip("M"))
        .child(box().width(1).height(30).fill(alpha(kDust, 0.35f)))
        .child(selector("DESKTOPS", "'FIBERGLASS'"))
        .child(box().width(1).height(30).fill(alpha(kDust, 0.35f)))
        .child(selector("APP SKINS", "'PROPHECY PRIME'"))
        .child(box().grow(1))
        .child(box().column().alignItems(Align::End).gap(3)
                   .child(t("BANDWIDTH  \xe2\x96\xb8\xe2\x96\xb8\xe2\x96\xb8"
                            "\xe2\x96\xb8\xe2\x96\xb8\xe2\x96\xa1\xe2\x96\xa1",
                            micro(11, alpha(kCyan, 0.8f), 200)))
                   .child(t("UPTIME 118:24:07", micro(10, kDustDim, 200))))
        .child(box().width(70).height(40)
                   .foreground(TickRail{alpha(kCyan, 0.45f), 6, 4, 10, 1, 3,
                                        false, true}));
    return row;
  }

  Element legalStrip() {
    using namespace tav;
    return box().width(1892).height(90).column().padding(6, 8)
        .alignItems(Align::Center).gap(4)
        .key("legal")
        .opacity(withFrom(0.0f, 1.0f, {400ms, &ch::easeOutQuad, 3750ms}))
        .child(box().row().width(Dim(1892)).alignItems(Align::Center)
                   .child(box().row().gap(6).alignItems(Align::Center)
                              .child(box().width(60).height(1)
                                         .fill(alpha(kDust, 0.35f)))
                              .child(t("SITE REQUIRES MACROMEDIA FLASH PLAYER 6",
                                       micro(10, kDustDim, 200))))
                   .child(box().grow(1))
                   .child(box().row().gap(7).alignItems(Align::Center)
                              .child(t("ARCHIVED VERSIONS:",
                                       micro(11, kDust, 240)))
                              .child(box().height(24).padding(8, 0)
                                         .outline(chamfer(7, kTL | kBR))
                                         .fill(C(0x2A0A0C))
                                         .stroke(util::stroke(
                                             1, Fill::color(alpha(kDust, 0.45f)),
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
                   .child(box().width(1).height(10).fill(alpha(kDust, 0.35f)))
                   .child(t("PRIVACY POLICY", micro(11, kDustDim, 200)))
                   .child(box().width(1).height(10).fill(alpha(kDust, 0.35f)))
                   .child(t("SITE MAP", micro(11, kDustDim, 200))));
  }

  /** The 970×110 sitefooter.gif, ×2 — deliberately monochrome oxblood,
   *  no accent colour anywhere. Crosshatch dither, bracketed windows,
   *  chevrons, and the three-circle gauge cluster in a dark bezel. */
  Element footerDock() {
    using namespace tav;
    Element strip =
        box().width(1892).height(220)
            .fill(Material::blend({{Material::linear({0, 0}, {0, 220},
                                                     {{0.0f, kD5},
                                                      {0.45f, kD2},
                                                      {1.0f, kD1}}),
                                    SkBlendMode::kSrcOver},
                                   {hatchA.material(), SkBlendMode::kSrcOver},
                                   {hatchB.material(), SkBlendMode::kSrcOver}}))
            .row().alignItems(Align::Center).padding(14, 12).gap(12)
            .key("dock")
            .opacity(withFrom(0.0f, 1.0f, {400ms, &ch::easeOutQuad, 3850ms}));

    strip.foreground(shapes::onEdges(
        shapes::Edge::Top,
        util::stroke(2, Fill::color(alpha(kD5, 1.0f)),
                     PathFormat::Align::Inner)));

    auto window = [&](const char *title, const char *a, const char *b,
                      float w) {
      return readout(w, 150, C(0x140404))
          .column().padding(10).gap(5)
          .child(box().row().alignItems(Align::Center).gap(6)
                     .child(t(title, type(black(), 12, alpha(kD5, 1.0f), 60,
                                          0.92f)))
                     .child(box().grow(1).height(1).fill(alpha(kD4, 0.9f)))
                     .child(t("\xc2\xbb", micro(11, kD5, 0))))
          .child(t(a, micro(10, alpha(kD5, 0.9f), 220)))
          .child(t(b, micro(10, alpha(kD4, 1.0f), 220)))
          .child(box().grow(1))
          .child(box().row().gap(5).alignItems(Align::Center)
                     .child(box().width(58).height(8)
                                .foreground(TickRail{alpha(kD5, 1.0f), 5, 3, 7,
                                                     1, 3, false, false}))
                     .child(box().grow(1))
                     .child(t("v v", micro(10, kD4, 200))));
    };

    strip.child(window("NAVIGATION", "SECTOR / PROPHECY", "NODE 04.11.22", 300));
    strip.child(window("EQUIPMENT", "RENDER FARM  08/08", "STORAGE  4.2 TB",
                       300));
    strip.child(window("DISPATCH", "QUEUE  00114", "LAST  04.05.06", 280));

    // the instanced chevron tick array — one atlas cell, one draw
    strip.child(box().width(260).height(150)
                    .outline(chamfer(7, kTR | kBL))
                    .fill(C(0x110303))
                    .foreground(InsetBevel{alpha(kD5, 0.8f), {0, 0, 0, 0.6f}, 0,
                                           1, 1})
                    .child(box().absolute().inset(10)
                               .child(instancing::instances(dockAtlas, dockPool,
                                                            instancing::Mode::Data)))
                    .child(box().absolute().left(Dim(10)).top(Dim(126))
                               .child(t("ARRAY 6\xc3\x9714 \xc2\xb7 IDLE",
                                        micro(10, alpha(kD5, 0.9f), 220)))));

    strip.child(box().grow(1));

    // three-circle gauge cluster in its bezel
    Element cluster =
        box().width(310).height(150)
            .outline(chamfer(9, kTL | kBR))
            .fill(Material::linear({0, 0}, {0, 150},
                                   {{0.0f, kD3}, {1.0f, C(0x0C0202)}}))
            .foreground(InsetBevel{alpha(kD5, 0.9f), {0, 0, 0, 0.6f}, 5, 2, 1})
            .foreground(Brackets{alpha(kD5, 1.0f), 12, 2, 5, 0xF})
            .row().justify(Justify::Center).alignItems(Align::Center).gap(14);
    for (int i = 0; i < 3; ++i) {
      cluster.child(
          box().width(80).height(80)
              .fill(sdf::material(sdf::circle(),
                                  {.fill = C(0x0A0202),
                                   .borderWidth = 3,
                                   .borderColor = kD5,
                                   .glowRadius = 0}))
              .justify(Justify::Center).alignItems(Align::Center)
              .child(box().absolute().inset(9).corners({31})
                         .fill(Material::sweep({31, 31},
                                               {{0.00f, alpha(kD5, 0.0f)},
                                                {0.60f, alpha(kD5, 0.0f)},
                                                {0.94f, alpha(kD5, 0.85f)},
                                                {1.00f, C(0x8A2A2C)}}))
                         .rotate(&gauge[(size_t)i]))
              .child(box().absolute().inset(24).corners({16})
                         .fill(alpha(kD1, 0.9f))
                         .stroke(util::stroke(1, Fill::color(alpha(kD4, 1.0f)),
                                              PathFormat::Align::Inner)))
              .child(t(i == 0 ? "01" : (i == 1 ? "02" : "03"),
                       micro(11, alpha(kD5, 1.0f), 140))));
    }
    strip.child(cluster);
    return strip;
  }

  Element rail(bool right) {
    using namespace tav;
    return box().absolute().left(Dim(right ? 1916.0f : 0.0f)).top(Dim(0))
        .width(24).height(Dim(1560))
        .fill(Material::linear({0, 0}, {0, 1560},
                               {{0.00f, C(0x6A1B21)},
                                {0.22f, kChrome},
                                {0.70f, C(0x2A0708)},
                                {1.00f, C(0x0A0000)}}))
        .foreground(shapes::onEdges(
            right ? shapes::Edge::Left : shapes::Edge::Right,
            util::stroke(1, Fill::color(alpha(C(0x99AAAA), 0.35f)),
                         PathFormat::Align::Inner)))
        .foreground(RailFlares{C(0x99AAAA), 6.0f, right ? 3.0f : 0.0f})
        .cache(Cache::None);
  }

  // ---- boot overlay (§9 beats 1–4) ---------------------------------------

  Element bootOverlay() {
    using namespace tav;
    const float cx = 970, cy = 780;

    auto hair = [&](float x, float y, float w, float h, float dx, float dy,
                    int delayMs) {
      return place(box().outline(ray(dx, dy))
                       .stroke(util::stroke(1.5f, Fill::color(kCyan)))
                       .trim(0.0f, withFrom(0.0f, 1.0f,
                                            {400ms, &ch::easeOutQuint,
                                             std::chrono::milliseconds(delayMs)})),
                   x, y, w, h);
    };

    Element o = stack().absolute().inset(0).zIndex(90);
    // the flat black card the boot sequence plays on
    o.child(box().absolute().inset(0).fill(C(0x120303))
                .opacity(withKeyframes<float>({{0ms, 1.0f},
                                               {1400ms, 1.0f},
                                               {1560ms, 0.0f}})));
    // 1. the pixel dot
    o.child(place(box().fill(kCyan), cx - 3, cy - 3, 6, 6)
                .opacity(withKeyframes<float>({{0ms, 0.0f},
                                               {150ms, 1.0f},
                                               {1350ms, 1.0f},
                                               {1450ms, 0.0f}})));
    // 2. the reticle, drawing OUTWARD from the dot on four rays
    o.child(hair(cx - 460, cy, 460, 1, -1, 1, 150));
    o.child(hair(cx, cy, 460, 1, 1, 1, 150));
    o.child(hair(cx, cy - 300, 1, 300, 1, -1, 220));
    o.child(hair(cx, cy, 1, 300, 1, 1, 220));
    o.child(place(box().outline(shapes::arc(-90, 359))
                      .stroke(util::stroke(1, Fill::color(alpha(kCyan, 0.7f))))
                      .trim(0.0f, withFrom(0.0f, 1.0f,
                                           {500ms, &ch::easeOutQuint, 260ms})),
                  cx - 90, cy - 90, 180, 180));
    // 3. the readout + the filling hairline
    o.child(place(box().column().alignItems(Align::Center).gap(9),
                  cx - 260, cy + 120, 520, 90)
                .opacity(withKeyframes<float>({{520ms, 0.0f},
                                               {620ms, 1.0f},
                                               {1350ms, 1.0f},
                                               {1450ms, 0.0f}}))
                .child(slot("bootpct"))
                .child(box().width(420).height(2).fill(alpha(kCyan, 0.18f))
                           .child(box().absolute().inset(0)
                                      .outline(ray(1, 1))
                                      .stroke(util::stroke(
                                          2, Fill::color(kCyan)))
                                      .trim(0.0f,
                                            withFrom(0.0f, 1.0f,
                                                     {800ms, &ch::easeNone,
                                                      550ms}))))
                .child(t("LOADING PROPHECY INTERFACE \xc2\xb7 970\xc3\x97655",
                         micro(11, alpha(kCyan, 0.6f), 240))));
    // 4. the boot-complete flash
    o.child(box().absolute().inset(0).fill(SkColor4f{1, 1, 1, 1})
                .opacity(withKeyframes<float>({{1330ms, 0.0f},
                                               {1390ms, 0.75f},
                                               {1460ms, 0.0f}}))
                .blend(SkBlendMode::kPlus));
    o.opacity(withKeyframes<float>({{1440ms, 1.0f}, {1470ms, 0.0f}}));
    return o;
  }

  Element bootReadout() {
    using namespace tav;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%03d", bootPct);
    return box().row().alignItems(Align::Baseline).gap(6)
        .child(t(buf, type(black(), 46, kCyan, 40, 0.9f)))
        .child(t("%", type(black(), 20, alpha(kCyan, 0.6f), 40, 0.9f)));
  }

  // =========================================================================

  Element describe(sketch::SketchContext &ctx) {
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

    return stack()
        .fill(Material::linear({0, 0}, {0, 1560},
                               {{0.00f, kBgTop},
                                {0.40f, kBgMid},
                                {1.00f, kBgBot}}))
        .child(box().absolute().inset(0).fill(grain).opacity(0.05f)
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

    // --- generated materials, built ONCE and held (identity = pruning) ---
    hazard = patterns::stripes(6, 10, kChromeHi);
    hazard.rotate(45);
    hatchA = patterns::stripes(1, 7, alpha(kD4, 0.55f));
    hatchA.rotate(45);
    hatchB = patterns::stripes(1, 7, alpha(kD1, 0.6f));
    hatchB.rotate(-45);
    dither = patterns::checker(1.5f, alpha(kPanelSh, 0.30f),
                               alpha(kPanelHi, 0.16f));
    grain = patterns::noise(0.85f, 3, 4.0f, true);

    spectrum = Material::sksl(spectrumFx(), {{"uBars", 34.0f}})
                   .uniform("uHot", alpha(kGlow, 1.0f))
                   .uniform("uCool", alpha(kTealBar, 1.0f))
                   .quantizeTime(10.0f); // §8: a deliberately chunky readout

    // ONE stripe material value, reused on the nav bar and all four panel
    // headers — a live uniform pan, not five redraw loops.
    stripesLive = Material::sksl(stripeFx(), {{"uOn", 6.0f}, {"uPeriod", 16.0f}})
                      .uniform("uColor", kChromeHi)
                      .uniform("uBase", kChrome)
                      .uniform("uPan", &stripePan);

    waterStreaks = Material::sksl(waterFx());

    // --- the instanced chevron array in the footer dock ---
    dockAtlas = std::make_shared<instancing::Atlas>(2.0f);
    const int chev = dockAtlas->cell(
        box().width(12).height(10)
            .outline([](SkSize s) {
              SkPathBuilder b;
              b.moveTo(0, 0);
              b.lineTo(s.width() * 0.55f, s.height() * 0.5f);
              b.lineTo(0, s.height());
              b.lineTo(s.width() * 0.30f, s.height() * 0.5f);
              b.close();
              return b.detach();
            })
            .fill(alpha(kD5, 1.0f)),
        {12, 10});
    dockPool = std::make_shared<instancing::Pool>();
    instancing::place::grid(*dockPool, 6 * 14, 14, {16, 14}, {0, 0}, {2, 4});
    {
      auto frames = dockPool->frames();
      auto tints = dockPool->tints();
      for (size_t i = 0; i < frames.size(); ++i) {
        frames[i] = chev;
        const float k = 0.35f + 0.65f * (float)((i * 7 + 3) % 11) / 10.0f;
        tints[i] = {1, k, k, 0.45f + 0.55f * k};
      }
      dockPool->touch();
    }

    // --- declared motion -------------------------------------------------
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      const float s = (float)t;
      wavePhase = s;
      stripePan = s * 2.5f;                       // §9: 20 px / 8 s
      portalGlow = 54.0f + 4.4f * std::sin(s * 1.5708f); // ±8%, period 4 s
      vuLeft = 0.45f + 0.42f * std::abs(std::sin(s * 3.1f));
      vuRight = 0.40f + 0.45f * std::abs(std::sin(s * 2.3f + 1.1f));

      // 7 tick clusters, each blinking its 3 dots in sequence with a
      // per-panel phase offset so they never lock step.
      for (int p = 0; p < 7; ++p) {
        const float off = (float)((p * 137) % 800) / 1000.0f;
        const float cyc = std::fmod(s + off, 2.7f);
        for (int i = 0; i < 3; ++i) {
          const float on = (cyc >= i * 0.4f && cyc < i * 0.4f + 0.32f);
          dot[(size_t)(p * 3 + i)] = 0.22f + 0.78f * on;
        }
      }

      // three gauges round-robin a radar sweep, 1 s each
      const int active = (int)std::fmod(s / 1.0, 3.0);
      for (int i = 0; i < 3; ++i)
        gauge[(size_t)i] = (i == active)
                               ? (float)std::fmod(s * 120.0, 360.0)
                               : gauge[(size_t)i].value();

      // press list: 2.5 s dwell then a 600 ms eased step, 6 entries
      const float span = 3.1f;
      const float u = std::fmod(std::max(0.0f, s - 4.0f), span * 6.0f);
      const int idx = (int)(u / span);
      float f = (u - idx * span - 2.5f) / 0.6f;
      f = f < 0 ? 0.0f : (f > 1 ? 1.0f : f);
      f = f < 0.5f ? 2 * f * f : 1 - 2 * (1 - f) * (1 - f); // easeInOutQuad
      pressScroll = -66.0f * ((float)idx + f);
      return true;
    });

    ctx.composer.render(describe(ctx));
    ctx.composer.renderSlot("bootpct", bootReadout());
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    // The DATA path, used for exactly one thing: the boot percentage is
    // TEXT CONTENT, not a paint property, so it cannot be a binding.
    // A slot() keeps the update local — the rest of the tree is untouched.
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

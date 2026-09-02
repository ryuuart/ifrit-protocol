// twoadvanced_v4.cpp — 2Advanced Studios, v4 "Prophecy" (2003–2006),
// the post-splash Home/Index interface (flashindex.swf, a 970×655 stage).
// Creative direction / design / UI / Flash animation: Eric Jordan.
//
// REFERENCE (everything measured, not remembered):
//   ·
//   http://web.archive.org/web/20040610062326/http://2advanced.com/flashindex.htm
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
//   · .../mp3Info.xml — the 2004 playlist; the 2006 one (01. DESERT
//     TRANCE / 02. NEVERRAIN / 03. SYNERGY / 04. EXHILE, track 01
//     selected) is read off 2Advanced's own 1200×900 interface capture,
//     https://v4prophecy.2advanced.com/images/prophecy-v4-flash-site.png.
//   · https://v4prophecy.2advanced.com/ — 2Advanced's own Ruffle
//     restoration of this exact movie. Its HTML shell still serves the
//     PRODUCTION artefacts, and this sketch draws them directly:
//     images/{leftsidepanel,rightsidepanel,sitebackground,sitefooter}.gif
//     and images/2alogobug.svg arrive over SigilLoader's https path
//     (fetched once, then served from the loader's disk cache). Every
//     real-asset site keeps its procedural stand-in as the fallback, so
//     a run with no network and a cold cache still renders — just with
//     rebuilt chrome instead of the original bitmaps.
//
// THE PALETTE IS NOT CYAN/ORANGE. The genre's reputation says orange;
// the chrome carries none. It is cyan-teal (#7BDAD6 / #01D0D5 / #579797)
// against oxblood (#571119 / #4A100F), with blood red (#700000) — not
// orange — on the CTAs. The single amber pixel-group on the whole page
// is the 2ADVANCED.NET press logo's mark inside AUXILIARY PANEL. Do not
// "correct" any of this from memory.
//
// The MAINFRAME hero was a real Cinema 4D composite (the site's own press
// copy name-checks Maxon). There is no 3D here: flat SDF shapes, one
// radial portal, an opacity-ramped skyline, three dome silhouettes with
// gloss rim-light, a mirrored blurred reflection under a hard horizon
// hairline, and a blurred kPlus bloom copy over the whole stack. Stacking
// ORDER is what does the work, not any one layer.
//
// Two things the hero depends on that no reference image states:
//  · the FOG RAMP runs toward the centre, not away from it: the near
//    buildings are the hard silhouettes and the far ones dissolve. Keyed
//    the other way — opacity to |x − cx| — the skyline reads as a bar
//    chart. And none of it reads at all without a full-width kPlus haze
//    band UNDER the skyline, because the outer thirds are otherwise
//    black-on-black.
//  · the reflection is sold by the specular COLUMN, not the blur. A
//    vertically-smeared bar of glow under the portal reads as water from
//    across the room; the mirrored blurred disc alone does not.

#include <include/core/SkFontMgr.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/brush/LayerStyles.h>
#include <sigilcompose/core/Material.h>
#include <sigilcompose/core/Pattern.h>
#include <sigilcompose/core/Patterns.h>
#include <sigilcompose/core/Sdf.h>
#include <sigilcompose/instances/Instances.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Placers.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilcompose/typography/TextFx.h>
#include <sigilcompose/typography/Type.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
// Absolute placement: this composition is pinned, so a node says
// where it goes rather than a layout deciding.
using sigil::compose::kit::at;
using namespace std::chrono_literals;
namespace ch = choreograph;

namespace tav {

// ---------------------------------------------------------------------------
// Palette — every value sampled from one of the reference artefacts above,
// never picked by eye.

constexpr SkColor4f kBgTop = hex(0x4A100F);  // page gradient, top
constexpr SkColor4f kBgMid = hex(0x1A0001);
constexpr SkColor4f kBgBot = hex(0x0A0000);   // …faded to near-black
constexpr SkColor4f kChrome = hex(0x571119);  // THE chrome maroon
constexpr SkColor4f kChromeHi = hex(0x6A1B21);
constexpr SkColor4f kD1 = hex(0x180707);  // footer-dock HUD darks
constexpr SkColor4f kD2 = hex(0x260909);
constexpr SkColor4f kD3 = hex(0x370C0D);
constexpr SkColor4f kD4 = hex(0x400E0F);
constexpr SkColor4f kD5 = hex(0x4C1010);
constexpr SkColor4f kD6 = hex(0x7A2626);    // dock hairline/label ink
constexpr SkColor4f kD7 = hex(0xA34040);    // dock title ink
constexpr SkColor4f kCyan = hex(0x7BDAD6);  // logo wordmark core
constexpr SkColor4f kCyanRing = hex(0x95C9CC);
constexpr SkColor4f kDust = hex(0x8D7777);  // the dusty-rose third neutral
constexpr SkColor4f kDustDim = hex(0x735757);
constexpr SkColor4f kTealBar = hex(0x2C7B80);  // status-bar segment
constexpr SkColor4f kGlow = hex(0x01D0D5);     // MAINFRAME portal core
constexpr SkColor4f kPanel = hex(0x579797);    // monitor-panel body
constexpr SkColor4f kPanelHi = hex(0x84B8B6);
constexpr SkColor4f kPanelSh = hex(0x3C8282);
constexpr SkColor4f kCta = hex(0x700000);  // LAUNCH / ARCHIVES core
constexpr SkColor4f kCtaHi = hex(0xB27E82);
constexpr SkColor4f kNear = hex(0xF3F3F3);
constexpr SkColor4f kBody = hex(0xC9DEDD);
constexpr SkColor4f kDate = hex(0x1C4040);
constexpr SkColor4f kHeadDim = hex(0xB8A0A0);

/** The shadow tone: the complement spelling of `mul()`, because a bevel
 *  is authored as "how much darker" rather than as a surviving fraction. */
inline SkColor4f dark(SkColor4f c, float k) { return mul(c, 1 - k); }

// ---------------------------------------------------------------------------
// Type — the SWF's embedded faces, substituted with the nearest the
// platform ships. Helvetica CondensedBlack is the whole chrome voice.

inline sk_sp<SkTypeface> face(const char* family, int weight, int width,
                              const char* fallbackFamily = nullptr) {
  auto mgr = sigil::weave::ports::systemFontManager();
  sk_sp<SkTypeface> f = mgr->matchFamilyStyle(
      family, SkFontStyle(weight, width, SkFontStyle::kUpright_Slant));
  if (!f && fallbackFamily)
    f = mgr->matchFamilyStyle(
        fallbackFamily,
        SkFontStyle(weight, width, SkFontStyle::kUpright_Slant));
  if (!f) f = mgr->matchFamilyStyle(nullptr, SkFontStyle::Bold());
  return f;
}

inline const sk_sp<SkTypeface>& condBlack() {
  static sk_sp<SkTypeface> f =
      face("Helvetica Neue", SkFontStyle::kBlack_Weight,
           SkFontStyle::kCondensed_Width, "Avenir Next Condensed");
  return f;
}
inline const sk_sp<SkTypeface>& blackFace() {
  static sk_sp<SkTypeface> f =
      face("Arial Black", SkFontStyle::kBlack_Weight,
           SkFontStyle::kNormal_Width, "Helvetica Neue");
  return f;
}
inline const sk_sp<SkTypeface>& arial() {
  static sk_sp<SkTypeface> f = face("Arial", SkFontStyle::kNormal_Weight,
                                    SkFontStyle::kNormal_Width, "Helvetica");
  return f;
}

/** Tracking is authored in Illustrator units — 1/1000 em — and converted
 *  to pixels here, so a tracking value stays the same when the size does. */
inline sigil::weave::TextStyle type(const sk_sp<SkTypeface>& tf, float size,
                                    SkColor4f color, float trackUnits = 0,
                                    float condense = 1.0f) {
  return sigil::compose::type({.face = tf,
                               .size = size,
                               .color = color,
                               .track = size * trackUnits / 1000.0f,
                               .condense = condense});
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

inline Element t(const char* s, sigil::weave::TextStyle st) {
  return text(toU8(s), std::move(st));
}

// ---------------------------------------------------------------------------
// Geometry vocabulary — chamfers, not radii. Nothing on this interface is
// round; every corner that is not square is cut at 45°.

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
  shapes::Corner mask = shapes::Corner::All;

  bool operator==(const Brackets&) const = default;
  void paint(SkCanvas& c, const PaintContext& ctx) const {
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
    if (has(mask, shapes::Corner::TopLeft)) L(o, o, 1, 1);
    if (has(mask, shapes::Corner::TopRight)) L(w - o, o, -1, 1);
    if (has(mask, shapes::Corner::BottomRight)) L(w - o, h - o, -1, -1);
    if (has(mask, shapes::Corner::BottomLeft)) L(o, h - o, 1, -1);
  }
};

/** The inner half of FDoubleBevelPanelClass: a second, fainter bevel
 *  nested `inset` px inside the first. A FOREGROUND, so it rides above
 *  the panel's own content — which is what makes it read as a frame. */
struct InsetBevel {
  SkColor4f hi{1, 1, 1, 0.18f}, lo{0, 0, 0, 0.35f};
  float inset = 6, hiW = 2, loW = 1;

  bool operator==(const InsetBevel&) const = default;
  void paint(SkCanvas& c, const PaintContext& ctx) const {
    const SkRect r = SkRect::MakeWH(ctx.size.width(), ctx.size.height())
                         .makeInset(inset, inset);
    if (r.isEmpty()) return;
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
  SkColor4f color = alpha(kCyan, 0.5f);
  float spacing = 8, shortLen = 4, longLen = 9, thickness = 1;
  int major = 4;
  bool vertical = false, farSide = false;

  bool operator==(const TickRail&) const = default;
  void paint(SkCanvas& c, const PaintContext& ctx) const {
    SkPaint p;
    p.setAntiAlias(false);
    p.setColor4f(color, nullptr);
    const float w = ctx.size.width(), h = ctx.size.height();
    const float run = vertical ? h : w;
    int i = 0;
    // the loop walks a distance; the accumulated float is the position
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
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
 *  it once per `period`. The rail is 24 px wide at this ×2 scale, so a
 *  90 px flare has to lean about 8° off VERTICAL to fit inside it. */
struct RailFlares {
  SkColor4f color = hex(0x99AAAA);
  float period = 6.0f, phase = 0.0f;

  bool operator==(const RailFlares&) const = default;
  bool isAnimated() const { return true; }
  void paint(SkCanvas& c, const PaintContext& ctx) const {
    const float w = ctx.size.width(), h = ctx.size.height();
    const float ys[3] = {h * 0.167f, h * 0.5f, h * 0.833f};
    const float len = 90.0f, lean = 12.6f;
    const double tt = ctx.elapsedSeconds + phase;
    const float scan = (float)std::fmod(tt, (double)period) / period;
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(2);
    for (float y : ys) {
      const float y0 = y - len * 0.5f;
      const float x0 = w * 0.5f - lean * 0.5f;
      SkPathBuilder b;
      b.moveTo(x0, y0);
      b.lineTo(x0 + lean, y0 + len);
      const SkPath path = b.detach();
      p.setColor4f(alpha(color, 0.55f), nullptr);
      c.drawPath(path, p);
      SkPaint f;
      f.setAntiAlias(true);
      f.setColor4f(alpha(color, 0.8f), nullptr);
      c.drawRect(SkRect::MakeXYWH(x0 + lean - 3, y0 + len, 6, 3), f);
      SkPaint g;
      g.setAntiAlias(true);
      g.setStyle(SkPaint::kStroke_Style);
      g.setStrokeWidth(2);
      g.setColor4f(alpha(kCyan, 0.9f * (1.0f - std::abs(scan - 0.5f) * 2)),
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

  bool operator==(const Scanlines&) const = default;
  void paint(SkCanvas& c, const PaintContext& ctx) const {
    SkPaint p;
    p.setColor4f(color, nullptr);
    // the loop walks a distance; the accumulated float is the position
    // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
    for (float y = 0; y < ctx.size.height(); y += period)
      c.drawRect(SkRect::MakeXYWH(0, y, ctx.size.width(), on), p);
  }
};

// ---------------------------------------------------------------------------

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
      stroke(3, Fill::color(lift(base, 0.15f)), PathFormat::Align::Inner)));
  e.foreground(shapes::onEdges(
      shapes::Edge::Bottom | shapes::Edge::Right,
      stroke(2, Fill::color(dark(base, 0.60f)), PathFormat::Align::Inner)));
  return e;
}

/** FDoubleBevelPanelClass: the same pair, plus a fainter one `inset` px
 *  in. MAINFRAME, FEATURE SYSTEM and PRESS UPDATES wear this; everything
 *  smaller is single. */
inline Element doubleBevel(Element e, SkColor4f base, float inset = 6) {
  e = singleBevel(std::move(e), base);
  e.foreground(InsetBevel{alpha(lift(base, 0.16f), 0.8f),
                          alpha(dark(base, 0.55f), 0.85f), inset, 2, 1});
  return e;
}

}  // namespace tav

// ===========================================================================

struct TwoAdvancedV4 : sketch::Sketch {
  // --- bound outputs: every idle motion is DECLARED, none re-describes ---
  ch::Output<float> stripePan{0.0f};    // hazard-stripe conveyor
  ch::Output<float> portalGlow{54.0f};  // MAINFRAME portal glow radius
  ch::Output<float> pressScroll{0.0f};  // PRESS UPDATES auto-scroll
  float pressOverflow = 0;              // entry list minus well, measured once
  ch::Output<float> wirePhase{0.0f};    // status-bus marquee phase
  float wireW = 1;                      // measured once (compose::measure)
  ch::Output<float> vuLeft{0.4f}, vuRight{0.6f};
  std::array<ch::Output<float>, 21> dot{};  // 7 clusters × 3 dots
  std::array<ch::Output<float>, 3> gauge{};
  std::array<ch::Output<float>, 3> gaugeAlpha{};

  // --- generated materials, HELD so their identity prunes across renders ---
  Pattern hazard;          // baked 45° stripe tile — the STATIC reuse path
  Pattern hatchA, hatchB;  // footer-dock crosshatch (two passes = crosshatch)
  Pattern dither;          // teal readout-box dither
  Material grain;          // page-background film grain (luminance, not RGB)
  Material spectrum, stripesLive, waterStreaks;

  // --- the production shell artefacts, fetched from the restoration host.
  // Any of these may be null (no network, cold cache); every use site
  // keeps its procedural stand-in for exactly that case.
  std::shared_ptr<const sigil::image::ImageAsset> railLeftGif, railRightGif;
  std::shared_ptr<const sigil::image::ImageAsset> siteBgGif;   // 1×1600 ramp
  std::shared_ptr<const sigil::image::ImageAsset> footerGif;   // 970×110
  std::shared_ptr<const sigil::image::ImageAsset> logoBugSvg;  // circular 2A

  /** A bitmap stretched to exactly (w, h) — how every shell GIF is
   *  placed: the 2004 page scaled them with IMG width/height attributes,
   *  and this sketch is a ×2 enlargement of those numbers. */
  static Material stretchFill(
      const std::shared_ptr<const sigil::image::ImageAsset>& asset, float w,
      float h, SkTileMode tx = SkTileMode::kClamp) {
    const sk_sp<SkImage>& img = asset->frames()[0].image;
    return Material::image(
        img, tx, SkTileMode::kClamp,
        SkMatrix::Scale(w / (float)img->width(), h / (float)img->height()),
        SkSamplingOptions(SkFilterMode::kLinear));
  }

  // --- instancing: the footer dock's chevron tick array ---
  std::shared_ptr<instancing::Atlas> dockAtlas;
  std::shared_ptr<instancing::Pool> dockPool;

  int bootPct = 0;
  bool booted = false;

  // --- the section cycle: the GLOBAL NAVIGATOR walked in order ----------
  // The SWF changed sections by collapsing the MAINFRAME viewport behind
  // sliding panels and a loading readout, then reopening on the new
  // content. The cycle here replays that transition grammar on the nav
  // taxonomy: shutters close L→R, the ACCESSING readout flashes up, the
  // shutters reopen — while the selection mark glides to the next item.
  static constexpr const char* kNavItems[7] = {
      "COMPANY",      "SERVICES",  "PORTFOLIO", "ACCOLADES",
      "EXPERIMENTAL", "EQUIPMENT", "CONTACT"};
  static constexpr double kCycleStart = 8.0, kHoldS = 4.9, kTransS = 0.9;
  std::array<ch::Output<float>, 6> shutter{};  // per-slat cover fraction
  ch::Output<float> shutterInfo{0.0f};         // ACCESSING plate opacity
  ch::Output<float> navIndX{0.0f};             // selection mark X offset
  int mfSection = -2;                          // section in the readout

  /** Nav item i's centre inside the 584-wide bar (SpaceEvenly over the
   *  572 inner px), as the translateX for a 24-wide mark at left 0. */
  static float navMarkX(int i) {
    return 6.0f + 572.0f * ((float)i + 0.5f) / 7.0f - 12.0f;
  }
  /** The section the cycle rests on after `stop` changes; the interface
   *  opens on PORTFOLIO and walks onward from there. */
  static int cycleTarget(int stop) { return (3 + stop) % 7; }

  // =========================================================================
  // Live materials (SkSL).

  /** The audio spectrum: a bar field whose heights are hashed per column
   *  per TIME STEP. uTime arrives quantized at 10 Hz (quantizeTime), so the
   *  bars STEP rather than slide — the era's digital readout feel, and the
   *  same reason a stylised meter animates on a beat instead of smoothly. */
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
      if (!e) SkDebugf("twoadvanced spectrum: %s\n", err.c_str());
      return e;
    }();
    return fx;
  }

  /** The diagonal hazard stripe as a LIVE material so every header bar can
   *  run its slow conveyor pan — 20 px per 8 s — off ONE bound uniform.
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
      if (!e) SkDebugf("twoadvanced stripe: %s\n", err.c_str());
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
          float band = smoothstep(0.62, 1.0, s) * (0.14 + 0.86 * (1.0 - v));
          float3 base = mix(float3(0.000, 0.042, 0.046),
                            float3(0.001, 0.014, 0.017), v);
          float3 c = base + float3(0.02, 0.17, 0.18) * band;
          return half4(half3(c), 1.0);
        }
      )"));
      if (!e) SkDebugf("twoadvanced water: %s\n", err.c_str());
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
          &dot[(size_t)cluster * 3 + (size_t)i]));
    return r;
  }

  /** The two-weight panel header: heavy half + regular half over the
   *  hazard-stripe ground, with a flavour line, a tick cluster and a tick
   *  rail. Four panels wear this identically. */
  Element panelHeader(const char* boldHalf, const char* restHalf,
                      const char* flavor, int cluster, float w) {
    using namespace tav;
    return box()
        .width(Dim(w))
        .height(28)
        .row()
        .alignItems(Align::Center)
        .padding(10, 0)
        .fill(stripesLive)
        .foreground(shapes::onEdges(shapes::Edge::Bottom,
                                    stroke(1, Fill::color(alpha(kCyan, 0.35f)),
                                           PathFormat::Align::Inner)))
        .child(t(boldHalf, heavy(17, kNear, 40)))
        .child(t(restHalf, type(arial(), 15, kHeadDim, 40, 0.95f)))
        .child(box().width(12))
        .child(box().width(1).height(12).fill(alpha(kCyan, 0.4f)))
        .child(box().width(10))
        .child(t(flavor, micro(11, alpha(kDust, 1.0f), 260)))
        .child(box().grow(1))
        .child(tickDots(cluster))
        .child(box().width(8))
        .child(box().width(34).height(10).foreground(
            TickRail{alpha(kCyan, 0.55f), 4, 3, 7, 1, 3, false, true}));
  }

  /** CTA: chamfered, blood-red ramp in UNIT space so the gradient follows
   *  whatever height the layout hands the button, gloss band on top. */
  Element cta(const char* lbl, float w = 116, float h = 34,
              SkColor4f hairline = tav::kNear) {
    using namespace tav;
    return box()
        .width(Dim(w))
        .height(Dim(h))
        .shape(shapes::chamfered(9, shapes::Corner::Diagonal))
        .fill(Material::linearUnit(
            {0, 0}, {0, 1},
            {{0.0f, kCtaHi}, {0.42f, kCta}, {1.0f, hex(0x3A0000)}}))
        .stroke(stroke(1, Fill::color(kChrome), PathFormat::Align::Outer))
        .foreground(styles::gloss(alpha(kCtaHi, 0.55f), h * 0.30f,
                                  {0, -h * 0.26f}, 0.62f, 0.30f))
        .foreground(stroke(1, Fill::color(alpha(hairline, 0.45f)),
                           PathFormat::Align::Inner))
        .row()
        .justify(Justify::Center)
        .alignItems(Align::Center)
        .child(t(lbl, label(15, kNear, 110)));
  }

  /** A dark readout window: chamfered, inset-bevelled, bracketed. */
  Element readout(float w, float h, SkColor4f ground = hex(0x1B0708)) {
    using namespace tav;
    return box()
        .width(Dim(w))
        .height(Dim(h))
        .shape(shapes::chamfered(7, shapes::Corner::AntiDiagonal))
        .fill(ground)
        .foreground(
            InsetBevel{alpha(kChromeHi, 0.6f), {0, 0, 0, 0.5f}, 0, 1, 1})
        .foreground(
            Brackets{alpha(kCyan, 0.55f), 8, 2, 3, shapes::Corner::All});
  }

  /** A radar wedge: shapes::sector, rotation BOUND. Every gauge on the
   *  page is this with a different bezel. */
  Element radarSweep(int i, SkColor4f tint, float inner = 0.30f) {
    return box()
        .inset(0)
        .shape(shapes::sector(-100, 78, inner))
        .fill(Material::linearUnit(
            {0, 0}, {1, 1},
            {{0.0f, alpha(tint, 0.85f)}, {1.0f, alpha(tint, 0.05f)}}))
        .rotate(&gauge[(size_t)i])
        .opacity(&gaugeAlpha[(size_t)i]);
  }

  // =========================================================================
  // Regions.

  Element statusBar() {
    using namespace tav;
    // Two segments meeting on a DIAGONAL seam, not a vertical edge. They
    // drop in one after the other: teal leads, maroon follows 80 ms later.
    Element teal =
        singleBevel(
            box()
                .left(Dim(0))
                .top(Dim(0))
                .width(560)
                .height(40)
                .shape(shapes::chamfered(40, shapes::Corner::BottomRight))
                .row()
                .alignItems(Align::Center)
                .padding(10, 0)
                .gap(8),
            kTealBar)
            .translateY(animate(from(-46.0f).to(0.0f),
                                {380ms, &ch::easeOutQuint, 1450ms}))
            .child(box()
                       .width(22)
                       .height(22)
                       .corners({5})
                       .fill(Material::radialUnit({0.5f, 0.42f}, 1.15f,
                                                  {{0.0f, kCyanRing},
                                                   {0.55f, kTealBar},
                                                   {1.0f, hex(0x0C2A2C)}}))
                       .stroke(stroke(1, Fill::color(alpha(kCyan, 0.7f)),
                                      PathFormat::Align::Inner))
                       .justify(Justify::Center)
                       .alignItems(Align::Center)
                       .child(box().width(9).height(9).corners({5}).stroke(
                           stroke(2, Fill::color(kCyan)))))
            // The teal segment's voice, verbatim from the interface
            // capture: the boot callsign, then the two region labels the
            // page hangs over its modules.
            .child(t("INITREQ 2A", micro(12, kNear, 240)))
            .child(box().width(1).height(14).fill(alpha(kCyan, 0.4f)))
            .child(t("\xe2\x80\xba GLOBAL AMBIENCE",
                     micro(11, alpha(kCyan, 0.9f), 240)))
            .child(box().grow(1))
            .child(box().width(90).height(12).foreground(
                TickRail{alpha(kNear, 0.45f), 6, 3, 8, 1, 3, false, true}))
            .child(box().width(46));

    Element maroon =
        singleBevel(box()
                        .left(Dim(548))
                        .top(Dim(0))
                        .width(Dim(1892.0f - 548.0f))
                        .height(40)
                        .shape(shapes::chamfered(40, shapes::Corner::TopLeft))
                        .row()
                        .alignItems(Align::Center)
                        .padding(58, 0, 14, 0)
                        .gap(10),
                    kChrome)
            .translateY(animate(from(-46.0f).to(0.0f),
                                {380ms, &ch::easeOutQuint, 1530ms}))
            .child(t("\xe2\x80\xba GLOBAL NAVIGATOR", micro(11, kDust, 260)))
            .child(box().grow(1))
            // V4.PROPHECY sits in its own hairline-outlined plate at the
            // bar's right end — the one piece of type up here that is
            // boxed rather than bare.
            .child(box()
                       .height(24)
                       .padding(9, 0)
                       .stroke(stroke(1, Fill::color(alpha(kNear, 0.75f)),
                                      PathFormat::Align::Inner))
                       .row()
                       .alignItems(Align::Center)
                       .child(t("V4.PROPHECY", heavy(14, kNear, 80))));

    return box()
        .left(Dim(0))
        .top(Dim(0))
        .width(1892)
        .height(40)
        .child(maroon)
        .child(teal);
  }

  Element audioModule() {
    using namespace tav;
    // The playlist verbatim from the interface capture: numbered with
    // periods, track 01 highlighted, no duration column anywhere.
    static const char* tracks[4] = {"01. DESERT TRANCE", "02. NEVERRAIN",
                                    "03. SYNERGY", "04. EXHILE"};
    Element list = box().column().width(268).gap(2);
    for (int i = 0; i < 4; ++i) {
      const bool sel = i == 0;
      list.child(
          box()
              .height(23)
              .row()
              .alignItems(Align::Center)
              .padding(6, 0)
              .gap(6)
              .fill(sel ? kChromeHi : alpha(hex(0x2A0A0C), 0.85f))
              .foreground(shapes::onEdges(
                  shapes::Edge::Left,
                  stroke(2, Fill::color(sel ? kCyan : alpha(kDust, 0.35f)),
                         PathFormat::Align::Inner)))
              .child(t(sel ? "\xe2\x96\xb8" : " ", micro(11, kCyan, 0)))
              .child(t(tracks[i], type(blackFace(), 13, sel ? kNear : kHeadDim,
                                       60, 0.92f))));
    }

    Element scope =
        box()
            .grow(1)
            .height(100)
            .shape(shapes::chamfered(8, shapes::Corner::AntiDiagonal))
            .fill(spectrum)
            .foreground(Scanlines{{0, 0, 0, 0.16f}, 3, 1})
            .foreground(
                Brackets{alpha(kCyan, 0.6f), 10, 2, 3, shapes::Corner::All})
            .foreground(stroke(1, Fill::color(alpha(kCyan, 0.35f)),
                               PathFormat::Align::Inner));

    auto key = [&](const char* glyph, bool hot) {
      return box()
          .width(38)
          .height(22)
          .shape(shapes::chamfered(6, shapes::Corner::Diagonal))
          .fill(Material::linearUnit({0, 0}, {0, 1},
                                     {{0.0f, hot ? kCtaHi : hex(0x5A2226)},
                                      {0.5f, hot ? kCta : hex(0x3A0F12)},
                                      {1.0f, hex(0x240607)}}))
          .stroke(stroke(1, Fill::color(alpha(kDust, 0.35f)),
                         PathFormat::Align::Inner))
          .justify(Justify::Center)
          .alignItems(Align::Center)
          .child(t(glyph, micro(11, hot ? kNear : kDust, 0)));
    };
    auto meter = [&](float w, const ch::Output<float>* bind, SkColor4f c) {
      return box()
          .width(Dim(w))
          .height(6)
          .fill(hex(0x1B0708))
          .child(box()
                     .left(Dim(0))
                     .top(Dim(0))
                     .width(Dim(w))
                     .height(6)
                     .fill(c)
                     .scaleX(bind)
                     .transformOrigin(0, 0.5f));
    };

    Element panel = singleBevel(
        box().width(596).height(174).column().padding(9).gap(6), hex(0x3E1013));
    panel.key("audio")
        .foreground(
            Brackets{kCyan, 18, 3, 4,
                     shapes::Corner::TopLeft | shapes::Corner::TopRight})
        .foreground(TickRail{alpha(kDust, 0.45f), 7, 3, 6, 1, 4, false, true})
        .child(box().row().gap(8).height(100).child(list).child(scope))
        .child(box()
                   .row()
                   .gap(5)
                   .alignItems(Align::Center)
                   .child(key("\xe2\x97\x82\xe2\x97\x82", false))
                   .child(key("\xe2\x96\xa0", false))
                   .child(key("\xe2\x96\xb8", true))
                   .child(key("\xe2\x96\xb8\xe2\x96\xb8", false))
                   .child(box().width(8))
                   .child(meter(64, &vuLeft, alpha(kCyan, 0.85f)))
                   .child(box().grow(1))
                   .child(t("VOL", micro(10, kDustDim, 200)))
                   .child(meter(56, &vuRight, alpha(kCyanRing, 0.8f))))
        .child(box()
                   .height(18)
                   .row()
                   .alignItems(Align::Center)
                   .padding(6, 0)
                   .fill(alpha(kChrome, 0.9f))
                   .child(t("AUDIO PREFERENCES", micro(11, kDust, 240)))
                   .child(box().grow(1))
                   .child(t("STREAM 128K \xc2\xb7 STEREO",
                            micro(10, kDustDim, 200))));
    return panel;
  }

  Element navBar() {
    using namespace tav;
    Element bar = singleBevel(box()
                                  .width(584)
                                  .height(46)
                                  .row()
                                  .justify(Justify::SpaceEvenly)
                                  .alignItems(Align::Center)
                                  .padding(6, 0),
                              kChrome);
    bar.fill(stripesLive).staggerChildren(40ms);  // the items arrive in order
    for (int i = 0; i < 7; ++i) {
      bar.child(box()
                    .column()
                    .alignItems(Align::Center)
                    .gap(3)
                    .translateY(animate(from(16.0f).to(0.0f),
                                        {240ms, &ch::easeOutQuint, 2250ms}))
                    .opacity(animate(from(0.0f).to(1.0f),
                                     {240ms, &ch::easeOutQuad, 2250ms}))
                    .child(t(kNavItems[i], label(13, kNear, 80)))
                    .child(box().width(8).height(2).fill(alpha(kDust, 0.6f))));
      if (i < 6)
        bar.child(box().width(1).height(20).fill(alpha(hex(0x2A0A0C), 0.9f)));
    }
    // The GLOBAL NAVIGATOR's live selection mark: one cyan bar whose X is
    // a single bound value, gliding between items as the section cycle
    // walks the taxonomy.
    bar.child(box()
                  .left(Dim(0))
                  .top(Dim(38))
                  .width(24)
                  .height(3)
                  .fill(kCyan)
                  .background(styles::OuterGlow{alpha(kGlow, 0.5f), 6, 0})
                  .translateX(&navIndX));
    return bar;
  }

  /** The strip under the nav bar. The real index never left 584×150 of
   *  gradient showing: five chamfered section thumbnails over a dark
   *  readout ground, each with its own bracket set and index number. */
  Element quickLaunch() {
    using namespace tav;
    static const char* secs[5] = {"PORTFOLIO", "SERVICES", "ACCOLADES",
                                  "EXPERIMENTAL", "EQUIPMENT"};
    static const char* nums[5] = {"01", "02", "03", "04", "05"};
    Element chips = box().row().gap(6).height(64);
    for (int i = 0; i < 5; ++i) {
      const float g = 0.28f + 0.16f * (float)i;
      chips.child(
          box()
              .grow(1)
              .column()
              .gap(3)
              .child(
                  box()
                      .grow(1)
                      .shape(shapes::chamfered(8, shapes::Corner::Diagonal))
                      .fill(Material::linearUnit(
                          {0, 0}, {0, 1},
                          {{0.0f, hex(0x06232A)}, {1.0f, hex(0x01090B)}}))
                      .stroke(stroke(1, Fill::color(alpha(kCyan, 0.35f)),
                                     PathFormat::Align::Inner))
                      .child(box().inset(0).fill(
                          Material::radialUnit({0.5f, 0.92f}, 1.0f,
                                               {{0.0f, alpha(kGlow, g)},
                                                {1.0f, alpha(kGlow, 0.0f)}})))
                      .child(at(box().fill(hex(0x010A0C)), 12, 16, 14, 26))
                      .child(at(box().fill(hex(0x02171B)), 30, 8, 20, 34))
                      .child(at(box().fill(hex(0x010A0C)), 54, 20, 16, 22))
                      .child(at(box().fill(alpha(kGlow, 0.5f)), 0, 41, 200, 1))
                      .foreground(Brackets{alpha(kCyan, 0.6f), 7, 1, 2,
                                           shapes::Corner::All})
                      .foreground(Scanlines{{0, 0, 0, 0.24f}, 3, 1})
                      .child(box().left(Dim(4)).top(Dim(3)).child(
                          t(nums[i], micro(9, alpha(kCyan, 0.9f), 140)))))
              .child(t(secs[i], micro(9, i == 0 ? kCyan : kDust, 200))));
    }

    Element panel = singleBevel(
        box().width(584).height(144).column().padding(8).gap(6), hex(0x3E1013));
    panel.key("quick")
        .translateY(
            animate(from(40.0f).to(0.0f), {380ms, &ch::easeOutQuint, 2450ms}))
        .opacity(
            animate(from(0.0f).to(1.0f), {320ms, &ch::easeOutQuad, 2450ms}))
        .foreground(
            Brackets{alpha(kCyan, 0.55f), 12, 2, 4,
                     shapes::Corner::BottomLeft | shapes::Corner::BottomRight})
        .child(box()
                   .row()
                   .alignItems(Align::Center)
                   .gap(8)
                   .height(16)
                   .child(t("QUICK", heavy(13, kNear, 40)))
                   .child(t(" LAUNCH", type(arial(), 12, kHeadDim, 40, 0.95f)))
                   .child(box().width(1).height(11).fill(alpha(kCyan, 0.4f)))
                   .child(t("SELECT A SECTOR", micro(10, kDust, 260)))
                   .child(box().grow(1))
                   .child(tickDots(4))
                   .child(box().width(60).height(9).foreground(TickRail{
                       alpha(kCyan, 0.5f), 5, 3, 7, 1, 4, false, true})))
        .child(chips)
        .child(box()
                   .height(16)
                   .row()
                   .alignItems(Align::Center)
                   .gap(8)
                   .padding(5, 0)
                   .fill(alpha(kChrome, 0.85f))
                   .child(t("\xe2\x96\xb8 INDEX", micro(9, kCyan, 200)))
                   .child(box().grow(1).height(1).fill(alpha(kDust, 0.3f)))
                   .child(t("05 SECTORS \xc2\xb7 42 PROJECTS",
                            micro(9, kDustDim, 200))));
    return panel;
  }

  Element masthead() {
    using namespace tav;
    Element emblem =
        box()
            .width(78)
            .height(78)
            .background(styles::OuterGlow{alpha(kGlow, 0.45f), 14, 0})
            .justify(Justify::Center)
            .alignItems(Align::Center);
    if (logoBugSvg) {
      // The production mark itself, recoloured to the wordmark cyan: a
      // solid fill masked by the SVG raster's coverage, so the vector
      // art contributes shape only and the palette stays sampled.
      emblem.child(box().width(62).height(62).fill(kCyan).mask(
          by::alpha(stretchFill(logoBugSvg, 62, 62))));
    } else {
      emblem
          .fill(sdf::material(
              sdf::circle(),
              {.fill = {0, 0, 0, 0}, .borderWidth = 4, .borderColor = kCyan}))
          .child(box()
                     .width(50)
                     .height(50)
                     .shape(shapes::polygon(6, 0))
                     .stroke(stroke(1, Fill::color(alpha(kCyanRing, 0.75f))))
                     .justify(Justify::Center)
                     .alignItems(Align::Center)
                     .child(t("2", type(blackFace(), 32, kCyan, 0, 0.85f))));
    }

    return box()
        .width(700)
        .height(236)
        .column()
        .translateX(
            animate(from(320.0f).to(0.0f), {420ms, &ch::easeOutQuint, 1850ms}))
        .opacity(
            animate(from(0.0f).to(1.0f), {300ms, &ch::easeOutQuad, 1850ms}))
        .child(
            box()
                .grow(1)
                .row()
                .alignItems(Align::Center)
                .padding(26, 0, 8, 0)
                .gap(18)
                .child(emblem)
                .child(box()
                           .column()
                           .gap(6)
                           .child(t("2ADVANCED STUDIOS",
                                    type(blackFace(), 25, kCyan, 80, 0.90f))
                                      .effect(styles::textGlow(
                                          alpha(kGlow, 0.55f), 6)))
                           .child(t("PROGRESSIVE DESIGN TECHNOLOGY",
                                    micro(12, kDust, 240)))
                           .child(box()
                                      .row()
                                      .gap(6)
                                      .alignItems(Align::Center)
                                      .child(box().width(30).height(1).fill(
                                          alpha(kCyan, 0.5f)))
                                      .child(t("EST. 1999 \xc2\xb7 IRVINE CA",
                                               micro(10, kDustDim, 200)))))
                .child(box().grow(1))
                .child(box()
                           .column()
                           .alignItems(Align::End)
                           .gap(4)
                           .child(t("BUILD 4.0.7", micro(10, kDustDim, 200)))
                           .child(t("FLASH 6 REQ.", micro(10, kDustDim, 200)))
                           .child(t("1024\xc3\x97"
                                    "768 MIN",
                                    micro(10, kDustDim, 200)))))
        // the glowing 2px cyan divider under the whole masthead panel
        .child(box().height(2).fill(kCyan).background(
            styles::OuterGlow{alpha(kGlow, 0.55f), 10, 1}))
        // …and the readout rail beneath it, so the masthead block ends on
        // the same baseline as QUICK LAUNCH instead of leaving an L-void
        .child(box()
                   .height(50)
                   .row()
                   .alignItems(Align::Center)
                   .padding(26, 0, 8, 0)
                   .gap(12)
                   .child(t("\xe2\x96\xb8 SESSION", micro(10, kCyan, 220)))
                   .child(t("A4-1187-PRPH", micro(10, kDustDim, 200)))
                   .child(box().width(1).height(12).fill(alpha(kDust, 0.3f)))
                   .child(t("LOCALE", micro(10, kDustDim, 220)))
                   .child(t("EN-US", micro(10, kDust, 200)))
                   .child(box().width(1).height(12).fill(alpha(kDust, 0.3f)))
                   .child(t("LOAD", micro(10, kDustDim, 220)))
                   .child(box()
                              .width(90)
                              .height(6)
                              .fill(hex(0x240607))
                              .child(box()
                                         .left(Dim(0))
                                         .top(Dim(0))
                                         .width(74)
                                         .height(6)
                                         .fill(alpha(kCyan, 0.8f))))
                   .child(t("543 KB", micro(10, kDust, 200)))
                   .child(box().grow(1))
                   .child(tickDots(4, alpha(kCyan, 0.9f)))
                   .child(box().width(120).height(10).foreground(TickRail{
                       alpha(kDust, 0.5f), 6, 3, 8, 1, 4, false, true})));
  }

  // ---- the hero -----------------------------------------------------------

  /** Composite ORDER is what sells this. sky → skyline → portal →
   *  ring → figures → water + reflection → horizon hairline.
   *  `still` builds the same scene with no live material and no entrance,
   *  so the bloom duplicate stays provably static and bakes ONCE. */
  Element heroScene(float w, float h, bool still) {
    using namespace tav;
    const float horizon = std::round(h * 0.66f);
    const float cx = w * 0.50f;

    Element scene = stack().width(Dim(w)).height(Dim(h)).clip();

    scene.child(
        box().inset(0).fill(Material::linearUnit({0, 0}, {0, 0.66f},
                                                 {{0.0f, hex(0x02070A)},
                                                  {0.62f, hex(0x03181D)},
                                                  {1.0f, hex(0x073038)}})));

    // the portal's wide halo, behind everything — LOW alpha; the disc
    // itself is small and the exponential SDF glow does the reaching.
    scene.child(
        at(box().fill(Material::radialUnit({0.5f, 0.5f}, 1.0f,
                                           {{0.00f, alpha(kGlow, 0.34f)},
                                            {0.30f, alpha(kTealBar, 0.16f)},
                                            {1.00f, alpha(kTealBar, 0.0f)}})),
           cx - 330, horizon - 420, 660, 620)
            .blend(SkBlendMode::kPlus));

    // the horizon haze band, full width, UNDER the skyline. Without it the
    // outer thirds are black-on-black and the silhouettes have nothing to
    // read against; one kPlus ramp is the whole of the fix.
    scene.child(
        at(box().fill(Material::linearUnit({0, 0}, {0, 1},
                                           {{0.00f, alpha(kTealBar, 0.0f)},
                                            {0.62f, alpha(kTealBar, 0.10f)},
                                            {1.00f, alpha(kTealBar, 0.34f)}})),
           0, horizon - 132, w, 132)
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
      const float o = 1.0f - 0.42f * d;
      const SkColor4f tint = {0.004f + 0.012f * (1 - d),
                              0.026f + 0.030f * (1 - d),
                              0.034f + 0.036f * (1 - d), 1.0f};
      Element slab =
          at(box().fill(tint), bx[i], horizon - bh[i], bw[i], bh[i]).opacity(o);
      slab.child(box()
                     .left(Dim(bw[i] * 0.25f))
                     .top(Dim(bh[i] * 0.22f))
                     .width(3)
                     .height(3)
                     .fill(alpha(kGlow, 0.55f)));
      slab.child(box()
                     .left(Dim(bw[i] * 0.62f))
                     .top(Dim(bh[i] * 0.48f))
                     .width(3)
                     .height(3)
                     .fill(alpha(kGlow, 0.38f)));
      scene.child(slab);
      if (i % 3 == 1)
        scene.child(at(box().fill(alpha(tint, o)), bx[i] + bw[i] * 0.22f,
                       horizon - bh[i] - 30, bw[i] * 0.56f, 30));
      if (i % 4 == 2)
        scene.child(at(box().fill(alpha(tint, o)), bx[i] - 14,
                       horizon - bh[i] * 0.55f, 14, bh[i] * 0.55f));
      if (bh[i] > 150)
        scene.child(at(box().fill(alpha(tint, o)), bx[i] + bw[i] * 0.5f - 1,
                       horizon - bh[i] - 26, 2, 26));
    }

    // beacons over the far skyline
    const float bl[5][2] = {
        {132, 0.30f}, {318, 0.22f}, {826, 0.26f}, {968, 0.18f}, {1084, 0.30f}};
    for (int i = 0; i < 5; ++i)
      scene.child(
          at(box().fill(alpha(i % 2 ? kGlow : hex(0xFF6A6A), bl[i][1] + 0.35f)),
             bl[i][0], horizon - 210 - 24 * (float)i, 3, 3)
              .blend(SkBlendMode::kPlus));

    // THE portal: one SDF circle. Its box must RESERVE sdf::pad() for the
    // glow — sdf::minBoxFor() is the only honest way to size it, since
    // pad eats into the half-size (a 300 box with glowRadius 54 leaves
    // almost no disc at all). 132 px of visible disc, reaching far.
    sdf::Style ps{.fill = alpha(kGlow, 0.95f),
                  .borderWidth = 3,
                  .borderColor = {0.90f, 1.0f, 1.0f, 0.95f},
                  .glowRadius = 54,
                  .glowColor = alpha(kGlow, 0.75f)};
    const float pbox = sdf::minBoxFor(ps, 132);
    Material pm = sdf::material(sdf::circle(), ps);
    if (!still) pm.uniform("uGlowR", &portalGlow);  // ±8 % sine, period 4 s
    Element portal = at(box().fill(pm), cx - pbox * 0.5f,
                        horizon - 108 - pbox * 0.5f, pbox, pbox)
                         .blend(SkBlendMode::kPlus);
    if (!still)
      // the one deliberately bouncy beat: the power core kicking on.
      // ease::outBack() takes its overshoot as a parameter and converts to
      // an EaseFn, so the kick is one animate() call rather than a
      // hand-written keyframe path through the overshoot and back.
      portal.scale(
          animate(from(0.80f).to(1.0f), {620ms, ease::outBack(2.1f), 2400ms}));
    scene.child(portal);

    // an orbital ring, trim-revealed with the panel
    Element ring =
        at(box()
               .shape(shapes::arc(-125, 310))
               .stroke(stroke(2, Fill::color(alpha(kCyanRing, 0.6f)))),
           cx - 118, horizon - 226, 236, 236);
    if (!still)
      ring.mask(by::spans(spans::upTo(
          animate(from(0.0f).to(1.0f), {700ms, &ch::easeOutQuint, 2600ms}))));
    scene.child(ring);

    // three helmeted figures, backlit: dome silhouette + gloss rim-light
    const float fxs[3] = {cx - 205, cx, cx + 200};
    const float fss[3] = {0.82f, 1.0f, 0.78f};
    for (int i = 0; i < 3; ++i) {
      const float dw = 66 * fss[i], dh = 100 * fss[i];
      Element fig = at(box()
                           .shape([](SkSize s) {
                             SkPathBuilder b;
                             b.moveTo(0, s.height());
                             b.lineTo(0, s.width() * 0.5f);
                             b.arcTo(SkRect::MakeWH(s.width(), s.width()), 180,
                                     180, false);
                             b.lineTo(s.width(), s.height());
                             b.close();
                             return b.detach();
                           })
                           .fill(hex(0x01080A)),
                       fxs[i] - dw * 0.5f, horizon - dh + 14, dw, dh);
      fig.foreground(styles::gloss(alpha(kCyanRing, 0.95f), dw * 0.09f,
                                   {0, -dh * 0.15f}, 0.28f, 0.20f));
      fig.foreground(shapes::onEdges(
          shapes::Edge::Top, stroke(1.5f, Fill::color(alpha(kGlow, 0.85f)),
                                    PathFormat::Align::Inner)));
      fig.child(box()
                    .left(Dim(dw * 0.22f))
                    .top(Dim(dh * 0.30f))
                    .width(Dim(dw * 0.56f))
                    .height(3)
                    .fill(alpha(kGlow, 0.55f)));
      scene.child(fig);
    }

    // water: streaks + a mirrored, blurred copy of the portal glow
    Element water = at(box().clip(), 0, horizon, w, h - horizon);
    if (still)
      water.fill(Material::linearUnit(
          {0, 0}, {0, 1}, {{0.0f, hex(0x001415)}, {1.0f, hex(0x011F21)}}));
    else
      water.fill(waterStreaks);
    water.child(box()
                    .left(Dim(cx - 190))
                    .top(Dim(-72))
                    .width(380)
                    .height(300)
                    .fill(Material::radialUnit({0.5f, 0.14f}, 1.05f,
                                               {{0.0f, alpha(kGlow, 0.75f)},
                                                {0.45f, alpha(kTealBar, 0.32f)},
                                                {1.0f, alpha(kTealBar, 0.0f)}}))
                    // smear the reflection down into the water: sigma 26
                    // along the 90° axis (straight down), 14 across it
                    .effect(Effect::directionalBlur(26, 90, 14))
                    .opacity(0.78f)
                    .blend(SkBlendMode::kPlus));
    // the specular COLUMN — the vertical smear of a light in water, and
    // the single cue that reads "reflection" from across the room
    water.child(box()
                    .left(Dim(cx - 40))
                    .top(Dim(0))
                    .width(80)
                    .height(Dim(h - horizon))
                    .fill(Material::linearUnit({0, 0}, {0, 1},
                                               {{0.00f, alpha(kGlow, 0.55f)},
                                                {0.35f, alpha(kGlow, 0.20f)},
                                                {1.00f, alpha(kGlow, 0.0f)}}))
                    // soften the column's sides: sigma 10 along the 0° axis
                    // (horizontal), only 3 down its length
                    .effect(Effect::directionalBlur(10, 0, 3))
                    .blend(SkBlendMode::kPlus));
    scene.child(water);

    // THE horizon hairline. A hard, bright edge where the water starts
    // sells the reflection below it more than the blur itself does.
    scene.child(at(box().fill(alpha(kGlow, 0.62f)), 0, horizon - 1, w, 2));
    scene.child(at(box().fill(alpha(kCyanRing, 0.16f)), 0, horizon + 3, w, 1));
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
    s.child(
        box().inset(0).fill(Material::radialUnit({0.5f, 0.5f}, 1.0f,
                                                 {{0.00f, {0, 0, 0, 0}},
                                                  {0.58f, {0, 0, 0, 0.10f}},
                                                  {1.00f, {0, 0, 0, 0.66f}}})));
    s.child(box().inset(0).foreground(Scanlines{}));
    s.child(box().inset(0).foreground(
        Brackets{alpha(kCyan, 0.7f), 22, 2, 8, shapes::Corner::All}));

    auto corner = [&](const char* a, const char* b, float l, float tp,
                      bool end) {
      return box()
          .column()
          .gap(2)
          .alignItems(end ? Align::End : Align::Start)
          .left(Dim(l))
          .top(Dim(tp))
          .child(t(a, micro(10, alpha(kCyan, 0.8f), 220)))
          .child(t(b, micro(10, alpha(kCyanRing, 0.45f), 220)));
    };
    s.child(corner("REND / MAXON C4D R8", "PASS 04 \xc2\xb7 FRM 0142", 20, 18,
                   false));
    s.child(corner("38.2144 N", "121.4944 W", w - 132, 18, true));
    s.child(corner("DEPTH 00.42", "PRESS 1013 HPA", 20, h - 42, false));
    s.child(box()
                .left(Dim(w - 214))
                .top(Dim(h - 32))
                .row()
                .gap(6)
                .alignItems(Align::Center)
                .child(box().width(120).height(8).foreground(
                    TickRail{alpha(kCyan, 0.6f), 6, 3, 8, 1, 4, false, false}))
                .child(t("SIG 88%", micro(10, alpha(kCyan, 0.85f), 200))));
    return s;
  }

  Element mainframe() {
    using namespace tav;
    Element body = box().grow(1).clip().child(hero(1174, 316));
    // The transition shutters: six slats over the viewport, each one's
    // cover fraction a bound value — the hero underneath is never
    // re-described, so its bloom bake survives every section change.
    const float slatW = 1174.0f / 6.0f;
    for (int i = 0; i < 6; ++i)
      body.child(box()
                     .left(Dim((float)i * slatW))
                     .top(Dim(0))
                     .width(Dim(slatW + 1))
                     .height(316)
                     .fill(Material::linearUnit(
                         {0, 0}, {1, 0},
                         {{0.0f, hex(0x2A0708)}, {1.0f, hex(0x1A0405)}}))
                     .foreground(shapes::onEdges(
                         shapes::Edge::Bottom,
                         stroke(3, Fill::color(alpha(kCyan, 0.5f)),
                                PathFormat::Align::Inner)))
                     .scaleY(&shutter[(size_t)i])
                     .transformOrigin(0.5f, 0.0f));
    // The ACCESSING readout that rides the closed shutters.
    body.child(box()
                   .left(Dim(1174.0f / 2 - 220))
                   .top(Dim(316.0f / 2 - 32))
                   .width(440)
                   .height(64)
                   .shape(shapes::chamfered(10, shapes::Corner::Diagonal))
                   .fill(alpha(hex(0x140404), 0.92f))
                   .stroke(stroke(1, Fill::color(alpha(kCyan, 0.6f)),
                                  PathFormat::Align::Inner))
                   .foreground(Brackets{alpha(kCyan, 0.7f), 10, 2, 3,
                                        shapes::Corner::All})
                   .justify(Justify::Center)
                   .alignItems(Align::Center)
                   .child(slot("mfload"))
                   .opacity(&shutterInfo));

    Element panel = doubleBevel(
        box().width(1180).height(350).column().padding(3), kChrome, 3);
    panel.key("mainframe")
        .translateY(
            animate(from(70.0f).to(0.0f), {520ms, &ch::easeOutQuint, 2400ms}))
        .opacity(
            animate(from(0.0f).to(1.0f), {300ms, &ch::easeOutQuad, 2400ms}))
        .child(panelHeader("MAIN", "FRAME",
                           "SENT BACK IN TIME TO HELP SHAPE A NEW PATH", 0,
                           1174))
        .child(body);
    return panel;
  }

  Element mfLoadReadout(int section) {
    using namespace tav;
    return box()
        .row()
        .gap(10)
        .alignItems(Align::Center)
        .child(t("ACCESSING", micro(12, alpha(kCyan, 0.85f), 260)))
        .child(t("\xe2\x96\xb8", micro(11, kCyan, 0)))
        .child(t(kNavItems[section], heavy(17, kNear, 80)))
        .child(box().width(60).height(10).foreground(
            TickRail{alpha(kCyan, 0.6f), 5, 3, 8, 1, 4, false, true}));
  }

  // ---- teal monitor panels ------------------------------------------------

  Element monitorBody(float w, float h) {
    using namespace tav;
    return box()
        .width(Dim(w))
        .height(Dim(h))
        .fill(Material::linearUnit({0, 0}, {0, 1},
                                   {{0.00f, kPanelHi},
                                    {0.15f, kPanel},
                                    {0.88f, kPanel},
                                    {1.00f, kPanelSh}}))
        .foreground(styles::gloss(alpha(kPanelHi, 0.5f), 40, {0, -h * 0.34f},
                                  0.72f, 0.28f))
        .foreground(
            shapes::onEdges(shapes::Edge::Top,
                            stroke(1, Fill::color(alpha(hex(0xCFEFEC), 0.7f)),
                                   PathFormat::Align::Inner)));
  }

  /** Four procedural "stills" — each a different flat-shape composition
   *  over the same portal recipe, tinted per index. */
  std::vector<Element> relatedStills() {
    using namespace tav;
    static const char* caps[4] = {"REEL 02", "BOARDS", "RIG TEST", "PLATE"};
    std::vector<Element> out;
    for (int i = 0; i < 4; ++i) {
      const float g = 0.30f + 0.18f * (float)i;
      Element cell =
          box()
              .grow(1)
              .column()
              .gap(3)
              .child(
                  box()
                      .grow(1)
                      .shape(shapes::chamfered(7, shapes::Corner::Diagonal))
                      .fill(Material::linearUnit(
                          {0, 0}, {0, 1},
                          {{0.0f, hex(0x0A2C33)}, {1.0f, hex(0x02171B)}}))
                      .stroke(stroke(1, Fill::color(alpha(hex(0x0B3B40), 0.9f)),
                                     PathFormat::Align::Inner))
                      .child(box().inset(0).fill(Material::radialUnit(
                          {0.3f + 0.15f * (float)i, 0.8f}, 0.95f,
                          {{0.0f, alpha(kGlow, g)},
                           {1.0f, alpha(kGlow, 0.0f)}})))
                      .child(at(box().fill(hex(0x011114)), 6 + 4 * (float)i, 18,
                                12, 30))
                      .child(at(box().fill(hex(0x01191D)), 24 + 3 * (float)i, 8,
                                16, 40))
                      .child(at(box().fill(alpha(kGlow, 0.55f)), 0, 40, 200, 1))
                      .foreground(Brackets{alpha(kCyan, 0.5f), 6, 1, 2,
                                           shapes::Corner::All})
                      .foreground(Scanlines{{0, 0, 0, 0.24f}, 3, 1}))
              .child(t(caps[i], micro(9, hex(0x123B3D), 220)));
      out.push_back(std::move(cell));
    }
    return out;
  }

  /** A label-over-value pair on a teal panel — the tabular voice. */
  Element specPair(const char* k, const char* v) {
    using namespace tav;
    return box()
        .column()
        .gap(2)
        .child(t(k, micro(9, alpha(hex(0x123B3D), 0.75f), 260)))
        .child(box().height(1).fill(alpha(kDate, 0.28f)))
        .child(t(v, type(blackFace(), 11, hex(0x0E3234), 40, 0.92f)));
  }

  Element featureSystem() {
    using namespace tav;
    Element thumb =
        box()
            .width(150)
            .height(150)
            .shrink(0)
            .shape(shapes::chamfered(12, shapes::Corner::Diagonal))
            .fill(Material::linearUnit(
                {0, 0}, {0, 1}, {{0.0f, hex(0x06232A)}, {1.0f, hex(0x011114)}}))
            .stroke(stroke(1, Fill::color(alpha(hex(0x0B3B40), 0.9f)),
                           PathFormat::Align::Inner))
            .child(box().inset(0).fill(
                Material::radialUnit({0.5f, 0.72f}, 0.95f,
                                     {{0.0f, alpha(kGlow, 0.8f)},
                                      {0.5f, alpha(kTealBar, 0.28f)},
                                      {1.0f, alpha(kTealBar, 0.0f)}})))
            .child(at(box().fill(hex(0x010A0C)), 18, 74, 30, 60))
            .child(at(box().fill(hex(0x02171B)), 52, 46, 44, 88))
            .child(at(box().fill(hex(0x010A0C)), 100, 62, 34, 72))
            .child(at(box().fill(alpha(kGlow, 0.6f)), 0, 108, 150, 1))
            .foreground(
                Brackets{alpha(kCyan, 0.85f), 12, 2, 4, shapes::Corner::All})
            .foreground(Scanlines{{0, 0, 0, 0.22f}, 3, 1});

    Element copy =
        box()
            .grow(1)
            .column()
            .gap(6)
            .child(
                box()
                    .row()
                    .gap(7)
                    .alignItems(Align::Center)
                    .child(box()
                               .width(9)
                               .height(9)
                               .shape(shapes::polygon(3, 90))
                               .fill(kDate))
                    .child(
                        t("01.30.06", type(blackFace(), 14, kDate, 40, 0.95f)))
                    .child(box().grow(1).height(1).fill(alpha(kDate, 0.35f))))
            .child(t("N.O.-XPLODE TV COMMERCIAL",
                     type(blackFace(), 17, hex(0x0E3234), 40, 0.92f)))
            .child(box()
                       .height(84)
                       .padding(9)
                       .fill(dither.material())
                       .foreground(stroke(1, Fill::color(alpha(kPanelSh, 0.9f)),
                                          PathFormat::Align::Inner))
                       // the ONE place this interface is not tracked caps
                       .child(t("2Advanced completes a broadcast spot for "
                                "BSN's N.O.-Xplode line \xe2\x80\x94 full CG "
                                "environment, character rig and compositing, "
                                "delivered in nine weeks on a Maxon pipeline "
                                "against a live-action plate.",
                                prose(13, hex(0x0B2C2E)))))
            // the related-work strip: four chamfered stills over the
            // dither ground, the way the FEATURE panel filled its slack
            .child(box()
                       .grow(1)
                       .row()
                       .gap(7)
                       .alignItems(Align::Stretch)
                       .children(relatedStills()))
            // the spec readout: dense, tabular, and never actually read
            .child(box()
                       .row()
                       .gap(14)
                       .child(specPair("CLIENT", "BSN / N.O.-XPLODE"))
                       .child(specPair("RUNTIME", "00:30 \xc2\xb7 NTSC"))
                       .child(specPair("TOOLS", "C4D R8 / AE 6.5"))
                       .child(specPair("DELIVERED", "01.24.06")))
            .child(box()
                       .row()
                       .gap(8)
                       .alignItems(Align::Center)
                       .child(t("\xe2\x80\xba VIEW CASE STUDY",
                                micro(11, hex(0x123B3D), 220)))
                       .child(box().grow(1))
                       .child(box()
                                  .width(150)
                                  .height(6)
                                  .fill(alpha(kPanelSh, 0.7f))
                                  .child(box()
                                             .left(Dim(0))
                                             .top(Dim(0))
                                             .width(112)
                                             .height(6)
                                             .fill(hex(0x0E3234))))
                       .child(t("74%", micro(10, hex(0x123B3D), 160)))
                       .child(box().width(126)));

    Element leftCol =
        box().width(150).shrink(0).column().gap(8).child(thumb).child(
            box()
                .grow(1)
                .column()
                .gap(4)
                .padding(8)
                .fill(dither.material())
                .foreground(stroke(1, Fill::color(alpha(kPanelSh, 0.9f)),
                                   PathFormat::Align::Inner))
                .child(t("CREDITS", micro(9, alpha(hex(0x123B3D), 0.8f), 260)))
                .child(box().height(1).fill(alpha(kDate, 0.28f)))
                .child(t("DIRECTION", micro(9, kDate, 200)))
                .child(t("ERIC JORDAN",
                         type(blackFace(), 11, hex(0x0E3234), 40, 0.92f)))
                .child(box().height(3))
                .child(t("STUDIO", micro(9, kDate, 200)))
                .child(t("2ADVANCED",
                         type(blackFace(), 11, hex(0x0E3234), 40, 0.92f)))
                .child(box().grow(1))
                .child(box()
                           .row()
                           .gap(4)
                           .alignItems(Align::Center)
                           .child(box().width(7).height(7).fill(
                               alpha(kDate, 0.8f)))
                           .child(t("ARCHIVED", micro(9, kDate, 200)))));

    Element bodyArea =
        monitorBody(690, 316).row().padding(11).gap(11).child(leftCol).child(
            copy);
    // the hazard wedge, bottom-left — the STATIC baked-tile pattern path
    bodyArea.child(at(box()
                          .shape([](SkSize s) {
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
    bodyArea.child(box()
                       .left(Dim(690 - 11 - 116))
                       .top(Dim(316 - 11 - 34))
                       .child(cta("LAUNCH", 116, 34, kPanelSh)));

    Element panel = doubleBevel(
        box().width(696).height(350).column().padding(3), kChrome, 3);
    panel.key("feature")
        .translateX(
            animate(from(90.0f).to(0.0f), {500ms, &ch::easeOutQuint, 2600ms}))
        .opacity(
            animate(from(0.0f).to(1.0f), {300ms, &ch::easeOutQuad, 2600ms}))
        .child(panelHeader("FEATURE", " SYSTEM", "LATEST TRANSMISSION", 1, 690))
        .child(bodyArea);
    return panel;
  }

  /** The entry column alone, so setup() can measure its laid-out height
   *  against the well and derive the real scroll overflow. kPressWellW is
   *  the width the entries wrap at inside the well: the 690 monitor body
   *  less its two 11 px paddings, the 8 px gap, the 16 px scrollbar and
   *  the clip's two 9 px paddings. kPressWellH is the matching viewport
   *  height: 376 less the two 11 px paddings, the 9 px column gap, the
   *  34 px footer row and the clip's two 9 px paddings. */
  static constexpr float kPressWellW = 690 - 2 * 11 - 8 - 16 - 2 * 9;
  static constexpr float kPressWellH = 376 - 2 * 11 - 9 - 34 - 2 * 9;
  Element pressList() {
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

    Element list = box().column().gap(9);
    for (const Entry& e : entries)
      list.child(
          box()
              .column()
              .gap(4)
              .child(
                  box()
                      .row()
                      .gap(7)
                      .alignItems(Align::Center)
                      .fill(alpha(kPanelSh, 0.55f))
                      .padding(6, 3)
                      .child(t(e.date, type(blackFace(), 13, kDate, 40, 0.95f)))
                      .child(box().grow(1).height(1).fill(alpha(kDate, 0.3f)))
                      .child(t("\xe2\x96\xb8", micro(9, kDate, 0))))
              .child(t(e.headline,
                       type(blackFace(), 13, hex(0x0E3234), 50, 0.92f)))
              .child(t(e.body, prose(12.5f, hex(0x0C2E30)))));
    return list;
  }

  Element pressUpdates() {
    using namespace tav;
    Element list = pressList().translateY(&pressScroll);

    Element scrollbar =
        box()
            .width(16)
            .column()
            .gap(3)
            .child(box()
                       .width(16)
                       .height(16)
                       .fill(kPanelSh)
                       .justify(Justify::Center)
                       .alignItems(Align::Center)
                       .child(t("\xe2\x96\xb4", micro(8, kBody, 0))))
            .child(
                box()
                    .grow(1)
                    .fill(alpha(kPanelSh, 0.6f))
                    .child(
                        box()
                            .left(Dim(2))
                            .top(Dim(6))
                            .width(12)
                            .height(90)
                            .fill(Material::linearUnit(
                                {0, 0}, {1, 0},
                                {{0.0f, hex(0xCFEFEC)}, {1.0f, kPanelHi}}))
                            .stroke(stroke(1, Fill::color(alpha(kDate, 0.4f)),
                                           PathFormat::Align::Inner))))
            .child(box()
                       .width(16)
                       .height(16)
                       .fill(kPanelSh)
                       .justify(Justify::Center)
                       .alignItems(Align::Center)
                       .child(t("\xe2\x96\xbe", micro(8, kBody, 0))));

    Element bodyArea =
        monitorBody(690, 376)
            .column()
            .padding(11)
            .gap(9)
            .child(box()
                       .grow(1)
                       .row()
                       .gap(8)
                       .child(box()
                                  .grow(1)
                                  .clip()
                                  .padding(9)
                                  .fill(dither.material())
                                  .foreground(stroke(
                                      1, Fill::color(alpha(kPanelSh, 0.9f)),
                                      PathFormat::Align::Inner))
                                  .child(list))
                       .child(scrollbar))
            .child(box()
                       .row()
                       .alignItems(Align::Center)
                       .gap(8)
                       .child(t("06 ENTRIES \xc2\xb7 PAGE 1/4",
                                micro(11, hex(0x123B3D), 220)))
                       .child(box().grow(1))
                       .child(cta("ARCHIVES", 116, 34, kPanelSh)));

    Element panel = doubleBevel(
        box().width(696).height(410).column().padding(3), kChrome, 3);
    panel.key("press")
        .translateY(
            animate(from(60.0f).to(0.0f), {420ms, &ch::easeOutQuint, 3250ms}))
        .opacity(
            animate(from(0.0f).to(1.0f), {300ms, &ch::easeOutQuad, 3250ms}))
        .child(panelHeader("PRESS", " UPDATES", "STUDIO WIRE", 2, 690))
        .child(bodyArea);
    return panel;
  }

  /** A module's red title bar inside AUXILIARY PANEL. */
  Element auxBar(const char* label) {
    using namespace tav;
    return box()
        .height(18)
        .row()
        .alignItems(Align::Center)
        .padding(6, 0)
        .gap(6)
        .fill(Material::linearUnit(
            {0, 0}, {0, 1}, {{0.0f, hex(0x5A1A20)}, {1.0f, hex(0x2E0A0C)}}))
        .child(t("\xc2\xbb", micro(10, kCyan, 0)))
        .child(t(label, micro(11, kNear, 160)));
  }

  /** The teal full-width VIEW bar the two right modules end on. */
  Element auxView() {
    using namespace tav;
    return box()
        .height(17)
        .fill(Material::linearUnit(
            {0, 0}, {0, 1},
            {{0.0f, kPanelHi}, {0.5f, kPanel}, {1.0f, kPanelSh}}))
        .stroke(stroke(1, Fill::color(alpha(hex(0xCFEFEC), 0.6f)),
                       PathFormat::Align::Inner))
        .justify(Justify::Center)
        .alignItems(Align::Center)
        .child(t("VIEW", label(11, kDate, 200)));
  }

  Element auxiliary() {
    using namespace tav;
    const SkColor4f kCopy = hex(0x7FD4D0);  // the module copy teal

    // Column 1: three icon rows, copy and link verbatim (including the
    // interface's own "inorder").
    struct Item {
      const char *glyph, *l1, *l2, *link;
    };
    static const Item items[3] = {
        {"\xe2\x96\xa0", "The Equipment store carries the latest",
         "2Advanced apparel and publications...", "\xe2\x80\xba VIEW"},
        {"\xe2\x96\xa3", "Chat live with a 2Advanced sales agent",
         "inorder to inquire about project pricing...", "\xe2\x80\xba OFFLINE"},
        {"\xe2\x9c\x89", "Subscribe to the 2Advanced Members",
         "List and receive exclusive news & press...",
         "\xe2\x80\xba SUBSCRIBE"},
    };
    Element supplementals = box().grow(1).basis(Dim(0)).column().gap(3).child(
        auxBar("SUPPLEMENTALS & "
               "ESSENTIALS"));
    for (const Item& it : items)
      supplementals.child(
          box()
              .row()
              .gap(8)
              .alignItems(Align::Center)
              .child(box()
                         .width(26)
                         .height(26)
                         .shrink(0)
                         .corners({4})
                         .fill(Material::linearUnit(
                             {0, 0}, {0, 1},
                             {{0.0f, hex(0x8E2A2A)}, {1.0f, hex(0x3A0C0E)}}))
                         .stroke(stroke(1, Fill::color(alpha(kNear, 0.4f)),
                                        PathFormat::Align::Inner))
                         .justify(Justify::Center)
                         .alignItems(Align::Center)
                         .child(t(it.glyph, micro(11, kPanelHi, 0))))
              .child(
                  box()
                      .grow(1)
                      .column()
                      .child(t(it.l1, prose(11.5f, kCopy)))
                      .child(box()
                                 .row()
                                 .child(t(it.l2, prose(11.5f, kCopy)))
                                 .child(box().grow(1))
                                 .child(t(it.link, micro(9, alpha(kNear, 0.85f),
                                                         160))))));

    // Column 2: the book plate is white — the one white rectangle on the
    // whole page — with the title set dark on it.
    Element photoshop =
        box()
            .grow(1)
            .basis(Dim(0))
            .column()
            .gap(4)
            .child(auxBar("PHOTOSHOP: SECRETS OF THE PROS"))
            .child(
                box()
                    .row()
                    .gap(8)
                    .grow(1)
                    .child(box()
                               .width(118)
                               .shrink(0)
                               .fill(hex(0xF2F0EA))
                               .column()
                               .padding(7, 6)
                               .gap(2)
                               .child(t("Photoshop",
                                        type(arial(), 15, hex(0x2A4A7A), 0)))
                               .child(t("Secrets of the Pros",
                                        type(arial(), 10, hex(0x333333), 0))))
                    .child(t("Eric Jordan appears in \"Photoshop: Secrets "
                             "of the Pros\", a book featuring 20 top "
                             "designers with insights on their "
                             "techniques/methods.",
                             prose(11.5f, kCopy))))
            .child(auxView());

    // Column 3: the 2ADVANCED.NET plate — its angular mark is the only
    // amber on the interface.
    Element press =
        box()
            .grow(1)
            .basis(Dim(0))
            .column()
            .gap(4)
            .child(auxBar("FEATURED PRESS"))
            .child(box()
                       .height(40)
                       .row()
                       .alignItems(Align::Center)
                       .padding(8, 0)
                       .gap(7)
                       .fill(Material::linearUnit(
                           {0, 0}, {0, 1},
                           {{0.0f, hex(0x2A0A0C)}, {1.0f, hex(0x140404)}}))
                       .stroke(stroke(1, Fill::color(alpha(kDust, 0.4f)),
                                      PathFormat::Align::Inner))
                       .child(box()
                                  .width(20)
                                  .height(20)
                                  .shape(shapes::chamfered(
                                      6, shapes::Corner::Diagonal))
                                  .fill(Material::linearUnit(
                                      {0, 0}, {0, 1},
                                      {{0.0f, hex(0xE8A83C)},
                                       {1.0f, hex(0x9A5E10)}})))
                       .child(box()
                                  .column()
                                  .gap(1)
                                  .child(t("2ADVANCED.NET",
                                           heavy(13, hex(0xD9DDE0), 60)))
                                  .child(t("PRECISION HOSTING PLATFORM",
                                           micro(8, kDust, 220)))))
            .child(t("2advanced Studios is pleased to announce the official "
                     "launch of 2advanced.net, a flexible and managed web "
                     "hosting platform.",
                     prose(11.5f, kCopy)))
            .child(box().grow(1))
            .child(auxView());

    Element panel = doubleBevel(
        box().width(1180).height(168).column().padding(3), kChrome, 3);
    panel.key("aux")
        .translateY(
            animate(from(56.0f).to(0.0f), {400ms, &ch::easeOutQuint, 3100ms}))
        .opacity(
            animate(from(0.0f).to(1.0f), {300ms, &ch::easeOutQuad, 3100ms}))
        .child(panelHeader("AUXILIARY", " PANEL",
                           "SENT BACK IN TIME TO HELP SHAPE A NEW PATH", 3,
                           1174))
        .child(box()
                   .grow(1)
                   .row()
                   .gap(10)
                   .padding(8, 6)
                   .fill(hex(0x300B0E))
                   .child(supplementals)
                   .child(photoshop)
                   .child(press));
    return panel;
  }

  /** The band the FWA screenshot leaves under AUXILIARY on the left
   *  column. A 2Advanced page never left 300 px of gradient showing. */
  Element transmissionLog() {
    using namespace tav;
    Element bars = box().row().gap(3).alignItems(Align::End).height(74);
    for (int i = 0; i < 34; ++i) {
      const float v = 0.18f + 0.80f * std::abs(std::sin(i * 0.73f) *
                                               std::cos(i * 0.31f + 1.1f));
      bars.child(
          box()
              .width(7)
              .height(Dim(74 * v))
              .shrink(0)
              .fill(Material::linearUnit({0, 0}, {0, 1},
                                         {{0.0f, alpha(kCyan, 0.95f)},
                                          {1.0f, alpha(kTealBar, 0.3f)}})));
    }

    static const char* hexLines[12] = {
        "0x0000  4A 10 0F 57 11 19 6A 1B  21 7B DA D6 95 C9 CC 8D",
        "0x0010  77 77 2C 7B 80 01 D0 D5  57 97 97 84 B8 B6 3C 82",
        "0x0020  82 70 00 00 B2 7E 82 F3  F3 F3 18 07 07 26 09 09",
        "0x0030  37 0C 0D 40 0E 0F 4C 10  10 2A 00 00 1A 00 01 0A",
        "0x0040  46 4C 41 53 48 49 4E 44  45 58 2E 53 57 46 00 00",
        "0x0050  43 57 53 06 EC 45 08 00  78 9C 00 00 00 00 00 00",
        "0x0060  50 52 4F 50 48 45 43 59  20 50 52 49 4D 45 00 00",
        "0x0070  46 53 69 6E 67 6C 65 42  65 76 65 6C 50 61 6E 65",
        "0x0080  6C 43 6C 61 73 73 00 46  44 6F 75 62 6C 65 42 65",
        "0x0090  76 65 6C 50 61 6E 65 6C  43 6C 61 73 73 00 00 00",
        "0x00A0  48 65 6C 76 65 74 69 63  61 20 39 35 20 42 6C 61",
        "0x00B0  63 6B 00 41 72 69 61 6C  20 42 6C 61 63 6B 00 00",
    };
    Element hexRows = box().column().gap(2);
    for (int i = 0; i < 12; ++i)
      hexRows.child(
          t(hexLines[i],
            type(arial(), 11, alpha(kCyanRing, i < 7 ? 0.6f : 0.30f), 60)));

    Element gauges = box().row().gap(10).alignItems(Align::Center);
    for (int i = 0; i < 3; ++i)
      gauges.child(
          box()
              .width(58)
              .height(58)
              .fill(sdf::material(sdf::circle(), {.fill = hex(0x140505),
                                                  .borderWidth = 2,
                                                  .borderColor = kChromeHi}))
              .justify(Justify::Center)
              .alignItems(Align::Center)
              .child(radarSweep(i, kCyan, 0.34f))
              .child(t(i == 0 ? "A" : (i == 1 ? "B" : "C"),
                       micro(11, alpha(kCyan, 0.85f), 120))));

    Element panel =
        singleBevel(box().width(1180).height(236).column().padding(9).gap(7),
                    hex(0x3E1013));
    panel.key("txlog")
        .translateY(
            animate(from(56.0f).to(0.0f), {400ms, &ch::easeOutQuint, 3400ms}))
        .opacity(
            animate(from(0.0f).to(1.0f), {320ms, &ch::easeOutQuad, 3400ms}))
        .foreground(Brackets{alpha(kCyan, 0.5f), 14, 2, 5,
                             shapes::Corner::AntiDiagonal})
        .child(box()
                   .row()
                   .alignItems(Align::Center)
                   .gap(9)
                   .height(20)
                   .child(t("TRANSMISSION", heavy(15, kNear, 40)))
                   .child(t(" LOG", type(arial(), 14, kHeadDim, 40, 0.95f)))
                   .child(box().width(1).height(12).fill(alpha(kCyan, 0.4f)))
                   .child(t("BUFFER 04 / 16 \xc2\xb7 UPLINK NOMINAL",
                            micro(11, kDust, 240)))
                   .child(box().grow(1))
                   .child(tickDots(6))
                   .child(box().width(120).height(10).foreground(TickRail{
                       alpha(kCyan, 0.5f), 5, 3, 8, 1, 4, false, true})))
        .child(
            box()
                .row()
                .gap(10)
                .grow(1)
                .child(
                    box()
                        .width(380)
                        .padding(8)
                        .clip()
                        .column()
                        .gap(5)
                        .fill(hex(0x180505))
                        .foreground(InsetBevel{
                            alpha(kChromeHi, 0.5f), {0, 0, 0, 0.5f}, 0, 1, 1})
                        .foreground(Brackets{alpha(kCyan, 0.4f), 9, 2, 3,
                                             shapes::Corner::All})
                        .child(
                            box()
                                .row()
                                .alignItems(Align::Center)
                                .gap(6)
                                .child(t("SPECTRUM 34CH",
                                         micro(10, alpha(kCyan, 0.9f), 220)))
                                .child(box().grow(1).height(1).fill(
                                    alpha(kChromeHi, 0.9f)))
                                .child(t("-12 dB", micro(10, kDustDim, 200))))
                        .child(box().grow(1))
                        .child(bars)
                        .child(box().height(9).foreground(TickRail{
                            alpha(kCyan, 0.4f), 10, 3, 7, 1, 4, false, false}))
                        .child(box()
                                   .row()
                                   .justify(Justify::SpaceBetween)
                                   .child(t("20", micro(9, kDustDim, 180)))
                                   .child(t("400", micro(9, kDustDim, 180)))
                                   .child(t("2K", micro(9, kDustDim, 180)))
                                   .child(t("8K", micro(9, kDustDim, 180)))
                                   .child(t("16K", micro(9, kDustDim, 180)))))
                .child(
                    box()
                        .grow(1)
                        .padding(8)
                        .clip()
                        .column()
                        .gap(4)
                        .fill(hex(0x140404))
                        .foreground(InsetBevel{
                            alpha(kChromeHi, 0.5f), {0, 0, 0, 0.5f}, 0, 1, 1})
                        .child(
                            box()
                                .row()
                                .alignItems(Align::Center)
                                .gap(6)
                                .child(t("PACKET DUMP \xc2\xb7 "
                                         "flashindex.swf",
                                         micro(10, alpha(kCyan, 0.85f), 220)))
                                .child(box().grow(1).height(1).fill(
                                    alpha(kChromeHi, 0.9f)))
                                .child(
                                    t("543,340 B", micro(10, kDustDim, 200))))
                        .child(hexRows)
                        .child(box().grow(1))
                        .child(
                            box()
                                .row()
                                .alignItems(Align::Center)
                                .gap(6)
                                .child(t("CRC 0x8E41", micro(9, kDustDim, 200)))
                                .child(box().width(1).height(9).fill(
                                    alpha(kDust, 0.3f)))
                                .child(t("CWS \xc2\xb7 SWF6 \xc2\xb7 "
                                         "ZLIB",
                                         micro(9, kDustDim, 200)))
                                .child(box().grow(1))
                                .child(t("\xe2\x96\xb8 VERIFIED",
                                         micro(9, alpha(kCyan, 0.8f), 200)))))
                .child(
                    box()
                        .column()
                        .gap(6)
                        .alignItems(Align::Center)
                        .padding(9, 6)
                        .fill(hex(0x1B0607))
                        .foreground(InsetBevel{
                            alpha(kChromeHi, 0.5f), {0, 0, 0, 0.5f}, 0, 1, 1})
                        .child(gauges)
                        .child(
                            t("EQUIPMENT STATUS", micro(10, kDustDim, 220)))));
    return panel;
  }

  /** The wire ticker: the content of ONE marquee unit. Measured once in
   *  setup() so the strip's wrap length is exact (marquee's
   *  width-pinned overload — an unpinned strip wraps against the clip). */
  Element wireContent() {
    using namespace tav;
    return box()
        .row()
        .alignItems(Align::Center)
        .height(24)
        .gap(14)
        .child(t("\xe2\x96\xb8 UPLINK NOMINAL", micro(11, kCyan, 220)))
        .child(t("\xc2\xb7", micro(11, kDustDim, 0)))
        .child(t("RENDER FARM 08/08 ONLINE", micro(11, kDust, 220)))
        .child(t("\xc2\xb7", micro(11, kDustDim, 0)))
        .child(t("PROPHECY PRIME SKIN NOW SHIPPING", micro(11, kDust, 220)))
        .child(t("\xc2\xb7", micro(11, kDustDim, 0)))
        .child(t("FWA SITE OF THE MONTH \xe2\x80\x94 03.11.06",
                 micro(11, kDust, 220)))
        .child(t("\xc2\xb7", micro(11, kDustDim, 0)))
        .child(t("SUBSCRIBE TO THE DISPATCH \xe2\x80\x94 41,208 STRONG",
                 micro(11, kDust, 220)))
        .child(t("\xc2\xb7", micro(11, kDustDim, 0)))
        .child(t("BEST VIEWED AT 1024\xc3\x97"
                 "768 \xc2\xb7 FLASH PLAYER 6",
                 micro(11, kDustDim, 220)));
  }

  /** The full-width status bus between the panel grid and SUB SYSTEM. */
  Element statusWire() {
    using namespace tav;
    Element strip = marquee(wireContent(), wireW, &wirePhase, 60);
    strip.grow(1).height(24);
    return singleBevel(box()
                           .width(1892)
                           .height(38)
                           .row()
                           .alignItems(Align::Center)
                           .padding(12, 0)
                           .gap(10),
                       hex(0x3E1013))
        .key("wire")
        .opacity(
            animate(from(0.0f).to(1.0f), {400ms, &ch::easeOutQuad, 3550ms}))
        .foreground(TickRail{alpha(kDust, 0.3f), 8, 3, 6, 1, 4, false, true})
        .child(t("STATUS BUS", heavy(12, kNear, 60)))
        .child(box().width(1).height(16).fill(alpha(kCyan, 0.4f)))
        .child(box().width(40).height(10).foreground(
            TickRail{alpha(kCyan, 0.55f), 5, 3, 7, 1, 4, false, false}))
        .child(strip)
        .child(box().width(1).height(16).fill(alpha(kDust, 0.35f)))
        .child(tickDots(5))
        .child(t("LIVE", micro(11, alpha(kCyan, 0.9f), 220)));
  }

  /** A tiny chamfered radio key — the SUB SYSTEM row's preference bank. */
  Element toggle(const char* lbl, bool on) {
    using namespace tav;
    return box()
        .height(18)
        .padding(7, 0)
        .shape(shapes::chamfered(5, shapes::Corner::Diagonal))
        .fill(on ? Material::linearUnit(
                       {0, 0}, {0, 1},
                       {{0.0f, hex(0x0A4148)}, {1.0f, hex(0x02181C)}})
                 : Material::solid(hex(0x220608)))
        .stroke(stroke(
            1, Fill::color(on ? alpha(kCyan, 0.7f) : alpha(kDust, 0.35f)),
            PathFormat::Align::Inner))
        .justify(Justify::Center)
        .alignItems(Align::Center)
        .child(t(lbl, micro(10, on ? kCyan : kDustDim, 160)));
  }

  std::vector<Element> footerLinks() {
    using namespace tav;
    static const char* l[8] = {"HOME",      "COMPANY",   "SERVICES",
                               "PORTFOLIO", "ACCOLADES", "EXPERIMENTAL",
                               "EQUIPMENT", "CONTACT"};
    std::vector<Element> out;
    for (int i = 0; i < 8; ++i) {
      if (i) out.push_back(box().width(1).height(9).fill(alpha(kDust, 0.3f)));
      out.push_back(t(l[i], micro(10, kDustDim, 200)));
    }
    return out;
  }

  Element subSystem() {
    using namespace tav;
    auto chip = [&](const char* glyph) {
      return box()
          .width(40)
          .height(40)
          .corners({20})
          .fill(Material::linearUnit(
              {0, 0}, {0, 1}, {{0.0f, hex(0x6A1B21)}, {1.0f, hex(0x220608)}}))
          .stroke(stroke(1, Fill::color(alpha(kDust, 0.5f)),
                         PathFormat::Align::Inner))
          .justify(Justify::Center)
          .alignItems(Align::Center)
          .child(t(glyph, heavy(15, kCyan, 0)));
    };
    auto selector = [&](const char* lbl, const char* value) {
      return box()
          .row()
          .gap(8)
          .alignItems(Align::Center)
          .child(box()
                     .width(46)
                     .height(34)
                     .shape(shapes::chamfered(8, shapes::Corner::Diagonal))
                     .fill(Material::radialUnit(
                         {0.5f, 0.76f}, 1.1f,
                         {{0.0f, hex(0x0A4148)}, {1.0f, hex(0x010D10)}}))
                     .stroke(stroke(1, Fill::color(alpha(kCyan, 0.5f)),
                                    PathFormat::Align::Inner)))
          .child(box()
                     .column()
                     .gap(1)
                     .child(t(lbl, micro(10, kDustDim, 240)))
                     .child(box()
                                .row()
                                .gap(5)
                                .alignItems(Align::Center)
                                .child(t("\xe2\x96\xb8", micro(9, kCyan, 0)))
                                .child(t(value, label(13, kNear, 90)))
                                .child(t("\xe2\x96\xbe", micro(9, kDust, 0)))));
    };

    Element row = singleBevel(box()
                                  .width(1892)
                                  .height(100)
                                  .row()
                                  .alignItems(Align::Center)
                                  .padding(14, 0)
                                  .gap(18),
                              hex(0x2E0B0D));
    row.key("subsys")
        .background(
            styles::Overlay{hazard.material(), SkBlendMode::kSrcOver, 0.16f})
        .opacity(
            animate(from(0.0f).to(1.0f), {400ms, &ch::easeOutQuad, 3650ms}))
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
        .child(box().width(1).height(30).fill(alpha(kDust, 0.35f)))
        .child(box()
                   .column()
                   .gap(3)
                   .child(t("SOUND", micro(10, kDustDim, 240)))
                   .child(box()
                              .row()
                              .gap(4)
                              .alignItems(Align::Center)
                              .child(toggle("ON", true))
                              .child(toggle("OFF", false))))
        .child(box().width(1).height(30).fill(alpha(kDust, 0.35f)))
        .child(box()
                   .column()
                   .gap(3)
                   .child(t("QUALITY", micro(10, kDustDim, 240)))
                   .child(box()
                              .row()
                              .gap(4)
                              .alignItems(Align::Center)
                              .child(toggle("LOW", false))
                              .child(toggle("MED", false))
                              .child(toggle("HIGH", true))))
        .child(box().width(1).height(30).fill(alpha(kDust, 0.35f)))
        .child(box()
                   .column()
                   .gap(3)
                   .child(t("RESOLUTION", micro(10, kDustDim, 240)))
                   .child(t("\xe2\x96\xb8 1024\xc3\x97"
                            "768 \xc2\xb7 32-BIT",
                            label(13, kNear, 90))))
        .child(box().grow(1))
        .child(box()
                   .column()
                   .alignItems(Align::End)
                   .gap(3)
                   .child(t("BANDWIDTH  \xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa0"
                            "\xe2\x96\xa0\xe2\x96\xa0\xe2\x96\xa1\xe2\x96\xa1",
                            micro(11, alpha(kCyan, 0.85f), 200)))
                   .child(t("UPTIME 118:24:07", micro(10, kDustDim, 200))))
        .child(box().width(70).height(40).foreground(
            TickRail{alpha(kCyan, 0.45f), 6, 4, 10, 1, 3, false, true}));
    return row;
  }

  Element legalStrip() {
    using namespace tav;
    return box()
        .width(1892)
        .height(90)
        .column()
        .padding(6, 8)
        .alignItems(Align::Center)
        .gap(4)
        .key("legal")
        .opacity(
            animate(from(0.0f).to(1.0f), {400ms, &ch::easeOutQuad, 3750ms}))
        .child(
            box()
                .alignSelf(Align::Stretch)
                .row()
                .alignItems(Align::Center)
                .child(box()
                           .row()
                           .gap(6)
                           .alignItems(Align::Center)
                           .child(box().width(60).height(1).fill(
                               alpha(kDust, 0.35f)))
                           .child(t("SITE REQUIRES MACROMEDIA FLASH "
                                    "PLAYER 6",
                                    micro(10, kDustDim, 200))))
                .child(box().grow(1))
                .child(
                    box()
                        .row()
                        .gap(7)
                        .alignItems(Align::Center)
                        .child(t("ARCHIVED VERSIONS:", micro(11, kDust, 240)))
                        .child(
                            box()
                                .height(24)
                                .padding(8, 0)
                                .shape(shapes::chamfered(
                                    7, shapes::Corner::Diagonal))
                                .fill(hex(0x2A0A0C))
                                .stroke(stroke(1,
                                               Fill::color(alpha(kDust, 0.45f)),
                                               PathFormat::Align::Inner))
                                .row()
                                .gap(6)
                                .alignItems(Align::Center)
                                .child(t("\xe2\x96\xb8", micro(9, kCyan, 0)))
                                .child(
                                    t("V3 'EXPANSIONS'", label(12, kNear, 90)))
                                .child(t("\xe2\x96\xbe", micro(9, kDust, 0))))))
        .child(box().grow(1))
        .child(box()
                   .row()
                   .gap(9)
                   .alignItems(Align::Center)
                   .child(box().width(40).height(1).fill(alpha(kDust, 0.3f)))
                   .children(footerLinks())
                   .child(box().width(40).height(1).fill(alpha(kDust, 0.3f))))
        .child(box().height(4))
        .child(t("COPYRIGHT (C) 2003 2ADVANCED STUDIOS, LLC.  ALL RIGHTS "
                 "RESERVED.",
                 micro(12, kDust, 240)))
        .child(box()
                   .row()
                   .gap(10)
                   .alignItems(Align::Center)
                   .child(t("LEGAL", micro(11, kDustDim, 200)))
                   .child(box().width(1).height(10).fill(alpha(kDust, 0.35f)))
                   .child(t("PRIVACY POLICY", micro(11, kDustDim, 200)))
                   .child(box().width(1).height(10).fill(alpha(kDust, 0.35f)))
                   .child(t("SITE MAP", micro(11, kDustDim, 200))));
  }

  /** The dock's oscilloscope bars — monochrome oxblood, like the rest of
   *  sitefooter.gif. Every height is a closed-form function of the bar
   *  index and never of time, so the whole strip stays picture-cached. */
  std::vector<Element> dockBars() {
    using namespace tav;
    std::vector<Element> bars;
    for (int i = 0; i < 56; ++i) {
      const float v = 0.14f + 0.82f * std::abs(std::sin(i * 0.51f) *
                                               std::cos(i * 0.19f + 0.7f));
      bars.push_back(
          box()
              .grow(1)
              .shrink(0)
              .height(Dim(72 * v))
              .fill(Material::linearUnit(
                  {0, 0}, {0, 1},
                  {{0.0f, alpha(kD7, 1.0f)}, {1.0f, alpha(kD4, 0.9f)}})));
    }
    return bars;
  }

  /** The 970×110 sitefooter.gif, ×2 — deliberately MONOCHROME oxblood,
   *  no accent colour anywhere (the real asset saves colour for the SWF's
   *  live states). Crosshatch dither, bracketed windows, chevrons, and the
   *  three-circle gauge cluster in its dark bezel. */
  Element footerDock() {
    using namespace tav;
    if (footerGif) {
      // The real 970×110 dock bitmap at ×2 — on the page it sat BELOW
      // the SWF as a plain image, monochrome oxblood, no live states.
      // Everything the procedural fallback rebuilds is already in it.
      return box()
          .width(1892)
          .height(220)
          .fill(stretchFill(footerGif, 1892, 220))
          .key("dock")
          .opacity(
              animate(from(0.0f).to(1.0f), {400ms, &ch::easeOutQuad, 3850ms}))
          .foreground(shapes::onEdges(
              shapes::Edge::Top,
              stroke(2, Fill::color(kD5), PathFormat::Align::Inner)));
    }
    Element strip =
        box()
            .width(1892)
            .height(220)
            .fill(Material::blend(
                {{Material::linearUnit(
                      {0, 0}, {0, 1}, {{0.0f, kD5}, {0.45f, kD2}, {1.0f, kD1}}),
                  SkBlendMode::kSrcOver},
                 {hatchA.material(), SkBlendMode::kSrcOver},
                 {hatchB.material(), SkBlendMode::kSrcOver}}))
            .row()
            .alignItems(Align::Center)
            .padding(14, 12)
            .gap(12)
            .key("dock")
            .opacity(
                animate(from(0.0f).to(1.0f), {400ms, &ch::easeOutQuad, 3850ms}))
            .foreground(shapes::onEdges(
                shapes::Edge::Top,
                stroke(2, Fill::color(kD5), PathFormat::Align::Inner)));

    auto window = [&](const char* title, const char* a, const char* b,
                      float w) {
      return readout(w, 150, hex(0x140404))
          .column()
          .padding(10)
          .gap(5)
          .child(box()
                     .row()
                     .alignItems(Align::Center)
                     .gap(6)
                     .child(t(title, type(blackFace(), 12, kD7, 60, 0.92f)))
                     .child(box().grow(1).height(1).fill(kD4))
                     .child(t("\xc2\xbb", micro(11, kD5, 0))))
          .child(t(a, micro(10, kD6, 220)))
          .child(t(b, micro(10, alpha(kD6, 0.7f), 220)))
          .child(box().grow(1))
          .child(box()
                     .row()
                     .gap(5)
                     .alignItems(Align::Center)
                     .child(box().width(58).height(8).foreground(
                         TickRail{kD6, 5, 3, 7, 1, 3, false, false}))
                     .child(box().grow(1))
                     .child(t("v v", micro(10, kD6, 200))));
    };

    strip.child(
        window("NAVIGATION", "SECTOR / PROPHECY", "NODE 04.11.22", 300));
    strip.child(
        window("EQUIPMENT", "RENDER FARM  08/08", "STORAGE  4.2 TB", 300));
    strip.child(window("DISPATCH", "QUEUE  00114", "LAST  04.05.06", 280));

    // the instanced chevron array — one atlas cell, one stamp
    strip.child(
        box()
            .width(260)
            .height(150)
            .shape(shapes::chamfered(7, shapes::Corner::AntiDiagonal))
            .fill(hex(0x110303))
            .foreground(InsetBevel{kD5, {0, 0, 0, 0.6f}, 0, 1, 1})
            .child(box().left(Dim(12)).top(Dim(12)).width(236).height(96).child(
                instancing::instances(dockAtlas, dockPool,
                                      instancing::Mode::Data)))
            .child(box().left(Dim(12)).top(Dim(122)).child(
                t("ARRAY 6\xc3\x97"
                  "14 \xc2\xb7 IDLE",
                  micro(10, kD6, 220)))));

    strip.child(
        box()
            .grow(1)
            .height(150)
            .shape(shapes::chamfered(7, shapes::Corner::AntiDiagonal))
            .fill(hex(0x140404))
            .column()
            .padding(10)
            .gap(5)
            .foreground(InsetBevel{kD5, {0, 0, 0, 0.6f}, 0, 1, 1})
            .foreground(Brackets{kD6, 9, 2, 3, shapes::Corner::All})
            .child(
                box()
                    .row()
                    .alignItems(Align::Center)
                    .gap(6)
                    .child(t("SIGNAL", type(blackFace(), 12, kD7, 60, 0.92f)))
                    .child(box().grow(1).height(1).fill(kD4))
                    .child(t("\xc2\xbb", micro(11, kD5, 0))))
            .child(box()
                       .grow(1)
                       .fill(hex(0x0D0202))
                       .foreground(InsetBevel{kD4, {0, 0, 0, 0.5f}, 0, 1, 1})
                       .row()
                       .alignItems(Align::End)
                       .gap(2)
                       .padding(6)
                       .children(dockBars()))
            .child(box()
                       .row()
                       .gap(6)
                       .alignItems(Align::Center)
                       .child(t("GAIN 0.42 \xc2\xb7 SWEEP 20 MS",
                                micro(10, kD6, 220)))
                       .child(box().grow(1))
                       .child(box().width(70).height(8).foreground(
                           TickRail{kD5, 5, 3, 7, 1, 3, false, false}))));

    Element cluster =
        box()
            .width(310)
            .height(150)
            .shape(shapes::chamfered(9, shapes::Corner::Diagonal))
            .fill(Material::linearUnit({0, 0}, {0, 1},
                                       {{0.0f, kD3}, {1.0f, hex(0x0C0202)}}))
            .foreground(InsetBevel{kD5, {0, 0, 0, 0.6f}, 5, 2, 1})
            .foreground(Brackets{kD6, 12, 2, 5, shapes::Corner::All})
            .row()
            .justify(Justify::Center)
            .alignItems(Align::Center)
            .gap(14);
    for (int i = 0; i < 3; ++i)
      cluster.child(
          box()
              .width(80)
              .height(80)
              .fill(sdf::material(sdf::circle(), {.fill = hex(0x0A0202),
                                                  .borderWidth = 3,
                                                  .borderColor = kD6}))
              .justify(Justify::Center)
              .alignItems(Align::Center)
              .child(radarSweep(i, hex(0xB65050), 0.42f))
              .child(box()
                         .inset(26)
                         .corners({16})
                         .fill(alpha(kD1, 0.92f))
                         .stroke(stroke(1, Fill::color(kD4),
                                        PathFormat::Align::Inner)))
              .child(t(i == 0 ? "01" : (i == 1 ? "02" : "03"),
                       micro(11, kD7, 140))));
    strip.child(cluster);
    return strip;
  }

  Element rail(bool right) {
    using namespace tav;
    Element r = box()
                    .left(Dim(right ? 1916.0f : 0.0f))
                    .top(Dim(0))
                    .width(24)
                    .height(Dim(1560))
                    .cache(Cache::None);
    const auto& gif = right ? railRightGif : railLeftGif;
    if (gif) {
      // The production rail bitmap, held to the shell's own display
      // geometry: the page shows the 26×780 GIF at 12×780 CSS px, and
      // this frame is ×2 of that page, so the node is 24×1560.
      r.fill(stretchFill(gif, 24, 1560));
      // The flare highlight stays live on top — the bitmap carries the
      // flare ART, and the travelling sheen is drawn over it.
      r.foreground(RailFlares{hex(0x99AAAA), 6.0f, right ? 3.0f : 0.0f});
      return r;
    }
    return r
        .fill(Material::linearUnit({0, 0}, {0, 1},
                                   {{0.00f, hex(0x6A1B21)},
                                    {0.22f, kChrome},
                                    {0.70f, hex(0x2A0708)},
                                    {1.00f, hex(0x0A0000)}}))
        .foreground(
            shapes::onEdges(right ? shapes::Edge::Left : shapes::Edge::Right,
                            stroke(1, Fill::color(alpha(hex(0x99AAAA), 0.35f)),
                                   PathFormat::Align::Inner)))
        .foreground(RailFlares{hex(0x99AAAA), 6.0f, right ? 3.0f : 0.0f});
  }

  // ---- boot overlay: dot, reticle, percentage, flash ----------------------

  Element bootOverlay() {
    using namespace tav;
    const float cx = 970, cy = 760;

    auto hair = [&](float x, float y, float w, float h, float dx, float dy,
                    int delayMs) {
      return at(box()
                    .shape(ray(dx, dy))
                    .stroke(spans::upTo(
                                animate(from(0.0f).to(1.0f),
                                        {400ms, &ch::easeOutQuint,
                                         std::chrono::milliseconds(delayMs)})),
                            stroke(1.5f, Fill::color(kCyan))),
                x, y, w, h);
    };

    Element o = stack().inset(0).zIndex(90);
    o.child(box()
                .inset(0)
                .fill(hex(0x120303))
                .opacity(animate(
                    through({{0ms, 1.0f}, {1400ms, 1.0f}, {1560ms, 0.0f}}))));
    // 1. the single cyan pixel-dot
    o.child(at(box().fill(kCyan), cx - 3, cy - 3, 6, 6)
                .opacity(animate(through({{0ms, 0.0f},
                                          {150ms, 1.0f},
                                          {1350ms, 1.0f},
                                          {1450ms, 0.0f}}))));
    // 2. the reticle drawing OUTWARD from it on four trimmed rays
    o.child(hair(cx - 470, cy, 470, 1, -1, 1, 150));
    o.child(hair(cx, cy, 470, 1, 1, 1, 150));
    o.child(hair(cx, cy - 300, 1, 300, 1, -1, 220));
    o.child(hair(cx, cy, 1, 300, 1, 1, 220));
    o.child(
        at(box()
               .shape(shapes::arc(-90, 359))
               .stroke(spans::upTo(animate(from(0.0f).to(1.0f),
                                           {500ms, &ch::easeOutQuint, 260ms})),
                       stroke(1, Fill::color(alpha(kCyan, 0.7f)))),
           cx - 92, cy - 92, 184, 184));
    // 3. the 0→100 readout (a slot: TEXT, so it cannot be a binding)
    o.child(
        at(box().column().alignItems(Align::Center).gap(9), cx - 260, cy + 120,
           520, 110)
            .opacity(animate(through({{520ms, 0.0f},
                                      {620ms, 1.0f},
                                      {1350ms, 1.0f},
                                      {1450ms, 0.0f}})))
            .child(slot("bootpct"))
            .child(box()
                       .width(420)
                       .height(2)
                       .fill(alpha(kCyan, 0.18f))
                       .child(box().inset(0).shape(ray(1, 1)).stroke(
                           spans::upTo(animate(from(0.0f).to(1.0f),
                                               {800ms, &ch::easeNone, 550ms})),
                           stroke(2, Fill::color(kCyan)))))
            .child(t("LOADING PROPHECY INTERFACE \xc2\xb7 970\xc3\x97"
                     "655",
                     micro(11, alpha(kCyan, 0.6f), 240))));
    // 4. the boot-complete flash
    o.child(box()
                .inset(0)
                .fill(SkColor4f{1, 1, 1, 1})
                .opacity(animate(
                    through({{1330ms, 0.0f}, {1390ms, 0.7f}, {1460ms, 0.0f}})))
                .blend(SkBlendMode::kPlus));
    o.opacity(animate(through({{1440ms, 1.0f}, {1480ms, 0.0f}})));
    return o;
  }

  Element bootReadout() {
    using namespace tav;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%03d", bootPct);
    return box()
        .row()
        .alignItems(Align::Baseline)
        .gap(6)
        .child(t(buf, type(blackFace(), 46, kCyan, 40, 0.9f)))
        .child(t("%", type(blackFace(), 20, alpha(kCyan, 0.6f), 40, 0.9f)));
  }

  // =========================================================================

  Element describe() {
    using namespace tav;
    Element stage =
        box().left(Dim(24)).top(Dim(0)).width(1892).height(Dim(1530));
    stage.child(statusBar());
    stage.child(at(audioModule(), 0, 48, 596, 174));
    stage.child(at(navBar(), 604, 48, 584, 46));
    stage.child(at(quickLaunch(), 604, 100, 584, 144));
    stage.child(at(masthead(), 1192, 8, 700, 236));
    stage.child(at(mainframe(), 0, 246, 1180, 350));
    stage.child(at(featureSystem(), 1196, 246, 696, 350));
    stage.child(at(auxiliary(), 0, 616, 1180, 168));
    stage.child(at(transmissionLog(), 0, 790, 1180, 236));
    stage.child(at(pressUpdates(), 1196, 616, 696, 410));
    stage.child(at(statusWire(), 0, 1038, 1892, 38));
    stage.child(at(subSystem(), 0, 1090, 1892, 96));
    stage.child(at(legalStrip(), 0, 1196, 1892, 96));
    stage.child(at(footerDock(), 0, 1310, 1892, 220));

    // sitebackground.gif is a 1×1600 strip tiled across the page: the
    // oxblood is only the top band, and it is nearly black by a third
    // of the way down. When the real strip is loaded it IS the page —
    // repeated in x, ×2 in y, clamped so the canvas shows the strip's
    // top 780 rows exactly as a 780-CSS-px-tall page did. The fallback
    // ramp keeps the same shape (a two-stop ramp reads as flat maroon
    // and gets figure/ground backwards — the PANELS are the light thing
    // on this page) and adds grain for the strip's vertical tooth.
    Element page = stack();
    if (siteBgGif) {
      page.fill(stretchFill(siteBgGif, 1940, 3200, SkTileMode::kRepeat));
    } else {
      page.fill(Material::linearUnit({0, 0}, {0, 1},
                                     {{0.00f, kBgTop},
                                      {0.13f, hex(0x2E0808)},
                                      {0.40f, hex(0x150202)},
                                      {1.00f, kBgBot}}))
          .child(box().inset(0).fill(grain).opacity(0.07f).blend(
              SkBlendMode::kOverlay));
    }
    return page.child(rail(false))
        .child(rail(true))
        .child(stage)
        .child(bootOverlay());
  }

  // =========================================================================

  void setup(sketch::SketchContext& ctx) override {
    ctx.captureAt(6.0);
    using namespace tav;
    ctx.canvas(1940, 1560);
    ctx.background(hex(0x0A0000));

    // --- the production shell bitmaps, from the restoration host ----------
    // The loader's https path caches on disk (CacheFirst), so only the
    // very first run touches the network; each call returns null when the
    // fetch fails AND nothing is cached, which the use sites treat as
    // "draw the procedural stand-in".
    {
      sigil::loader::Hub& hub = ctx.assets.hub();
      const std::string base = "https://v4prophecy.2advanced.com/images/";
      railLeftGif = hub.image(base + "leftsidepanel.gif");
      railRightGif = hub.image(base + "rightsidepanel.gif");
      siteBgGif = hub.image(base + "sitebackground.gif");
      footerGif = hub.image(base + "sitefooter.gif");
      logoBugSvg = hub.image(base + "2alogobug.svg", {.width = 124});
    }

    // --- generated materials, built ONCE and HELD (identity = pruning) ---
    hazard = patterns::stripes(6, 10, kChromeHi);
    hazard.rotate(45);
    hatchA = patterns::stripes(1, 7, alpha(kD4, 0.5f));
    hatchA.rotate(45);
    hatchB = patterns::stripes(1, 7, alpha(kD1, 0.55f));
    hatchB.rotate(-45);
    dither =
        patterns::checker(1.5f, alpha(kPanelSh, 0.28f), alpha(kPanelHi, 0.15f));
    // LUMINANCE grain (equal channels), so the kOverlay pass reads as
    // LIGHT on the oxblood ramp instead of hue-shifting it — patterns::
    // noise() is fractal RGB and turns the page into rainbow terrazzo.
    // `stretch` gives the grain a slight vertical tooth, which is what
    // the real 1×1600 sitebackground.gif strip has.
    grain = patterns::grain(0.9f, 3, 4.0f, 1.25f, 1.6f);

    spectrum = Material::sksl(spectrumFx(), {{"uBars", 32.0f}})
                   .uniform("uHot", kGlow)
                   .uniform("uCool", kTealBar)
                   .quantizeTime(10.0f);  // 10 steps a second, not a slide

    // ONE stripe material value, reused by the nav bar and four panel
    // headers; the pan is a bound uniform, not five redraw loops.
    stripesLive =
        Material::sksl(stripeFx(), {{"uOn", 6.0f}, {"uPeriod", 16.0f}})
            .uniform("uColor", kChromeHi)
            .uniform("uBase", kChrome)
            .uniform("uPan", &stripePan);

    waterStreaks = Material::sksl(waterFx());

    // measure the ticker's ONE unit so the marquee's wrap is exact
    wireW = std::ceil(ctx.measure(wireContent()).width());
    if (wireW < 1) wireW = 1;

    // measure the press entries at the well's own wrap width, so the
    // auto-scroll walks the REAL overflow rather than a guessed one
    pressOverflow = std::max(
        0.0f,
        ctx.measure(box().width(Dim(kPressWellW)).child(pressList())).height() -
            kPressWellH);

    // --- the instanced chevron array in the footer dock ---
    dockAtlas = std::make_shared<instancing::Atlas>(2.0f);
    const int chev =
        dockAtlas->cell(box()
                            .shape([](SkSize s) {
                              SkPathBuilder b;
                              b.moveTo(0, 0);
                              b.lineTo(s.width() * 0.62f, s.height() * 0.5f);
                              b.lineTo(0, s.height());
                              b.lineTo(s.width() * 0.30f, s.height() * 0.5f);
                              b.close();
                              return b.detach();
                            })
                            .fill(kD6),
                        {12, 10});
    dockPool = std::make_shared<instancing::Pool>();
    instancing::place::grid(*dockPool, size_t{6} * 14, 14, {14, 12}, {0, 0},
                            {2, 4});
    {
      auto frames = dockPool->frames();
      auto tints = dockPool->tints();
      for (size_t i = 0; i < frames.size(); ++i) {
        frames[i] = chev;
        const float k = 0.35f + 0.65f * (float)((i * 7 + 3) % 11) / 10.0f;
        tints[i] = {1, 1, 1, k};
      }
      dockPool->commit();
    }

    // --- the idle motion, all of it driven from this one ticker -----------
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      const float s = (float)t;
      stripePan = s * 2.5f;  // 20 px / 8 s
      wirePhase = -(float)std::fmod(t * 46.0, (double)(wireW + 60.0f));
      portalGlow = 54.0f + 4.4f * std::sin(s * 1.5708f);  // ±8 %, period 4 s
      vuLeft = 0.45f + 0.42f * std::abs(std::sin(s * 3.1f));
      vuRight = 0.40f + 0.45f * std::abs(std::sin(s * 2.3f + 1.1f));

      // the section cycle: shutters, ACCESSING plate, selection mark
      {
        float ph = -1.0f;  // transition phase, <0 outside a change
        int target = 2, from = 2;
        if (t >= kCycleStart) {
          const double u = std::fmod(t - kCycleStart, 7.0 * kHoldS);
          const int stop = (int)(u / kHoldS);
          const double within = u - stop * kHoldS;
          target = cycleTarget(stop);
          from = stop == 0 ? 2 : cycleTarget(stop - 1);
          if (within < kTransS) ph = (float)(within / kTransS);
        }
        for (int i = 0; i < 6; ++i) {
          float cover = 0.0f;
          if (ph >= 0.0f) {
            // close L→R over the first 0.4, reopen R→L over the last 0.4
            const float closeAt = 0.04f * (float)i;
            const float openAt = 0.60f + 0.04f * (float)(5 - i);
            cover = std::clamp((ph - closeAt) / 0.14f, 0.0f, 1.0f) -
                    std::clamp((ph - openAt) / 0.14f, 0.0f, 1.0f);
          }
          shutter[(size_t)i] = cover;
        }
        shutterInfo = ph < 0.0f
                          ? 0.0f
                          : std::clamp((ph - 0.22f) / 0.08f, 0.0f, 1.0f) -
                                std::clamp((ph - 0.70f) / 0.08f, 0.0f, 1.0f);
        // the mark glides during the middle of the change
        const float glide =
            ph < 0.0f ? 1.0f : std::clamp((ph - 0.3f) / 0.4f, 0.0f, 1.0f);
        const float eased = glide * glide * (3.0f - 2.0f * glide);
        navIndX = navMarkX(from) + (navMarkX(target) - navMarkX(from)) * eased;
      }

      // seven tick clusters, each blinking its three dots in sequence,
      // phase-offset per panel so they never lock step
      for (int p = 0; p < 7; ++p) {
        const float off = (float)((p * 137) % 800) / 1000.0f;
        const float cyc = std::fmod(s + off, 2.7f);
        for (int i = 0; i < 3; ++i) {
          const bool on = cyc >= i * 0.4f && cyc < i * 0.4f + 0.32f;
          dot[(size_t)p * 3 + (size_t)i] = on ? 1.0f : 0.22f;
        }
      }

      // three gauges round-robin a radar sweep, 1 s each
      const int active = (int)std::fmod((double)s, 3.0);
      for (int i = 0; i < 3; ++i) {
        if (i == active) gauge[(size_t)i] = (float)std::fmod(s * 200.0, 360.0);
        gaugeAlpha[(size_t)i] = i == active ? 1.0f : 0.22f;
      }

      // PRESS UPDATES auto-scroll: walk the list a 62 px entry at a time,
      // clamped to the overflow measured in setup(), so the last step
      // parks the list bottom-flush instead of walking it off the top and
      // leaving the well empty. A list that fits its well holds still.
      const float span = 3.1f;
      const float overflow = pressOverflow;
      const int steps = (int)(overflow / 62.0f);
      if (steps > 0) {
        const float u =
            std::fmod(std::max(0.0f, s - 4.5f), span * (float)(steps + 1));
        const int idx = std::min((int)(u / span), steps);
        float f = (u - (float)idx * span - 2.5f) / 0.6f;
        f = std::clamp(f, 0.0f, 1.0f);
        f = f < 0.5f ? 2 * f * f : 1 - 2 * (1 - f) * (1 - f);  // easeInOutQuad
        pressScroll = -std::min(overflow, 62.0f * ((float)idx + f));
      } else {
        pressScroll = 0.0f;
      }
      return true;
    });

    ctx.composer.render(describe());
    ctx.composer.renderSlot("bootpct", bootReadout());
    ctx.composer.renderSlot("mfload", mfLoadReadout(mfSection = 2));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // The ACCESSING readout's section name is text content, so it rides
    // the DATA path: re-rendered into its slot only when the cycle's
    // target changes.
    {
      int target = 2;
      if (elapsed >= kCycleStart) {
        const double u = std::fmod(elapsed - kCycleStart, 7.0 * kHoldS);
        target = cycleTarget((int)(u / kHoldS));
      }
      if (target != mfSection) {
        mfSection = target;
        ctx.composer.renderSlot("mfload", mfLoadReadout(target));
      }
    }
    // The boot percentage is TEXT CONTENT, not a paint property, so it
    // cannot be a binding. The slot() keeps the churn local — the rest
    // of the tree is untouched.
    if (booted) return;
    const double u = (elapsed - 0.55) / 0.80;
    const int pct = (int)std::lround(std::clamp(u, 0.0, 1.0) * 100.0);
    if (pct == bootPct) return;
    bootPct = pct;
    if (pct >= 100) booted = true;
    ctx.composer.renderSlot("bootpct", bootReadout());
  }
};

SIGIL_SKETCH(TwoAdvancedV4, "Study \xc2\xb7 Screens",
             "2Advanced Studios v4 \"Prophecy\" (2003\xe2\x80\x93"
             "06) \xe2\x80\x94 chamfered Flash chrome, four deep")

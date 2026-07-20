// flourish_border.cpp — "Aurelia", a living flourish border on a box.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/flourish_border.cpp
//
// A gilt-on-oxblood ornamental frame wrapping a central parchment
// cartouche — and, deliberately, a STRESS TEST that fans across the
// whole SigilCompose surface in one scene. What it exercises, and where:
//
//   layout ............. deep stack/row/column, absolute/inset, zIndex
//   outline() .......... rounded rect, squircle, star, hand-built leaf +
//                        scalloped-cartouche silhouettes (SkPathBuilder)
//   PathFormat ......... three gilt rule weights (solid/dashed/hairline)
//                        + a bead chain (stampPath → SkPath1DPathEffect)
//   ContourWalk ........ a dense acanthus vine as an ELEMENT stamp (baked
//                        once via snapshot(); the leaf carries its OWN
//                        contour-walked veins — recursion level 2) plus a
//                        separate animatedWalk .draw glow that breathes
//   Slice .............. a carved nine-slice frame authored offscreen in
//                        setup() and wrapped via ImageAsset::wrap
//   shapes::onEdges .... crests on the top/bottom edges only
//   layout(scheme) ..... AlongPath stud ring, Radial petal rings, Scatter
//                        spark field
//   connector .......... arc + orthogonal filaments between the medallions
//   flowAround ......... the motto weaves around the breathing seal
//   effects ............ Blur bloom on the title; backdrop Blur behind the
//                        cartouche; an SkSL engraving hatch as a Fill
//   image().region() ... a small atlas frieze (tile-map subsystem)
//   animation .......... Choreograph Output bindings (rotate/scale/opacity
//                        /translate), a timeline draw-on entrance (RampTo),
//                        with() fill transitions, Cache::None live leaves
//   caching ............ Texture-baked static frame, Picture medallions,
//                        Cache::None overlays, Auto everywhere else — the
//                        volatility partition kept honest (every bound node
//                        is a sibling of the bake, never inside it)
//   queries ............ optional bounds()/hitTest()/stats() readout via a
//                        slot() (flip kShowStats)
//
// Headless capture — settle past the entrance first:
//   ComposeSketch <this> --frame aurelia.png --at 2.4

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Layouts.h>
#include <sigilcompose/Routers.h>
#include <sigilcompose/Shapes.h>

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkPerlinNoiseShader.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace sigil::compose;
using namespace sigil::compose::util;
namespace ch = choreograph;
using namespace std::chrono_literals;

namespace {

// ---------------------------------------------------------------------------
// Tuning dials — turn the abuse up or down without touching the scene.

constexpr float kVineSpacing = 19.0f; // px between acanthus stamps
constexpr int kRosettes = 56;         // studs distributed on the inner rule
constexpr int kPetals = 12;           // petals ringing each medallion
constexpr int kSparks = 44;           // scatter spark field in the cartouche
constexpr int kMotes = 130;           // drifting gold dust (Cache::None)
constexpr int kFriezeTiles = 22;      // atlas frieze tiles (image().region())
constexpr bool kShowStats = false;    // gated bounds()/hitTest()/stats() HUD

// ---------------------------------------------------------------------------
// Palette — gilt on oxblood velvet.

constexpr SkColor4f kVelvetCore{0.105f, 0.050f, 0.065f, 1};
constexpr SkColor4f kVelvetEdge{0.028f, 0.018f, 0.028f, 1};
constexpr SkColor4f kGold{0.830f, 0.660f, 0.320f, 1};
constexpr SkColor4f kGoldBright{0.980f, 0.860f, 0.540f, 1};
constexpr SkColor4f kBronze{0.470f, 0.300f, 0.150f, 1};
constexpr SkColor4f kLeaf{0.680f, 0.560f, 0.280f, 1};
constexpr SkColor4f kParchment{0.720f, 0.625f, 0.450f, 1};
constexpr SkColor4f kInk{0.190f, 0.115f, 0.080f, 1};
constexpr SkColor4f kRubric{0.560f, 0.150f, 0.130f, 1};

SkColor toSk(SkColor4f c) { return c.toSkColor(); }

sigil::weave::TextStyle type(float size, SkColor4f color,
                             float tracking = 0.0f) {
  sigil::weave::TextStyle style;
  style.shaping.fontSize = size;
  style.shaping.letterSpacing = tracking;
  style.paint.foreground.setColor(toSk(color));
  style.paint.foreground.setAntiAlias(true);
  return style;
}

// ---------------------------------------------------------------------------
// Calligraphic-stroke primitives (self-contained ports of the ornament kit,
// which is not on the sketch include path — pure public Skia).

void appendCubic(std::vector<SkPoint> &out, SkPoint p0, SkPoint c1, SkPoint c2,
                 SkPoint p1, int steps = 20) {
  for (int i = out.empty() ? 0 : 1; i <= steps; ++i) {
    const float t = (float)i / (float)steps, u = 1 - t;
    out.push_back({u * u * u * p0.x() + 3 * u * u * t * c1.x() +
                       3 * u * t * t * c2.x() + t * t * t * p1.x(),
                   u * u * u * p0.y() + 3 * u * u * t * c1.y() +
                       3 * u * t * t * c2.y() + t * t * t * p1.y()});
  }
}

void appendSpiral(std::vector<SkPoint> &out, SkPoint center, float r0, float r1,
                  float a0, float a1, int steps = 26) {
  for (int i = out.empty() ? 0 : 1; i <= steps; ++i) {
    const float t = (float)i / (float)steps;
    const float r = r0 + (r1 - r0) * t;
    const float a = a0 + (a1 - a0) * t;
    out.push_back({center.x() + std::cos(a) * r, center.y() + std::sin(a) * r});
  }
}

// A filled path tracing the centerline with a width that swells in the
// middle and thins to hairlines at both tips — the calligraphic taper.
SkPath taperedStroke(const std::vector<SkPoint> &pts, float wMax,
                     float wTip = 0.4f) {
  SkPathBuilder b;
  const size_t n = pts.size();
  if (n < 2)
    return b.detach();
  auto normalAt = [&](size_t i) -> SkVector {
    const SkPoint &a = pts[i == 0 ? 0 : i - 1];
    const SkPoint &c = pts[i + 1 < n ? i + 1 : n - 1];
    SkVector d = {c.x() - a.x(), c.y() - a.y()};
    const float len = std::sqrt(d.x() * d.x() + d.y() * d.y());
    if (len < 1e-4f)
      return {0, 0};
    return {-d.y() / len, d.x() / len};
  };
  auto halfWidth = [&](size_t i) {
    const float t = (float)i / (float)(n - 1);
    const float swell = std::pow(std::max(0.0f, std::sin(t * 3.14159265f)),
                                 0.65f); // max() guards the NaN at the tip
    return 0.5f * (wTip + (wMax - wTip) * swell);
  };
  b.moveTo(pts[0].x() + normalAt(0).x() * halfWidth(0),
           pts[0].y() + normalAt(0).y() * halfWidth(0));
  for (size_t i = 1; i < n; ++i)
    b.lineTo(pts[i].x() + normalAt(i).x() * halfWidth(i),
             pts[i].y() + normalAt(i).y() * halfWidth(i));
  for (size_t i = n; i-- > 0;)
    b.lineTo(pts[i].x() - normalAt(i).x() * halfWidth(i),
             pts[i].y() - normalAt(i).y() * halfWidth(i));
  b.close();
  return b.detach();
}

// Emits the leading `fraction` of a point list (draw-on reveal), keeping at
// least two points so the taper never degenerates.
std::vector<SkPoint> revealed(const std::vector<SkPoint> &pts, float fraction) {
  const size_t keep = std::max<size_t>(
      2, (size_t)std::lround((float)pts.size() * std::clamp(fraction, 0.f, 1.f)));
  if (keep >= pts.size())
    return pts;
  return {pts.begin(), pts.begin() + (std::ptrdiff_t)keep};
}

void drawDiamond(SkCanvas &c, SkPoint at, float r, SkColor4f color) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(color, nullptr);
  SkPathBuilder d;
  d.moveTo(at.x(), at.y() - r);
  d.lineTo(at.x() + r, at.y());
  d.lineTo(at.x(), at.y() + r);
  d.lineTo(at.x() - r, at.y());
  d.close();
  c.drawPath(d.detach(), p);
}

// ---------------------------------------------------------------------------
// Outline generators (std::function<SkPath(SkSize)> for Element::outline()).

// A pointed leaf (lens) filling the box — the acanthus stamp silhouette.
shapes::OutlineFn leafOutline() {
  return [](SkSize s) {
    const float w = s.width(), h = s.height();
    SkPathBuilder b;
    b.moveTo(w * 0.06f, h * 0.5f);
    b.quadTo(w * 0.42f, h * 0.04f, w * 0.96f, h * 0.5f);
    b.quadTo(w * 0.42f, h * 0.96f, w * 0.06f, h * 0.5f);
    b.close();
    return b.detach();
  };
}

// A lobed cartouche plaque: rounded lobes bulging from each edge — the
// scalloped nameplate silhouette (hand-built with quadTo).
shapes::OutlineFn scallopOutline(float lobe = 15.0f) {
  return [lobe](SkSize s) {
    SkPathBuilder b;
    const float w = s.width(), h = s.height(), inset = lobe * 0.6f;
    auto edge = [&](SkPoint from, SkPoint to) {
      const float dx = to.x() - from.x(), dy = to.y() - from.y();
      const float len = std::sqrt(dx * dx + dy * dy);
      const int n = std::max(1, (int)(len / lobe));
      const float nx = dy / len, ny = -dx / len;
      for (int i = 0; i < n; ++i) {
        const float t0 = (float)i / n, t1 = (float)(i + 1) / n;
        const SkPoint mid = {
            from.x() + dx * (t0 + t1) * 0.5f + nx * lobe * 0.55f,
            from.y() + dy * (t0 + t1) * 0.5f + ny * lobe * 0.55f};
        b.quadTo(mid, {from.x() + dx * t1, from.y() + dy * t1});
      }
    };
    b.moveTo(inset, inset);
    edge({inset, inset}, {w - inset, inset});
    edge({w - inset, inset}, {w - inset, h - inset});
    edge({w - inset, h - inset}, {inset, h - inset});
    edge({inset, h - inset}, {inset, inset});
    b.close();
    return b.detach();
  };
}

// A rounded-rect path generator for AlongPath (the inner stud ring).
std::function<SkPath(SkSize)> roundRectPath(float insetPx, float radius) {
  return [insetPx, radius](SkSize s) {
    return SkPath::RRect(
        SkRRect::MakeRectXY(SkRect::MakeLTRB(insetPx, insetPx,
                                             s.width() - insetPx,
                                             s.height() - insetPx),
                            radius, radius));
  };
}

// ---------------------------------------------------------------------------
// Shaders / effects (built once, guarded).

// Fractal-noise parchment: the base tint modulated by soft-light noise. Skia
// fractal noise is COLORED, so we mute it toward gray (blend over a mid tone)
// before the soft-light pass, keeping the grain without rainbow speckle.
Fill parchmentFill(SkColor4f base, float freq = 0.04f) {
  sk_sp<SkShader> noise = SkShaders::MakeFractalNoise(freq, freq, 3, 7.0f);
  if (!noise)
    return Fill::color(base);
  sk_sp<SkShader> muted = SkShaders::Blend(
      SkBlendMode::kLuminosity, SkShaders::Color(SkColorSetARGB(255, 128, 128, 128)),
      std::move(noise));
  return Fill::shader(SkShaders::Blend(SkBlendMode::kSoftLight,
                                       SkShaders::Color(toSk(base)),
                                       std::move(muted)));
}

// A faint diagonal engraving hatch — SkSL, no uniforms (bakes into caches).
// Faint dark lines over transparent: a foreground/background decoration.
sk_sp<SkRuntimeEffect> makeHatch() {
  auto result = SkRuntimeEffect::MakeForShader(SkString(R"(
      half4 main(float2 p) {
        float d = mod(p.x * 0.7 + p.y, 7.0);
        float line = smoothstep(2.4, 3.1, d) - smoothstep(3.1, 3.8, d);
        return half4(0.0, 0.0, 0.0, 0.20 * line);
      })"));
  return result.effect;
}

// Opaque engraved metal — SkSL used directly as a Fill::shader (the disc
// face of each medallion). No uniforms, so it bakes into the picture cache.
sk_sp<SkRuntimeEffect> makeEngraved() {
  auto result = SkRuntimeEffect::MakeForShader(SkString(R"(
      half4 main(float2 p) {
        float d = mod(p.x * 0.6 - p.y * 0.4, 6.0);
        float line = smoothstep(2.2, 3.0, d) - smoothstep(3.0, 3.8, d);
        float r = length(p - float2(64.0, 64.0)) / 90.0;
        half3 base = mix(half3(0.46, 0.31, 0.15), half3(0.24, 0.15, 0.10),
                         clamp(r, 0.0, 1.0));
        half3 gild = half3(0.92, 0.74, 0.38);
        return half4(mix(base, gild, line * 0.6), 1.0);
      })"));
  return result.effect;
}

// ---------------------------------------------------------------------------
// Offscreen textures (authored once in setup(); the Slice/region subsystems).

// A carved gilt frame drawn at `size`px with a TRANSPARENT center — the
// nine-slice source stretched around the central box.
sk_sp<SkImage> makeCarvedFrame(int size = 192) {
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size, size));
  SkCanvas &c = *surface->getCanvas();
  c.clear(SK_ColorTRANSPARENT);
  const float s = (float)size;
  SkPaint p;
  p.setAntiAlias(true);

  // Bronze band (thick), gilded trims inside and out.
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(s * 0.075f);
  p.setColor4f(kBronze, nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.06f, s * 0.06f, s * 0.94f, s * 0.94f),
                  s * 0.12f, s * 0.12f, p);
  p.setStrokeWidth(1.8f);
  p.setColor4f(kGold, nullptr);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.115f, s * 0.115f, s * 0.885f,
                                   s * 0.885f),
                  s * 0.09f, s * 0.09f, p);
  c.drawRoundRect(SkRect::MakeLTRB(s * 0.02f, s * 0.02f, s * 0.98f, s * 0.98f),
                  s * 0.14f, s * 0.14f, p);

  // Corner bosses with gilt studs (in the fixed nine-slice corners).
  p.setStyle(SkPaint::kFill_Style);
  const float at[4][2] = {{0.11f, 0.11f},
                          {0.89f, 0.11f},
                          {0.89f, 0.89f},
                          {0.11f, 0.89f}};
  for (auto &corner : at) {
    SkColor4f dark = kBronze;
    dark.fR *= 0.6f;
    dark.fG *= 0.6f;
    dark.fB *= 0.6f;
    p.setColor4f(dark, nullptr);
    c.drawCircle(s * corner[0], s * corner[1], s * 0.07f, p);
    drawDiamond(c, {s * corner[0], s * corner[1]}, s * 0.036f, kGoldBright);
  }
  return surface->makeImageSnapshot();
}

// A 4-cell gem atlas (16px tiles) for the frieze — image().region() source.
sk_sp<SkImage> makeGemAtlas() {
  sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(64, 16));
  SkCanvas &c = *s->getCanvas();
  c.clear(SK_ColorTRANSPARENT);
  const SkColor4f gems[4] = {kRubric,
                             {0.20f, 0.36f, 0.52f, 1},
                             kLeaf,
                             kGoldBright};
  SkPaint p;
  p.setAntiAlias(true);
  for (int i = 0; i < 4; ++i) {
    const float ox = (float)i * 16;
    SkColor4f g = gems[i];
    // faceted diamond gem
    drawDiamond(c, {ox + 8, 8}, 6.0f, g);
    SkColor4f hi = g;
    hi.fR = std::min(1.f, hi.fR + 0.3f);
    hi.fG = std::min(1.f, hi.fG + 0.3f);
    hi.fB = std::min(1.f, hi.fB + 0.3f);
    drawDiamond(c, {ox + 7, 6}, 2.2f, hi);
    p.setStyle(SkPaint::kStroke_Style);
    p.setStrokeWidth(0.8f);
    p.setColor4f(kGold, nullptr);
    SkPathBuilder d;
    d.moveTo(ox + 8, 2);
    d.lineTo(ox + 14, 8);
    d.lineTo(ox + 8, 14);
    d.lineTo(ox + 2, 8);
    d.close();
    c.drawPath(d.detach(), p);
    p.setStyle(SkPaint::kFill_Style);
  }
  return s->makeImageSnapshot();
}

std::shared_ptr<sigil::image::ImageAsset> wrapImage(sk_sp<SkImage> img) {
  return std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(std::move(img)));
}

} // namespace

// ===========================================================================

struct FlourishBorder : sigil::compose::sketch::Sketch {
  // Canvas.
  static constexpr float kW = 1440, kH = 1024;
  static constexpr float kFrameInset = 58; // frame band offset from the edge

  // Motion channels (all paint-only; declared volatility).
  ch::Output<float> reveal{0.0f};    // scrollwork draw-on 0→1
  ch::Output<float> titleDrop{-26};  // title enters from above
  ch::Output<float> titleFade{0.0f}; // title/seal opacity
  ch::Output<float> sealBreathe{1};  // seal scale
  ch::Output<float> spin[4];         // medallion rotations
  ch::Output<float> breathe[4];      // medallion scales (phase-offset)
  ch::Output<float> flare{0.0f};     // periodic medallion glow flash

  // Built once in setup().
  sk_sp<SkRuntimeEffect> hatch, engraved;
  std::shared_ptr<sigil::image::ImageAsset> carvedFrame, gemAtlas;
  bool accent = false; // toggles a with()-transitioned rubric tint

  // ---- ornament components ------------------------------------------------

  // The acanthus leaf stamp: an element with its own contour-walked veins
  // (recursion level 2 — the stamp's decoration walks the stamp's outline).
  Element acanthusLeaf() const {
    ContourWalk veins; // recursion level 2: the stamp walks its own contour
    veins.spacing = 4.0f;
    veins.draw = [](SkCanvas &c, const PathSample &s, const PaintContext &) {
      SkPaint p;
      p.setAntiAlias(true);
      p.setColor4f(kGoldBright, nullptr);
      c.drawCircle(0, 0, 0.8f, p); // a fine gilt bead along the leaf edge
      (void)s;
    };
    // A gilt midrib down the leaf (a foreground paint program).
    Decoration midrib{PaintProgram([](SkCanvas &c, const PaintContext &ctx) {
      SkPaint p;
      p.setAntiAlias(true);
      p.setStyle(SkPaint::kStroke_Style);
      p.setStrokeWidth(1.2f);
      p.setColor4f(kGoldBright, nullptr);
      c.drawLine(ctx.size.width() * 0.1f, ctx.size.height() * 0.5f,
                 ctx.size.width() * 0.92f, ctx.size.height() * 0.5f, p);
    })};
    return box()
        .width(34)
        .height(26)
        .outline(leafOutline())
        .fill(linearGradient({0, 0}, {34, 26}, {kLeaf, kBronze}))
        .foreground(stroke(1.2f, Fill::color(kGoldBright)))
        .foreground(midrib)
        .foreground(veins);
  }

  // A small crest for the top/bottom edges (onEdges target).
  Element crest() const {
    return box()
        .width(20)
        .height(14)
        .outline(shapes::star(3, 0.5f))
        .fill(Fill::color(kGold));
  }

  // A gilt petal ringing a medallion (Radial layout child).
  Element petal() const {
    return box()
        .width(16)
        .height(22)
        .outline(shapes::rounded(shapes::star(4, 0.36f), 2.0f))
        .fill(Fill::color(kGoldBright))
        .foreground(stroke(0.8f, Fill::color(kBronze)));
  }

  // A six-point stud distributed along the inner rule (AlongPath child).
  Element rosette() const {
    return box()
        .width(13)
        .height(13)
        .outline(shapes::star(6, 0.5f))
        .fill(Fill::color(kGold));
  }

  // ---- the static baked frame band ---------------------------------------

  Element frameBand() const {
    // Rules at three inset radii → a genuine line-weight hierarchy.
    PathFormat dashed;
    dashed.width = 1.3f;
    dashed.strokeFill = Fill::color(kGold);
    dashed.dashIntervals = {15, 8};

    SkPathBuilder beadPath; // a small diamond bead, stamped along the contour
    beadPath.moveTo(0, -2.6f);
    beadPath.lineTo(2.6f, 0);
    beadPath.lineTo(0, 2.6f);
    beadPath.lineTo(-2.6f, 0);
    beadPath.close();
    PathFormat beadChain;
    beadChain.width = 1.0f;
    beadChain.strokeFill = Fill::color(kGoldBright);
    beadChain.stampPath = beadPath.detach();
    beadChain.stampAdvance = 16.0f;

    // The dense acanthus vine — an ELEMENT stamp, baked once (static).
    ContourWalk vine;
    vine.spacing = kVineSpacing;
    vine.stamp = acanthusLeaf();

    // Crests on top and bottom edges only (per-edge extraction).
    ContourWalk crestWalk;
    crestWalk.spacing = 96.0f;
    crestWalk.stamp = crest();

    // Studs distributed by arc length along an inner rounded rectangle.
    std::vector<Element> studs;
    studs.reserve(kRosettes);
    for (int i = 0; i < kRosettes; ++i)
      studs.push_back(rosette());

    // Layer within the band: outer solid rule + vine + crests, then a nested
    // inset carrying the dashed rule + bead chain, then the stud ring.
    return box()
        .absolute()
        .inset(kFrameInset)
        .corners({30})
        .background(shadow({0, 0, 0, 0.55f}, {0, 6}, 20))
        .foreground(stroke(3.0f, Fill::color(kGold)))          // bold rule
        .foreground(vine)                                      // acanthus run
        .foreground(shapes::onEdges(shapes::Edge::Top | shapes::Edge::Bottom,
                                    Decoration(crestWalk)))    // edge crests
        .cache(Cache::Texture)                                 // bake once
        .child(box()
                   .absolute()
                   .inset(18)
                   .corners({22})
                   .foreground(beadChain)
                   .foreground(dashed))
        .child(box()
                   .absolute()
                   .inset(30)
                   .foreground(stroke(0.8f, Fill::color(kBronze)))) // hairline
        .child(layout(layouts::AlongPath{roundRectPath(30, 22)})
                   .absolute()
                   .inset(0)
                   .children(std::move(studs)));
  }

  // A live breathing glow riding the frame outline — animatedWalk .draw only
  // (no stamp, so nothing re-bakes; sits OUTSIDE the Texture boundary).
  Element frameGlow() const {
    ContourWalk glow;
    glow.spacing = 30.0f;
    glow.animatedWalk = true;
    glow.draw = [](SkCanvas &c, const PathSample &s, const PaintContext &ctx) {
      SkPaint p;
      p.setAntiAlias(true);
      const float w =
          0.5f + 0.5f * std::sin(s.fraction * 18.85f +
                                 (float)ctx.elapsedSeconds * 2.4f);
      p.setColor4f({kGoldBright.fR, kGoldBright.fG, kGoldBright.fB, 0.16f + 0.5f * w},
                   nullptr);
      c.drawCircle(0, 0, 1.2f + 1.8f * w, p);
    };
    return box()
        .absolute()
        .inset(kFrameInset)
        .corners({30})
        .foreground(glow)
        .blend(SkBlendMode::kPlus)
        .cache(Cache::None);
  }

  // ---- corner medallions (spinning, phase-offset breathing) --------------

  struct MedProps {
    int q;
    bool accent;
    bool operator==(const MedProps &) const = default;
  };

  Element medallion(MedProps mp) const {
    const int q = mp.q;
    const float cx = (q == 1 || q == 2) ? kW - kFrameInset : kFrameInset;
    const float cy = (q >= 2) ? kH - kFrameInset : kFrameInset;
    constexpr float kD = 128;

    std::vector<Element> petals;
    petals.reserve(kPetals);
    for (int i = 0; i < kPetals; ++i)
      petals.push_back(petal());

    // SkSL-engraved disc fill (guarded), rune ring, gilt rims.
    Fill discFill =
        engraved ? Fill::shader(SkRuntimeShaderBuilder(engraved).makeShader())
                 : Fill::color({0.14f, 0.07f, 0.05f, 1});

    return box()
        .key("med" + std::to_string(q))
        .absolute()
        .inset(cx - kD / 2, cy - kD / 2, kW - (cx + kD / 2), kH - (cy + kD / 2))
        .width(kD)
        .height(kD)
        .transformOrigin(0.5f, 0.5f)
        .rotate(&spin[q])
        .scale(&breathe[q])
        .cache(Cache::Picture)
        .child(box()
                   .absolute()
                   .inset(20)
                   .outline(shapes::squircle(4.0f))
                   .fill(discFill)
                   .foreground(stroke(2.4f, Fill::color(kGold)))
                   .foreground(stroke(0.8f, Fill::color(kGoldBright))))
        // real placed petals ringing the disc
        .child(layout(layouts::Radial{0.82f})
                   .absolute()
                   .inset(0)
                   .children(std::move(petals)))
        // a central boss that flares periodically (opacity binding)
        .child(box()
                   .absolute()
                   .inset(kD / 2 - 12, kD / 2 - 12, kD / 2 - 12, kD / 2 - 12)
                   .outline(shapes::star(8, 0.5f))
                   .fill(Fill::color(mp.accent ? kGoldBright : kGold))
                   .opacity(&flare));
  }

  // ---- filaments between medallions --------------------------------------

  Element filaments() const {
    PathFormat gild;
    gild.width = 1.1f;
    gild.strokeFill = Fill::color({kGold.fR, kGold.fG, kGold.fB, 0.7f});

    SkPathBuilder dot;
    dot.addCircle(0, 0, 1.4f);
    PathFormat beaded;
    beaded.width = 1.0f;
    beaded.strokeFill = Fill::color(kGoldBright);
    beaded.stampPath = dot.detach();
    beaded.stampAdvance = 12.0f;

    return stack()
        .absolute()
        .inset(0)
        .zIndex(2)
        // edge swags (arc router) — a gentle garland between corners
        .child(connector("med0", "med1", routers::arc(0.05f))
                   .absolute()
                   .inset(0)
                   .foreground(gild))
        .child(connector("med1", "med2", routers::arc(0.05f))
                   .absolute()
                   .inset(0)
                   .foreground(gild))
        .child(connector("med2", "med3", routers::arc(0.05f))
                   .absolute()
                   .inset(0)
                   .foreground(gild))
        .child(connector("med3", "med0", routers::arc(0.05f))
                   .absolute()
                   .inset(0)
                   .foreground(gild))
        // diagonals (orthogonal router), beaded
        .child(connector("med0", "med2", routers::orthogonal(24.0f))
                   .absolute()
                   .inset(0)
                   .foreground(beaded))
        .child(connector("med1", "med3", routers::orthogonal(24.0f))
                   .absolute()
                   .inset(0)
                   .foreground(beaded));
  }

  // ---- the central cartouche (the box being framed) ----------------------

  Element cartouche() const {
    Slice carved;
    carved.asset = carvedFrame;
    const int cs = carvedFrame ? carvedFrame->width() : 192;
    carved.xDivs = {cs / 3, cs * 2 / 3};
    carved.yDivs = {cs / 3, cs * 2 / 3};

    // engraving hatch as a background decoration above the parchment fill
    Decoration hatchDeco{PaintProgram{}};
    if (hatch) {
      auto fx = hatch;
      hatchDeco = Decoration(PaintProgram(
          [fx](SkCanvas &c, const PaintContext &ctx) {
            SkPaint p;
            p.setShader(SkRuntimeShaderBuilder(fx).makeShader());
            p.setAlphaf(0.6f);
            c.save();
            c.clipPath(ctx.outline, true);
            c.drawRect(SkRect::MakeSize(ctx.size), p);
            c.restore();
          }));
    }

    PathFormat innerTrim;
    innerTrim.width = 1.2f;
    innerTrim.strokeFill = Fill::color(kBronze);
    innerTrim.dashIntervals = {9, 5};

    // A scatter of faint sparks inside the field (Scatter layout).
    std::vector<Element> sparks;
    sparks.reserve(kSparks);
    for (int i = 0; i < kSparks; ++i)
      sparks.push_back(box()
                           .width(3)
                           .height(3)
                           .outline(shapes::star(4, 0.4f))
                           .fill(Fill::color({kBronze.fR, kBronze.fG, kBronze.fB,
                                              0.5f}))
                           .opacity(0.5f));

    // A frieze of atlas gems (image().region() tile-map).
    std::vector<Element> frieze;
    frieze.reserve(kFriezeTiles);
    for (int i = 0; i < kFriezeTiles; ++i)
      frieze.push_back(image(gemAtlas)
                           .region(SkRect::MakeXYWH((float)(i % 4) * 16, 0, 16, 16))
                           .width(18)
                           .height(18));

    // Title, gilded: a Blur+kPlus gold bloom behind a sharp dark-bronze face
    // (dark-on-parchment reads; the bloom supplies the gilt halo). Each copy
    // is centered by its own inset-0 column layer so the two overlap exactly
    // (a bare stack can't center absolute-by-default children).
    auto titleLayer = [this](SkColor4f color, bool bloom) {
      auto t = text(u8"AURELIA", type(48, color, 6.0f))
                   .key(bloom ? "titleBloom" : "title")
                   .opacity(&titleFade);
      if (bloom)
        t.effect(Effect::filter(SkImageFilters::Blur(7, 7, nullptr)))
            .blend(SkBlendMode::kPlus);
      else
        t.translateY(&titleDrop);
      return box()
          .absolute()
          .inset(0)
          .column()
          .alignItems(Align::Center)
          .justify(Justify::Center)
          .child(std::move(t));
    };

    return box()
        .key("cartouche")
        .absolute()
        .inset(392, 316, 392, 316) // centered box, ~656×392
        .corners({20})
        .zIndex(3)
        .clip()
        .backdrop(Effect::filter(SkImageFilters::Blur(10, 10, nullptr)))
        .background(shadow({0, 0, 0, 0.5f}, {0, 8}, 22))
        .fill(parchmentFill(kParchment))
        .background(hatchDeco)
        .background(carved)
        .column()
        .padding(52, 44)
        .gap(14)
        .alignItems(Align::Center)
        // scatter sparks behind the content
        .child(layout(layouts::Scatter{7, 0.7f})
                   .absolute()
                   .inset(30)
                   .children(std::move(sparks)))
        // a scalloped nameplate holding the bloomed title (stack overlaps the
        // two centered title layers)
        .child(stack()
                   .width(372)
                   .height(104)
                   .outline(scallopOutline(16))
                   .fill(Fill::color({kParchment.fR * 1.05f, kParchment.fG * 1.05f,
                                      kParchment.fB * 1.02f, 1}))
                   .foreground(stroke(1.4f, Fill::color(kGold)))
                   .child(titleLayer(kGoldBright, true))
                   .child(titleLayer({0.34f, 0.20f, 0.09f, 1}, false)))
        // the breathing seal + the motto that flows around it
        .child(box()
                   .key("seal")
                   .width(70)
                   .height(70)
                   .transformOrigin(0.5f, 0.5f)
                   .scale(&sealBreathe)
                   .outline(shapes::star(12, 0.66f))
                   .fill(with(Fill::color(accent ? kRubric : kBronze), {600ms}))
                   .foreground(stroke(1.6f, Fill::color(kGoldBright))))
        .child(text(u8"Bound in gilt and oxblood, this cartouche is framed by "
                    u8"a vine that draws itself on, corner by corner, while the "
                    u8"medallions turn and the rules hold their three weights "
                    u8"of gold — every ornament a different corner of the "
                    u8"compose surface, woven around this seal.",
                    type(15.5f, kInk))
                   .key("motto")
                   .flowAround("seal", 12))
        // the atlas-gem frieze
        .child(box()
                   .row()
                   .gap(2)
                   .justify(Justify::Center)
                   .children(std::move(frieze)))
        .child(text(u8"— a stress test that chose to be beautiful —",
                    type(13, kRubric)));
  }

  // ---- the eight draw-on scrollwork sweeps (Cache::None, read reveal) -----

  Element scrollworkCorner(int q) const {
    return custom([this, q](SkCanvas &c, const PaintContext &ctx) {
             // Read the entrance progress LIVE every frame (this leaf is
             // Cache::None); per-corner stagger over [q*0.16, q*0.16+0.55].
             const float rev = reveal.value();
             const float local =
                 std::clamp((rev - (float)q * 0.16f) / 0.55f, 0.0f, 1.0f);
             const float w = ctx.size.width(), h = ctx.size.height();
             const bool right = (q == 1 || q == 2);
             const bool bottom = (q >= 2);
             c.save();
             if (right) {
               c.translate(w, 0);
               c.scale(-1, 1);
             }
             if (bottom) {
               c.translate(0, h);
               c.scale(1, -1);
             }
             // Two mirrored calligraphic sweeps flowing out of the corner
             // along the two adjacent edges, rolling into spiral eyes — a
             // corner-scale flourish, not a canvas-spanning line.
             const float armLen = 300.0f;
             const float x0 = kFrameInset + 12, y0 = kFrameInset + 12;

             auto sweep = [&](bool along) {
               std::vector<SkPoint> pts;
               appendCubic(pts, {x0, y0}, {x0 + armLen * 0.16f, y0 - 30},
                           {x0 + armLen * 0.44f, y0 - 10},
                           {x0 + armLen * 0.62f, y0 + 8});
               const SkPoint eye{x0 + armLen * 0.72f, y0 + 22};
               appendSpiral(pts, eye, 17.0f, 1.5f, -1.7f, -1.7f + 7.6f, 36);
               SkMatrix m;
               if (along) // rotate the arm 90° about the corner for the edge
                 m.setRotate(90, x0, y0);
               else
                 m.setIdentity();
               for (auto &p : pts)
                 p = m.mapPoint(p);
               SkPaint gp;
               gp.setAntiAlias(true);
               gp.setColor4f(kGold, nullptr);
               c.drawPath(taperedStroke(revealed(pts, local), 7.0f), gp);
               // an under-curl answering the sweep, thinner
               std::vector<SkPoint> under;
               appendCubic(under, {x0 + 6, y0 + 26}, {x0 + armLen * 0.16f, y0 + 40},
                           {x0 + armLen * 0.30f, y0 + 34}, {x0 + armLen * 0.40f, y0 + 30});
               appendSpiral(under, {x0 + armLen * 0.47f, y0 + 24}, 11.0f, 1.2f,
                            2.4f, 2.4f - 6.6f, 30);
               for (auto &p : under)
                 p = m.mapPoint(p);
               c.drawPath(taperedStroke(revealed(under, local), 3.4f), gp);
               // a gilt berry seated in the spiral eye
               if (local > 0.85f) {
                 const SkPoint e = m.mapPoint(eye);
                 SkPaint bp;
                 bp.setAntiAlias(true);
                 bp.setColor4f(kGoldBright, nullptr);
                 c.drawCircle(e.x(), e.y(), 3.4f, bp);
               }
             };
             sweep(false);
             sweep(true);
             // a gilt diamond seated on the corner once revealed
             if (local > 0.98f)
               drawDiamond(c, {x0, y0}, 6.0f, kGoldBright);
             c.restore();
           })
        .absolute()
        .inset(0)
        .zIndex(4)
        .cache(Cache::None);
  }

  // ---- live overlays: shimmer sweep + gold dust --------------------------

  Element goldDust() const {
    return custom([](SkCanvas &c, const PaintContext &ctx) {
             SkPaint p;
             p.setAntiAlias(true);
             const double t = ctx.elapsedSeconds;
             for (int i = 0; i < kMotes; ++i) {
               const float fx = (float)i * 137.5f;
               const float x = std::fmod(fx + (float)t * (7.0f + (float)(i % 5)),
                                         ctx.size.width());
               const float y = std::fmod(fx * 0.618f + 40.0f, ctx.size.height());
               const float a =
                   0.05f + 0.16f * (0.5f + 0.5f * std::sin((float)t * 1.6f +
                                                           (float)i * 2.1f));
               p.setColor4f({kGoldBright.fR, kGoldBright.fG, kGoldBright.fB, a},
                            nullptr);
               c.drawCircle(x, y + 12.0f * std::sin((float)t * 0.7f + (float)i),
                            1.3f + (float)(i % 3) * 0.6f, p);
             }
           })
        .absolute()
        .inset(0)
        .zIndex(6)
        .cache(Cache::None)
        .blend(SkBlendMode::kPlus);
  }

  Element shimmer() const {
    return custom([](SkCanvas &c, const PaintContext &ctx) {
             const float w = ctx.size.width(), h = ctx.size.height();
             const float t = (float)ctx.elapsedSeconds;
             // a diagonal band of light sweeping across the frame
             const float sweep = std::fmod(t * 220.0f, w + h + 400.0f) - 200.0f;
             SkPoint pts[2] = {{sweep, 0}, {sweep + 160, h}};
             const SkColor4f cols[3] = {{1, 1, 1, 0},
                                        {kGoldBright.fR, kGoldBright.fG,
                                         kGoldBright.fB, 0.25f},
                                        {1, 1, 1, 0}};
             const float stops[3] = {0.0f, 0.5f, 1.0f};
             SkPaint p;
             p.setShader(SkShaders::LinearGradient(
                 pts, SkGradient({{cols, 3}, {stops, 3}, SkTileMode::kClamp},
                                 {})));
             c.drawRect(SkRect::MakeWH(w, h), p);
           })
        .absolute()
        .inset(kFrameInset)
        .zIndex(5)
        .cache(Cache::None)
        .blend(SkBlendMode::kPlus);
  }

  // ---- assembly -----------------------------------------------------------

  Element describe() const {
    auto root =
        stack()
            .fill(radialGradient({kW / 2, kH / 2}, 900,
                                 {kVelvetCore, kVelvetEdge}))
            .child(frameBand())
            .child(frameGlow())
            .child(filaments())
            .child(memo(MedProps{0, accent}, [this](const MedProps &p) {
                     return medallion(p);
                   }).key("med0"))
            .child(memo(MedProps{1, accent}, [this](const MedProps &p) {
                     return medallion(p);
                   }).key("med1"))
            .child(memo(MedProps{2, accent}, [this](const MedProps &p) {
                     return medallion(p);
                   }).key("med2"))
            .child(memo(MedProps{3, accent}, [this](const MedProps &p) {
                     return medallion(p);
                   }).key("med3"))
            .child(cartouche())
            .child(scrollworkCorner(0))
            .child(scrollworkCorner(1))
            .child(scrollworkCorner(2))
            .child(scrollworkCorner(3))
            .child(shimmer())
            .child(goldDust());
    if (kShowStats)
      root.child(slot("stats"));
    return root;
  }

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(kW, kH);
    ctx.background(kVelvetEdge);

    hatch = makeHatch();
    engraved = makeEngraved();
    carvedFrame = wrapImage(makeCarvedFrame(192));
    gemAtlas = wrapImage(makeGemAtlas());

    // Reset motion channels for a clean (re)load entrance.
    reveal = 0.0f;
    titleDrop = -26.0f;
    titleFade = 0.0f;
    flare = 0.0f;
    for (int q = 0; q < 4; ++q) {
      spin[q] = 0.0f;
      breathe[q] = 1.0f;
    }

    // Entrance: the scrollwork draws itself on, the title drops and fades in.
    ctx.ticker.timeline().apply(&reveal).then<ch::RampTo>(
        1.0f, 2.6f, &ch::easeOutQuint);
    ctx.ticker.timeline().apply(&titleDrop).then<ch::RampTo>(0.0f, 1.0f,
                                                             &ch::easeOutQuint);
    ctx.ticker.timeline().apply(&titleFade).then<ch::RampTo>(1.0f, 1.2f);
    ctx.ticker.timeline().apply(&flare).then<ch::RampTo>(1.0f, 1.4f);

    // Continuous motion: medallions turn (mirrored pairs counter-rotate) and
    // breathe with a per-corner phase offset; the seal breathes slowly.
    ctx.ticker.add([this, t = 0.0](double dt) mutable {
      t += dt;
      for (int q = 0; q < 4; ++q) {
        const float dir = (q == 0 || q == 2) ? 1.0f : -1.0f;
        spin[q] = (float)(t * 7.0) * dir;
        breathe[q] =
            1.0f + 0.05f * (float)std::sin(t * 1.3 + q * 1.5707963);
      }
      sealBreathe = 1.0f + 0.06f * (float)std::sin(t * 1.1);
      return true;
    });

    ctx.composer.render(describe());
  }

  double nextAccent = 4.0;
  double nextStats = 0.0;

  void update(double elapsed, sketch::SketchContext &ctx) override {
    bool dirty = false;
    // Periodic rubric flare: retarget the medallion boss opacity and toggle
    // the seal tint (a with() transition lerps the fill on re-render).
    if (elapsed >= nextAccent) {
      nextAccent = elapsed + 4.0;
      accent = !accent;
      flare = 1.0f;
      ctx.ticker.timeline().apply(&flare).then<ch::RampTo>(0.55f, 1.1f,
                                                           &ch::easeOutQuint);
      dirty = true;
    }
    if (dirty)
      ctx.composer.render(describe());

    // Gated HUD: bounds()/hitTest()/stats() into an isolated slot at ~2 Hz.
    if (kShowStats && elapsed >= nextStats) {
      nextStats = elapsed + 0.5;
      const auto &st = ctx.composer.stats();
      auto b = ctx.composer.bounds("cartouche");
      auto hit = ctx.composer.hitTest({kW / 2, kH / 2});
      std::string line = "instances " + std::to_string(st.instances) +
                         "  painted " + std::to_string(st.nodesPainted) +
                         "  pics " + std::to_string(st.picturesLive) +
                         "  memoHits " + std::to_string(st.memoHits);
      if (b)
        line += "  cartW " + std::to_string((int)b->width());
      if (hit)
        line += "  hit@center " + *hit;
      ctx.composer.renderSlot(
          "stats", box().absolute().inset(20, 12, 20, kH - 40)
                       .child(text(std::u8string(line.begin(), line.end()),
                                   type(13, kGoldBright))));
    }
  }
};

SIGIL_SKETCH(FlourishBorder)

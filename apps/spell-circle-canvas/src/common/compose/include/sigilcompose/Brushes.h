#pragma once

/** @file
 * SigilCompose brushes — the LINE vocabulary between elements, grounded in
 * real game linework (REFERENCES.md §5): a brush is a LAYERED STROKE STACK
 * (widths, colors, blurs, dashes, blends, bottom-up) applied to the node's
 * outline — a rail's route, a connector's wire, a border. Every brush is a
 * comparable VALUE (defaulted equality → prunes), attached with `.stroke()`.
 *
 * The stock set transcribes measured grammars:
 *  - filament(): Ori's 4-layer additive glow (envelope 4–6× core — THE
 *    organic-glow signature); state via the whole stack's opacity.
 *  - circuit(): FUI trace tiers (1/2/4px data/main/power + under-glow).
 *  - rope(): Path of Exile's 3-state rope (counter-dashed strand layers;
 *    verified palette ladder Normal→Intermediate→Active).
 *
 * The width law from the research applies: state changes shift COLOR
 * dramatically but width by ≤1.3× — hierarchy encodes importance, state
 * encodes progress.
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/Decorations.h" // PathSample
#include "sigilcompose/Lines.h"       // lines::displace (the wave op)
#include "sigilcompose/Shapes.h"      // detail::hashNoise (seeded jitter)

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkMaskFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkPicture.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/effects/SkDiscretePathEffect.h>
#include <include/effects/SkDashPathEffect.h>

#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace sigil::compose {

/** One stroke pass of a layered brush. */
struct StrokeLayer {
  float width = 2.0f;
  SkColor4f color = {1, 1, 1, 1};
  float blurSigma = 0;                // soft halo layers
  std::vector<SkScalar> dash;         // empty → solid
  float dashPhase = 0;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool roundCap = true;

  bool operator==(const StrokeLayer &) const = default;
};

/** The layered stroke stack — painted bottom-up along the outline. */
struct LayeredBrush {
  std::vector<StrokeLayer> layers;

  bool operator==(const LayeredBrush &) const = default;

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    for (const StrokeLayer &layer : layers) {
      SkPaint p;
      p.setAntiAlias(true);
      p.setStyle(SkPaint::kStroke_Style);
      p.setStrokeWidth(layer.width);
      p.setStrokeCap(layer.roundCap ? SkPaint::kRound_Cap
                                    : SkPaint::kButt_Cap);
      p.setColor4f(layer.color, nullptr);
      p.setBlendMode(layer.blend);
      if (layer.blurSigma > 0)
        p.setMaskFilter(
            SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, layer.blurSigma));
      if (!layer.dash.empty())
        p.setPathEffect(SkDashPathEffect::Make(
            SkSpan(layer.dash.data(), layer.dash.size()), layer.dashPhase));
      c.drawPath(ctx.outline, p);
    }
  }
};

namespace brushes {

/** Ori-style organic filament (REFERENCES.md §5): four strokes bottom-up —
 *  wide additive glow, mid glow, bright core, white center. Scale sets the
 *  envelope (1.0 → 14px envelope over a 2.5px core). */
inline LayeredBrush filament(SkColor4f glow = {0.435f, 0.847f, 1.0f, 1},
                             SkColor4f core = {0.875f, 0.965f, 1.0f, 1},
                             float scale = 1.0f) {
  SkColor4f g18 = glow, g45 = glow, c90 = core;
  g18.fA = 0.18f;
  g45.fA = 0.45f;
  c90.fA = 0.90f;
  return LayeredBrush{{
      {14 * scale, g18, 8 * scale, {}, 0, SkBlendMode::kPlus},
      {7 * scale, g45, 3 * scale, {}, 0, SkBlendMode::kPlus},
      {2.5f * scale, c90},
      {1 * scale, {1, 1, 1, 0.7f}},
  }};
}

/** FUI circuit trace (REFERENCES.md §5). Tiers: 0 = data (1px, 55%),
 *  1 = main (2px, 85%), 2 = power (4px + 8px under-glow). Pair with an
 *  octilinear-ish router with 45° chamfers for the full look. */
inline LayeredBrush circuit(SkColor4f color = {0.208f, 0.878f, 0.824f, 1},
                            int tier = 1) {
  SkColor4f c = color;
  LayeredBrush b;
  if (tier >= 2) {
    SkColor4f under = color;
    under.fA = 0.15f;
    b.layers.push_back({8, under, 4});
    c.fA = 1.0f;
    b.layers.push_back({4, c, 0, {}, 0, SkBlendMode::kSrcOver, false});
  } else if (tier == 1) {
    c.fA = 0.85f;
    b.layers.push_back({2, c, 0, {}, 0, SkBlendMode::kSrcOver, false});
  } else {
    c.fA = 0.55f;
    b.layers.push_back({1, c, 0, {}, 0, SkBlendMode::kSrcOver, false});
  }
  return b;
}

/** Path of Exile's rope connector, 3-state (REFERENCES.md §5 — palette
 *  ladder verified against Path of Building): counter-dashed strand layers
 *  read as rope; Active adds the warm halo and specular ridge. state:
 *  0 Normal, 1 Intermediate (hover-path), 2 Active. */
inline LayeredBrush rope(int state) {
  struct P { SkColor4f body, ridge; };
  static constexpr P kStates[3] = {
      {{0.227f, 0.200f, 0.165f, 1}, {0.341f, 0.286f, 0.227f, 1}}, // #3A332A/#57493A
      {{0.420f, 0.353f, 0.251f, 1}, {0.553f, 0.459f, 0.314f, 1}}, // #6B5A40/#8D7550
      {{0.541f, 0.447f, 0.282f, 1}, {0.780f, 0.659f, 0.420f, 1}}, // #8A7248/#C7A86B
  };
  const P &p = kStates[state < 0 ? 0 : state > 2 ? 2 : state];
  SkColor4f bodyLit = {p.body.fR * 1.15f, p.body.fG * 1.15f,
                       p.body.fB * 1.15f, 1};
  SkColor4f ridgeLit = {p.ridge.fR * 1.3f, p.ridge.fG * 1.3f,
                        p.ridge.fB * 1.3f, 0.6f};
  LayeredBrush b;
  if (state >= 2)
    b.layers.push_back({18, {1.0f, 0.788f, 0.439f, 0.13f}, 6}); // halo
  b.layers.push_back({11, p.body, 0, {}, 0, SkBlendMode::kSrcOver, false});
  b.layers.push_back({7, p.ridge, 0, {7, 5}, 0});   // strand
  b.layers.push_back({7, bodyLit, 0, {7, 5}, 6});   // counter-strand
  b.layers.push_back({2, ridgeLit, 0, {7, 5}, 3});  // ridge light
  return b;
}

/** The §5 pulse-travel profile as a brush: plus-blended halo, colored
 *  body, white-hot core. Stroke it on a SHORT trim window of a rail
 *  (trim(&phase, &phaseEnd)) and march the window along the route —
 *  the energy packet on any connector. */
inline LayeredBrush pulse(SkColor4f halo = {1.0f, 0.79f, 0.44f, 0.35f},
                          SkColor4f core = {1, 1, 1, 0.9f},
                          float scale = 1.0f) {
  SkColor4f body = halo;
  body.fA = std::min(1.0f, halo.fA * 2.2f);
  return LayeredBrush{{
      {12 * scale, halo, 5 * scale, {}, 0, SkBlendMode::kPlus},
      {5 * scale, body, 2 * scale, {}, 0, SkBlendMode::kPlus},
      {2 * scale, core},
  }};
}

} // namespace brushes

// ---------------------------------------------------------------------------
// The Illustrator brush model — a brush is a PIPELINE: geometry ops over the
// path (the SkComposePathEffect idea, at our seam), then paint legs that
// INSTANCE real components along the result, each with a programmatic
// per-instance twist. Four Illustrator archetypes map onto three values:
//   Scatter brush  → brushes::ScatterBrush (jittered instances + mod fn)
//   Pattern brush  → brushes::PatternBrush (side/corner/start/end tiles,
//                    integer-fit stretch — the Illustrator tile semantics)
//   Calligraphic   → brushes::Ribbon (variable-width fill; nib angle)
//   Art brush      → a one-tile PatternBrush stretched over the run (true
//                    arc warping is queued on SkVertices)

namespace ops {

/** A path→path geometry op — our SkPathEffect-shaped extension point
 *  (Skia's own subclassing seam is sealed in its public API). Chain them
 *  with chain(); apply to any decoration with brushes::restyle(). */
using PathOp = std::function<SkPath(const SkPath &)>;

inline PathOp wave(float amplitude, float wavelength) {
  return [amplitude, wavelength](const SkPath &p) {
    return lines::displace(p, amplitude, wavelength, false);
  };
}
inline PathOp zigzag(float amplitude, float wavelength) {
  return [amplitude, wavelength](const SkPath &p) {
    return lines::displace(p, amplitude, wavelength, true);
  };
}
/** Round every corner (SkCornerPathEffect). */
inline PathOp rounded(float radius) {
  return [radius](const SkPath &p) {
    SkPathBuilder out;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx = SkCornerPathEffect::Make(radius);
        fx && fx->filterPath(&out, p, &rec))
      return out.detach();
    return p;
  };
}
/** The hand-drawn jitter (SkDiscretePathEffect — the rough.js look;
 *  grounded params in REFERENCES.md §9: deviation ≈ 2, and rough.js draws
 *  TWO passes, full + half deviation with different seeds, for the
 *  sketchy double-line — compose two restyle()s to match). */
inline PathOp sketchy(float segLength = 8.0f, float deviation = 2.0f,
                      uint32_t seed = 7) {
  return [segLength, deviation, seed](const SkPath &p) {
    SkPathBuilder out;
    SkStrokeRec rec(SkStrokeRec::kFill_InitStyle);
    if (sk_sp<SkPathEffect> fx =
            SkDiscretePathEffect::Make(segLength, deviation, seed);
        fx && fx->filterPath(&out, p, &rec))
      return out.detach();
    return p;
  };
}

/** Chain ops left-to-right — compose like SkComposePathEffect. */
inline PathOp chain(std::vector<PathOp> steps) {
  return [steps = std::move(steps)](const SkPath &p) {
    SkPath r = p;
    for (const PathOp &op : steps)
      if (op)
        r = op(r);
    return r;
  };
}

} // namespace ops

/** Anything with `SkPath apply(const SkPath&) const` — a geometry op as a
 *  VALUE. Optional `float bleed() const` declares extra paint reach (wave
 *  amplitude, offset distance). Value-comparable ops keep the whole Brush
 *  prunable; a raw ops::PathOp still converts (conservatively unequal). */
template <typename G>
concept GeometryScheme = requires(const G &g, const SkPath &p) {
  { g.apply(p) } -> std::convertible_to<SkPath>;
};

/** Type-erased comparable geometry op — Decoration's pattern applied to
 *  the path-transform seam (Skia seals SkPathEffect subclassing; this is
 *  our equivalent, as values). */
class GeometryOp {
public:
  template <GeometryScheme G>
  GeometryOp(G scheme) // NOLINT: implicit by design
      : m_bleed([&] {
          if constexpr (requires { { scheme.bleed() } -> std::convertible_to<float>; })
            return (float)scheme.bleed();
          else
            return 0.0f;
        }()) {
    if constexpr (std::equality_comparable<G>) {
      m_held = scheme;
      m_equals = [](const std::any &a, const std::any &b) {
        return std::any_cast<const G &>(a) == std::any_cast<const G &>(b);
      };
    }
    m_apply = [s = std::move(scheme)](const SkPath &p) { return s.apply(p); };
  }
  GeometryOp(ops::PathOp fn) // NOLINT: escape hatch, never prunes
      : m_apply(std::move(fn)) {}

  SkPath apply(const SkPath &p) const { return m_apply ? m_apply(p) : p; }
  float bleed() const { return m_bleed; }
  bool operator==(const GeometryOp &o) const {
    return m_equals && o.m_equals && m_held.type() == o.m_held.type() &&
           m_equals(m_held, o.m_held);
  }

private:
  float m_bleed = 0.0f;
  std::function<SkPath(const SkPath &)> m_apply;
  std::any m_held;
  std::function<bool(const std::any &, const std::any &)> m_equals;
};

namespace ops {

/** The struct forms — comparable, prunable, designated-init friendly. */
struct Wave {
  float amplitude = 4.0f, wavelength = 24.0f;
  bool zigzag = false;
  bool operator==(const Wave &) const = default;
  float bleed() const { return amplitude; }
  SkPath apply(const SkPath &p) const {
    return lines::displace(p, amplitude, wavelength, zigzag);
  }
};
struct Rounded {
  float radius = 6.0f;
  bool operator==(const Rounded &) const = default;
  SkPath apply(const SkPath &p) const { return rounded(radius)(p); }
};
struct Sketchy {
  float segLength = 8.0f, deviation = 2.0f;
  uint32_t seed = 7;
  bool operator==(const Sketchy &) const = default;
  float bleed() const { return deviation * 2; }
  SkPath apply(const SkPath &p) const {
    return sketchy(segLength, deviation, seed)(p);
  }
};
struct Offset {
  float px = 0.0f;
  float step = 4.0f;
  bool operator==(const Offset &) const = default;
  float bleed() const { return std::abs(px); }
  SkPath apply(const SkPath &p) const { return lines::offsetAlong(p, px, step); }
};

} // namespace ops

/** THE BRUSH: one composable value — a geometry PIPELINE over the outline
 *  (ops applied in order, the SkComposePathEffect idea as data) feeding
 *  ordered paint LEGS (any Decoration: a lines::Line, a LayeredBrush
 *  stack, Scatter/Pattern instancing, a Ribbon, a raw PathFormat). The
 *  Illustrator model, closed under composition:
 *
 *    element.stroke(Brush{}
 *        .op(ops::Rounded{6})
 *        .op(ops::Wave{.amplitude = 3, .wavelength = 30})
 *        .leg(lines::cased(3, ink, 5))
 *        .leg(brushes::ScatterBrush{.art = spark(), .spacing = 40}));
 *
 *  A Brush of comparable ops and legs is itself comparable — the whole
 *  styled connector prunes and caches as ONE value. animated legs declare
 *  volatility through; bleed aggregates pipeline reach + leg reach. */
struct Brush {
  std::vector<GeometryOp> pipeline;
  std::vector<Decoration> legs;

  Brush &op(GeometryOp g) {
    pipeline.push_back(std::move(g));
    return *this;
  }
  Brush &leg(Decoration d) {
    legs.push_back(std::move(d));
    return *this;
  }

  bool operator==(const Brush &o) const {
    return pipeline == o.pipeline && legs == o.legs;
  }
  bool animated() const {
    for (const Decoration &d : legs)
      if (d.animated())
        return true;
    return false;
  }
  float bleed() const {
    float ops = 0, leg = 0;
    for (const GeometryOp &g : pipeline)
      ops += g.bleed(); // pipeline reaches compound (offset THEN wave)
    for (const Decoration &d : legs)
      leg = std::max(leg, d.bleed());
    return ops + leg;
  }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPath styled = ctx.outline;
    for (const GeometryOp &g : pipeline)
      styled = g.apply(styled);
    const PaintContext restyled{ctx.size,        std::move(styled),
                                ctx.elapsedSeconds, ctx.contentScale,
                                ctx.animating,   ctx.fonts};
    for (const Decoration &d : legs)
      d.paint(c, restyled);
  }
};

namespace brushes {

/** Run a geometry pipeline, then paint `inner` on the restyled outline —
 *  any decoration (LayeredBrush, lines::Line, PathFormat…) gains waves,
 *  jitter, rounding without knowing. The op is an incomparable callable:
 *  memo the host node (or keep it pointer-stable) to prune. */
struct Restyled {
  ops::PathOp op;
  Decoration inner;
  float extraBleed = 8.0f; // the op's own overhang (wave amplitude…)

  bool animated() const { return inner.animated(); }
  float bleed() const { return inner.bleed() + extraBleed; }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    PaintContext restyled{ctx.size,        op ? op(ctx.outline) : ctx.outline,
                          ctx.elapsedSeconds, ctx.contentScale,
                          ctx.animating,   ctx.fonts};
    inner.paint(c, restyled);
  }
};

inline Restyled restyle(ops::PathOp op, Decoration inner,
                        float extraBleed = 8.0f) {
  return Restyled{std::move(op), std::move(inner), extraBleed};
}

/** One placed instance's deviation from its slot — the programmatic twist
 *  (mirrors GlyphMod; return {.skip = true} to drop a slot). */
struct StampMod {
  float dAlong = 0, dNormal = 0; ///< px, in the sample's tangent frame
  float scale = 1;
  float rotateDeg = 0;
  float alpha = 1;
  bool skip = false;
};
using StampModFn =
    std::function<StampMod(const PathSample &, size_t index, size_t count)>;

namespace detail {
inline void drawStamp(SkCanvas &c, const SkPicture &pic,
                      const PathSample &sample, bool align, float rotateDeg,
                      float scaleX, float scaleY, const StampMod &m) {
  if (m.skip || m.alpha <= 0.003f || m.scale <= 0.001f)
    return;
  const SkRect cull = pic.cullRect();
  c.save();
  c.translate(sample.position.x(), sample.position.y());
  if (align)
    c.rotate(std::atan2(sample.tangent.y(), sample.tangent.x()) *
             57.29578f);
  c.translate(m.dAlong, m.dNormal); // tangent frame (post-align)
  c.rotate(rotateDeg + m.rotateDeg);
  c.scale(scaleX * m.scale, scaleY * m.scale);
  c.translate(-cull.width() / 2, -cull.height() / 2);
  if (m.alpha < 1.0f) {
    SkPaint fade;
    fade.setAlphaf(m.alpha);
    c.drawPicture(&pic, nullptr, &fade);
  } else {
    c.drawPicture(&pic);
  }
  c.restore();
}
} // namespace detail

/** The SCATTER brush: an Element instanced along the path at `spacing`,
 *  with seeded jitter and the StampMod hook. The art bakes ONCE via
 *  snapshot() (its own decorations and all) and replays per slot. Keep
 *  the art Element pointer-stable across renders to prune; a mod fn makes
 *  the value incomparable (memo the host). */
struct ScatterBrush {
  Element art;
  float spacing = 24.0f;
  uint32_t seed = 0; ///< 0 = a regular run, no jitter roll
  float jitterAlong = 0, jitterNormal = 0; ///< ±px
  float jitterScale = 0;                   ///< ±fraction of 1
  float jitterRotateDeg = 0;               ///< ±deg
  bool alignToPath = true;
  float reach = 32.0f; ///< cull reserve: half the art's extent + jitter
  StampModFn mod;
  bool animatedMod = false; ///< mod reads time → repaint per frame

  bool animated() const { return animatedMod; }
  float bleed() const { return reach; }
  bool operator==(const ScatterBrush &o) const {
    return art.node() == o.art.node() && spacing == o.spacing &&
           seed == o.seed && jitterAlong == o.jitterAlong &&
           jitterNormal == o.jitterNormal && jitterScale == o.jitterScale &&
           jitterRotateDeg == o.jitterRotateDeg &&
           alignToPath == o.alignToPath && reach == o.reach && !mod &&
           !o.mod && animatedMod == o.animatedMod;
  }

  struct Cache {
    sk_sp<SkPicture> pic;
    const void *bakedFor = nullptr; // the art node the bake belongs to —
                                    // copies that swap art re-bake
  };
  std::shared_ptr<Cache> cache = std::make_shared<Cache>();

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    if (spacing <= 0 || !ctx.fonts)
      return;
    if (!cache->pic || cache->bakedFor != art.node().get()) {
      // shell box: snapshot ignores the ROOT's own dims
      cache->pic = snapshot(box().child(art), *ctx.fonts);
      cache->bakedFor = art.node().get();
    }
    if (!cache->pic)
      return;

    std::vector<PathSample> samples;
    SkContourMeasureIter iter(ctx.outline, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      for (float d = spacing * 0.5f; d < len; d += spacing) {
        SkPoint pos;
        SkVector tan;
        if (contour->getPosTan(d, &pos, &tan))
          samples.push_back({pos, tan, d, len > 0 ? d / len : 0});
      }
    }
    for (size_t i = 0; i < samples.size(); ++i) {
      StampMod m;
      if (mod)
        m = mod(samples[i], i, samples.size());
      if (seed != 0) {
        const uint32_t k = (uint32_t)i;
        m.dAlong += shapes::detail::hashNoise(seed, 4 * k) * jitterAlong;
        m.dNormal += shapes::detail::hashNoise(seed, 4 * k + 1) * jitterNormal;
        m.scale *= 1.0f +
                   shapes::detail::hashNoise(seed, 4 * k + 2) * jitterScale;
        m.rotateDeg +=
            shapes::detail::hashNoise(seed, 4 * k + 3) * jitterRotateDeg;
      }
      detail::drawStamp(c, *cache->pic, samples[i], alignToPath, 0, 1, 1, m);
    }
  }
};

/** The PATTERN brush (Illustrator tile semantics): a SIDE tile repeated an
 *  INTEGER number of times per run and stretched along the tangent to fit
 *  exactly (never a torn tile at the end); optional CORNER tiles where the
 *  tangent breaks by more than `cornerAngleDeg` (placed on the bisector);
 *  optional START/END tiles on open contours. Runs are the stretches
 *  between corners. An art brush is the one-tile degenerate case. */
struct PatternBrush {
  Element side;
  std::optional<Element> start, end, corner;
  float advance = 0;           ///< tile length along the path (0 → intrinsic)
  float cornerAngleDeg = 35.0f; ///< PER-SAMPLE tangent break — gently
                                ///< ROUNDED corners intentionally take no
                                ///< corner tile (no hard break exists)
  bool stretchToFit = true;    ///< false: natural size, slack spread evenly
  float reach = 32.0f;         ///< cull reserve
  StampModFn mod;              ///< side tiles only
  bool animatedMod = false;

  bool animated() const { return animatedMod; }
  float bleed() const { return reach; }
  bool operator==(const PatternBrush &o) const {
    auto node = [](const std::optional<Element> &e) {
      return e ? e->node().get() : nullptr;
    };
    return side.node() == o.side.node() && node(start) == node(o.start) &&
           node(end) == node(o.end) && node(corner) == node(o.corner) &&
           advance == o.advance && cornerAngleDeg == o.cornerAngleDeg &&
           stretchToFit == o.stretchToFit && reach == o.reach && !mod &&
           !o.mod && animatedMod == o.animatedMod;
  }

  struct Cache {
    sk_sp<SkPicture> side, start, end, corner;
    const void *bakedFor = nullptr; // side-art identity; swap → full re-bake
  };
  std::shared_ptr<Cache> cache = std::make_shared<Cache>();

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    if (!ctx.fonts)
      return;
    if (cache->bakedFor != side.node().get()) {
      *cache = Cache{};
      cache->bakedFor = side.node().get();
    }
    auto bake = [&](const std::optional<Element> &e, sk_sp<SkPicture> &slot) {
      if (e && !slot) // shell box: snapshot ignores the ROOT's own dims
        slot = snapshot(box().child(*e), *ctx.fonts);
    };
    if (!cache->side)
      cache->side = snapshot(box().child(side), *ctx.fonts);
    bake(start, cache->start);
    bake(end, cache->end);
    bake(corner, cache->corner);
    if (!cache->side)
      return;
    const float tileLen =
        advance > 0 ? advance : std::max(cache->side->cullRect().width(), 1.0f);

    size_t placed = 0;
    // Two passes: count side tiles first so mod sees the true total.
    std::vector<std::pair<PathSample, float>> sideSlots; // sample + scaleX
    std::vector<std::pair<PathSample, const SkPicture *>> caps;

    SkContourMeasureIter iter(ctx.outline, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      const bool closed = contour->isClosed();

      // Corners: where successive tangents break by more than the
      // threshold (sampled at a fine step, deduped within a tile).
      std::vector<float> corners;
      if (cache->corner) {
        const float step = std::clamp(tileLen * 0.25f, 1.0f, 6.0f);
        SkVector prev{0, 0};
        bool havePrev = false;
        const float cosThresh =
            std::cos(cornerAngleDeg * 0.017453293f);
        SkVector tanAtStart{0, 0};
        for (float d = 0; d <= len; d += step) {
          SkPoint pos;
          SkVector tan;
          if (!contour->getPosTan(std::min(d, len), &pos, &tan))
            continue;
          if (!havePrev)
            tanAtStart = tan;
          if (havePrev) {
            const float dot = prev.x() * tan.x() + prev.y() * tan.y();
            if (dot < cosThresh &&
                (corners.empty() || d - corners.back() > tileLen * 0.5f))
              corners.push_back(d - step * 0.5f);
          }
          prev = tan;
          havePrev = true;
        }
        // The SEAM of a closed contour is a corner too when the exit and
        // entry tangents break (a rectangle's start point) — the forward
        // scan never compares across the wrap.
        if (closed && havePrev) {
          const float dot =
              prev.x() * tanAtStart.x() + prev.y() * tanAtStart.y();
          if (dot < cosThresh &&
              (corners.empty() || corners.front() > tileLen * 0.5f))
            corners.insert(corners.begin(), 0.0f);
        }
      }

      // Open-contour caps reserve their slots at the ends.
      float head = 0, tail = 0;
      if (!closed && cache->start)
        head = advance > 0 ? advance : cache->start->cullRect().width();
      if (!closed && cache->end)
        tail = advance > 0 ? advance : cache->end->cullRect().width();

      // Runs between corners (and cap margins).
      std::vector<float> bounds{head};
      for (float d : corners)
        if (d > head && d < len - tail)
          bounds.push_back(d);
      bounds.push_back(len - tail);

      for (size_t r = 0; r + 1 < bounds.size(); ++r) {
        const float a = bounds[r], b = bounds[r + 1];
        const float L = b - a;
        if (L < tileLen * 0.25f)
          continue;
        const int n = std::max(1, (int)std::lround(L / tileLen));
        const float slot = L / (float)n;
        const float sx = stretchToFit ? slot / tileLen : 1.0f;
        for (int i = 0; i < n; ++i) {
          const float d = a + slot * ((float)i + 0.5f);
          SkPoint pos;
          SkVector tan;
          if (contour->getPosTan(d, &pos, &tan))
            sideSlots.push_back(
                {{pos, tan, d, len > 0 ? d / len : 0}, sx});
        }
      }

      // Corner tiles sit on the bisector of the break.
      if (cache->corner)
        for (float d : corners) {
          SkPoint pos;
          SkVector before, after;
          if (!contour->getPosTan(d, &pos, nullptr))
            continue;
          contour->getPosTan(std::max(d - 2.0f, 0.0f), nullptr, &before);
          contour->getPosTan(std::min(d + 2.0f, len), nullptr, &after);
          const SkVector bis{before.x() + after.x(), before.y() + after.y()};
          caps.push_back({{pos, bis, d, len > 0 ? d / len : 0},
                          cache->corner.get()});
        }
      if (!closed && cache->start) {
        SkPoint pos;
        SkVector tan;
        if (contour->getPosTan(head * 0.5f, &pos, &tan))
          caps.push_back({{pos, tan, 0, 0}, cache->start.get()});
      }
      if (!closed && cache->end) {
        SkPoint pos;
        SkVector tan;
        if (contour->getPosTan(len - tail * 0.5f, &pos, &tan))
          caps.push_back({{pos, tan, len, 1}, cache->end.get()});
      }
    }

    for (const auto &[sample, sx] : sideSlots) {
      StampMod m;
      if (mod)
        m = mod(sample, placed, sideSlots.size());
      detail::drawStamp(c, *cache->side, sample, true, 0, sx, 1, m);
      ++placed;
    }
    for (const auto &[sample, pic] : caps)
      detail::drawStamp(c, *pic, sample, true, 0, 1, 1, {});
  }
};

/** The variable-width RIBBON: a filled band whose width follows a profile —
 *  linear taper by default, a calligraphic nib when `nibAngleDeg` ≥ 0
 *  (width peaks perpendicular to the nib, the Illustrator calligraphic
 *  model), or any custom `widthFn` (incomparable — memo the host). */
struct Ribbon {
  Fill fill = Fill::color({1, 1, 1, 1});
  float widthStart = 10.0f, widthEnd = 2.0f;
  float nibAngleDeg = -1.0f;  ///< ≥0 → calligraphic (widthStart = full)
  float nibContrast = 0.15f;  ///< thinnest fraction at nib-aligned tangents
  float step = 3.0f; // clamped ≥ 0.5px at paint (0 would never advance)
  std::function<float(const PathSample &)> widthFn;

  float bleed() const { return std::max(widthStart, widthEnd); }
  bool operator==(const Ribbon &o) const {
    return fill == o.fill && widthStart == o.widthStart &&
           widthEnd == o.widthEnd && nibAngleDeg == o.nibAngleDeg &&
           nibContrast == o.nibContrast && step == o.step && !widthFn &&
           !o.widthFn;
  }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    if (fill.kind == Fill::Kind::Color)
      p.setColor4f(fill.colorValue, nullptr);
    else if (fill.kind == Fill::Kind::Shader)
      p.setShader(fill.shaderValue);

    const float stride = std::max(step, 0.5f);
    SkContourMeasureIter iter(ctx.outline, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      std::vector<SkPoint> left, right;
      for (float d = 0;; d += stride) {
        const float at = std::min(d, len);
        SkPoint pos;
        SkVector tan;
        if (!contour->getPosTan(at, &pos, &tan))
          break;
        const PathSample s{pos, tan, at, len > 0 ? at / len : 0};
        float w;
        if (widthFn) {
          w = widthFn(s);
        } else if (nibAngleDeg >= 0) {
          const float a = std::atan2(tan.y(), tan.x()) -
                          nibAngleDeg * 0.017453293f;
          w = widthStart *
              (nibContrast + (1 - nibContrast) * std::abs(std::sin(a)));
        } else {
          w = widthStart + (widthEnd - widthStart) * s.fraction;
        }
        const SkVector n{-tan.y(), tan.x()};
        left.push_back({pos.x() + n.x() * w / 2, pos.y() + n.y() * w / 2});
        right.push_back({pos.x() - n.x() * w / 2, pos.y() - n.y() * w / 2});
        if (at >= len)
          break;
      }
      if (left.size() < 2)
        continue;
      SkPathBuilder band;
      band.moveTo(left.front());
      for (size_t i = 1; i < left.size(); ++i)
        band.lineTo(left[i]);
      for (size_t i = right.size(); i-- > 0;)
        band.lineTo(right[i]);
      band.close();
      c.drawPath(band.detach(), p);
    }
  }
};

/** Linear taper (comet body, ink pull-away). */
inline Ribbon taper(float widthStart, float widthEnd, Fill fill) {
  Ribbon r;
  r.widthStart = widthStart;
  r.widthEnd = widthEnd;
  r.fill = std::move(fill);
  return r;
}

/** The calligraphic nib: full width perpendicular to `nibAngleDeg`,
 *  `contrast` fraction when the path runs along the nib. */
inline Ribbon calligraphic(float nibAngleDeg, float width, Fill fill,
                           float contrast = 0.15f) {
  Ribbon r;
  r.widthStart = width;
  r.nibAngleDeg = nibAngleDeg;
  r.nibContrast = contrast;
  r.fill = std::move(fill);
  return r;
}

} // namespace brushes
} // namespace sigil::compose

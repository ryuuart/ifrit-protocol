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
#include <include/core/SkStrokeRec.h> // ops::rounded / ops::sketchy filterPath
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <include/core/SkVertices.h>
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
 *  0 Normal, 1 Intermediate (hover-path), 2 Active.
 *
 *  `scale` is the zoom the rope is drawn at — every width, dash and blur
 *  moves together, the way the game's own line art does. The default is
 *  the widely-spaced study; a dense cluster wants ~0.6. */
inline LayeredBrush rope(int state, float scale = 1.0f) {
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
  const float k = scale <= 0 ? 1.0f : scale;
  LayeredBrush b;
  if (state >= 2)
    b.layers.push_back({18 * k, {1.0f, 0.788f, 0.439f, 0.13f}, 6 * k}); // halo
  b.layers.push_back(
      {11 * k, p.body, 0, {}, 0, SkBlendMode::kSrcOver, false});
  b.layers.push_back({7 * k, p.ridge, 0, {7 * k, 5 * k}, 0});   // strand
  b.layers.push_back({7 * k, bodyLit, 0, {7 * k, 5 * k}, 6 * k}); // counter
  b.layers.push_back({2 * k, ridgeLit, 0, {7 * k, 5 * k}, 3 * k}); // ridge
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
    // HAIRLINE rec: under a fill rec SkDiscretePathEffect force-CLOSES
    // open contours — the transit study's phantom river channel (the
    // return chord braided the real run under jitter divergence).
    SkStrokeRec rec(SkStrokeRec::kHairline_InitStyle);
    if (sk_sp<SkPathEffect> fx =
            SkDiscretePathEffect::Make(segLength, deviation, seed);
        fx && fx->filterPath(&out, p, &rec))
      return out.detach();
    return p;
  };
}

/** Dump the path's contour census (count/lengths/closedness/bounds) to
 *  stderr and pass it through unchanged — drop into any pipeline position
 *  when a construction misbehaves. */
inline PathOp debug(const char *tag = "brush") {
  std::string t = tag;
  return [t](const SkPath &p) {
    const SkRect b = p.getBounds();
    SkDebugf("[ops::debug %s] bounds (%.1f,%.1f %.1fx%.1f)\n", t.c_str(),
             b.left(), b.top(), b.width(), b.height());
    SkContourMeasureIter iter(p, false);
    int i = 0;
    while (sk_sp<SkContourMeasure> c = iter.next())
      SkDebugf("  contour %d: len %.1f %s\n", i++, c->length(),
               c->isClosed() ? "CLOSED" : "open");
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
struct Sketchy { // open contours STAY open (hairline rec — see sketchy())
  float segLength = 8.0f, deviation = 2.0f;
  uint32_t seed = 7;
  bool operator==(const Sketchy &) const = default;
  float bleed() const { return deviation * 2; }
  SkPath apply(const SkPath &p) const {
    return sketchy(segLength, deviation, seed)(p);
  }
};
struct Square { // boxy: battlement/meander-key displacement
  float amplitude = 5.0f, wavelength = 32.0f;
  bool operator==(const Square &) const = default;
  float bleed() const { return amplitude; }
  SkPath apply(const SkPath &p) const {
    return lines::displaceSquare(p, amplitude, wavelength);
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
  /** One paint leg: a Decoration plus its own pipeline SUFFIX (applied
   *  after the shared pipeline, this leg only) — the asymmetric-casing
   *  ask: one Brush reads as one material ("road with lane and curb"),
   *  each side riding its own ops::Offset. */
  struct Leg {
    Decoration dec;
    std::vector<GeometryOp> ops;
    bool operator==(const Leg &o) const {
      return dec == o.dec && ops == o.ops;
    }
  };

  std::vector<GeometryOp> pipeline;
  std::vector<Leg> legs;

  Brush &op(GeometryOp g) {
    pipeline.push_back(std::move(g));
    return *this;
  }
  Brush &leg(Decoration d, std::vector<GeometryOp> legOps = {}) {
    legs.push_back(Leg{std::move(d), std::move(legOps)});
    return *this;
  }

  bool operator==(const Brush &o) const {
    return pipeline == o.pipeline && legs == o.legs;
  }
  bool animated() const {
    for (const Leg &l : legs)
      if (l.dec.animated())
        return true;
    return false;
  }
  float bleed() const {
    float shared = 0;
    for (const GeometryOp &g : pipeline)
      shared += g.bleed(); // pipeline reaches compound (offset THEN wave)
    float worst = 0;
    for (const Leg &l : legs) {
      float legReach = l.dec.bleed();
      for (const GeometryOp &g : l.ops)
        legReach += g.bleed();
      worst = std::max(worst, legReach);
    }
    return shared + worst;
  }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPath styled = ctx.outline;
    for (const GeometryOp &g : pipeline)
      styled = g.apply(styled);
    for (const Leg &l : legs) {
      SkPath legPath = styled;
      for (const GeometryOp &g : l.ops)
        legPath = g.apply(legPath);
      const PaintContext restyled{ctx.size,        std::move(legPath),
                                  ctx.elapsedSeconds, ctx.contentScale,
                                  ctx.animating,   ctx.fonts};
      l.dec.paint(c, restyled);
    }
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

/** WHERE instances land along a path — the QGIS marker-line placement
 *  grammar (Interval | Vertex | FirstVertex | LastVertex | InnerVertices |
 *  CentralPoint | SegmentCenter), verified in REFERENCES.md §9. Vertex
 *  modes read the path's REAL verbs (the route's bends), not tangent
 *  sampling; `interval` > 1 is px, ≤ 1 is a FRACTION of each contour
 *  (the decorator px-or-% spec). */
struct Placement {
  enum class Mode : uint8_t {
    Interval,      ///< every `interval` px (or fraction), phase `offset`
    Vertex,        ///< every path vertex (bends + endpoints)
    FirstVertex,   ///< each contour's first point
    LastVertex,    ///< each contour's last point
    InnerVertices, ///< bends only — no endpoints
    CentralPoint,  ///< the arc-length midpoint of each contour
    SegmentCenter, ///< the midpoint of every straight segment
  };
  Mode mode = Mode::Interval;
  float interval = 24.0f; ///< px, or contour fraction when ≤ 1
  float offset = 0.0f;    ///< leading phase for Interval (same units)
  bool operator==(const Placement &) const = default;
};

namespace detail {
/** Resolve a Placement into concrete samples (position + tangent). */
inline std::vector<PathSample> placementSamples(const SkPath &path,
                                                const Placement &p) {
  std::vector<PathSample> out;
  using Mode = Placement::Mode;
  if (p.mode == Mode::Interval || p.mode == Mode::CentralPoint) {
    SkContourMeasureIter iter(path, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      const float step = p.interval <= 1.0f ? len * std::max(p.interval, 0.001f)
                                            : p.interval;
      const float phase =
          p.offset <= 1.0f && p.offset >= -1.0f && p.mode == Mode::Interval &&
                  p.interval <= 1.0f
              ? len * p.offset
              : p.offset;
      auto sampleAt = [&](float d) {
        SkPoint pos;
        SkVector tan;
        if (contour->getPosTan(std::clamp(d, 0.0f, len), &pos, &tan))
          out.push_back({pos, tan, d, len > 0 ? d / len : 0});
      };
      if (p.mode == Mode::CentralPoint) {
        sampleAt(len * 0.5f);
      } else {
        for (float d = phase + step * 0.5f; d < len; d += step)
          sampleAt(d);
      }
    }
    return out;
  }
  // Vertex family: walk the REAL verbs per contour.
  std::vector<std::vector<SkPoint>> contours;
  SkPath::RawIter it(path);
  SkPoint pts[4];
  for (SkPath::Verb v = it.next(pts); v != SkPath::kDone_Verb;
       v = it.next(pts)) {
    switch (v) {
    case SkPath::kMove_Verb:
      contours.push_back({pts[0]});
      break;
    case SkPath::kLine_Verb:
      contours.back().push_back(pts[1]);
      break;
    case SkPath::kQuad_Verb:
      contours.back().push_back(pts[2]);
      break;
    case SkPath::kConic_Verb:
      contours.back().push_back(pts[2]);
      break;
    case SkPath::kCubic_Verb:
      contours.back().push_back(pts[3]);
      break;
    default:
      break;
    }
  }
  for (const auto &c : contours) {
    if (c.empty())
      continue;
    auto tangentAt = [&](size_t i) {
      const SkPoint prev = c[i > 0 ? i - 1 : i];
      const SkPoint next = c[i + 1 < c.size() ? i + 1 : i];
      SkVector t{next.x() - prev.x(), next.y() - prev.y()};
      const float m = std::hypot(t.x(), t.y());
      return m > 1e-4f ? SkVector{t.x() / m, t.y() / m} : SkVector{1, 0};
    };
    const float n = (float)c.size();
    switch (p.mode) {
    case Mode::Vertex:
      for (size_t i = 0; i < c.size(); ++i)
        out.push_back({c[i], tangentAt(i), 0, n > 1 ? (float)i / (n - 1) : 0});
      break;
    case Mode::FirstVertex:
      out.push_back({c.front(), tangentAt(0), 0, 0});
      break;
    case Mode::LastVertex:
      out.push_back({c.back(), tangentAt(c.size() - 1), 0, 1});
      break;
    case Mode::InnerVertices:
      for (size_t i = 1; i + 1 < c.size(); ++i)
        out.push_back({c[i], tangentAt(i), 0, n > 1 ? (float)i / (n - 1) : 0});
      break;
    case Mode::SegmentCenter:
      for (size_t i = 0; i + 1 < c.size(); ++i) {
        const SkPoint mid{(c[i].x() + c[i + 1].x()) / 2,
                          (c[i].y() + c[i + 1].y()) / 2};
        SkVector t{c[i + 1].x() - c[i].x(), c[i + 1].y() - c[i].y()};
        const float m = std::hypot(t.x(), t.y());
        if (m > 1e-4f)
          out.push_back({mid, {t.x() / m, t.y() / m}, 0,
                         n > 1 ? ((float)i + 0.5f) / (n - 1) : 0});
      }
      break;
    default:
      break;
    }
  }
  return out;
}
} // namespace detail

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
  float spacing = 24.0f; ///< Interval-mode sugar (px, or fraction ≤ 1)
  /** Full placement grammar — set `place.mode` for Vertex/SegmentCenter/
   *  CentralPoint… families; `spacing` feeds Interval when place is
   *  default-constructed. */
  Placement place{};
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
           place == o.place && seed == o.seed &&
           jitterAlong == o.jitterAlong &&
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

    Placement resolved = place;
    if (resolved.mode == Placement::Mode::Interval && resolved.interval == 24.0f)
      resolved.interval = spacing; // the spacing sugar feeds Interval
    std::vector<PathSample> samples =
        detail::placementSamples(ctx.outline, resolved);
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
  /** Arc length a corner tile RESERVES on each adjacent run, px. 0 uses the
   *  corner art's own width.
   *
   *  This used to not exist, and the omission was invisible for exactly the
   *  reason it was dangerous. Side tiles were laid out over the full
   *  corner-to-corner span and the corner tile was then drawn ON TOP at the
   *  break point, so side tiles ran underneath it. With a corner tile the
   *  same size as a side tile the overlap lands where a tile boundary
   *  already was and nothing looks wrong; with a real elbow — a 48 px
   *  corner against a 24 px side, which is what an ornamental frame
   *  actually wants — the side run visibly continues under the elbow.
   *
   *  Now each corner reserves `cornerLength / 2` at each end of its two
   *  adjacent runs and the side run's integer fit is recomputed over the
   *  SHORTENED span, so tiles butt against the corner instead of sliding
   *  beneath it. Frames whose corner art is the same size as their side art
   *  will shift side-tile phase very slightly; that is the corner finally
   *  taking up its own room. */
  float cornerLength = 0.0f;
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
           cornerLength == o.cornerLength &&
           stretchToFit == o.stretchToFit && reach == o.reach && !mod &&
           !o.mod && animatedMod == o.animatedMod;
  }

  struct Cache {
    sk_sp<SkPicture> side, start, end, corner;
    const void *bakedSide = nullptr;
    const void *bakedStart = nullptr;
    const void *bakedEnd = nullptr;
    const void *bakedCorner = nullptr;
  };
  std::shared_ptr<Cache> cache = std::make_shared<Cache>();

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    if (!ctx.fonts)
      return;
    auto node = [](const std::optional<Element> &e) -> const void * {
      return e ? e->node().get() : nullptr;
    };
    const void *sideNode = side.node().get();
    const void *startNode = node(start);
    const void *endNode = node(end);
    const void *cornerNode = node(corner);
    if (cache->bakedSide != sideNode || cache->bakedStart != startNode ||
        cache->bakedEnd != endNode || cache->bakedCorner != cornerNode) {
      *cache = Cache{};
      cache->bakedSide = sideNode;
      cache->bakedStart = startNode;
      cache->bakedEnd = endNode;
      cache->bakedCorner = cornerNode;
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

      // Runs between corners (and cap margins). Each corner RESERVES half
      // its own length at each end of its two adjacent runs, so the side
      // tiles butt against the corner art instead of running under it.
      const float cornerRoom =
          cache->corner
              ? (cornerLength > 0 ? cornerLength
                                  : cache->corner->cullRect().width())
              : 0.0f;
      const float halfCorner = cornerRoom * 0.5f;
      std::vector<float> bounds{head};
      for (float d : corners)
        if (d > head && d < len - tail) {
          bounds.push_back(d - halfCorner); // run ends before the corner
          bounds.push_back(d + halfCorner); // next run starts after it
        }
      bounds.push_back(len - tail);

      for (size_t r = 0; r + 1 < bounds.size(); ++r) {
        // Odd spans are the reserved corner gaps themselves — skip them.
        if (halfCorner > 0 && r % 2 == 1)
          continue;
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
          auto sampleDistance = [closed, len](float at) {
            if (!closed || len <= 0.0f)
              return std::clamp(at, 0.0f, len);
            at = std::fmod(at, len);
            return at < 0.0f ? at + len : at;
          };
          if (!contour->getPosTan(sampleDistance(d - 2.0f), nullptr, &before) ||
              !contour->getPosTan(sampleDistance(d + 2.0f), nullptr, &after))
            continue;
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
  /** Upper bound on what `widthFn` returns, in px.
   *
   *  `bleed()` grows the recording cull, and it cannot look inside a
   *  `std::function` — so a width function returning 166 on a ribbon
   *  whose widthStart/widthEnd are the defaults declared 10 px of reach
   *  and the band was silently CLIPPED. A flow map is exactly this shape
   *  (Minard's retreat band runs 166 px at Kowno), and the failure looks
   *  like a rendering bug rather than a missing declaration.
   *
   *  Set it whenever you set `widthFn`. It is the one number the library
   *  cannot derive for you. */
  float widthMax = 0.0f;

  float bleed() const {
    const float base = std::max(widthStart, widthEnd);
    return widthFn ? std::max(base, widthMax) : base;
  }
  bool operator==(const Ribbon &o) const {
    return fill == o.fill && widthStart == o.widthStart &&
           widthEnd == o.widthEnd && nibAngleDeg == o.nibAngleDeg &&
           nibContrast == o.nibContrast && step == o.step &&
           widthMax == o.widthMax && !widthFn && !o.widthFn;
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

/** The ART brush proper (Illustrator's third brush kind, the one the
 *  stamp/tile brushes can't fake): ONE art cell stretched and continuously
 *  BENT along each contour via SkVertices. The art bakes once to a texture
 *  (2x oversampled, like the instancing atlas); each contour is walked
 *  into a triangle-strip ribbon (position ± normal·h per station) whose
 *  texture coordinates sweep the art from end to end — curvature warps the
 *  art smoothly, where a stamp run breaks into rigid segments. One
 *  drawVertices per contour. `stationPx` is warp fidelity: one strip
 *  station per N arc-px (6 px follows tight metro curves; loosen for
 *  long gentle paths). */
struct ArtBrush {
  Element art;
  float height = 0;        ///< ribbon height (0 → the art's intrinsic)
  float stationPx = 6.0f;  ///< arc-length between strip stations
  float reach = 32.0f;     ///< cull reserve: half height + art overhang

  bool animated() const { return false; }
  float bleed() const { return reach; }
  bool operator==(const ArtBrush &o) const {
    return art.node() == o.art.node() && height == o.height &&
           stationPx == o.stationPx && reach == o.reach;
  }

  struct Cache {
    sk_sp<SkImage> image; // the 2x bake
    SkSize artSize{0, 0}; // logical art size
    const void *bakedFor = nullptr;
  };
  std::shared_ptr<Cache> cache = std::make_shared<Cache>();

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    if (!ctx.fonts)
      return;
    if (!cache->image || cache->bakedFor != art.node().get()) {
      cache->bakedFor = art.node().get();
      cache->image = nullptr;
      // shell box: snapshot/measure ignore the ROOT's own dims
      const SkSize sz = measure(box().child(art), *ctx.fonts);
      if (sz.isEmpty())
        return;
      sk_sp<SkPicture> pic = snapshot(box().child(art), *ctx.fonts);
      sk_sp<SkSurface> surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
          std::max(1, (int)std::ceil(sz.width() * 2.0f)),
          std::max(1, (int)std::ceil(sz.height() * 2.0f))));
      if (!pic || !surface)
        return;
      surface->getCanvas()->clear(SK_ColorTRANSPARENT);
      surface->getCanvas()->scale(2.0f, 2.0f);
      surface->getCanvas()->drawPicture(pic);
      cache->image = surface->makeImageSnapshot();
      cache->artSize = sz;
    }
    if (!cache->image)
      return;

    const float texW = (float)cache->image->width();
    const float texH = (float)cache->image->height();
    const float half =
        0.5f * (height > 0 ? height : cache->artSize.height());
    SkPaint p;
    p.setAntiAlias(true);
    p.setShader(cache->image->makeShader(
        SkTileMode::kClamp, SkTileMode::kClamp,
        SkSamplingOptions(SkFilterMode::kLinear)));

    SkContourMeasureIter iter(ctx.outline, false);
    std::vector<SkPoint> positions, texs;
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float length = contour->length();
      if (length < 1.0f)
        continue;
      const int stations =
          std::max(2, (int)std::ceil(length / std::max(1.0f, stationPx)));
      positions.clear();
      texs.clear();
      positions.reserve((size_t)(stations + 1) * 2);
      texs.reserve((size_t)(stations + 1) * 2);
      for (int i = 0; i <= stations; ++i) {
        const float f = (float)i / (float)stations;
        SkPoint pos;
        SkVector tan;
        if (!contour->getPosTan(length * f, &pos, &tan))
          continue;
        const SkVector normal{-tan.fY, tan.fX};
        positions.push_back(pos + normal * half);
        positions.push_back(pos - normal * half);
        texs.push_back({texW * f, 0.0f});
        texs.push_back({texW * f, texH});
      }
      if (positions.size() < 4)
        continue;
      c.drawVertices(SkVertices::MakeCopy(
                         SkVertices::kTriangleStrip_VertexMode,
                         (int)positions.size(), positions.data(), texs.data(),
                         nullptr),
                     SkBlendMode::kModulate, p);
    }
  }
};

/** Art warped along the path: the drawVertices ribbon. `height` 0 keeps
 *  the art's intrinsic height. */
inline ArtBrush artAlong(Element art, float height = 0,
                         float stationPx = 6.0f) {
  ArtBrush b;
  b.art = std::move(art);
  b.height = height;
  b.stationPx = stationPx;
  b.reach = std::max(32.0f, height);
  return b;
}

} // namespace brushes
} // namespace sigil::compose

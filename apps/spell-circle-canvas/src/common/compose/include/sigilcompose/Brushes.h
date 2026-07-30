#pragma once

/** @file
 * SigilCompose brushes — the LINE vocabulary between elements, applied to
 * the node's outline (a rail's route, a connector's wire, a border) and
 * attached with `.stroke()`. Two families over ONE seam:
 *  - the LAYERED STROKE STACK (widths, colors, blurs, dashes, blends,
 *    bottom-up) — the measured game-linework grammars of REFERENCES.md §5;
 *  - the ILLUSTRATOR PIPELINE model (`Brush`: geometry ops over the path
 *    feeding paint legs) and its archetypes — Scatter, Pattern, Ribbon,
 *    Art.
 *
 * Equality: a brush of comparable parts is a comparable VALUE (defaulted
 * equality → prunes). A mod fn or a width fn is the documented exception —
 * incomparable callables never prune, so memo the host.
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
#include <include/core/SkStrokeRec.h> // filterPath recs
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <include/core/SkVertices.h>
#include <include/effects/SkCornerPathEffect.h>
#include <include/effects/SkDiscretePathEffect.h>
#include <include/effects/SkDashPathEffect.h>

#include <cmath>
#include <functional>
#include <limits>
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

  /** Extra paint reach past the outline, so a cached recording's cull does
   *  not truncate the halo. Declaring nothing meant zero, and the whole
   *  point of an additive stack is that it paints WIDE of the path:
   *  filament() is a 14 px envelope under an 8 px blur, i.e. 31 px of
   *  reach that a node culling at its own bounds simply lost. Per layer,
   *  not per extreme — a wide hard core and a narrow soft halo do not
   *  compound. 3σ covers >99% of a Gaussian. */
  float bleed() const {
    float reach = 0;
    for (const StrokeLayer &layer : layers)
      reach = std::max(reach, layer.width * 0.5f + layer.blurSigma * 3.0f);
    return reach;
  }
  /** The widest layer's full mark (see PathFormat::reach). */
  float reach() const {
    float widest = 0;
    for (const StrokeLayer &layer : layers)
      widest = std::max(widest, layer.width + layer.blurSigma * 3.0f);
    return widest;
  }

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

// The four LayeredBrush PRESETS that used to sit here — filament(),
// circuit(), rope(), pulse() — moved to kit::brush::presets:: in R2
// (kit/Strokes.h) unchanged. They were compositions with craft names
// living in the CORE, under a namespace (`brushes::`) that R3 deleted.
// Their old spellings went with it: `kit::brush::presets::filament` is
// the name, and reaching it means including <sigilcompose/kit/Strokes.h>.

// ---------------------------------------------------------------------------
// The Illustrator brush model — a brush is a PIPELINE: shapers over the
// path (the SkComposePathEffect idea, at our seam), then paint LAYERS that
// INSTANCE real components along the result, each with a programmatic
// per-instance twist. Four Illustrator archetypes map onto three values:
//   Scatter brush  → brush::Scatter (jittered instances + mod fn)
//   Pattern brush  → brush::Pattern (side/corner/start/end tiles,
//                    integer-fit stretch — the Illustrator tile semantics)
//   Calligraphic   → brush::Ribbon (variable-width fill; nib angle)
//   Art brush      → brush::Art (one cell continuously bent along
//                    the contour via SkVertices; `artAlong()`)

// ---------------------------------------------------------------------------
// THE ONE MECHANISM DOOR
//
// `ops::` is what is LEFT of the escape hatch after R3, and it is left
// deliberately. Everything else that lived here — the comparable structs
// `Wave`/`Rounded`/`Sketchy`/`Square`/`Offset`, plus `Brush::op()` and the
// `vector<GeometryOp>` per-layer suffix — was DELETED, because
// `kit::brush::shapers::` now has a twin for every one of them and
// `Brush::layer(dec, {shaper…})` reaches them (ROADMAP §33, R3).
//
// What has NO replacement is the RAW LAMBDA: a `Shaper` requires equality,
// by design, so a one-off closure can never be one. That capability would
// have vanished with nothing to say instead, so it stays — as exactly one
// door, reached through `brush::restyle(op, decoration)`, documented as a
// mechanism and priced as one (it never prunes).

namespace ops {

/** THE ESCAPE HATCH: a path→path geometry op as a raw callable — our
 *  SkPathEffect-shaped extension point, because Skia's own subclassing
 *  seam is sealed in its public API. It can do anything, and it can never
 *  prune: an incomparable callable compares conservatively unequal, so a
 *  node wearing one re-records every render (memo the host, or keep it
 *  pointer-stable).
 *
 *  Reach for it only when no `kit::brush::shapers` value and no shaper you
 *  could write yourself can say what you mean — a shaper is a comparable
 *  struct with `SkPath shape(const SkPath &) const` and writing one is
 *  four lines. Chain lambdas with chain(); apply to any decoration with
 *  `brush::restyle()`, which is the only thing that takes one. */
using PathOp = std::function<SkPath(const SkPath &)>;

/** Dump the path's contour census (count/lengths/closedness/bounds) to
 *  stderr and pass it through unchanged — drop into any pipeline position
 *  when a construction misbehaves. Lowercase and kept: it is a
 *  DIAGNOSTIC, has no capitalised twin to be confused with, and a
 *  pass-through that prints has nothing to prune. */
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

/** Chain escape-hatch ops left-to-right — compose like
 *  SkComposePathEffect. Lowercase and kept for the same reason as
 *  debug(): it is the PathOp family's own combinator, not a duplicate of
 *  a comparable value. Shapers chain by listing them — `Brush::shaped()`
 *  appends, and each `.layer()` carries its own list. */
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

/** What `brush::restyle()` carries: EITHER a comparable `Shaper` (the one
 *  geometry seam, so a restyle of a stock shaper still prunes) OR a raw
 *  `ops::PathOp` (the mechanism door, conservatively unequal forever). A
 *  bare lambda literal must be assigned to an `ops::PathOp` first — two
 *  user-defined conversions do not chain.
 *
 *  Nothing else builds one. `Brush`'s pipeline and its per-layer suffixes
 *  are `Shaper` lists as of R3; the `apply()`-spelled `GeometryScheme`
 *  concept it used to accept was the second word for `shape()` and died
 *  with the `ops::` structs. */
class GeometryOp {
public:
  GeometryOp(ops::PathOp fn) // NOLINT: escape hatch, never prunes
      : m_apply(std::move(fn)) {}
  /** Any shaper VALUE, directly — `restyle(shapers::Wave{...}, dec)`. The
   *  hop through Shaper cannot be implicit (two user-defined conversions
   *  do not chain), so it is spelled here once. */
  template <ShaperScheme S>
  GeometryOp(S scheme) // NOLINT: implicit by design
      : GeometryOp(Shaper(std::move(scheme))) {}
  /** A Shaper IS a geometry op — the seam value under its taught name. */
  GeometryOp(Shaper s) // NOLINT: implicit by design
      : m_bleed(s.bleed()) {
    m_held = s;
    m_equals = [](const std::any &a, const std::any &b) {
      return std::any_cast<const Shaper &>(a) ==
             std::any_cast<const Shaper &>(b);
    };
    m_apply = [held = std::move(s)](const SkPath &p) {
      return held.shape(p);
    };
  }

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


// ---------------------------------------------------------------------------
// THE BRUSH KINDS AND COMPOSITES (ROADMAP §33 stage two)
//
// A brush is what PAINTS. There are exactly four KINDS — the leaf tools —
// and two COMPOSITES, which combine any brushes at all, including other
// composites. That is the whole taxonomy; everything else on this shelf is
// a value built out of it.
//
//   kinds       brush::solid   brush::Pattern   brush::Scatter   brush::Art
//   composites  brush::layers(…)                brush::weave(…)
//
// The KINDS are the types that were already here under mechanism names;
// `brush::` is where they are taught, and the old spellings keep
// compiling (§27). `solid` replaces `PathFormat` — "path format" names
// the implementation, and `pen` was rejected because it implies
// calligraphy, which is a profile, not a kind.

namespace brush {

/** THE plain stroke: a width, a paint, and optional dash/stamp/effect.
 *  Successor to `PathFormat`, which is the same type under its old
 *  mechanism name. */
using Solid = PathFormat;
/** `brush::solid(width, fill[, align])` — the one-line spelling.
 *  Designated initialisers still work through `brush::Solid{…}`. */
inline Solid solid(float width, Fill fill,
                   PathFormat::Align align = PathFormat::Align::Center) {
  Solid s;
  s.width = width;
  s.strokeFill = std::move(fill);
  s.align = align;
  return s;
}

/** One strand of a composite: WHERE it runs and WHAT paints it.
 *
 *  A pair, deliberately — two parallel lists matched by index was the
 *  first shape tried and it reproduced §10d's defect exactly (add a
 *  strand, silently shift every brush). One strand is one value. */
struct Strand {
  StrandPath path;
  Decoration brush;
  bool operator==(const Strand &o) const {
    return path == o.path && brush == o.brush;
  }
};

/** THE COMPOSITE. `brush::weave(...)` and `brush::layers(...)` are two
 *  author intents over this one machine:
 *
 *  **`layers` == `weave` with coincident self-strands.** Coincident
 *  strands produce no crossings, so the rule never fires and list order
 *  applies everywhere — which IS what "fixed order, bottom-up" means.
 *  Both words are kept because they name different intents (the
 *  `alternate` == `sequence({Over, Under})` precedent), and neither is a
 *  special case in the code below.
 *
 *  Composites NEST: any strand's brush may be another composite, so a
 *  braid painted by layers, or a whole braid used as one strand of a
 *  bigger weave, needs no new vocabulary. */
struct Weave {
  std::vector<Strand> strands;
  /** How discovered crossings resolve. Default is list order — see
   *  CrossingRule. There is ONE of these; pins go on it via
   *  `.except(i, order)`, never as stacked entries. */
  CrossingRule crossing;
  /** Override the mark half-width the repair region is built from, in px.
   *  0 (the default) asks each strand's brush — `Decoration::reach()` —
   *  which is the right answer for everything that reports one.
   *
   *  A strand whose brush is a bare PaintProgram reports reach 0 (it has no
   *  width to declare), and the repair clamps that to a 2 px tube — far too
   *  thin for most custom marks. **Set `patch` explicitly on a weave whose
   *  strands are custom programs.**
   *
   *  WHAT THE REPAIR DOES, honestly: for every crossing the rule decides
   *  against list order, the over-strand is repainted through the region
   *  where the two marks overlap (`crossingPatch`), bounded by THE KNOT'S
   *  OWN TERRITORY — half the arc distance to its nearest neighbouring
   *  crossing on the tighter of the two strands (measured around the
   *  cycle, on a closed strand).
   *
   *  That bound is not a margin, it is what keeps a braid a braid: without
   *  it the neighbouring overlap regions touch, merge into one, and the
   *  first crossing's patch owns the whole run. The cost is that a repair
   *  reaches only half way to the next knot. So with OPAQUE strand brushes
   *  the repair is exact WHERE A CROSSING HAS ROOM — and adjacent shallow
   *  crossings each own only half the distance between them, so the
   *  under-strand can show between two close knots.
   *
   *  With TRANSLUCENT strands it double-covers: the over-strand's alpha is
   *  composited twice inside the patch, so the crossing reads darker than
   *  the strand does elsewhere. That is not a bug in the patch size — it is
   *  the patch MODEL, and it is one of the two named hard cases ROADMAP §33
   *  pins for the element-level crossover pass (the other being several
   *  crossings over one region). **Weaves want opaque inks until that pass
   *  lands.** */
  float patch = 0.0f;

  bool operator==(const Weave &o) const {
    return strands == o.strands && crossing == o.crossing &&
           patch == o.patch;
  }
  bool isAnimated() const {
    for (const Strand &s : strands)
      if (s.brush.isAnimated())
        return true;
    return false;
  }
  float bleed() const {
    float worst = 0;
    for (const Strand &s : strands)
      worst = std::max(worst, s.path.reach() + s.brush.bleed());
    return worst;
  }
  /** The widest mark any strand paints, off its own path. */
  float reach() const {
    float worst = 0;
    for (const Strand &s : strands)
      worst = std::max(worst, s.path.reach() + s.brush.reach());
    return worst;
  }
  /** Forwarded so the element can register the derive borrows without
   *  looking inside a type-erased brush (BorrowingDecoration). */
  std::vector<std::string> borrows() const {
    std::vector<std::string> keys;
    for (const Strand &s : strands) {
      if (s.path.source() == StrandPath::Source::Borrowed)
        keys.push_back(s.path.key());
      for (const std::string &nested : s.brush.borrows())
        keys.push_back(nested);
    }
    return keys;
  }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    if (strands.empty())
      return;
    // 1. Resolve every strand's geometry. A relative strand is a
    //    displacement of the boundary in the (along, across) frame the
    //    band owns; an absolute one brings its own path — and if NO
    //    strand is relative, the boundary is simply an unpainted host.
    std::vector<SkPath> paths;
    paths.reserve(strands.size());
    for (const Strand &s : strands) {
      switch (s.path.source()) {
      case StrandPath::Source::Relative:
        paths.push_back(s.path.profile().max() == 0.0f
                            ? ctx.outline
                            : profileOffset(ctx.outline, s.path.profile()));
        break;
      case StrandPath::Source::Borrowed:
        paths.push_back(ctx.borrowedPath(s.path.key()));
        break;
      case StrandPath::Source::Authored:
        paths.push_back(s.path.path());
        break;
      }
    }

    const auto paintStrand = [&](size_t i) {
      const PaintContext sub{ctx.size,        paths[i],
                             ctx.elapsedSeconds, ctx.contentScale,
                             ctx.animating,   ctx.fonts,
                             ctx.borrowed};
      strands[i].brush.paint(c, sub);
    };

    // 2. List order first — the whole picture, correct wherever nothing
    //    crosses, which is every layers() and most of any weave.
    for (size_t i = 0; i < paths.size(); ++i)
      paintStrand(i);

    // 3. Repair the crossings the rule disagrees with. Crossings are
    //    DISCOVERED, never authored.
    const std::vector<Crossing> crossings = discoverCrossings(paths);
    if (crossings.empty())
      return;
    const auto reachOf = [&](size_t i) {
      // The MARK's full width, not the cull's bleed(): an Align::Inner
      // stroke bleeds zero while painting a mark `width` wide, and a
      // region derived from bleed() was measurably too small.
      return patch > 0 ? patch : std::max(strands[i].brush.reach(), 1.0f);
    };

    // Each strand's arc length, so a crossing's `along` fractions convert to
    // px — which is what bounds one knot's patch away from its neighbours'.
    // Also whether the strand is a CYCLE, because on a closed contour the
    // fractions 0.02 and 0.98 are neighbours, not opposites.
    std::vector<float> lengths(paths.size(), 0.0f);
    std::vector<char> cyclic(paths.size(), 0);
    for (size_t i = 0; i < paths.size(); ++i) {
      SkContourMeasureIter it(paths[i], false);
      int contours = 0;
      bool lastClosed = false;
      while (sk_sp<SkContourMeasure> m = it.next()) {
        lengths[i] += m->length();
        lastClosed = m->isClosed();
        ++contours;
      }
      // ONE closed contour, and no more: with several contours the `along`
      // parameter runs them end to end, so its two ends are not adjacent
      // and wrapping would be a lie.
      cyclic[i] = (contours == 1 && lastClosed) ? 1 : 0;
    }
    // THE KNOT'S TERRITORY: half the arc distance to the nearest adjacent
    // crossing, on either strand, whichever is closer. Without it the
    // lenses of an ordinary braid touch, pathops merges them into ONE
    // contour, and crossing 0's patch owns the whole run — the weave then
    // reads as a single strand laid on top of the others.
    const auto positionOn = [](const Crossing &x, size_t strandIndex) {
      return x.a == strandIndex ? x.alongA : x.alongB;
    };
    const auto territoryOf = [&](const Crossing &x) {
      float limit = std::numeric_limits<float>::max();
      for (const size_t s : {x.a, x.b}) {
        const float mine = positionOn(x, s);
        for (const Crossing &other : crossings) {
          if (&other == &x || (other.a != s && other.b != s))
            continue;
          float delta = std::abs(positionOn(other, s) - mine);
          // On a CYCLE the seam is not a boundary: two knots at 0.02 and
          // 0.98 sit 4% apart, not 96%. Without this, crossings straddling
          // the seam read as maximally distant, the bound vanishes, and
          // the lenses merge again — two overlapping rings put both knots
          // in one patch and painted the whole thing in one colour.
          //
          // Conditional on closedness, because wrapping an OPEN strand
          // whose crossings sit near its two ends would over-clip: those
          // ends really are far apart.
          if (cyclic[s])
            delta = std::min(delta, 1.0f - delta);
          const float gap = delta * lengths[s];
          if (gap > 0.01f)
            limit = std::min(limit, gap * 0.5f);
        }
      }
      // No neighbour on either strand: the lens needs no bound, and a
      // number large enough to contain it is the honest spelling of that.
      if (limit == std::numeric_limits<float>::max())
        limit = 1e6f;
      return limit;
    };

    for (const Crossing &x : crossings) {
      const Order order = crossing.decide(x);
      const size_t top = order == Order::Over ? x.a : x.b;
      // `b` painted later, so it is already on top. Nothing to do.
      if (top == x.b)
        continue;
      c.save();
      c.clipPath(crossingPatch(paths[x.a], reachOf(x.a), paths[x.b],
                               reachOf(x.b), x.at, territoryOf(x)),
                 true);
      paintStrand(top);
      c.restore();
    }
  }
};

/** FIXED ORDER, bottom-up: the first brush paints first, the last on top.
 *  Formally a weave of coincident self-strands (see Weave), which is why
 *  double and triple lines are `layers` plus offset shapers and never
 *  element duplication. */
inline Weave layers(std::vector<Decoration> stack) {
  Weave w;
  w.strands.reserve(stack.size());
  for (Decoration &d : stack)
    w.strands.push_back(Strand{strand::self(), std::move(d)});
  return w;
}
/** PER-CROSSING order: strands that may trade sides, and a rule for who
 *  passes over whom where they meet. */
inline Weave weave(std::vector<Strand> strands,
                   CrossingRule rule = crossing::alternate()) {
  Weave w;
  w.strands = std::move(strands);
  w.crossing = std::move(rule);
  return w;
}

} // namespace brush

/** THE BRUSH: one composable value — a geometry PIPELINE over the outline
 *  (shapers applied in order, the SkComposePathEffect idea as data)
 *  feeding ordered paint LAYERS (any Decoration: a lines::Line, a
 *  LayeredBrush stack, Scatter/Pattern instancing, a Ribbon, a raw
 *  PathFormat). The Illustrator model, closed under composition:
 *
 *    element.stroke(Brush{}
 *        .shaped(kit::brush::shapers::Rounded{6})
 *        .shaped(kit::brush::shapers::Wave{.amplitude = 3, .wavelength = 30})
 *        .layer(lines::cased(3, ink, 5))
 *        .layer(brush::Scatter{.art = spark(), .spacing = 40}));
 *
 *  A Brush of comparable shapers and layers is itself comparable — the
 *  whole styled connector prunes and caches as ONE value. Animated layers
 *  declare volatility through; bleed aggregates pipeline reach + layer
 *  reach.
 *
 *  `layer()`, not `leg()` (ROADMAP §33 ruling 14): a Brush's stacked
 *  marks are the same idea as `brush::layers(...)`, the fixed-order
 *  composite, the way a strand is the unit of a weave. `leg` named a
 *  mechanism nobody else in the grammar used. */
struct Brush {
  /** One paint layer: a Decoration plus its own pipeline SUFFIX (applied
   *  after the shared pipeline, this layer only) — the asymmetric-casing
   *  ask: one Brush reads as one material ("road with lane and curb"),
   *  each side riding its own `shapers::Offset`. */
  struct Layer {
    Decoration dec;
    std::vector<Shaper> shapers;
    bool operator==(const Layer &o) const {
      return dec == o.dec && shapers == o.shapers;
    }
  };

  std::vector<Shaper> pipeline;
  std::vector<Layer> layers;

  /** THE geometry-deviation seam: any comparable value with
   *  `SkPath shape(const SkPath &) const`. Stock shapers are kit values
   *  (`kit::brush::shapers::wave/jitter/offset`), peers of anything you
   *  write — there is deliberately no sugar method over this. */
  Brush &shaped(Shaper s) {
    pipeline.push_back(std::move(s));
    return *this;
  }
  /** One mark in the stack, bottom-up, with an optional shaper SUFFIX
   *  that deviates this layer's geometry only.
   *
   *  The suffix takes shapers, the same seam `shaped()` takes — before R3
   *  it took `GeometryOp`, which is why `ops::` had to stay public for a
   *  phase (a kit shaper could not reach a `vector<GeometryOp>`: two
   *  user-defined conversions do not chain). For a raw incomparable
   *  lambda, wrap the layer's decoration in `brush::restyle(op, dec)` —
   *  that is the one deliberate mechanism door and it is documented as
   *  one. */
  Brush &layer(Decoration d, std::vector<Shaper> suffix = {}) {
    layers.push_back(Layer{std::move(d), std::move(suffix)});
    return *this;
  }

  bool operator==(const Brush &o) const {
    return pipeline == o.pipeline && layers == o.layers;
  }
  bool isAnimated() const {
    for (const Layer &l : layers)
      if (l.dec.isAnimated())
        return true;
    return false;
  }
  /** The widest mark any layer paints, plus the pipeline's own reach. */
  float reach() const {
    float shared = 0;
    for (const Shaper &g : pipeline)
      shared += g.bleed();
    float worst = 0;
    for (const Layer &l : layers) {
      float layerReach = l.dec.reach();
      for (const Shaper &g : l.shapers)
        layerReach += g.bleed();
      worst = std::max(worst, layerReach);
    }
    return shared + worst;
  }
  /** A layer may be a composite that borrows keyed paths; forward them so
   *  the element registers the derive borrow (BorrowingDecoration). */
  std::vector<std::string> borrows() const {
    std::vector<std::string> keys;
    for (const Layer &l : layers)
      for (const std::string &k : l.dec.borrows())
        keys.push_back(k);
    return keys;
  }
  float bleed() const {
    float shared = 0;
    for (const Shaper &g : pipeline)
      shared += g.bleed(); // pipeline reaches compound (offset THEN wave)
    float worst = 0;
    for (const Layer &l : layers) {
      float layerReach = l.dec.bleed();
      for (const Shaper &g : l.shapers)
        layerReach += g.bleed();
      worst = std::max(worst, layerReach);
    }
    return shared + worst;
  }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPath styled = ctx.outline;
    for (const Shaper &g : pipeline)
      styled = g.shape(styled);
    for (const Layer &l : layers) {
      SkPath layerPath = styled;
      for (const Shaper &g : l.shapers)
        layerPath = g.shape(layerPath);
      const PaintContext restyled{ctx.size,        std::move(layerPath),
                                  ctx.elapsedSeconds, ctx.contentScale,
                                  ctx.animating,   ctx.fonts,
                                  ctx.borrowed};
      l.dec.paint(c, restyled);
    }
  }
};

namespace brush {

/** Run a geometry pipeline, then paint `inner` on the restyled outline —
 *  any decoration (LayeredBrush, lines::Line, PathFormat…) gains waves,
 *  jitter, rounding without knowing.
 *
 *  THE ONE MECHANISM DOOR (ROADMAP §33, R3). It takes a `GeometryOp`, so
 *  a comparable shaper value (`kit::brush::shapers::Wave{...}`, or one you
 *  wrote) and a raw `ops::PathOp` lambda both spell it — and the lambda
 *  has nowhere else to go, which is the whole reason `ops::` survived the
 *  deletion. The WRAPPER is incomparable either way — it has no
 *  operator== — so memo the host node (or keep it pointer-stable) to
 *  prune, whichever op you hand it. Prefer `Brush::shaped(value)` when a
 *  shaper can say it: that prunes. */
struct Restyled {
  GeometryOp op;
  Decoration inner;
  float extraBleed = 8.0f; // the op's own overhang (wave amplitude…)

  bool isAnimated() const { return inner.isAnimated(); }
  float bleed() const { return inner.bleed() + extraBleed; }
  float reach() const { return inner.reach(); }
  /** Forwarded, or a wrapped weave's strand::from(key) would never be
   *  registered for the derive pass (BorrowingDecoration). */
  std::vector<std::string> borrows() const { return inner.borrows(); }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    // GeometryOp::apply passes the path through when it holds nothing,
    // which is what the old `op ? op(outline) : outline` guard bought.
    PaintContext restyled{ctx.size,        op.apply(ctx.outline),
                          ctx.elapsedSeconds, ctx.contentScale,
                          ctx.animating,   ctx.fonts,
                          ctx.borrowed};
    inner.paint(c, restyled);
  }
};

inline Restyled restyle(GeometryOp op, Decoration inner,
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
  /** px, or contour fraction when ≤ 1. UNSET means "take the host brush's
   *  own spacing" — `Scatter::spacing` resolves it. It is an optional
   *  and not a defaulted float because the default WAS 24, compared against
   *  24 to detect "unset", so an author writing `.interval = 24` got
   *  `spacing` instead of the number they typed, silently (audit I8). An
   *  optional cannot be spelled by accident. */
  std::optional<float> interval;
  float offset = 0.0f; ///< leading phase for Interval (same units)
  bool operator==(const Placement &) const = default;
};

namespace detail {
/** Resolve a Placement into concrete samples (position + tangent).
 *  @param spacing the host brush's spacing — what an UNSET `interval`
 *         resolves to (the sugar; see Placement::interval). Passed in
 *         rather than defaulted here because only the brush owns it. */
inline std::vector<PathSample> placementSamples(const SkPath &path,
                                                const Placement &p,
                                                float spacing) {
  std::vector<PathSample> out;
  using Mode = Placement::Mode;
  if (p.mode == Mode::Interval || p.mode == Mode::CentralPoint) {
    const float interval = p.interval.value_or(spacing);
    SkContourMeasureIter iter(path, false);
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float len = contour->length();
      const float step =
          interval <= 1.0f ? len * std::max(interval, 0.001f) : interval;
      const float phase =
          p.offset <= 1.0f && p.offset >= -1.0f && p.mode == Mode::Interval &&
                  interval <= 1.0f
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
/** THE corner hit type lives in Lines.h now — there was one per scanner,
 *  and they described the same thing. */
using CornerHit = sigil::compose::lines::detail::CornerHit;

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
 *  the value incomparable (memo the host).
 *
 *  THE CACHE IN THIS VALUE IS THE FALLBACK (§16, closed — as in
 *  Pattern): inside a composer the bake lives in the INSTANCE's
 *  StampCache, handed in via PaintContext::stamps and keyed on the
 *  art's node with a weak guard, so a brush value rebuilt by every
 *  describe finds its art's bake instead of re-rastering it. What
 *  still re-bakes is a NEW ART NODE each describe: keep the art
 *  Element pointer-stable (a member, a static, a captured value) —
 *  its node is the cache key. This member cache serves standalone
 *  paints (no composer, no PaintContext::stamps). */
struct Scatter {
  Element art;
  float spacing = 24.0f; ///< Interval-mode sugar (px, or fraction ≤ 1)
  /** Full placement grammar — set `place.mode` for Vertex/SegmentCenter/
   *  CentralPoint… families; `spacing` feeds Interval when place is
   *  `interval` is unset. */
  Placement place{};
  uint32_t seed = 0; ///< 0 = a regular run, no jitter roll
  float jitterAlong = 0, jitterNormal = 0; ///< ±px
  float jitterScale = 0;                   ///< ±fraction of 1
  float jitterRotateDeg = 0;               ///< ±deg
  bool alignToPath = true;
  float reach = 32.0f; ///< cull reserve: half the art's extent + jitter
  StampModFn mod;
  bool animatedMod = false; ///< mod reads time → repaint per frame

  bool isAnimated() const { return animatedMod; }
  float bleed() const { return reach; }
  bool operator==(const Scatter &o) const {
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
    // §16: prefer the instance-side store — a brush value rebuilt every
    // describe finds its art's bake there; the value member remains the
    // standalone-paint fallback.
    sk_sp<SkPicture> pic;
    if (ctx.stamps) {
      if (const StampCache::Entry *e = ctx.stamps->get(art.node()))
        pic = e->pic;
      if (!pic) {
        // shell box: snapshot ignores the ROOT's own dims
        pic = snapshot(box().child(art), *ctx.fonts);
        ctx.stamps->put(art.node(), {pic, nullptr, {0, 0}});
      }
    } else {
      if (!cache->pic || cache->bakedFor != art.node().get()) {
        cache->pic = snapshot(box().child(art), *ctx.fonts);
        cache->bakedFor = art.node().get();
      }
      pic = cache->pic;
    }
    if (!pic)
      return;

    // An unset place.interval takes `spacing` — the sugar, resolved where
    // the spacing lives rather than by comparing against a sentinel value
    // an author could type (audit I8).
    std::vector<PathSample> samples =
        detail::placementSamples(ctx.outline, place, spacing);
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
      detail::drawStamp(c, *pic, samples[i], alignToPath, 0, 1, 1, m);
    }
  }
};

/** Which way a corner tile faces. **There is no default**, and that is
 *  the whole design: it is not a preference, it is a statement about what
 *  the art LOOKS LIKE, and the library cannot see the art.
 *
 *  WHICH ONE:
 *
 *  - `Bisector` — for an ORNAMENT: art symmetric about its own bisector,
 *    drawn once and serving all four corners of a frame. A fleuron, a
 *    rosette, a bracket.
 *  - `Outgoing` — for anything with a distinguishable ENTRY and EXIT: an
 *    elbow of pipe, a flow tick, an arrow turning a corner, a cross whose
 *    arms are meant to lie along the edges. This class is not exotic — it
 *    is **two of the five corner consumers in this corpus, and both of
 *    them shipped broken.**
 *
 *  Bisector does NOT buy you one art instead of two. The arms of a
 *  bisector-aligned tile sit at `(turn/2, 180 − turn/2)` off the bisector
 *  and mirror with the SIGN of the turn, so a handed ornament costs two
 *  drawings either way.
 *
 *  ---
 *  **CHANGELOG — if your art predates `f706f5d` (2026-07-22 12:03), IT IS
 *  ALIGNED WRONG AND YOU MUST ASK FOR `Outgoing`.**
 *
 *  Before that commit the bisector was computed by re-probing at d±2 from
 *  a point already past the vertex, so both probes landed on the SAME leg
 *  and every corner in every study behaved as `Outgoing` — not as a
 *  choice, as a bug. Art authored then is authored in the outgoing frame.
 *  `f706f5d` fixed the probe and added an alignment field defaulting to
 *  `Bisector`, and the two correct halves together silently re-aimed every
 *  corner stamp in the corpus by half the turn angle. It was
 *  source-compatible, warned nothing, and edited no file its victims
 *  owned. (The commit's subject line is about caching, so
 *  `git log --oneline` gives no hint either.)
 *
 *  Two studies shipped visibly wrong through review. The worst case of the
 *  class is structural and worth knowing: **every corner of an annular
 *  sector is a right angle**, so the bisector sits 45° from BOTH legs —
 *  and a shape with 90° symmetry (a Greek cross) is therefore
 *  corner-agnostic under `Outgoing` and uniformly, maximally wrong under
 *  `Bisector`. Twenty-eight crosses had been twenty-eight saltires.
 *
 *  **TO AUDIT A STUDY, IN THIRTY SECONDS:** set the other value in a
 *  scratch copy, re-render the same `--at`, and diff. If nothing moves,
 *  the art is rotationally forgiving and the study is *proved* clean; if
 *  corners snap, it was broken. Render a third variant with no corner art
 *  to mask the stamps, and you can MEASURE the rotation instead of judging
 *  it. Judging it by eye failed twice on the same plate. */
enum class CornerAlign { Bisector, Outgoing };

/** CORNER ART AND HOW IT IS AIMED — one value, because the second half is
 *  not optional information about the first.
 *
 *  This is ROADMAP §27's own conclusion, finally landed (§33 ruling 8):
 *  *"a required constructor argument would be better and costs a source
 *  break at all five consumers."* A `std::optional<CornerAlign>` beside
 *  the art could only WARN at paint time, which is a diagnostic for a
 *  mistake the type system can refuse outright. There is no default
 *  constructor and no default member initializer: you cannot hand this
 *  brush a corner tile without saying which way it faces.
 *
 *      pb.corner = brush::CornerArt{elbow, brush::CornerAlign::Outgoing};
 */
struct CornerArt {
  Element art;
  CornerAlign align;
  CornerArt(Element artIn, CornerAlign alignIn)
      : art(std::move(artIn)), align(alignIn) {}
  bool operator==(const CornerArt &o) const {
    return art.node() == o.art.node() && align == o.align;
  }
};

/** The PATTERN brush (Illustrator tile semantics): a SIDE tile repeated an
 *  INTEGER number of times per run and stretched along the tangent to fit
 *  exactly (never a torn tile at the end); optional CORNER tiles where the
 *  tangent breaks by more than `cornerAngleDeg` (placed on the bisector,
 *  or on the outgoing leg — `CornerArt` carries the choice and requires
 *  it); optional START/END tiles on open contours. Runs are the stretches
 *  between corners. An art brush is the one-tile degenerate case. */
struct Pattern {
  Element side;
  std::optional<Element> start, end;
  /** Corner tiles, and their alignment — see CornerArt. Absent means the
   *  runs simply meet at the break. */
  std::optional<CornerArt> corner;
  float advance = 0;           ///< tile length along the path (0 → intrinsic)
  /** PER-SAMPLE tangent break — gently ROUNDED corners intentionally take
   *  no corner tile (no hard break exists).
   *
   *  35° here against 30° in the other corner scanners is a deliberate
   *  FREEZE, not a drift: a default that encodes a judgement about the
   *  caller's art cannot be changed compatibly, because the test is
   *  whether any existing caller's OUTPUT changes (ROADMAP §27). */
  float cornerAngleDeg = 35.0f;
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

  bool isAnimated() const { return animatedMod; }
  float bleed() const { return reach; }
  bool operator==(const Pattern &o) const {
    auto node = [](const std::optional<Element> &e) {
      return e ? e->node().get() : nullptr;
    };
    return side.node() == o.side.node() && node(start) == node(o.start) &&
           node(end) == node(o.end) && corner == o.corner &&
           advance == o.advance && cornerAngleDeg == o.cornerAngleDeg &&
           cornerLength == o.cornerLength &&
           stretchToFit == o.stretchToFit && reach == o.reach && !mod &&
           !o.mod && animatedMod == o.animatedMod;
  }

  /** The baked tile art. Keyed on the art Element's node POINTER, which is
   *  what makes the next paragraph a trap.
   *
   *  THE CACHE IN THIS VALUE IS THE FALLBACK (§16, closed): inside a
   *  composer the bakes live in the INSTANCE's StampCache, handed in via
   *  PaintContext::stamps and keyed on the art's node with a weak guard —
   *  so a brush value rebuilt by every describe (the renderSlot() trap
   *  that once cost eighteen snapshot() passes per frame) finds its
   *  art's bake instead of re-rastering it. What still re-bakes is a
   *  NEW ART NODE each describe: keep the art Element pointer-stable
   *  (a member, a static, a captured value) — its node is the cache
   *  key. This member cache serves standalone paints (no composer, no
   *  PaintContext::stamps): copies share it; fresh values start empty. */
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
    const void *cornerNode = corner ? corner->art.node().get() : nullptr;
    if (cache->bakedSide != sideNode || cache->bakedStart != startNode ||
        cache->bakedEnd != endNode || cache->bakedCorner != cornerNode) {
      *cache = Cache{};
      cache->bakedSide = sideNode;
      cache->bakedStart = startNode;
      cache->bakedEnd = endNode;
      cache->bakedCorner = cornerNode;
    }
    // §16: each slot warms from the instance-side store first, so a
    // Pattern value rebuilt every describe (empty member cache) reuses
    // its arts' bakes; misses bake once and publish back.
    auto bake = [&](const Element &e, sk_sp<SkPicture> &slot) {
      if (slot)
        return;
      if (ctx.stamps)
        if (const StampCache::Entry *hit = ctx.stamps->get(e.node()))
          slot = hit->pic;
      if (!slot) { // shell box: snapshot ignores the ROOT's own dims
        slot = snapshot(box().child(e), *ctx.fonts);
        if (ctx.stamps && slot)
          ctx.stamps->put(e.node(), {slot, nullptr, {0, 0}});
      }
    };
    bake(side, cache->side);
    if (start)
      bake(*start, cache->start);
    if (end)
      bake(*end, cache->end);
    if (corner)
      bake(corner->art, cache->corner);
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
      //
      // The scan STRADDLES the vertex — it compares the tangent at d-step
      // with the tangent at d, so the break is first seen one step AFTER
      // the bend. Taking the midpoint of that bracket put the corner art
      // up to half a step off the actual vertex (measured: a bend at
      // (96, 240) drew its corner box centred on (98.5, 239.5)), and it
      // also broke the bisector: probing d±2 from a point already past
      // the vertex lands on the SAME leg twice, so every corner rotated
      // to the outgoing tangent. On a rectangle three corners came out
      // one way and the fourth — the seam, whose probes wrap — 45° off.
      //
      // So bisect the bracket instead of guessing inside it, and keep the
      // two leg tangents the bisection converged on. Eight halvings take
      // a 6 px step under 0.03 px, which is below the rasterizer's own
      // resolution, and the bisector is then exact by construction rather
      // than by a 2 px probe that assumes both legs are longer than 2 px.
      // ONE corner scanner (lines::detail::findCorners). This used to be a
      // second copy of the same bisecting search, with its own 35 degree
      // default and no diagnostic — so the same shape got different corners
      // depending on which decoration asked, and the n-gon warning added
      // for borders never reached a pattern brush.
      std::vector<detail::CornerHit> corners;
      if (cache->corner)
        corners = sigil::compose::lines::detail::findCorners(
            *contour, cornerAngleDeg, tileLen * 0.5f,
            std::clamp(tileLen * 0.25f, 1.0f, 6.0f));

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
      for (const detail::CornerHit &hit : corners)
        if (hit.d > head && hit.d < len - tail) {
          bounds.push_back(hit.d - halfCorner); // run ends before the corner
          bounds.push_back(hit.d + halfCorner); // next run starts after it
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

      // Corner tiles sit on the bisector of the break — the two leg
      // tangents came out of the detection, so no re-probing is needed.
      // No diagnostic here any more: the alignment is a REQUIRED
      // constructor argument of CornerArt (§27's own conclusion, §33
      // ruling 8), so "corner art with no stated alignment" is a state
      // that cannot be described.
      if (cache->corner)
        for (const detail::CornerHit &hit : corners) {
          SkPoint pos;
          if (!contour->getPosTan(hit.d, &pos, nullptr))
            continue;
          SkVector dir{hit.in.x() + hit.out.x(), hit.in.y() + hit.out.y()};
          // A hairpin's legs cancel: in + out ≈ 0 and atan2(0,0) is a
          // silent zero rotation. Fall back to the outgoing leg.
          if (dir.length() < 1e-3f || corner->align == CornerAlign::Outgoing)
            dir = hit.out;
          caps.push_back({{pos, dir, hit.d, len > 0 ? hit.d / len : 0},
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
 *  model), or any `Profile` on the shared width seam. */
struct Ribbon {
  Fill fill = Fill::color({1, 1, 1, 1});
  float widthStart = 10.0f, widthEnd = 2.0f;
  float nibAngleDeg = -1.0f;  ///< ≥0 → calligraphic (widthStart = full)
  float nibContrast = 0.15f;  ///< thinnest fraction at nib-aligned tangents
  float step = 3.0f; // clamped ≥ 0.5px at paint (0 would never advance)

  /** THE WIDTH LAW, on the shared PROFILE seam — and what replaced the
   *  deleted `widthFn`/`widthMax` pair (ROADMAP §33, the widthFn→Profile
   *  note).
   *
   *  A `Profile` is `float across(float along)` plus a REQUIRED
   *  `float max()` plus EQUALITY, so setting it closes both holes the old
   *  callable pair left open at once: `bleed()` asks how far the mark
   *  reaches instead of trusting a second field nobody set (the
   *  §25/audit-I9 silent-clip trap, now structurally impossible), and the
   *  reconciler compares two ribbons instead of declaring every
   *  varying-width ribbon unequal forever — a `widthFn` ribbon could
   *  never prune, so its whole band re-recorded every frame.
   *
   *  `across(along)` is the FULL width at that fraction of the spine, the
   *  same value `band(spine, across(...))` reads — one vocabulary for a
   *  band's taper, a strand's displacement and a ribbon's width.
   *
   *  **A law that must not slide under a reveal is keyed in PX**: give the
   *  scheme `static constexpr bool alongIsPx = true` and `across` is
   *  handed arc-length px from the spine's start instead of a fraction.
   *  Under `trim()`/`spans::upTo` the decoration receives the REVEALED
   *  contour, so a fraction is a fraction of what has been drawn so far
   *  and a fraction-keyed law walks along the mark as it writes; px does
   *  not move — provided the reveal is anchored at the spine's start
 *  (upTo/range-from-0); a moving-begin window or wrap measures px from
 *  the revealed piece's start. See `PxKeyedProfileScheme`.
   *
   *  GEOMETRY: a profiled ribbon is `bandRegion()`, so its rails go
   *  through `profileOffset` — a CONSTANT profile picks up
   *  `lines::offsetAcross`'s real-vertex corner repair (arc outside a
   *  turn, miter inside) instead of the spur the sample-and-displace walk
   *  below leaves on the inside of every rectangle corner; a VARYING one
   *  is sampled per rail at a uniform 2 px and zipped by arc length.
   *
   *  Default-constructed means ABSENT: the nib, then the
   *  widthStart→widthEnd taper apply. */
  Profile width;

  /** Is the profile seam in use? (A default-constructed Profile compares
   *  equal to itself — see Profile::operator== — so this is the honest
   *  presence test, and a zero-width profile paints nothing either way.) */
  bool hasProfile() const { return !(width == Profile{}); }

  float bleed() const {
    if (hasProfile())
      return width.max();
    return std::max(widthStart, widthEnd);
  }
  bool operator==(const Ribbon &o) const {
    return fill == o.fill && widthStart == o.widthStart &&
           widthEnd == o.widthEnd && nibAngleDeg == o.nibAngleDeg &&
           nibContrast == o.nibContrast && step == o.step && width == o.width;
  }

  void paint(SkCanvas &c, const PaintContext &ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    if (fill.kind == Fill::Kind::Color)
      p.setColor4f(fill.colorValue, nullptr);
    else if (fill.kind == Fill::Kind::Shader)
      p.setShader(fill.shaderValue);

    if (hasProfile()) {
      // One geometry with band(): the region between the two profile
      // rails, per contour, with proper joins.
      const SkPath region =
          bandRegion(ctx.outline, Across{width}, Formation::Centered);
      if (!region.isEmpty())
        c.drawPath(region, p);
      return;
    }

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
        if (nibAngleDeg >= 0) {
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
 *  long gentle paths).
 *
 *  THE CACHE IN THIS VALUE IS THE FALLBACK (§16, closed — as in
 *  Pattern): inside a composer the 2x bake lives in the INSTANCE's
 *  StampCache, handed in via PaintContext::stamps and keyed on the
 *  art's node with a weak guard, so a brush value rebuilt by every
 *  describe finds its art's bake — the most expensive of the three
 *  to miss. What still re-bakes is a NEW ART NODE each describe:
 *  keep the art Element pointer-stable (a member, a static, a
 *  captured value) — its node is the cache key. This member cache
 *  serves standalone paints (no composer, no PaintContext::stamps). */
struct Art {
  Element art;
  float height = 0;        ///< ribbon height (0 → the art's intrinsic)
  float stationPx = 6.0f;  ///< arc-length between strip stations
  float reach = 32.0f;     ///< cull reserve: half height + art overhang

  bool isAnimated() const { return false; }
  float bleed() const { return reach; }
  bool operator==(const Art &o) const {
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
      // §16: the most expensive of the three bakes (2x raster) — the
      // instance-side store is consulted before any raster work.
      if (ctx.stamps) {
        if (const StampCache::Entry *hit = ctx.stamps->get(art.node());
            hit && hit->image) {
          cache->image = hit->image;
          cache->artSize = hit->artSize;
        }
      }
    }
    if (!cache->image) {
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
      if (ctx.stamps && cache->image)
        ctx.stamps->put(art.node(), {nullptr, cache->image, cache->artSize});
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
inline Art artAlong(Element art, float height = 0,
                         float stationPx = 6.0f) {
  Art b;
  b.art = std::move(art);
  b.height = height;
  b.stationPx = stationPx;
  b.reach = std::max(32.0f, height);
  return b;
}

} // namespace brush

// ---------------------------------------------------------------------------
// `namespace brushes` is GONE (R3). Everything it held lives in `brush::`
// under the taught name — the kinds are `brush::Pattern`/`Scatter`/`Art`/
// `Ribbon`, not `PatternBrush`/`ScatterBrush`/`ArtBrush`, because a type
// suffixed with its own namespace was the two-names-for-one-identity
// defect (§22) the deletion phase exists to remove.

namespace brush {
/** A Ribbon on the PROFILE seam — the taught constructor, because the
 *  profile is the half of a ribbon that has a shared vocabulary. */
inline Ribbon ribbon(Profile width, Fill fill) {
  Ribbon r;
  r.width = std::move(width);
  r.fill = std::move(fill);
  return r;
}
} // namespace brush

} // namespace sigil::compose

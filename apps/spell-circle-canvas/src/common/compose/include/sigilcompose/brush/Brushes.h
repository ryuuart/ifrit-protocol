#pragma once

/** @file
 * SigilCompose brushes — THE BRUSH AND ITS COMPOSITES. Everything in the
 * brush family paints `PaintContext::outline`, whatever produced it (a
 * node's shape, a rail's route, a connector's wire), and attaches with
 * `.stroke()`; this header is the value that GATHERS the marks.
 *
 * Two families over one seam:
 *  - the LAYERED STROKE STACK (`LayeredBrush`, in
 *    <sigilcompose/brush/Layered.h>): several passes over the same path
 *    with their own widths, colours, blurs, dashes and blend modes,
 *    painted bottom-up — how an additive glow or a multi-tier circuit
 *    trace is built.
 *  - the PIPELINE model (`Brush`): geometry shapers over the path feeding
 *    ordered paint layers, with the leaf kinds on the two shelves beside
 *    this file — the STAMPED kinds in <sigilcompose/brush/Stamps.h>
 *    (`brush::Scatter`, `brush::Pattern`) and the SWEPT ones in
 *    <sigilcompose/brush/Ribbons.h> (`brush::Ribbon`, `brush::Art`) — and
 *    the composites here, `brush::layers` and `brush::weave`, which may
 *    contain any brush at all including each other.
 *
 * EQUALITY IS THE THING TO WATCH. A brush assembled from comparable parts
 * is itself a comparable value, so a styled connector prunes and caches as
 * one value. Any raw callable in it — a `StampModFn`, a
 * `geometry::path::ops::PathOp` — makes it conservatively unequal forever,
 * so its node re-patches on every describe; memo the host node, or keep the
 * value itself alive rather than rebuilding it.
 *
 * Two numbers every brush declares, and they are not the same: `bleed()` is
 * how far paint escapes the outline, which grows a cached recording's cull;
 * `reach()` is how wide the MARK is in total, which is what a repair region
 * has to cover. Under-reporting either truncates or thins silently.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <sigilcompose/brush/Decorations.h>  // PathSample
#include <sigilcompose/brush/Lines.h>        // lines::displace (the wave op)
#include <sigilgeometry/kit/Shapers.h>

#include <memory>
#include <vector>

#include "sigilcompose/Compose.h"
#include "sigilcompose/brush/GeometryOps.h"
#include "sigilcompose/brush/Layered.h"

namespace sigil::compose {

// ---------------------------------------------------------------------------
// THE BRUSH KINDS AND COMPOSITES
//
// A brush is what PAINTS. There are exactly four KINDS — the leaf tools —
// and two COMPOSITES, which combine any brushes at all, including other
// composites. That is the whole taxonomy; everything else here is a value
// built out of it.
//
//   kinds       brush::solid   brush::Pattern   brush::Scatter   brush::Art
//   composites  brush::layers(…)                brush::weave(…)

namespace brush {

/** THE plain stroke: a width, a paint, and an optional dash, stamp or path
 *  effect. The same type as `PathFormat`, under the name it carries in this
 *  taxonomy. */
using Solid = PathFormat;
/** `brush::solid(width, fill[, align])` — the one-line spelling.
 *  Designated initialisers still work through `brush::Solid{…}`. */
Solid solid(float width, Fill fill,
            PathFormat::Align align = PathFormat::Align::Center);

/** One strand of a composite: WHERE it runs and WHAT paints it.
 *
 *  Deliberately one value rather than two index-matched lists. Parallel
 *  lists let an inserted strand silently shift every brush after it, and
 *  nothing checks that the two lists are the same length. */
struct Strand {
  StrandPath path;
  Decoration brush;
  bool operator==(const Strand& o) const {
    return path == o.path && brush == o.brush;
  }
};

/** THE COMPOSITE. `brush::weave(...)` and `brush::layers(...)` are two
 *  author intents over this one machine:
 *
 *  **`layers` is `weave` with coincident self-strands.** Coincident
 *  strands cross nowhere, so no crossing rule ever fires and list order
 *  applies everywhere — which is exactly what "fixed order, bottom-up"
 *  means. Neither word is a special case in the code below; they differ
 *  only in what the author is saying.
 *
 *  Composites NEST: any strand's brush may be another composite, so a
 *  braid painted by layers, or a whole braid used as one strand of a
 *  bigger weave, needs no new vocabulary. */
struct Weave {
  std::vector<Strand> strands;
  /** How discovered crossings resolve. Default is list order — see
   *  CrossingRule. There is ONE of these; pins go on it via
   *  `.except(i, order)`, never as stacked entries. */
  geometry::path::CrossingRule crossing;
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
   *  the strand does elsewhere. That is inherent to repainting rather than
   *  a patch-size problem, and it also affects a region several crossings
   *  share. **Weaves want opaque inks.** */
  float patch = 0.0f;

  /** THE CROSSING CACHE. Discovering crossings is O(strands² × segments²)
   *  and runs on every paint without it, which a live weave pays every
   *  frame.
   *
   *  The key is the whole input: the vector of RESOLVED strand paths,
   *  compared by path content. That is sound because discovery is a pure
   *  function of those paths — the crossing RULE reads the discovered set
   *  and never feeds it — and it is complete because every way the answer
   *  can change lands in the paths first: an authored edit, an outline
   *  change under a relative strand, a changed borrowed path, a changed
   *  strand count. No callable enters the key, so there is no case where
   *  the key cannot be compared.
   *
   *  Held on the VALUE by shared_ptr, so copies share it and a freshly
   *  built value starts cold. Deliberately absent from operator==: a cache
   *  is not part of the value. Two live copies painting DIFFERENT geometry
   *  through the same cache thrash it back to per-paint discovery, which
   *  costs time and never correctness. */
  struct CrossingCache {
    std::vector<SkPath> key;  ///< the resolved paths the answer belongs to
    std::vector<geometry::path::Crossing> found;
    bool valid = false;
    int computes = 0;  ///< how many discoveries actually ran; for tests to
                       ///< observe, never read by the paint itself
  };
  std::shared_ptr<CrossingCache> crossingCache =
      std::make_shared<CrossingCache>();

  bool operator==(const Weave& o) const {
    return strands == o.strands && crossing == o.crossing && patch == o.patch;
  }
  bool isAnimated() const {
    for (const Strand& s : strands)
      if (s.brush.isAnimated()) return true;
    return false;
  }
  /** A strand that blends makes the whole weave blend: forwarded, or the
   *  node is baked into a layer of its own and the strand's mark resolves
   *  against transparent black instead of the page (BlendingDecoration). */
  bool blends() const {
    for (const Strand& s : strands)
      if (s.brush.blends()) return true;
    return false;
  }
  float bleed(SkSize size) const {
    float worst = 0;
    for (const Strand& s : strands)
      worst = std::max(worst, s.path.reach() + s.brush.bleed(size));
    return worst;
  }
  /** The widest mark any strand paints, off its own path. */
  float reach(SkSize size) const {
    float worst = 0;
    for (const Strand& s : strands)
      worst = std::max(worst, s.path.reach() + s.brush.reach(size));
    return worst;
  }
  /** Forwarded so the element can register the derive borrows without
   *  looking inside a type-erased brush (BorrowingDecoration). */
  std::vector<std::string> borrows() const {
    std::vector<std::string> keys;
    for (const Strand& s : strands) {
      if (s.path.source() == StrandPath::Source::Borrowed)
        keys.push_back(s.path.key());
      for (const std::string& nested : s.brush.borrows())
        keys.push_back(nested);
    }
    return keys;
  }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** FIXED ORDER, bottom-up: the first brush paints first, the last on top.
 *  Formally a weave of coincident self-strands (see Weave), which is why
 *  double and triple lines are `layers` plus offset shapers and never
 *  element duplication. */
Weave layers(std::vector<Decoration> stack);
/** PER-CROSSING order: strands that may trade sides, and a rule for who
 *  passes over whom where they meet. */
Weave weave(
    std::vector<Strand> strands,
    geometry::path::CrossingRule rule = geometry::path::crossing::alternate());

}  // namespace brush

/** THE BRUSH: one composable value — a geometry PIPELINE over the outline,
 *  shapers applied in order, feeding ordered paint LAYERS, each of which
 *  may be any Decoration at all (a lines::Line, a LayeredBrush stack, a
 *  Scatter or Pattern, a Ribbon, a plain stroke). Closed under
 *  composition:
 *
 *    element.stroke(Brush{}
 *        .shaped(geometry::shapers::Rounded{6})
 *        .shaped(geometry::shapers::Wave{.amplitude = 3, .wavelength = 30})
 *        .layer(lines::presets::cased(3, ink, 5))
 *        .layer(brush::Scatter{.art = spark(), .spacing = 40}));
 *
 *  A Brush of comparable shapers and layers is itself comparable, so the
 *  whole styled connector prunes and caches as ONE value. An animated layer
 *  declares volatility through the brush; `bleed()` sums the pipeline's
 *  reach and adds the widest layer's. */
struct Brush {
  /** One paint layer: a Decoration plus its own pipeline SUFFIX, applied
   *  after the shared pipeline and only to this layer. That is what makes
   *  an asymmetric casing one brush — a road with a lane and a curb, each
   *  riding its own offset shaper — instead of three stacked elements. */
  struct Layer {
    Decoration dec;
    std::vector<geometry::path::Shaper> shapers;
    bool operator==(const Layer& o) const {
      return dec == o.dec && shapers == o.shapers;
    }
  };

  std::vector<geometry::path::Shaper> pipeline;
  std::vector<Layer> layers;

  /** Append to the shared geometry pipeline. A `Shaper` is any comparable
   *  value with `SkPath shape(const SkPath &) const`; the stock ones
   *  (`geometry::shapes::wave/jitter/offset`) are peers of anything you
   *  write, which is why there is no shorthand for them here. */
  Brush& shaped(geometry::path::Shaper s) {
    pipeline.push_back(std::move(s));
    return *this;
  }
  /** One mark in the stack, bottom-up, with an optional shaper SUFFIX that
   *  deviates this layer's geometry only.
   *
   *  The suffix takes the same comparable `Shaper` seam `shaped()` takes.
   *  For a raw incomparable lambda, wrap this layer's decoration in
   *  `brush::restyle(op, dec)` instead — the one mechanism door, at the
   *  cost of pruning. */
  Brush& layer(Decoration d, std::vector<geometry::path::Shaper> suffix = {}) {
    layers.push_back(Layer{std::move(d), std::move(suffix)});
    return *this;
  }

  bool operator==(const Brush& o) const {
    return pipeline == o.pipeline && layers == o.layers;
  }
  bool isAnimated() const {
    for (const Layer& l : layers)
      if (l.dec.isAnimated()) return true;
    return false;
  }
  /** Forwarded, for the reason a weave forwards it. */
  bool blends() const {
    for (const Layer& l : layers)
      if (l.dec.blends()) return true;
    return false;
  }
  /** The widest mark any layer paints, plus the pipeline's own reach. */
  float reach(SkSize size) const {
    float shared = 0;
    for (const geometry::path::Shaper& g : pipeline) shared += g.bleed();
    float worst = 0;
    for (const Layer& l : layers) {
      float layerReach = l.dec.reach(size);
      for (const geometry::path::Shaper& g : l.shapers) layerReach += g.bleed();
      worst = std::max(worst, layerReach);
    }
    return shared + worst;
  }
  /** A layer may be a composite that borrows keyed paths; forward them so
   *  the element registers the derive borrow (BorrowingDecoration). */
  std::vector<std::string> borrows() const {
    std::vector<std::string> keys;
    for (const Layer& l : layers)
      for (const std::string& k : l.dec.borrows()) keys.push_back(k);
    return keys;
  }
  float bleed(SkSize size) const {
    float shared = 0;
    for (const geometry::path::Shaper& g : pipeline)
      shared += g.bleed();  // pipeline reaches compound (offset THEN wave)
    float worst = 0;
    for (const Layer& l : layers) {
      float layerReach = l.dec.bleed(size);
      for (const geometry::path::Shaper& g : l.shapers) layerReach += g.bleed();
      worst = std::max(worst, layerReach);
    }
    return shared + worst;
  }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

namespace brush {

/** Run a geometry pipeline, then paint `inner` on the restyled outline —
 *  any decoration (LayeredBrush, lines::Line, PathFormat…) gains waves,
 *  jitter, rounding without knowing.
 *
 *  THE ONE MECHANISM DOOR. It takes a `GeometryOp`, which a comparable
 *  shaper value and a raw `geometry::path::ops::PathOp` lambda both
 *  convert to — and the lambda has nowhere else to go.
 *
 *  The WRAPPER is incomparable either way, because it has no operator== at
 *  all, so a node wearing one never prunes whichever op it was handed:
 *  memo the host node, or keep the value pointer-stable. Prefer
 *  `Brush::shaped(value)` whenever a shaper can say it — that prunes. */
struct Restyled {
  GeometryOp op;
  Decoration inner;
  float extraBleed = 8.0f;  // the op's own overhang (wave amplitude…)

  bool isAnimated() const { return inner.isAnimated(); }
  /** Forwarded, for the reason a weave forwards it. */
  bool blends() const { return inner.blends(); }
  float bleed(SkSize size) const { return inner.bleed(size) + extraBleed; }
  float reach(SkSize size) const { return inner.reach(size); }
  /** Forwarded, or a wrapped weave's strand::from(key) would never be
   *  registered for the derive pass (BorrowingDecoration). */
  std::vector<std::string> borrows() const { return inner.borrows(); }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

inline Restyled restyle(GeometryOp op, Decoration inner,
                        float extraBleed = 8.0f) {
  return Restyled{std::move(op), std::move(inner), extraBleed};
}

}  // namespace brush

}  // namespace sigil::compose

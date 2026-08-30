#pragma once

/** @file
 * SigilCompose brushes — the vocabulary of MARKS along a path. Everything
 * here paints `PaintContext::outline`, whatever produced it (a node's
 * shape, a rail's route, a connector's wire), and attaches with
 * `.stroke()`.
 *
 * Two families over one seam:
 *  - the LAYERED STROKE STACK (`LayeredBrush`): several passes over the
 *    same path with their own widths, colours, blurs, dashes and blend
 *    modes, painted bottom-up — how an additive glow or a multi-tier
 *    circuit trace is built.
 *  - the PIPELINE model (`Brush`): geometry shapers over the path feeding
 *    ordered paint layers, with four leaf kinds — `brush::solid`,
 *    `brush::Pattern`, `brush::Scatter`, `brush::Art` — and two composites,
 *    `brush::layers` and `brush::weave`, which may contain any brush at all
 *    including each other.
 *
 * EQUALITY IS THE THING TO WATCH. A brush assembled from comparable parts
 * is itself a comparable value, so a styled connector prunes and caches as
 * one value. Any raw callable in it — a `StampModFn`, an `ops::PathOp` —
 * makes it conservatively unequal forever, so its node re-patches on every
 * describe; memo the host node, or keep the value itself alive rather than
 * rebuilding it.
 *
 * Two numbers every brush declares, and they are not the same: `bleed()` is
 * how far paint escapes the outline, which grows a cached recording's cull;
 * `reach()` is how wide the MARK is in total, which is what a repair region
 * has to cover. Under-reporting either truncates or thins silently.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPicture.h>
#include <sigilcompose/brush/Decorations.h>  // PathSample
#include <sigilcompose/brush/Lines.h>        // lines::displace (the wave op)
#include <sigilcompose/shape/Shapes.h>

#include <any>
#include <functional>
#include <memory>
#include <optional>
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
    std::vector<Crossing> found;
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
  float bleed() const {
    float worst = 0;
    for (const Strand& s : strands)
      worst = std::max(worst, s.path.reach() + s.brush.bleed());
    return worst;
  }
  /** The widest mark any strand paints, off its own path. */
  float reach() const {
    float worst = 0;
    for (const Strand& s : strands)
      worst = std::max(worst, s.path.reach() + s.brush.reach());
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
Weave weave(std::vector<Strand> strands,
            CrossingRule rule = crossing::alternate());

}  // namespace brush

/** THE BRUSH: one composable value — a geometry PIPELINE over the outline,
 *  shapers applied in order, feeding ordered paint LAYERS, each of which
 *  may be any Decoration at all (a lines::Line, a LayeredBrush stack, a
 *  Scatter or Pattern, a Ribbon, a plain stroke). Closed under
 *  composition:
 *
 *    element.stroke(Brush{}
 *        .shaped(kit::brush::shapers::Rounded{6})
 *        .shaped(kit::brush::shapers::Wave{.amplitude = 3, .wavelength = 30})
 *        .layer(lines::cased(3, ink, 5))
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
    std::vector<Shaper> shapers;
    bool operator==(const Layer& o) const {
      return dec == o.dec && shapers == o.shapers;
    }
  };

  std::vector<Shaper> pipeline;
  std::vector<Layer> layers;

  /** Append to the shared geometry pipeline. A `Shaper` is any comparable
   *  value with `SkPath shape(const SkPath &) const`; the stock ones
   *  (`kit::brush::shapers::wave/jitter/offset`) are peers of anything you
   *  write, which is why there is no shorthand for them here. */
  Brush& shaped(Shaper s) {
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
  Brush& layer(Decoration d, std::vector<Shaper> suffix = {}) {
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
  /** The widest mark any layer paints, plus the pipeline's own reach. */
  float reach() const {
    float shared = 0;
    for (const Shaper& g : pipeline) shared += g.bleed();
    float worst = 0;
    for (const Layer& l : layers) {
      float layerReach = l.dec.reach();
      for (const Shaper& g : l.shapers) layerReach += g.bleed();
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
  float bleed() const {
    float shared = 0;
    for (const Shaper& g : pipeline)
      shared += g.bleed();  // pipeline reaches compound (offset THEN wave)
    float worst = 0;
    for (const Layer& l : layers) {
      float layerReach = l.dec.bleed();
      for (const Shaper& g : l.shapers) layerReach += g.bleed();
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
 *  shaper value and a raw `ops::PathOp` lambda both convert to — and the
 *  lambda has nowhere else to go.
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
  float bleed() const { return inner.bleed() + extraBleed; }
  float reach() const { return inner.reach(); }
  /** Forwarded, or a wrapped weave's strand::from(key) would never be
   *  registered for the derive pass (BorrowingDecoration). */
  std::vector<std::string> borrows() const { return inner.borrows(); }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

inline Restyled restyle(GeometryOp op, Decoration inner,
                        float extraBleed = 8.0f) {
  return Restyled{std::move(op), std::move(inner), extraBleed};
}

/** WHERE instances land along a path. The vertex modes read the path's
 *  REAL verbs — the route's authored bends — rather than sampling
 *  tangents, so a marker at a bend sits exactly on it. `interval` above 1
 *  is px; at or below 1 it is a FRACTION of each contour's length. */
struct Placement {
  enum class Mode : uint8_t {
    Interval,       ///< every `interval` px (or fraction), phase `offset`
    Vertex,         ///< every path vertex (bends + endpoints)
    FirstVertex,    ///< each contour's first point
    LastVertex,     ///< each contour's last point
    InnerVertices,  ///< bends only — no endpoints
    CentralPoint,   ///< the arc-length midpoint of each contour
    SegmentCenter,  ///< the midpoint of every straight segment
  };
  Mode mode = Mode::Interval;
  /** px, or a contour fraction when ≤ 1. UNSET means "take the host
   *  brush's own spacing", which `Scatter::spacing` supplies.
   *
   *  An optional rather than a defaulted float on purpose: a sentinel
   *  value would be a number an author could also type deliberately, and
   *  typing it would silently select the host's spacing instead. */
  std::optional<float> interval;
  float offset = 0.0f;  ///< leading phase for Interval (same units)
  bool operator==(const Placement&) const = default;
};

/** One placed instance's deviation from its slot — the programmatic twist
 *  (mirrors GlyphMod; return {.skip = true} to drop a slot). */
struct StampMod {
  float dAlong = 0, dNormal = 0;  ///< px, in the sample's tangent frame
  float scale = 1;
  float rotateDeg = 0;
  float alpha = 1;
  bool skip = false;
};
using StampModFn =
    std::function<StampMod(const PathSample&, size_t index, size_t count)>;

/** The SCATTER brush: an Element instanced along the path at `spacing`,
 *  with seeded jitter and the StampMod hook. The art bakes ONCE via
 *  snapshot() (its own decorations and all) and replays per slot. Keep
 *  the art Element pointer-stable across renders to prune; a mod fn makes
 *  the value incomparable (memo the host).
 *
 *  THE CACHE IN THIS VALUE IS THE FALLBACK. Inside a composer the bake
 *  lives in the INSTANCE's stamp cache, handed in through
 *  `PaintContext::stamps` and keyed on the art's node, so a brush value
 *  rebuilt by every describe still finds its art's bake instead of
 *  re-rastering it. What defeats that is a NEW ART NODE each describe,
 *  since the node IS the key: keep the art Element pointer-stable — a
 *  member, a static, a captured value. The member cache here serves
 *  standalone paints, where there is no composer and no stamp cache. */
struct Scatter {
  Element art;
  float spacing = 24.0f;  ///< Interval-mode sugar (px, or fraction ≤ 1)
  /** Full placement grammar — set `place.mode` for the Vertex,
   *  SegmentCenter and CentralPoint families. In Interval mode an unset
   *  `place.interval` falls back to `spacing`. */
  Placement place{};
  uint32_t seed = 0;  ///< 0 = a regular run, no jitter roll
  float jitterAlong = 0, jitterNormal = 0;  ///< ±px
  float jitterScale = 0;                    ///< ±fraction of 1
  float jitterRotateDeg = 0;                ///< ±deg
  bool alignToPath = true;
  float reach = 32.0f;  ///< cull reserve: half the art's extent + jitter
  StampModFn mod;
  bool animatedMod = false;  ///< mod reads time → repaint per frame

  bool isAnimated() const { return animatedMod; }
  float bleed() const { return reach; }
  bool operator==(const Scatter& o) const {
    return art.node() == o.art.node() && spacing == o.spacing &&
           place == o.place && seed == o.seed && jitterAlong == o.jitterAlong &&
           jitterNormal == o.jitterNormal && jitterScale == o.jitterScale &&
           jitterRotateDeg == o.jitterRotateDeg &&
           alignToPath == o.alignToPath && reach == o.reach && !mod && !o.mod &&
           animatedMod == o.animatedMod;
  }

  /** The scatter's baked stamp, shared by every copy of the brush
   *  value. `bakedFor` pins the bake to the art it came from, so a
   *  copy that swaps art re-bakes instead of stamping the old one. */
  struct Cache {
    sk_sp<SkPicture> pic;
    const void* bakedFor = nullptr;  // the art node the bake belongs to —
                                     // copies that swap art re-bake
  };
  std::shared_ptr<Cache> cache = std::make_shared<Cache>();

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** Which way a corner tile faces. **There is no default**, deliberately:
 *  this is not a preference but a statement about what the art LOOKS LIKE,
 *  and nothing here can see the art.
 *
 *  WHICH ONE:
 *
 *  - `Bisector` — for an ORNAMENT symmetric about its own bisector, drawn
 *    once and serving all four corners of a frame: a fleuron, a rosette, a
 *    plain bracket.
 *  - `Outgoing` — for anything with a distinguishable ENTRY and EXIT: an
 *    elbow of pipe, a flow tick, an arrow turning a corner, a cross whose
 *    arms are meant to lie along the edges.
 *
 *  Getting it wrong rotates every corner stamp by half the turn angle,
 *  which is a rotation and not an absence — easy to miss and easy to
 *  mistake for the art itself. The worst case is structural: **every
 *  corner of an annular sector is a right angle**, so its bisector sits
 *  45° from both legs, and art with 90° symmetry — a Greek cross — is
 *  corner-agnostic under `Outgoing` and uniformly 45° off under
 *  `Bisector`, turning every cross into a saltire.
 *
 *  To check which an existing composition needs, render it under one value
 *  and then the other and compare: if nothing moves, the art is
 *  rotationally forgiving and either is correct; if the corners snap, one
 *  of the two renders was wrong. Judging the rotation by eye on a busy
 *  drawing is unreliable.
 *
 *  `Bisector` does NOT save you a drawing. The arms of a bisector-aligned
 *  tile sit at `(turn/2, 180 − turn/2)` off the bisector and mirror with
 *  the SIGN of the turn, so a handed ornament costs two drawings either
 *  way. */
enum class CornerAlign { Bisector, Outgoing };

/** CORNER ART AND HOW IT IS AIMED — one value, because the second half is
 *  not optional information about the first.
 *
 *  There is no default constructor and no default member initializer, so
 *  a corner tile cannot be handed to this brush without saying which way
 *  it faces. Defaulting the alignment would leave the mistake detectable
 *  only as a warning at paint time, which the type system can refuse
 *  outright instead.
 *
 *      pb.corner = brush::CornerArt{elbow, brush::CornerAlign::Outgoing};
 */
struct CornerArt {
  Element art;
  CornerAlign align;
  CornerArt(Element artIn, CornerAlign alignIn)
      : art(std::move(artIn)), align(alignIn) {}
  bool operator==(const CornerArt& o) const {
    return art.node() == o.art.node() && align == o.align;
  }
};

/** The PATTERN brush: a SIDE tile repeated an INTEGER number of times per
 *  run and stretched along the tangent to close the remainder, so a run
 *  never ends on a torn tile. Optional CORNER tiles go where the tangent
 *  breaks by more than `cornerAngleDeg`, aimed by the `CornerArt` value,
 *  which requires you to say how; optional START and END tiles cap open
 *  contours. A run is a stretch between two corners. */
struct Pattern {
  Element side;
  std::optional<Element> start, end;
  /** Corner tiles, and their alignment — see CornerArt. Absent means the
   *  runs simply meet at the break. */
  std::optional<CornerArt> corner;
  float advance = 0;  ///< tile length along the path (0 → intrinsic)
  /** The tangent break that counts as a corner. A gently ROUNDED corner
   *  has no hard break, so it takes no corner tile — and a regular n-gon
   *  turns 360/n per vertex, so at this default nothing above 10 sides is
   *  seen as having corners at all. Lower it for those.
   *
   *  35° here where the other corner scanners default to 30°. Changing
   *  either number changes what existing compositions draw, so both stay
   *  as they are. */
  float cornerAngleDeg = 35.0f;
  /** Arc length a corner tile RESERVES on each adjacent run, px. 0 uses
   *  the corner art's own width.
   *
   *  Each corner takes `cornerLength / 2` off the end of each of its two
   *  adjacent runs, and the side run's integer fit is computed over the
   *  SHORTENED span, so side tiles butt against the corner instead of
   *  running underneath it. The reservation matters most when the corner
   *  art is much larger than a side tile — an elbow twice the side tile's
   *  width — where without it the side run continues visibly beneath the
   *  elbow. Setting it also shifts side-tile phase slightly, since the
   *  runs are shorter. */
  float cornerLength = 0.0f;
  bool stretchToFit = true;  ///< false: natural size, slack spread evenly
  float reach = 32.0f;       ///< cull reserve
  StampModFn mod;            ///< side tiles only
  bool animatedMod = false;

  bool isAnimated() const { return animatedMod; }
  float bleed() const { return reach; }
  bool operator==(const Pattern& o) const {
    auto node = [](const std::optional<Element>& e) {
      return e ? e->node().get() : nullptr;
    };
    return side.node() == o.side.node() && node(start) == node(o.start) &&
           node(end) == node(o.end) && corner == o.corner &&
           advance == o.advance && cornerAngleDeg == o.cornerAngleDeg &&
           cornerLength == o.cornerLength && stretchToFit == o.stretchToFit &&
           reach == o.reach && !mod && !o.mod && animatedMod == o.animatedMod;
  }

  /** The baked tile art, keyed on each art Element's node POINTER — which
   *  is what makes the rule below matter.
   *
   *  THE CACHE IN THIS VALUE IS THE FALLBACK. Inside a composer the bakes
   *  live in the INSTANCE's stamp cache, handed in through
   *  `PaintContext::stamps` and keyed on the art's node, so a brush value
   *  rebuilt by every describe still finds its art's bakes rather than
   *  rasterizing them again. What defeats that is a NEW ART NODE each
   *  describe, since the node IS the key: keep the art Elements
   *  pointer-stable — a member, a static, a captured value. The member
   *  cache here serves standalone paints; copies share it and a fresh
   *  value starts empty. */
  struct Cache {
    sk_sp<SkPicture> side, start, end, corner;
    const void* bakedSide = nullptr;
    const void* bakedStart = nullptr;
    const void* bakedEnd = nullptr;
    const void* bakedCorner = nullptr;
  };
  std::shared_ptr<Cache> cache = std::make_shared<Cache>();

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** The variable-width RIBBON: a filled band whose width follows a law —
 *  a linear taper by default, a calligraphic nib when `nibAngleDeg` ≥ 0
 *  (the width peaks where the path runs perpendicular to the nib), or any
 *  `Profile` on the shared width seam. */
struct Ribbon {
  Fill fill = Fill::color({1, 1, 1, 1});
  float widthStart = 10.0f, widthEnd = 2.0f;
  float nibAngleDeg = -1.0f;  ///< ≥0 → calligraphic (widthStart = full)
  float nibContrast = 0.15f;  ///< thinnest fraction at nib-aligned tangents
  float step = 3.0f;  // clamped ≥ 0.5px at paint (0 would never advance)

  /** THE WIDTH LAW, on the shared PROFILE seam.
   *
   *  A `Profile` is `float across(float along)` plus a REQUIRED
   *  `float max()` plus EQUALITY, and both of those additions are
   *  load-bearing. The declared maximum is what `bleed()` reports, so a
   *  wide ribbon cannot be silently clipped by a cull that assumed a
   *  narrow one; the equality is what lets a varying-width ribbon prune,
   *  where a bare width callable would compare unequal forever and
   *  re-record its whole band every frame.
   *
   *  `across(along)` is the FULL width at that fraction of the spine, the
   *  same value `band(spine, across(...))` reads — one vocabulary for a
   *  band's taper, a strand's displacement and a ribbon's width.
   *
   *  **A law that must not slide under a reveal is keyed in PX**: give the
   *  scheme `static constexpr bool alongIsPx = true` and `across` is
   *  handed arc-length px from the spine's start instead of a fraction.
   *  Under a span reveal such as `spans::upTo` the decoration receives the
   *  REVEALED contour, so a fraction is a fraction of what has been drawn
   *  so far and a fraction-keyed law walks along the mark as it writes;
   *  px does not move — provided the reveal is anchored at the spine's
   *  start. A window whose BEGIN moves, or one that wraps, measures px
   *  from the revealed piece's own start. See `PxKeyedProfileScheme`.
   *
   *  GEOMETRY: a profiled ribbon is `bandRegion()`, so its rails go
   *  through `profileOffset` — a CONSTANT profile picks up
   *  `geometry::path::parallel`'s real-vertex corner repair (arc outside a
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
    if (hasProfile()) return width.max();
    return std::max(widthStart, widthEnd);
  }
  bool operator==(const Ribbon& o) const {
    return fill == o.fill && widthStart == o.widthStart &&
           widthEnd == o.widthEnd && nibAngleDeg == o.nibAngleDeg &&
           nibContrast == o.nibContrast && step == o.step && width == o.width;
  }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** Linear taper (comet body, ink pull-away). */
Ribbon taper(float widthStart, float widthEnd, Fill fill);

/** The calligraphic nib: full width perpendicular to `nibAngleDeg`,
 *  `contrast` fraction when the path runs along the nib. */
Ribbon calligraphic(float nibAngleDeg, float width, Fill fill,
                    float contrast = 0.15f);

/** The ART brush: ONE art cell stretched and continuously BENT along each
 *  contour. This is what the stamp and tile brushes cannot do — they break
 *  a curve into rigid segments, where this warps the art smoothly through
 *  it.
 *
 *  The art bakes once to a texture at 2× oversample, and each contour is
 *  walked into a triangle-strip ribbon (position ± normal·h per station)
 *  whose texture coordinates sweep the art from one end to the other. One
 *  drawVertices per contour. `stationPx` is the warp fidelity, one station
 *  per that many arc px: a few px follows tight curves, and a larger value
 *  is cheaper on long gentle paths.
 *
 *  THE CACHE IN THIS VALUE IS THE FALLBACK, and missing it costs more
 *  here than for the other brushes, because the bake is a rasterized
 *  texture rather than a picture. Inside a composer it lives in the
 *  INSTANCE's stamp cache, handed in through `PaintContext::stamps` and
 *  keyed on the art's node, so a brush value rebuilt by every describe
 *  still finds it. What defeats that is a NEW ART NODE each describe,
 *  since the node IS the key: keep the art Element pointer-stable. The
 *  member cache here serves standalone paints. */
struct Art {
  Element art;
  float height = 0;        ///< ribbon height (0 → the art's intrinsic)
  float stationPx = 6.0f;  ///< arc-length between strip stations
  float reach = 32.0f;     ///< cull reserve: half height + art overhang

  bool isAnimated() const { return false; }
  float bleed() const { return reach; }
  bool operator==(const Art& o) const {
    return art.node() == o.art.node() && height == o.height &&
           stationPx == o.stationPx && reach == o.reach;
  }

  /** The art's rastered strip, shared by every copy of the brush value
   *  and pinned to the art it came from. */
  struct Cache {
    sk_sp<SkImage> image;  // the 2x bake
    SkSize artSize{0, 0};  // logical art size
    const void* bakedFor = nullptr;
  };
  std::shared_ptr<Cache> cache = std::make_shared<Cache>();

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** Art warped along the path: the drawVertices ribbon. `height` 0 keeps
 *  the art's intrinsic height. */
Art artAlong(Element art, float height = 0, float stationPx = 6.0f);

}  // namespace brush

namespace brush {
/** A Ribbon built on the PROFILE seam — the constructor to prefer, since
 *  the profile is the half of a ribbon that shares a vocabulary with
 *  bands and strands. */
Ribbon ribbon(Profile width, Fill fill);
}  // namespace brush

}  // namespace sigil::compose

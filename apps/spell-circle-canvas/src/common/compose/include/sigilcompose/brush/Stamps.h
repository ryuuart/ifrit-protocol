#pragma once

/** @file
 * SigilCompose brushes — THE STAMPED KINDS: a mark laid down as COPIES of
 * a picture rather than as a stroke.
 *
 * `brush::Scatter` places its art at samples along the outline — a
 * `brush::Placement` says where those samples fall, and a
 * `StampModifierFunction` may deviate each copy. `brush::Pattern` lays a tile
 * end to end and fits the count to the run, with `brush::CornerArt` for the
 * elbow a turn is made of. Both are leaf kinds of `Brush`
 * (<sigilcompose/brush/Brushes.h>), and both bake their art as a PICTURE,
 * so a mask filter inside a tile is re-run on every stamp.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPicture.h>
#include <sigilcompose/brush/Decorations.h>  // PathSample

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "sigilcompose/Compose.h"

namespace sigil::compose::brush {

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
 *  (mirrors GlyphModifier; return {.skip = true} to drop a slot). */
struct StampModifier {
  float dAlong = 0, dNormal = 0;  ///< px, in the sample's tangent frame
  float scale = 1;
  float rotateDeg = 0;
  float alpha = 1;
  bool skip = false;
};
using StampModifierFunction =
    std::function<StampModifier(const PathSample&, size_t index, size_t count)>;

/** The SCATTER brush: an Element instanced along the path at `spacing`,
 *  with seeded jitter and the StampModifier hook. The art bakes ONCE via
 *  snapshot() (its own decorations and all) and replays per slot. Keep
 *  the art Element pointer-stable across renders to prune; a modifier fn makes
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
  /** How far a stamp escapes the outline: half the art's extent plus the
   *  jitter. The CULL's number, measured from the path outwards — the
   *  mark's own width is `reach()`, twice this. */
  float bleedPx = 32.0f;
  StampModifierFunction modifier;
  bool animatedModifier = false;  ///< modifier reads time → repaint per frame

  bool isAnimated() const { return animatedModifier; }
  float bleed() const { return bleedPx; }
  /** The mark's full width: a stamp is centred on the path, so it spans
   *  the reserve on both sides of it. */
  float reach() const { return bleedPx * 2.0f; }
  bool operator==(const Scatter& o) const {
    return art.node() == o.art.node() && spacing == o.spacing &&
           place == o.place && seed == o.seed && jitterAlong == o.jitterAlong &&
           jitterNormal == o.jitterNormal && jitterScale == o.jitterScale &&
           jitterRotateDeg == o.jitterRotateDeg &&
           alignToPath == o.alignToPath && bleedPx == o.bleedPx && !modifier &&
           !o.modifier && animatedModifier == o.animatedModifier;
  }

  /** The scatter's baked stamp, shared by every copy of the brush
   *  value. `bakedFor` pins the bake to the art it came from, so a
   *  copy that swaps art re-bakes instead of stamping the old one. */
  struct Cache {
    sk_sp<SkPicture> picture;
    // The art node the bake belongs to — copies that swap art re-bake.
    std::weak_ptr<detail::ElementNode> bakedFor;
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
  /** How far a tile escapes the outline: half a tile's extent across the
   *  path. The CULL's number — the mark's own width is `reach()`. */
  float bleedPx = 32.0f;
  StampModifierFunction modifier;  ///< side tiles only
  bool animatedModifier = false;

  bool isAnimated() const { return animatedModifier; }
  float bleed() const { return bleedPx; }
  /** The mark's full width: a tile is centred on the path, so it spans
   *  the reserve on both sides of it. */
  float reach() const { return bleedPx * 2.0f; }
  bool operator==(const Pattern& o) const {
    auto node = [](const std::optional<Element>& e) {
      return e ? e->node().get() : nullptr;
    };
    return side.node() == o.side.node() && node(start) == node(o.start) &&
           node(end) == node(o.end) && corner == o.corner &&
           advance == o.advance && cornerAngleDeg == o.cornerAngleDeg &&
           cornerLength == o.cornerLength && stretchToFit == o.stretchToFit &&
           bleedPx == o.bleedPx && !modifier && !o.modifier &&
           animatedModifier == o.animatedModifier;
  }

  /** The baked tile art, keyed on each art Element's NODE — which is what
   *  makes the rule below matter.
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
    std::weak_ptr<detail::ElementNode> bakedSide, bakedStart, bakedEnd,
        bakedCorner;
  };
  std::shared_ptr<Cache> cache = std::make_shared<Cache>();

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

}  // namespace sigil::compose::brush

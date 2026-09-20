#pragma once

/** @file
 * @ingroup geometry-path
 *
 * SigilGeometry path operations — the Pathfinder panel and the Distort
 * menu, as values. Three families:
 *
 *  - BOOLEANS over Skia's pathops, with simplify and offset beside
 *    them, and the two POLYLINE corner treatments. All pure: SkPath in,
 *    SkPath out, and a pathops failure comes back empty.
 *  - STRIP JOINERY over pieces of stock — a segment cut to a width —
 *    mitred to the joints each piece stands in.
 *  - DISTORTS as parameter structs, each carrying its dials and
 *    applying on demand, so a recipe stays editable.
 */

#include <include/core/SkPath.h>
#include <sigilcore/compute/Chance.h>

#include <concepts>
#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <initializer_list>
#include <ranges>
#include <span>
#include <vector>

/** THE PATHFINDER PANEL AND THE DISTORT MENU, as values: the booleans
 *  over two outlines, the self-intersection cleanup and the stroke
 *  expansion beside them, the corner treatments over a polyline, and the
 *  warps that bend an outline without changing its node count. Every one
 *  is a pure function — a path in, a path out — and a failure comes back
 *  as an empty path rather than an error. */
namespace sigil::geometry::path::operations {

/** Everything either shape covers. */
SkPath unite(const SkPath& a, const SkPath& b);
/** Minus Front: `a` with everything the front shape covers taken out. */
SkPath subtract(const SkPath& a, const SkPath& b);
/** Only what both shapes cover. */
SkPath intersect(const SkPath& a, const SkPath& b);
/** What one shape covers and the other does not. */
SkPath exclude(const SkPath& a, const SkPath& b);
/** N-ary union — merge a whole stack at once, over any range of paths: a
 *  vector or a span as it stands, a brace list, or a view that builds them
 *  as it is walked (`lines | std::views::transform(expand)`). */
SkPath unite(std::span<const SkPath> paths);
/** The same union over a brace list. */
inline SkPath unite(std::initializer_list<SkPath> paths) {
  return unite(std::span<const SkPath>(paths.begin(), paths.size()));
}
/** The same union over any range of paths, including a view that
 *  builds them as it is walked. */
template <std::ranges::input_range R>
  requires(!std::convertible_to<R &&, std::span<const SkPath>> &&
           std::convertible_to<std::ranges::range_reference_t<R>, SkPath>)
SkPath unite(R&& paths) {
  // A range that is not already contiguous is walked into one, because the
  // union builder wants every path before it resolves any of them.
  std::vector<SkPath> held;
  for (auto&& path : paths) held.push_back(path);
  return unite(std::span<const SkPath>(held));
}

/** Resolve self-intersections and redundant winding into a clean
 *  even-odd-equivalent outline (Pathfinder's Merge, roughly). */
SkPath simplify(const SkPath& path);

/** How two pieces of an offset mark meet at a corner. */
enum class Join : uint8_t { Round, Miter, Bevel };
/** How an offset mark ends where the source has an end. */
enum class Cap : uint8_t { Butt, Round, Square };

/** THE DIALS OF ONE OFFSET. `position` is what makes this a single
 *  operator rather than a family, and it is CONTINUOUS: 0 is the single
 *  curve a distance to the LEFT of travel, 1 the same distance to the
 *  right, 0.5 both at once — the band that straddles the source — and
 *  everything between is that band slid across it. */
struct OffsetOptions {
  Join join = Join::Round;
  Cap cap = Cap::Round;
  /** How many distances a mitred corner's point may stand from the
   *  corner it came from before it is cut off. */
  float miterLimit = 4.0f;
  /** Where the offset sits across the source: 0 is wholly left of
   *  travel, 0.5 straddles it, 1 is wholly right. Clamped. */
  float position = 0.5f;
  /** MOVE THE SOURCE'S OWN NODES rather than form a new outline: the
   *  answer has the nodes the source had, in the same order and of the
   *  same kinds, so the two still interpolate. Which way "out" is comes
   *  from the contour's own winding, which makes this the one spelling
   *  whose sign follows the drawing rather than the boolean.
   *  @silent `position` and `step` are set beside it: they describe a
   *  band and a walk, and this is the WHOLE offset when it is on. */
  bool keepCompatible = false;
  /** The stride the sideways walk takes where a walk is used — away
   *  from `position` 0.5, where Skia's stroker answers instead. */
  float step = 4.0f;
  bool operator==(const OffsetOptions&) const = default;
};

/** OFFSET: the mark @p path becomes a distance to the side of itself —
 *  ONE operator for what an outline offset, a concentric frame, a
 *  parallel rail and a bolder silhouette all are. A positive
 *  @p distance offsets to the LEFT of travel, which for a filled shape
 *  at the default `position` is outward: the library-wide sign
 *  convention. A band that STRADDLES the source answers the source
 *  grown or shrunk; one that lies to a side is answered as itself. */
SkPath offset(const SkPath& path, float distance,
              const OffsetOptions& options = {});

/** WHICH CORNERS A ROUNDING TAKES, AND HOW HARD.
 *
 *  Every dial here is off by default, and with all of them off the
 *  rounding is Skia's own corner effect over every corner of the path —
 *  which is the common case and stays exactly as cheap as it was. */
struct CornerOptions {
  /** Only corners that turn by more than this many degrees. Zero rounds
   *  every corner there is. */
  float minTurnDeg = 0.0f;
  /** Round only the corners that turn OUTWARD, read off the contour's
   *  own winding — a reflex corner is left as it is. */
  bool outwardOnly = false;
  /** SCALE EACH RADIUS BY THE CORNER'S ANGLE so that every corner's arc
   *  stands the same distance out from the vertex it replaced: an acute
   *  corner takes a smaller radius and an obtuse one a larger. Without
   *  it a shallow corner reads as barely rounded beside a sharp one cut
   *  by the same number. */
  bool visual = false;
  bool operator==(const CornerOptions&) const = default;
};

/** ROUND CORNERS: every sharp corner of @p path replaced by an arc of
 *  @p radius. A non-positive radius, and a path the effect refuses,
 *  come back unchanged rather than empty.
 *  @silent any option is set and a corner's legs are not both STRAIGHT:
 *  with options this is a polyline treatment, where Skia's own corner
 *  effect, which the default options use, has no such limit. */
SkPath roundCorners(const SkPath& path, float radius,
                    const CornerOptions& options = {});

/** CUT EVERY LINE-LINE CORNER of @p path with a straight bevel @p cut px
 *  along each leg — the 45-degree face of the game-UI and PCB corner
 *  convention, which `SkCornerPathEffect` cannot spell because it only
 *  rounds. The cut clamps to half of each adjacent leg; a closed
 *  polyline contour chamfers its closing vertex too.
 *  @silent the contour holds ANY curve segment: a chamfer over an arc
 *  or a rounded route copies that contour through untouched. */
SkPath chamferCorners(const SkPath& path, float cut);

/** A SQUARE WAVE across the mark: the contour walked at a fixed
 *  wavelength and displaced by +/- @p amplitude on its normal with
 *  vertical jumps between — battlements, the Greek meander key, a stepped
 *  circuit trace. The wavelength is rounded so a whole number of periods
 *  fits the contour, which is what keeps a closed mark from meeting itself
 *  mid-step. */
SkPath displaceSquare(const SkPath& src, float amplitude, float wavelength);

// ---------------------------------------------------------------------------
// Strip joinery: pieces of stock, and what happens where they meet.

/** ONE PIECE OF STOCK: a segment cut to a width, the piece a lattice, a
 *  trellis, a window bar, a rail frame and a Voronoi cage are all made
 *  of. It carries no material, no order and no identity — a caller keeps
 *  those beside it, indexed the same way. */
struct Strip {
  glm::vec2 from{0, 0};
  glm::vec2 to{0, 0};
  float width = 1;

  /** Value equality: the same segment cut to the same width. */
  bool operator==(const Strip&) const = default;
};

/** HOW A SET OF PIECES IS CUT WHERE IT MEETS ITSELF. */
struct StripOptions {
  /** THE CUT AT A NODE. `Miter` planes each end back to the seams it
   *  shares with its neighbours, so the pieces fill the node with no gap
   *  and no overlap — real mitred joinery. `Bevel` stops each end a
   *  half-width short, blunting the point. `Round` finishes each end
   *  with an arc of its own half-width about the node. */
  Join join = Join::Miter;
  /** How many half-widths a mitred point may stand from its node before
   *  it is cut back: the sharper the angle, the further a true mitre
   *  reaches, and a needle-thin one reaches off the page. */
  float miterLimit = 4.0f;
  /** HOW NEAR A MEETING IS A MEETING: two ends within this of each other
   *  are one node, and a crossing within this of either piece's end is a
   *  meeting rather than a lap. */
  float tolerance = 0.5f;
  /** How far a lap's seam may run, in widths of the piece it crosses. A
   *  bound rather than a preference: two pieces crossing at a grazing
   *  angle overlap along a length that runs away as the angle closes. */
  float lapLimit = 3.0f;
  bool operator==(const StripOptions&) const = default;
};

/** THE OUTLINE OF EACH PIECE, ITS ENDS CUT TO THE JOINTS IT STANDS IN —
 *  one closed contour per piece, in the order @p pieces were given, and
 *  an empty path for a piece of no length. A node is wherever ends meet,
 *  and the seam between each neighbouring pair is the bisector of their
 *  two directions; an end that meets nothing is cut square across.
 *  @silent nothing here reads which piece is ON TOP — a lattice is one
 *  layer of stock at a time, and `stripLaps` is where layers cross. */
std::vector<SkPath> stripOutlines(std::span<const Strip> pieces,
                                  const StripOptions& options = {});

/** THE WHOLE JOINED FIGURE: every piece's outline united into one path,
 *  which for a mitred set is the lattice as a single silhouette with its
 *  joints closed. */
SkPath strips(std::span<const Strip> pieces, const StripOptions& options = {});

/** WHERE TWO PIECES CROSS RATHER THAN MEET: the half-lap, the joint a
 *  lattice is held together by. */
struct StripLap {
  /** The two pieces, in the order they were given. */
  int pieces[2]{0, 0};
  glm::vec2 at{0, 0};
  /** Each piece's own unit direction, indexed as `pieces`. */
  glm::vec2 along[2]{};
  /** Where the crossing falls along each piece, as a fraction of it. */
  float at01[2]{0, 0};
  /** HALF THE LENGTH OF THE OVERLAP ALONG EACH PIECE: the OTHER piece's
   *  width carried across at the angle the two cross, which is the seam
   *  a half-lap shows on the piece that passes over. Bounded by
   *  `lapLimit`. */
  float halfSpan[2]{0, 0};

  /** Value equality: the same two pieces crossing at the same place.
   *  The arrays compare element for element. */
  bool operator==(const StripLap&) const = default;
};

/** Every place two pieces cross away from their ends. A crossing at an
 *  end is a meeting, which `stripOutlines` mitres instead, and two
 *  parallel pieces cross nowhere. The laps come in the order the pairs
 *  stand in, the lower index first. */
std::vector<StripLap> stripLaps(std::span<const Strip> pieces,
                                const StripOptions& options = {});

// ---------------------------------------------------------------------------
// Distorts. All resample-based: segmentPx bounds fidelity (smaller =
// truer curves, more points).

/** ROUGHEN — seeded jitter along the contour normal; `smooth` rebuilds
 *  with Catmull-Rom. The displacement is drawn from ONE SEEDED STREAM,
 *  the same value every other seeded thing in this tree draws from, so
 *  a roughened outline re-rolls identically on every platform, and each
 *  contour draws from its own stream. `seed` and `parameter` are the
 *  pair every seeded value here is described by: the parameter is the
 *  sequence's own dial, and a source that has none ignores it. */
struct Roughen {
  float amplitude = 4;
  float segmentPx = 8;
  uint64_t seed = 1;
  uint32_t parameter = 0;
  bool smooth = true;
  core::chance::Source source = core::chance::Source::Pcg;

  /** Value equality, dial for dial — so a description holding this
   *  distort can be compared with the one the frame before held. */
  bool operator==(const Roughen&) const = default;

  SkPath apply(const SkPath& path) const;
  SkPath operator()(const SkPath& path) const { return apply(path); }
};

/** Zig Zag — a regular wave along the contour; `smooth` = sine ridges,
 *  otherwise hard saw teeth. `wavelengthPx` is crest to crest. */
struct Zigzag {
  float amplitude = 6;
  float wavelengthPx = 24;
  bool smooth = false;

  /** Value equality, dial for dial. */
  bool operator==(const Zigzag&) const = default;

  SkPath apply(const SkPath& path) const;
  SkPath operator()(const SkPath& path) const { return apply(path); }
};

/** Pucker (amount < 0) & Bloat (amount > 0) — the radial power warp,
 *  ±1 full strength. EACH CONTOUR WARPS ABOUT ITS OWN centroid, so a
 *  donut's hole puckers about the hole and a word's letters each about
 *  themselves; one centroid over the whole figure would drag the outer
 *  contours across the inner ones. */
struct PuckerBloat {
  float amount = 0.5f;
  float segmentPx = 6;

  /** Value equality, dial for dial. */
  bool operator==(const PuckerBloat&) const = default;

  SkPath apply(const SkPath& path) const;
  SkPath operator()(const SkPath& path) const { return apply(path); }
};

/** Twirl — rotation about EACH CONTOUR'S OWN centroid, strongest at the
 *  middle and easing to zero at that contour's silhouette radius. */
struct Twirl {
  float angleDeg = 60;
  float segmentPx = 6;

  /** Value equality, dial for dial. */
  bool operator==(const Twirl&) const = default;

  SkPath apply(const SkPath& path) const;
  SkPath operator()(const SkPath& path) const { return apply(path); }
};

/** A step in a non-destructive recipe; every distort above converts. */
using PathOperation = std::function<SkPath(const SkPath&)>;

/** Left-to-right composition: chain({offsetBy(4), Roughen{...}}). */
PathOperation chain(std::vector<PathOperation> steps);

/** offset() as a recipe step. */
inline PathOperation offsetBy(float delta, const OffsetOptions& options = {}) {
  return
      [delta, options](const SkPath& p) { return offset(p, delta, options); };
}

}  // namespace sigil::geometry::path::operations

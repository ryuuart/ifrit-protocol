#pragma once
/** @file
 * FILLING A SHAPE WITH POINTS.
 *
 * Grains inside a figure, a circle packing's seeds, stipple, confetti,
 * the starting positions of a flock, the sites a Voronoi diagram is built
 * from — all of them are the same construction: an area, and a rule for
 * where inside it the points go. Written out by hand it becomes a
 * rejection loop with a try limit, a hand-rolled polar square root, and a
 * quadratic overlap test; written once it is an area value and a
 * distribution value.
 *
 * The area is a `Region`: a set of rings read by the even-odd rule, so a
 * rect, a disc, a glyph with counters and a scatter of islands are one
 * type. The rule is a `Distribution`: one value with props, of which
 * uniform, jittered-grid, poisson-disc and blue-noise are stock spellings
 * rather than separate functions.
 *
 * NO DRAWING HAPPENS HERE. The answer is points, in the region's own
 * coordinates; what is stamped on them is the caller's.
 */
#include <include/core/SkPath.h>
#include <include/core/SkRect.h>

#include <cstdint>
#include <glm/vec2.hpp>
#include <span>
#include <vector>

#include "sigilcore/compute/Chance.h"
#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::path {

/** THE AREA A SCATTER FILLS: rings, read by the even-odd rule.
 *
 *  Inside is inside an odd number of rings, which makes a ring within a
 *  ring a hole and two rings side by side two islands — the rule
 *  `containsEvenOdd` answers a point with, and the rule a path filled
 *  with `SkPathFillType::kEvenOdd` is drawn by, so a point placed here
 *  and a pixel painted there agree about what the shape is. A ring's own
 *  closure flag is not consulted: an area is being filled, so every ring
 *  joins its last point to its first. */
struct Region {
  std::vector<Polyline> rings;

  /** Every contour of `path`, flattened at `tolerance`. */
  [[nodiscard]] static Region of(const SkPath& path, float tolerance = 0.25f);
  /** The rect as one ring of four points. */
  [[nodiscard]] static Region of(SkRect rect);
  /** A disc as one ring of `segments` points. The ring is INSCRIBED, so
   *  it lies slightly inside the true circle; more segments close the
   *  gap. */
  [[nodiscard]] static Region disc(glm::vec2 centre, float radius,
                                   int segments = 96);
  /** The rings as given. */
  [[nodiscard]] static Region of(std::span<const Polyline> rings);

  /** The rect every ring fits in. */
  [[nodiscard]] SkRect bounds() const;
  /** Whether the point is inside, by the even-odd rule. */
  [[nodiscard]] bool contains(glm::vec2 point) const;
  /** THE AREA ENCLOSED: the signed areas of the rings summed, taken
   *  positive. A hole wound against the ring that contains it subtracts,
   *  which is how `flatten` delivers a path with counters; two rings
   *  wound the same way add, which is what two islands are. */
  [[nodiscard]] float area() const;
};

/** HOW THE POINTS ARE SPREAD, and the whole of the difference between
 *  the named scatters.
 *
 *  Three rules, because there are three: independent draws, a lattice,
 *  and a minimum separation. Everything else a catalogue would list as
 *  its own kind is one of these with a prop moved — a jittered grid is
 *  `Lattice` with `jitter` above zero, an exact grid is `Lattice` with
 *  `jitter` at zero, and blue noise is any of them with
 *  `relaxIterations` above zero. The stock values below spell each of
 *  those by name. */
enum class Spread : uint8_t {
  /** Independent draws over the region, each accepted where it lands.
   *  Clumps and leaves gaps, which is what independence looks like and
   *  is right for grain and confetti. */
  Random,
  /** A square lattice at the spacing, each point displaced inside its own
   *  cell by `jitter`. */
  Lattice,
  /** No two points closer than the radius, filled by growing outward from
   *  what has already been placed, so the answer is as dense as that
   *  separation allows rather than as dense as it was asked for. */
  Poisson,
};

/** WHAT IS HELD FIXED, and what the number `amount` therefore means. */
enum class Rate : uint8_t {
  /** `amount` points — exact for `Spread::Random`, and a CAP for the
   *  other two, which answer what their spacing or separation fits. */
  Count,
  /** `amount` points per unit area, turned into a count by the region's
   *  own area. */
  Density,
  /** `amount` between one point and the next: the lattice pitch, the
   *  poisson separation, and for `Spread::Random` the average spacing a
   *  count is derived from. */
  Spacing,
};

/** ONE SCATTER AS ONE VALUE.
 *
 *  Every member is a plain number or a small enumeration, so the value
 *  compares exactly, a memo keyed on one may be skipped, and a look
 *  chosen once for a whole drawing is carried as a token rather than as
 *  seven arguments repeated at every call. */
struct Distribution {
  Spread spread = Spread::Random;
  Rate rate = Rate::Count;
  /** The count, the density or the spacing, as `rate` says. */
  float amount = 1000;
  /** The seed the whole scatter is drawn from. One seed is one point
   *  set, to the bit, on every machine. */
  uint64_t seed = 1;
  /** Where the stream's words come from. A low-discrepancy source fills
   *  a region more evenly than a mixer does with no other change, which
   *  is why it is a prop here rather than a fourth spread. */
  core::chance::Source source = core::chance::Source::Pcg;
  /** The word a sequence source needs — a Halton base, a stratum count —
   *  and nothing for the mixers. */
  uint32_t parameter = 0;
  /** `Spread::Lattice` only: how far inside its own cell a point may
   *  wander, 0 (the exact lattice) to 1 (anywhere in the cell). */
  float jitter = 1.0f;
  /** HOW MANY RELAXATION PASSES the placed points take, pushing each
   *  other apart without leaving the region. Zero is the raw scatter;
   *  above zero walks any spread toward blue noise, which is why blue
   *  noise is not a spread of its own. */
  int relaxIterations = 0;
  /** A BOUND, not a preference: a density over a large region would
   *  otherwise answer with a vector nobody asked the size of. */
  int maxPoints = 1000000;

  friend bool operator==(const Distribution&, const Distribution&) = default;
};

/** THE POINTS. In the region's own coordinates, in the order they were
 *  placed. An empty region, a non-positive amount or a `maxPoints` of
 *  zero all answer nothing. */
[[nodiscard]] std::vector<glm::vec2> sample(const Region& region,
                                            const Distribution& distribution);

// ---- the stock spellings --------------------------------------------------
//
// Each is one `Distribution` with its props set, named for what a caller
// asks for. They add no behaviour: anything one of them spells can be
// spelled by setting the same fields, and a caller that wants to vary a
// scatter takes the value one of these answers and edits it.

/** `count` independent draws over the region. */
[[nodiscard]] Distribution uniform(int count, uint64_t seed = 1);
/** As many points as fit with no two closer than `radius`. */
[[nodiscard]] Distribution poisson(float radius, uint64_t seed = 1);
/** About `count` points, pushed apart until they are evenly spread: an
 *  independent scatter plus the relaxation passes. */
[[nodiscard]] Distribution blueNoise(int count, uint64_t seed = 1,
                                     int iterations = 6);
/** An exact square lattice at `spacing`, clipped to the region. */
[[nodiscard]] Distribution grid(float spacing);
/** The same lattice with each point moved inside its own cell. */
[[nodiscard]] Distribution jittered(float spacing, uint64_t seed = 1,
                                    float jitter = 1.0f);

}  // namespace sigil::geometry::path

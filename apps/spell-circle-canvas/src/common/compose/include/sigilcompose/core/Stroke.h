#pragma once

/** @file
 * SigilCompose stroke grammar — WHERE a stroke goes and HOW a composite
 * mark is built. Spans claims runs of a boundary by arc length and the
 * `spans::` factories spell the claims; Across names a band's width and
 * Around its borrowed spine; StrandPath says where one strand of a
 * composite runs; and the resolver seams are what the kernel answers a
 * span claim through.
 *
 * The path arithmetic under all of it is SigilGeometry's: the width law
 * (`geometry::path::Profile`), the deviation
 * (`geometry::path::Shaper`), the band a width cuts
 * (`geometry::path::bandRegion`, `geometry::path::Formation`) and who
 * passes over whom where two paths meet
 * (`geometry::path::CrossingRule`).
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilcore/comparable/Erased.h>
#include <sigilgeometry/path/Band.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilgeometry/path/Crossings.h>
#include <sigilgeometry/path/Profile.h>
#include <sigilgeometry/path/Shaper.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/schedule/Schedule.h>
#include <sigilmotion/values/Animated.h>

#include <any>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sigil::compose {

namespace detail {
struct Instance;
}  // namespace detail
class StrokeResolverOps;

// ---------------------------------------------------------------------------
// The stroke grammar — WHERE a stroke goes
//
// The words: SHAPE is the region an element occupies (Element::shape);
// LINE is an element whose shape is an open path; BAND is a derived shape
// around a spine (band(), below); STROKE is the slot that dresses a
// boundary, and BRUSH is what paints. "Frame" and "border" are not
// concepts — they are strokes of a boundary; "bounding box" is
// query-side vocabulary (Composer::bounds), never a shape.

/** One claimed run of a boundary, as fractions of its TOTAL arc length —
 *  every contour end to end. This is `SkTrimPathEffect`'s coordinate, and
 *  the one every span, reveal and motion path in the library speaks. */
struct Span {
  float begin = 0.0f, end = 1.0f;
  bool operator==(const Span&) const = default;
};

/** What a Spans value is resolved against. `fitRects` are the derive
 *  pass's answers for spans::fit(), keyed; `values` holds the resolved
 *  animatable endpoints in declaration order — THREE per term, `begin`,
 *  `end`, `offset` — which is how a reveal can be a transition or a
 *  binding without Spans knowing about either. Short arrays are
 *  tolerated: a missing slot reads its default, so a caller that only
 *  cares about endpoints may pass two. */
struct SpanInput {
  const SkPath* outline = nullptr;
  const std::vector<std::pair<std::string, SkRect>>* fitRects = nullptr;
  const std::vector<float>* values = nullptr;
};

/** WHERE a stroke pass goes: a comparable value built by the `spans::`
 *  factories and combined with `|` (union).
 *
 *  **FRACTION 0 IS THE BOTTOM-LEFT CORNER** on a box, and the boundary
 *  runs UP the left edge from there. That is `SkPath::addRRect`'s own
 *  convention (start index 3, clockwise in Skia's y-down space), inherited
 *  unchanged. Anything reasoning about WHERE a fraction lands needs it:
 *  `upTo(0.25)` on a square claims the LEFT edge, not the top one. A
 *  custom `shape()` seams wherever its own path starts.
 *
 *  Deliberately a CLOSED vocabulary rather than an open seam. The seam
 *  convention — one named required member on a comparable value — governs
 *  shapers, profiles and crossing rules, whose whole point is that a user
 *  writes new ones. A span is an interval set instead: richer values such
 *  as `spans::brackets` are COMPOSITIONS of these terms, not new
 *  kinds, so the value stays trivially comparable and prunable. */
class Spans {
 public:
  enum class Rule : uint8_t {
    Range,    ///< [begin, end] outright — and upTo(t) is range(0, t)
    Wrap,     ///< [begin, end] on the boundary read as a CYCLE (spans::wrap)
    Corners,  ///< a window of `arm` px either side of every tangent break
    Edges,    ///< everything EXCEPT within `arm` px of a break
    Every,    ///< `count` equal slots, each claiming its leading `duty`
    At,       ///< one slot (`index`) of `count`
    Fit,      ///< the run the keyed element covers, grown by `margin` px
    Rest,     ///< the complement (see Element::stroke)
  };
  /** One term of the union. Only the members its Rule reads are
   *  meaningful; the rest keep their defaults so the value compares. */
  struct Term {
    Rule rule = Rule::Range;
    motion::Animatable<float> begin = 0.0f, end = 1.0f;
    /** Added to BOTH endpoints before the interval is read. Read by Range
     *  and Wrap only; other rules ignore it.
     *
     *  It exists for the one case endpoint arithmetic cannot spell: a
     *  window whose ENDS are driven by one Output and whose POSITION is
     *  driven by another. A bound endpoint holds exactly one source
     *  pointer, and summing two live values into one number needs two. */
    motion::Animatable<float> offset = 0.0f;
    float arm = 0.0f;          ///< Corners/Edges: px of arc length
    float angleDeg = 30.0f;    ///< Corners/Edges: the tangent break that counts
    float duty = 1.0f;         ///< Every: fraction of each slot claimed
    float margin = 0.0f;       ///< Fit: px grown around the keyed content
    int count = 1, index = 0;  ///< Every/At
    std::string key;           ///< Fit: the content key; Rest: the pass name
  };
  std::vector<Term> terms;

  /** SLIDE THE WHOLE CLAIM: `by` is added to both endpoints of every
   *  Range/Wrap term. Same value kind as the endpoints themselves, so it
   *  may be a constant, an `animate(...)` or a bound Output.
   *
   *      .stroke(spans::wrap(&start, &end).offset(&drift), ants)
   *
   *  Endpoint arithmetic (`bind(&o).offset(w)`) covers every case where
   *  ONE Output drives the window; this covers the case where the ends
   *  and the position are driven independently.
   *
   *  Three things about the call, all of them easy to assume wrongly:
   *  - it MUTATES and returns `*this` by reference, so it chains off a
   *    temporary safely only while that temporary lives — bind the result
   *    to a `Spans` value (or pass it straight to `stroke()`, which takes
   *    one by value) rather than to `auto &`;
   *  - it applies to the terms PRESENT AT CALL TIME, so
   *    `range(a,b).offset(o) | corners(8)` offsets only the range, while
   *    `(range(a,b) | corners(8)).offset(o)` writes both (the corner term
   *    ignores it);
   *  - on an empty value it does nothing, silently — there is no term to
   *    carry the offset and nothing to warn about. */
  Spans& offset(motion::Animatable<float> by);

  /** Structural equality. Declared here and defined beside the
   *  reconciler's own property comparator, so an animated endpoint
   *  compares the way every other animated property does — by binding
   *  identity when live, by value when described. */
  bool operator==(const Spans& other) const;

  /** Resolve to intervals. Rest terms return nothing — the complement
   *  needs the element's OTHER passes and is computed by the painter. */
  std::vector<Span> resolve(const SpanInput& in) const;
  /** How many floats `SpanInput::values` must carry: begin, end and
   *  offset, in that order, for every term. */
  size_t valueCount() const { return terms.size() * 3; }
  bool hasRest() const {
    for (const Term& t : terms)
      if (t.rule == Rule::Rest) return true;
    return false;
  }
};

/** Union. `spans::corners(18) | spans::at(0, 4)` is one pass. */
inline Spans operator|(Spans a, const Spans& b) {
  a.terms.insert(a.terms.end(), b.terms.begin(), b.terms.end());
  return a;
}

/** The span factories — the WHERE half of `.stroke(where, what)`. */
namespace detail {
/** The corner scan every stroke grammar and decoration shares: the
 *  geometry library's `Contour::corners`, with one diagnostic attached. A
 *  scan that finds nothing on a contour whose sharpest turn is above the
 *  noise a smooth curve produces at this step (4°) says so once, because
 *  a threshold that is simply too high for the shape is the common
 *  authoring mistake and draws nothing without it. */
std::vector<geometry::path::Contour::Corner> cornersOrWarn(
    const geometry::path::Contour& contour, float angleDeg,
    float minSpacing = 3.0f, float step = 2.0f);
/** The same diagnostic for a whole path, ahead of a corner construction
 *  that reports nothing itself. */
void warnIfNoCorners(const SkPath& path, float angleDeg);
}  // namespace detail

namespace spans {
/** `[begin, end]` of the boundary's arc length. Both ends take the full
 *  Animatable treatment (constant, `animate(...)`, or a bound Output). */
Spans range(motion::Animatable<float> begin, motion::Animatable<float> end);
/** THE SEAM-CROSSING RANGE: the boundary read as a CYCLE, so a window
 *  whose `begin` is past its `end` claims [begin,1] AND [0,end] — the
 *  marching-ants and orbiting-comet idiom.
 *
 *      .stroke(spans::wrap(bind(&phase), bind(&phase).offset(0.25f)), ants)
 *
 *  Both ends take the full Animatable treatment, so the window marches by
 *  driving them; two shaped bindings on ONE Output are how a fixed-length
 *  window is spelled.
 *
 *  A DEDICATED TERM rather than `range()` learning to wrap, for two
 *  reasons. `range(0.9, 0.1)` is legal and means the empty/reversed
 *  window that endpoint normalisation swaps, so teaching it to wrap would
 *  silently change what existing descriptions draw. And the no-overlap
 *  law reads over RESOLVED runs, where this is the only term that yields
 *  two runs from one pair of endpoints — a reader tracking down a claim
 *  conflict needs the call site to say that the term is cyclic.
 *
 *  DEGENERATE ENDS are read from the RAW endpoints, before the fractional
 *  wrap: `end - begin <= 0` claims nothing and `>= 1` claims the whole
 *  boundary, which is why a window driven past 1.0 keeps its length. The
 *  seam itself (fraction 0) is the outline's own start point, and a
 *  seam-crossing claim is stitched into ONE contour so caps and additive
 *  brushes never double-hit there. */
Spans wrap(motion::Animatable<float> begin, motion::Animatable<float> end);
/** THE REVEAL: `range(0, end)`. `spans::upTo(animate(from(0.f).to(1.f),
 *  {600ms}))` is a stroke that DRAWS ON, and a bound Output scrubs it.
 *  Works the same way under every brush, because it claims a run of the
 *  boundary rather than modifying the mark. */
Spans upTo(motion::Animatable<float> end);
/** A window of `arm` px of arc length either side of every tangent break
 *  — the four corner L's, and the reticle bracket vocabulary. Follows any
 *  silhouette: chamfer the shape and the marks move to the chamfers.
 *  `angleDeg` is what counts as a break. A regular n-gon turns 360/n at
 *  each vertex, so at the 30° default nothing above 12 sides registers a
 *  corner at all — lower the angle for rounder silhouettes. The scan
 *  warns rather than adapting the threshold for you. */
Spans corners(float arm, float angleDeg = 30.0f);
/** The complement of corners(): the runs BETWEEN the breaks, stopping
 *  `arm` px short of each — the rule with open corners. */
Spans edges(float arm, float angleDeg = 30.0f);
/** `count` equal slots around the boundary, each claiming the leading
 *  `duty` of its own slot. `duty == 1` tiles the boundary completely. */
Spans every(int count, float duty = 1.0f);
/** One slot of `count` — `every()`'s singular. Positional, so it moves
 *  when the geometry changes; use it on settled compositions. */
Spans at(int index, int count);
/** The run the KEYED element covers, grown by `margin` px: a gap sized
 *  from content, resolved in the derive phase against that element's
 *  resolved box (the flowAround pattern, applied to a boundary). */
Spans fit(std::string_view key, float margin = 0.0f);
/** Everything this element's other CLAIMING passes left over. */
Spans rest();
/** The complement of ONE named pass — and, unlike bare rest(), allowed
 *  to overlay other passes on purpose. */
Spans rest(std::string_view passName);
}  // namespace spans

/** The band's width, named at the call site: `band(spine, across(22))`.
 *  Takes a constant or any Profile (a taper, a kit oscillation). */
struct Across {
  geometry::path::Profile profile;
  /** The brush engine that sweeps the profile into a region — installed by
   *  `across()`, excluded from equality. */
  core::Erased<StrokeResolverOps> resolver;
  bool operator==(const Across& o) const { return profile == o.profile; }
  /** FIELD PIN: a member added here must be ruled on in operator== above,
   *  then this count bumped. `resolver` is excluded on purpose. */
  static void fieldPin(Across& v) {
    auto& [profile, resolver] = v;
    static_assert(
        std::tuple_size_v<decltype(std::tie(profile, resolver))> == 2,
        "Across gained or lost a member — rule on it in Across::operator== "
        "(the resolver is excluded), then bump this count.");
  }
};
// the profile copies its scheme into owned storage; nothing on the stack
// outlives the call
// NOLINTNEXTLINE(clang-analyzer-core.StackAddressEscape)
/** Defined by the brush tier, which installs the engine that sweeps the
 *  profile: `across()` is a brush verb, and a band drawn through it links
 *  SigilComposeBrush. */
Across across(float px);
Across across(geometry::path::Profile p);

// ---------------------------------------------------------------------------
// THE STROKE RESOLVER — the seam the kernel resolves span claims through

/** The interval arithmetic every span answer is read with. One body for
 *  the stroke passes and the mask gates, because a pass under a gate
 *  claims `where ∩ gate` and two spellings of the intersection would let
 *  the two disagree. */
class SpanArithmeticOps {
 public:
  virtual ~SpanArithmeticOps() = default;
  /** Clamp to [0,1], drop empties, sort and merge — the one normal form
   *  every span answer is in. */
  virtual std::vector<Span> normalize(const std::vector<Span>& spans) const = 0;
  /** The runs BOTH normalized sets cover; the answer is normalized too. */
  virtual std::vector<Span> intersect(const std::vector<Span>& a,
                                      const std::vector<Span>& b) const = 0;
  /** Everything in [0,1] a normalized set does not cover. */
  virtual std::vector<Span> complement(
      const std::vector<Span>& spans) const = 0;
  /** The sub-geometry of @p src covered by @p spans — fractions of the
   *  path's TOTAL arc length, SkTrimPathEffect's coordinate. */
  virtual SkPath spanPath(const SkPath& src,
                          const std::vector<Span>& spans) const = 0;
};

/** WHAT THE KERNEL ASKS OF A BOUNDARY'S STROKE GRAMMAR: which runs each
 *  span-qualified pass claims this frame, and the region a band's spine
 *  sweeps. Installed on the description by `stroke(spans, …)`,
 *  `background(spans, …)` and `across()`; a description built without
 *  those verbs carries none, and the kernel then paints no span pass and
 *  no band region. */
class StrokeResolverOps : public SpanArithmeticOps {
 public:
  /** Every stroke pass's claimed runs for this frame, in pass order, with
   *  rest() complements applied — resolved against @p outline, the node's
   *  UNMASKED boundary. Empty when the node has no passes. */
  virtual std::vector<std::vector<Span>> claims(
      const detail::Instance& inst, const SkPath& outline) const = 0;
  /** The region @p spine sweeps at @p width across it, on @p formation's
   *  side. Empty when the profile is zero everywhere. */
  virtual SkPath bandRegion(const SkPath& spine, const Across& width,
                            geometry::path::Formation formation) const = 0;
};

/** The resolver as a description carries it — on its stroke passes and on
 *  a band's width — excluded from structural equality. */
using StrokeResolver = core::Erased<StrokeResolverOps>;

/** A band spine borrowed from another element's resolved shape, through
 *  the derive phase: `band(around("dial"), across(14))`. */
struct Around {
  std::string key;
  bool operator==(const Around&) const = default;
};
inline Around around(std::string_view key) { return Around{std::string(key)}; }

// ---------------------------------------------------------------------------
// Strands — WHERE a composite's marks run

/** Where one strand of a composite runs. Two families:
 *
 *  RELATIVE — a displacement of the stroked boundary in its (along,
 *  across) frame, **the same frame a band owns** (across positive to the
 *  LEFT of travel; see bandPointAt). Any Profile is one:
 * `geometry::path::profile::self()` rides the boundary,
 * `geometry::path::profile::offset(px)` runs parallel, and a custom profile
 * value is accepted here directly.
 *
 *  ABSOLUTE — `strand::from(key)` borrows a keyed element's resolved path
 *  through the derive phase, and `strand::path(p)` is authored geometry
 *  (SkPath is comparable, so it prunes). **With only absolute strands the
 *  boundary is an unpainted host** — nothing runs on it, which is a real
 *  and useful shape of composite.
 *
 *  Paths are DATA: a path participates as an element's shape, as borrowed
 *  geometry, or as pure guide data in no tree. This is the third case. */
class StrandPath {
 public:
  enum class Source : uint8_t { Relative, Borrowed, Authored };

  StrandPath() = default;
  StrandPath(geometry::path::Profile p)  // NOLINT: implicit by design (.path =
                                         // geometry::path::profile::self())
      : m_source(Source::Relative), m_profile(std::move(p)) {}
  static StrandPath borrowed(std::string key) {
    StrandPath s;
    s.m_source = Source::Borrowed;
    s.m_key = std::move(key);
    return s;
  }
  static StrandPath authored(SkPath path) {
    StrandPath s;
    s.m_source = Source::Authored;
    s.m_path = std::move(path);
    return s;
  }

  Source source() const { return m_source; }
  const geometry::path::Profile& profile() const { return m_profile; }
  const std::string& key() const { return m_key; }
  const SkPath& path() const { return m_path; }
  /** How far off the boundary this strand can run — 0 for the absolute
   *  family, whose geometry is its own. */
  float reach() const {
    return m_source == Source::Relative ? m_profile.max() : 0.0f;
  }
  bool operator==(const StrandPath& o) const {
    return m_source == o.m_source && m_profile == o.m_profile &&
           m_key == o.m_key && m_path == o.m_path;
  }

 private:
  Source m_source = Source::Relative;
  geometry::path::Profile m_profile;
  std::string m_key;
  SkPath m_path;
};

namespace strand {
/** Borrow a keyed element's resolved path (derive phase, cycle-guarded
 *  like every other borrow). */
inline StrandPath from(std::string_view key) {
  return StrandPath::borrowed(std::string(key));
}
/** Authored geometry, in the host element's local space. */
inline StrandPath path(SkPath p) { return StrandPath::authored(std::move(p)); }
}  // namespace strand

}  // namespace sigil::compose

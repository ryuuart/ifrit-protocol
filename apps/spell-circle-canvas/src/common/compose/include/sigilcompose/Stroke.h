#pragma once

/** @file
 * SigilCompose stroke grammar — WHERE a stroke goes and HOW a composite mark
 * is built. Spans claims runs of a boundary by arc length and the `spans::`
 * factories spell the claims; the Profile seam says how far a mark sits
 * across its spine, with Across, Around and Formation naming a band's
 * width, spine and side; the Shaper seam is the one way geometry deviates;
 * StrandPath says where one strand of a composite runs; and Crossing,
 * CrossingRule and the `crossing::` rules decide which strand is on top
 * where two meet.
 */

#include <include/core/SkPath.h>
#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigilgeometry/Contour.h>

#include <any>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilcompose/Motion.h"

namespace sigil::compose {

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
 *  as `kit::spans::brackets` are COMPOSITIONS of these terms, not new
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
    Animatable<float> begin = 0.0f, end = 1.0f;
    /** Added to BOTH endpoints before the interval is read. Read by Range
     *  and Wrap only; other rules ignore it.
     *
     *  It exists for the one case endpoint arithmetic cannot spell: a
     *  window whose ENDS are driven by one Output and whose POSITION is
     *  driven by another. A bound endpoint holds exactly one source
     *  pointer, and summing two live values into one number needs two. */
    Animatable<float> offset = 0.0f;
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
  Spans& offset(Animatable<float> by);

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
std::vector<geometry::Contour::Corner> cornersOrWarn(
    const geometry::Contour& contour, float angleDeg, float minSpacing = 3.0f,
    float step = 2.0f);
/** The same diagnostic for a whole path, ahead of a corner construction
 *  that reports nothing itself. */
void warnIfNoCorners(const SkPath& path, float angleDeg);
}  // namespace detail

namespace spans {
/** `[begin, end]` of the boundary's arc length. Both ends take the full
 *  Animatable treatment (constant, `animate(...)`, or a bound Output). */
Spans range(Animatable<float> begin, Animatable<float> end);
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
Spans wrap(Animatable<float> begin, Animatable<float> end);
/** THE REVEAL: `range(0, end)`. `spans::upTo(animate(from(0.f).to(1.f),
 *  {600ms}))` is a stroke that DRAWS ON, and a bound Output scrubs it.
 *  Works the same way under every brush, because it claims a run of the
 *  boundary rather than modifying the mark. */
Spans upTo(Animatable<float> end);
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

// ---------------------------------------------------------------------------
// The profile seam — how far a mark sits ACROSS its spine

/** A profile value: `float across(float along) const`, `float max()
 *  const`, and EQUALITY. Both extra members are required, and both are
 *  load-bearing.
 *
 *  `max()` is what every cull and bleed calculation is sized from. A
 *  varying width whose reach cannot be asked for can only be clipped, and
 *  clipping in a cached picture is silent.
 *
 *  Equality is required because a profile is read LIVE, every frame.
 *  Anything an author hands the library must participate in reconciler
 *  equality, or a node that prunes goes on reading the value it was
 *  described with and never sees the new one. An incomparable callable is
 *  therefore not a profile; write a struct with `operator==`.
 *
 *  A PROFILE THAT RETURNS A NON-FINITE WIDTH DELETES THE WHOLE BAND. One
 *  NaN vertex makes the built path non-finite and Skia draws none of it,
 *  with no error. The seam does not guard this — clamp inside your own
 *  law. Trigonometric laws are the usual source: `sqrt(sin(pi*along))` is
 *  NaN at `along == 1` because the float pi rounds up.
 *
 *  `along` is a fraction of the spine's arc length; `across` is px on its
 *  normal, positive to the LEFT of travel — see bandPointAt for the one
 *  statement of that convention. */
template <typename P>
concept ProfileScheme =
    std::equality_comparable<P> && requires(const P& p, float along) {
      { p.across(along) } -> std::convertible_to<float>;
      { p.max() } -> std::convertible_to<float>;
    };

/** THE PX KEY — optional, one line.
 *
 *  A scheme that declares `static constexpr bool alongIsPx = true` is
 *  keyed in PX OF ARC LENGTH from the spine's start rather than in a
 *  fraction of it. Consumers that have measured their spine
 *  (`profileOffset`, the band's rails) hand it `along * lengthPx` through
 *  `Profile::acrossAt`. Nothing else about the seam changes, and a scheme
 *  that says nothing stays fraction-keyed.
 *
 *  WHY IT EXISTS. A decoration under a reveal (`spans::upTo`, a span
 *  gate) is handed the REVEALED contour, so a fraction is a fraction of
 *  what has been drawn SO FAR: a law keyed to it SLIDES along the mark as
 *  the reveal grows. That looks identical in a still frame and wrong in
 *  motion. Absolute distance from the start does not move, which is what
 *  a calligraphic pressure law or a flow-width law actually means.
 *
 *  The conversion cannot live in the author's value, because it needs the
 *  length of the contour ACTUALLY being painted and only the paint-time
 *  consumer knows that. So the seam converts, once, for every consumer. */
template <typename P>
concept PxKeyedProfileScheme = ProfileScheme<P> && requires {
  { P::alongIsPx } -> std::convertible_to<bool>;
};

/** Type-erased comparable profile — Decoration's pattern applied to the
 *  width seam. One shared vocabulary: a band's taper, a weave strand's
 *  offset and a ribbon's width are all this same value. */
class Profile {
 public:
  template <ProfileScheme P>
  Profile(P scheme)  // NOLINT: implicit by design (across(myTaper))
      : m_max((float)scheme.max()) {
    if constexpr (PxKeyedProfileScheme<P>) m_alongIsPx = P::alongIsPx;
    // The concept requires equality, so every profile keeps a comparator —
    // there is no conservatively-unequal fallback here, unlike Decoration.
    m_held = scheme;
    m_equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const P&>(a) == std::any_cast<const P&>(b);
    };
    m_across = [s = std::move(scheme)](float along) { return s.across(along); };
  }
  Profile() = default;

  /** The law at `along`, IN THE PROFILE'S OWN KEY — a fraction of the
   *  spine normally, px of arc length when `keyedInPx()`. A consumer that
   *  has measured its spine should call `acrossAt` instead and never think
   *  about which. */
  float across(float along) const { return m_across ? m_across(along) : 0.0f; }
  /** The law at `along`, ALWAYS a fraction of the spine, given the spine's
   *  measured length in px. The one call `profileOffset` and the band's
   *  rails make: it is the bridge that lets a px-keyed law stay put under
   *  a reveal (see PxKeyedProfileScheme). */
  float acrossAt(float along, float lengthPx) const {
    return across(m_alongIsPx ? along * lengthPx : along);
  }
  /** Is this profile's law keyed in px of arc length rather than in
   *  fraction? Part of the value's TYPE, so it never differs between two
   *  profiles that compare equal. */
  bool keyedInPx() const { return m_alongIsPx; }
  /** The widest this profile ever reaches — what bleed and cull are
   *  computed from, so nothing it draws is silently truncated. */
  float max() const { return m_max; }
  bool operator==(const Profile& o) const {
    // Reflexive on the DEFAULT-CONSTRUCTED value too: two empty profiles
    // are the same nothing, and a value that does not compare equal to
    // itself makes every containing description patch forever.
    if (!m_equals || !o.m_equals) return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

 private:
  float m_max = 0.0f;
  bool m_alongIsPx = false;
  std::function<float(float)> m_across;
  std::any m_held;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

/** The core profile presets: the two every other profile is defined
 *  against. Richer families — the oscillating `wave`, and `braid` built on
 *  it — live in kit, since the kernel only needs to hold the seam. */
namespace strand {
/** across ≡ 0: the boundary itself. */
struct Self {
  float across(float) const { return 0.0f; }
  float max() const { return 0.0f; }
  bool operator==(const Self&) const = default;
};
/** across ≡ px: a parallel. Parallels are rails — they never cross.
 *
 *  **Positive is LEFT of travel**, which is outside a clockwise path.
 *  `kit::brush::shapers::offset`, `geometry::parallel`,
 *  `lines::Rail::across`, `Profile::across` and `TextPath::offset` all
 *  mean this same side; see bandPointAt. */
struct Offset {
  float px = 0.0f;
  float across(float) const { return px; }
  float max() const { return std::abs(px); }
  bool operator==(const Offset&) const = default;
};
inline Profile self() { return Profile(Self{}); }
inline Profile offset(float px) { return Profile(Offset{px}); }
}  // namespace strand

/** The band's width, named at the call site: `band(spine, across(22))`.
 *  Takes a constant or any Profile (a taper, a kit oscillation). */
struct Across {
  Profile profile;
  bool operator==(const Across&) const = default;
};
inline Across across(float px) { return Across{strand::offset(px)}; }
inline Across across(Profile p) { return Across{std::move(p)}; }

/** Which side of the spine the band occupies. Explicit because the
 *  offset-path lineage has no defensible default beyond "both". */
enum class Formation : uint8_t { Centered, Outward, Inward };

/** A band spine borrowed from another element's resolved shape, through
 *  the derive phase: `band(around("dial"), across(14))`. */
struct Around {
  std::string key;
  bool operator==(const Around&) const = default;
};
inline Around around(std::string_view key) { return Around{std::string(key)}; }

// ---------------------------------------------------------------------------
// The shaper seam — the ONE way geometry deviates

/** A shaper value: `SkPath shape(const SkPath &) const`, plus equality.
 *
 *  It bends ONE CONTINUOUS MARK — a wave, a zigzag, a jitter, an offset —
 *  and that is the whole of the geometry-deviation vocabulary. Building a
 *  mark out of repeated CELLS instead is a pattern, which is a brush kind
 *  rather than a shaper; the two are named apart because they compose
 *  differently.
 *
 *  SkPath in, SkPath out: dash and width are path operations, so nothing
 *  richer is needed. `bleed()` is optional and declares how far the
 *  deviation reaches (a wave's amplitude), so the paint cull can grow by
 *  it and a cached picture is not truncated.
 *
 *  There are deliberately no sugar methods over this seam. Stock shapers
 *  are ordinary kit values (`kit::brush::shapers::`), peers of anything
 *  you write — which is what a seam is for. */
template <typename S>
concept ShaperScheme =
    std::equality_comparable<S> && requires(const S& s, const SkPath& p) {
      { s.shape(p) } -> std::convertible_to<SkPath>;
    };

/** Type-erased comparable shaper. */
class Shaper {
 public:
  template <ShaperScheme S>
  Shaper(S scheme)  // NOLINT: implicit by design (.shaped(myWave))
      : m_bleed([&] {
          if constexpr (requires {
                          { scheme.bleed() } -> std::convertible_to<float>;
                        })
            return (float)scheme.bleed();
          else
            return 0.0f;
        }()) {
    m_held = scheme;
    m_equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const S&>(a) == std::any_cast<const S&>(b);
    };
    m_shape = [s = std::move(scheme)](const SkPath& p) { return s.shape(p); };
  }
  Shaper() = default;

  SkPath shape(const SkPath& p) const { return m_shape ? m_shape(p) : p; }
  float bleed() const { return m_bleed; }
  bool operator==(const Shaper& o) const {
    if (!m_equals || !o.m_equals) return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

 private:
  float m_bleed = 0.0f;
  std::function<SkPath(const SkPath&)> m_shape;
  std::any m_held;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

// ---------------------------------------------------------------------------
// Strands — WHERE a composite's marks run

/** Where one strand of a composite runs. Two families:
 *
 *  RELATIVE — a displacement of the stroked boundary in its (along,
 *  across) frame, **the same frame a band owns** (across positive to the
 *  LEFT of travel; see bandPointAt). Any Profile is one: `strand::self()`
 *  rides the boundary, `strand::offset(px)` runs parallel, and a custom
 *  profile value is accepted here directly.
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
  StrandPath(Profile p)  // NOLINT: implicit by design (.path = strand::self())
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
  const Profile& profile() const { return m_profile; }
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
  Profile m_profile;
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

/** Displace a path in its own (along, across) frame — the primitive
 *  behind a relative strand, and exactly the band's frame: `along` is a
 *  fraction of total arc length, positive `across` is LEFT of travel
 *  (outside a clockwise path). A constant profile delegates to
 *  `geometry::parallel`, which means the same side. */
SkPath profileOffset(const SkPath& spine, const Profile& profile);

/** THE REGION a band occupies: the spine walked at both profile rails,
 *  per contour, through `profileOffset` — so corners get
 *  `geometry::parallel`'s real-vertex repair (arc outside a turn, miter
 *  inside) instead of the sample-and-displace spur a naive walk leaves on
 *  the inside of every rectangle.
 *
 *  Public because a varying-width MARK along a spine IS this region: a
 *  milled groove, or a ribbon, is this band filled. Sharing one geometry
 *  keeps the corner repair from being reimplemented per consumer. */
SkPath bandRegion(const SkPath& spine, const Across& width,
                  Formation formation = Formation::Centered);

// ---------------------------------------------------------------------------
// Crossings — WHICH mark is on top where two strands meet

/** Who is on top. Read against a Crossing's `a`, which is always the
 *  LOWER strand index, so the question is well-posed: Over means strand
 *  `a` passes over strand `b`. */
enum class Order : uint8_t { Over, Under };

/** One discovered crossing. **Crossings are never authored** — they are
 *  found by path intersection and numbered along the boundary. */
struct Crossing {
  /** ORDINAL in the discovered list, 0-based: the crossings are sorted by
   *  `alongA` (position on the lower-indexed strand) and then numbered.
   *  It is NOT a coordinate in any parameterisation and NOT stable under
   *  a change of geometry — add a strand or move one and the same knot may
   *  take a different ordinal. This is the number `CrossingRule::except()`
   *  pins, which is exactly why pins are documented as positional. */
  size_t index = 0;
  /** Strand indices, always `a < b` — `b` is the one list order paints
   *  later, i.e. on top when nothing says otherwise. */
  size_t a = 0, b = 0;
  SkPoint at{0, 0};
  /** Where the crossing falls along each strand, as fractions of that
   *  strand's arc length. */
  float alongA = 0.0f, alongB = 0.0f;
  bool operator==(const Crossing&) const = default;
};

/** A crossing rule value: `Order decide(const Crossing &) const`, plus
 *  equality — one named required member on a comparable value, like every
 *  other seam here. Never a bare lambda: a rule is read live every frame,
 *  so it has to participate in reconciler equality or the node holding it
 *  can never prune. */
template <typename D>
concept CrossingScheme =
    std::equality_comparable<D> && requires(const D& d, const Crossing& c) {
      { d.decide(c) } -> std::convertible_to<Order>;
    };

/** The rule ladder, as ONE comparable value. Climb only as far as the
 *  composition needs:
 *
 *      crossing::alternate()                    // == sequence({Over, Under})
 *      crossing::sequence({Over, Over, Under})  // any repeating pattern
 *      crossing::pairs({{0,1},{1,2},{2,0}})     // dominance, cyclic allowed
 *      MyRule{}                                 // your own decide() value
 *
 *  and pin exceptions onto whatever you chose with `.except(i, order)`.
 *
 *  The default is LIST ORDER: later strands pass over earlier ones. That
 *  is what makes `layers` and `weave` formally one machine. */
class CrossingRule {
 public:
  CrossingRule() = default;
  template <CrossingScheme D>
  CrossingRule(D scheme)  // NOLINT: implicit by design (.crossing = MyRule{})
      : m_kind(Kind::Custom) {
    m_held = scheme;
    m_equals = [](const std::any& x, const std::any& y) {
      return std::any_cast<const D&>(x) == std::any_cast<const D&>(y);
    };
    m_decide = [s = std::move(scheme)](const Crossing& c) {
      return s.decide(c);
    };
  }

  static CrossingRule sequence(std::vector<Order> pattern) {
    CrossingRule r;
    r.m_kind = Kind::Sequence;
    r.m_pattern = std::move(pattern);
    return r;
  }
  static CrossingRule pairs(std::vector<std::pair<int, int>> dominance) {
    CrossingRule r;
    r.m_kind = Kind::Pairs;
    r.m_dominance = std::move(dominance);
    return r;
  }

  /** Pin ONE crossing, layered over whatever rule this already is.
   *
   *  **Pins are POSITIONAL**: the index is a position in the discovered
   *  order, so a stable RULE survives a geometry change and a pin does
   *  not — move a strand and pin 3 lands on a different meeting. Use
   *  rules while a composition is still moving, and pins only once it is
   *  settled and you are correcting one knot by eye.
   *
   *  Pins compose onto the base rule and never stack as separate
   *  entries: there is one `.crossing` field, and this is how it takes
   *  exceptions. */
  CrossingRule& except(size_t index, Order order) {
    for (auto& pin : m_pins)
      if (pin.first == index) {
        pin.second = order;
        return *this;
      }
    m_pins.emplace_back(index, order);
    return *this;
  }

  Order decide(const Crossing& c) const {
    for (const auto& pin : m_pins)
      if (pin.first == c.index) return pin.second;
    switch (m_kind) {
      case Kind::Sequence:
        if (!m_pattern.empty()) return m_pattern[c.index % m_pattern.size()];
        break;
      case Kind::Pairs:
        for (const auto& [over, under] : m_dominance) {
          if (over == (int)c.a && under == (int)c.b) return Order::Over;
          if (over == (int)c.b && under == (int)c.a) return Order::Under;
        }
        break;
      case Kind::Custom:
        if (m_decide) return m_decide(c);
        break;
      case Kind::ListOrder:
        break;
    }
    // List order: `b` is later in the list, so `a` is underneath.
    return Order::Under;
  }

  bool operator==(const CrossingRule& o) const {
    if (m_kind != o.m_kind || m_pattern != o.m_pattern ||
        m_dominance != o.m_dominance || m_pins != o.m_pins)
      return false;
    if (m_kind != Kind::Custom) return true;
    if (!m_equals || !o.m_equals) return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

 private:
  enum class Kind : uint8_t { ListOrder, Sequence, Pairs, Custom };
  Kind m_kind = Kind::ListOrder;
  std::vector<Order> m_pattern;
  std::vector<std::pair<int, int>> m_dominance;
  std::vector<std::pair<size_t, Order>> m_pins;
  std::function<Order(const Crossing&)> m_decide;
  std::any m_held;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

namespace crossing {
/** Over, under, over, under — the plain-weave rule, and formally just
 *  `sequence({Over, Under})`. Both spellings exist because they name two
 *  author intents over one machine. */
inline CrossingRule alternate() {
  return CrossingRule::sequence({Order::Over, Order::Under});
}
inline CrossingRule sequence(std::vector<Order> pattern) {
  return CrossingRule::sequence(std::move(pattern));
}
/** Strand DOMINANCE: `{{over, under}, …}`. Cycles are legal and are the
 *  point — `{{0,1},{1,2},{2,0}}` is the impossible-braid rule Penrose
 *  tilings and heraldic knots are full of. */
inline CrossingRule pairs(std::vector<std::pair<int, int>> dominance) {
  return CrossingRule::pairs(std::move(dominance));
}
}  // namespace crossing

/** Every crossing among a set of strand paths, numbered along the
 *  boundary (ascending by position on the lowest-indexed strand
 *  involved). Only PROPER crossings count: coincident strands and
 *  endpoint touches, such as a shared polygon vertex, are meetings rather
 *  than crossings, and reporting them would put a knot at every corner. */
std::vector<Crossing> discoverCrossings(const std::vector<SkPath>& strands);

/** The region where two strands' MARKS actually overlap at one crossing:
 *  the intersection of the two paths stroked to their own reach, reduced to
 *  the component containing `at` and bounded by @p maxRadius px around it.
 *
 *  Exact at any angle, which a disc is not — two marks meeting at 12° overlap
 *  in a long lens whose extent along each strand goes as reach/sin(theta),
 *  and a disc sized for the perpendicular case leaves the under-strand
 *  showing straight across the over-strand's mark.
 *
 *  `maxRadius` is not a safety margin, it is REQUIRED for correctness on any
 *  ordinary braid. Once reach/sin(theta) approaches the spacing between
 *  knots, neighbouring lenses touch and path ops merge them into ONE
 *  contour — at which point the first crossing's patch owns the whole run
 *  and the weave degenerates to "one strand on top" for half its knots.
 *  Pass half the arc distance to the adjacent crossing, so each knot can
 *  only ever claim its own half.
 *
 *  Falls back to a disc when the intersection is empty (degenerate or
 *  non-overlapping input). */
SkPath crossingPatch(const SkPath& a, float reachA, const SkPath& b,
                     float reachB, SkPoint at, float maxRadius);

}  // namespace sigil::compose

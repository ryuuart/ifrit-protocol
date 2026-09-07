#pragma once

/** @file
 * THE WIDTH LAW: how far a mark sits ACROSS its spine, as a comparable
 * value.
 *
 * `along` is a fraction of the spine's arc length (or px of it, for a
 * law that says so); `across` is px on the spine's normal, positive to
 * the LEFT of travel — which, with y pointing down, is OUTSIDE a
 * clockwise path, and clockwise is Skia's own direction for rects and
 * circles. Everything in this leaf that takes a signed distance from a
 * path means that same side.
 */

#include <algorithm>
#include <any>
#include <cmath>
#include <concepts>
#include <functional>
#include <utility>
#include <vector>

namespace sigil::geometry::path {

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
 *  normal, positive to the LEFT of travel, which with y pointing down is
 *  OUTSIDE a clockwise path. */
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

/** The core profile presets: the laws that read nothing but their own
 *  numbers — the boundary itself, the parallel, the linear run between
 *  two widths, and the stepped table. Richer families — an oscillating
 *  wave, a braid built on it — are a kit's, since this leaf only holds
 *  the seam. */
namespace profile {
/** across ≡ 0: the boundary itself. */
struct Self {
  float across(float) const { return 0.0f; }
  float max() const { return 0.0f; }
  bool operator==(const Self&) const = default;
};
/** across ≡ px: a parallel. Parallels are rails — they never cross.
 *
 *  **Positive is LEFT of travel**, which is outside a clockwise path —
 *  the same side `parallel` means. */
struct Offset {
  float px = 0.0f;
  float across(float) const { return px; }
  float max() const { return std::abs(px); }
  bool operator==(const Offset&) const = default;
};
/** across runs LINEARLY from `startPx` to `endPx` along the spine — the
 *  brush that lifts, the ribbon that closes, the leader that narrows to
 *  its point.
 *
 *  The two ends are signed, and the sign is the side (positive is LEFT of
 *  travel), so a taper from +8 to −8 crosses the spine at the middle
 *  rather than narrowing: that is a strand trading sides, not a taper. A
 *  taper to 0 is the point.
 *
 *  Keyed in the FRACTION of arc length, so it stretches to whatever spine
 *  it is handed. A taper that must keep its px shape under a reveal wants
 *  its own px-keyed law — see PxKeyedProfileScheme. */
struct Taper {
  float startPx = 0.0f;
  float endPx = 0.0f;
  float across(float along) const {
    const float t = along < 0.0f ? 0.0f : (along > 1.0f ? 1.0f : along);
    return startPx + (endPx - startPx) * t;
  }
  float max() const { return std::max(std::abs(startPx), std::abs(endPx)); }
  bool operator==(const Taper&) const = default;
};

/** A STEPPED width: a run of spans, each holding one width for its share
 *  of the spine — the flow that thins at every junction it passes, the
 *  rule that changes weight at a stated station, the bar whose thickness
 *  is a measurement rather than a curve.
 *
 *  `widthsPx` is read against `upTo`, the span boundaries in the profile's
 *  own key, ASCENDING. Width `i` holds from boundary `i−1` (or the start)
 *  to boundary `i`, and the LAST width holds from the last boundary to
 *  the end — so `widthsPx` carries one more entry than `upTo`. Fewer and
 *  the tail reads the last width there is; more and the extra widths are
 *  never read. Empty is a width of zero everywhere.
 *
 *  A STEP IS A STEP. The width does not interpolate across a boundary,
 *  because the thing this describes is a measurement that changes at a
 *  place, not a curve sampled at one — a law that eased between its
 *  stations would draw a shape nobody measured. `Taper` is the
 *  interpolating one, and two of them beside each other is the ramp
 *  between two stated widths. */
struct Spans {
  std::vector<float> upTo;
  std::vector<float> widthsPx;
  float across(float along) const {
    if (widthsPx.empty()) return 0.0f;
    size_t i = 0;
    while (i < upTo.size() && along >= upTo[i]) ++i;
    return widthsPx[i < widthsPx.size() ? i : widthsPx.size() - 1];
  }
  float max() const {
    float widest = 0.0f;
    for (float w : widthsPx) widest = std::max(widest, std::abs(w));
    return widest;
  }
  bool operator==(const Spans&) const = default;
};

inline Profile self() { return Profile(Self{}); }
inline Profile offset(float px) { return Profile(Offset{px}); }
inline Profile taper(float startPx, float endPx) {
  return Profile(Taper{startPx, endPx});
}
inline Profile spans(std::vector<float> upTo, std::vector<float> widthsPx) {
  return Profile(Spans{std::move(upTo), std::move(widthsPx)});
}
}  // namespace profile

}  // namespace sigil::geometry::path

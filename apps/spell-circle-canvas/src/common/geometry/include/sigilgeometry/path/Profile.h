#pragma once

/** @file
 * @ingroup geometry-path
 *
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
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <utility>
#include <vector>

namespace sigil::geometry::path {

/** One stop of a width law: the width across the spine at a place along
 *  it. */
struct Stop {
  float along = 0;
  float width = 0;
  bool operator==(const Stop&) const = default;
};

/** What a width law does between two stops. */
enum class Between : uint8_t {
  /** Runs evenly from one width to the next. */
  Linear,
  /** Holds each width until the next stop — a measurement that changes
   *  at a place. */
  Step,
  /** Eases from one to the next, level at both. */
  Smooth,
};

/** How a law given as stops reads them. */
struct ProfileOptions {
  Between between = Between::Linear;
  /** `along` is px of arc length from the spine's start rather than a
   *  fraction of it — a law that must stay put under a reveal. */
  bool inPixels = false;
  bool operator==(const ProfileOptions&) const = default;
};

/** A PROFILE VALUE: `float across(float along) const`, `float max()
 *  const`, and EQUALITY, all three load-bearing. `along` is a fraction
 *  of the spine's arc length; `across` is px on its normal, positive to
 *  the LEFT of travel. `max()` is what every cull and bleed is sized
 *  from, and equality is what lets a pruning node see a new law.
 *  @trap A NON-FINITE WIDTH DELETES THE WHOLE BAND: one NaN vertex
 *  makes the path non-finite and Skia draws none of it, with no error.
 *  Clamp inside the law — the seam does not guard it. */
template <typename P>
concept ProfileScheme =
    std::equality_comparable<P> && requires(const P& p, float along) {
      { p.across(along) } -> std::convertible_to<float>;
      { p.max() } -> std::convertible_to<float>;
    };

/** THE PX KEY — optional, one line. A scheme that declares
 *  `static constexpr bool alongIsPx = true` is keyed in PX OF ARC
 *  LENGTH from the spine's start rather than in a fraction of it, and
 *  the seam converts for every consumer that has measured its spine.
 *  A scheme that says nothing stays fraction-keyed.
 *  @trap Under a reveal a fraction is a fraction of what has been drawn
 *  SO FAR, so a fraction-keyed law SLIDES as the reveal grows — right
 *  in a still frame and wrong in motion. */
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
  /** A constant width of `px` on the whole spine — the parallel, and the
   *  width a caller means by a bare number (`band(spine, 22)`). Positive
   *  is left of travel. */
  Profile(float px);  // NOLINT: implicit by design (band(spine, 22))
  /** A law through @p stops, `{along, width}` in order along the spine:
   *  `Profile{{0, 14}, {1, 4}}` narrows from 14 px to 4. Before the first
   *  stop and after the last the width holds. */
  Profile(std::initializer_list<Stop> stops, ProfileOptions options = {});
  /** The same law from a run of stops held elsewhere. */
  Profile(std::vector<Stop> stops, ProfileOptions options = {});
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
/** ACROSS RUNS LINEARLY from `startPx` to `endPx` along the spine — the
 *  brush that lifts, the ribbon that closes, the leader that narrows to
 *  its point, where a taper to 0 IS the point. Keyed in the FRACTION of
 *  arc length, so it stretches to whatever spine it is handed.
 *  @trap Both ends are signed and the sign is the SIDE, so a taper from
 *  +8 to −8 crosses the spine at the middle rather than narrowing:
 *  that is a strand trading sides, not a taper. */
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

/** A STEPPED WIDTH: a run of steps, each holding one width for its
 *  share of the spine. `widthsPx` is read against `upTo`, the span
 *  boundaries in the profile's own key, ASCENDING, and carries one more
 *  entry than `upTo` because the last width holds to the end. Empty is
 *  a width of zero everywhere. A STEP IS A STEP — the width does not
 *  interpolate across a boundary, since this describes a measurement
 *  that changes at a place; `Taper` is the interpolating one. */
struct Steps {
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
  bool operator==(const Steps&) const = default;
};

/** A LAW THROUGH STOPS, keyed in the fraction of arc length: the width
 *  at each stop, and between two stops what `between` says. */
struct Stops {
  std::vector<Stop> stops;
  Between between = Between::Linear;
  float across(float along) const {
    if (stops.empty()) return 0.0f;
    if (along <= stops.front().along) return stops.front().width;
    for (size_t index = 1; index < stops.size(); ++index) {
      const Stop& from = stops[index - 1];
      const Stop& to = stops[index];
      if (along > to.along) continue;
      if (between == Between::Step) return from.width;
      const float span = to.along - from.along;
      float t = span > 0.0f ? (along - from.along) / span : 1.0f;
      if (between == Between::Smooth) t = t * t * (3.0f - 2.0f * t);
      return from.width + (to.width - from.width) * t;
    }
    return stops.back().width;
  }
  float max() const {
    float widest = 0.0f;
    for (const Stop& stop : stops) widest = std::max(widest, std::abs(stop.width));
    return widest;
  }
  bool operator==(const Stops&) const = default;
};

/** The same law keyed in PX of arc length from the spine's start. */
struct StopsInPixels : Stops {
  static constexpr bool alongIsPx = true;
  bool operator==(const StopsInPixels&) const = default;
};

/** The spine itself: a band of no width, which is the rail a stroke
 *  already draws. */
inline Profile self() { return Profile(Self{}); }
/** A constant width of @p px across the whole spine; positive is left
 *  of travel. */
inline Profile offset(float px) { return Profile(Offset{px}); }
/** A width running evenly from @p startPx at the start of the spine to
 *  @p endPx at its end. */
inline Profile taper(float startPx, float endPx) {
  return Profile(Taper{startPx, endPx});
}
/** A stepped width: @p widthsPx holds the width in each step, and
 *  @p upTo the fraction of arc length each step ends at. */
inline Profile steps(std::vector<float> upTo, std::vector<float> widthsPx) {
  return Profile(Steps{std::move(upTo), std::move(widthsPx)});
}
}  // namespace profile

inline Profile::Profile(float px) : Profile(profile::Offset{px}) {}

inline Profile::Profile(std::vector<Stop> stops, ProfileOptions options)
    : Profile(options.inPixels
                  ? Profile(profile::StopsInPixels{
                        {std::move(stops), options.between}})
                  : Profile(profile::Stops{std::move(stops), options.between})) {}

inline Profile::Profile(std::initializer_list<Stop> stops,
                        ProfileOptions options)
    : Profile(std::vector<Stop>(stops), options) {}

}  // namespace sigil::geometry::path

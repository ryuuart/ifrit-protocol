#pragma once

/** @file
 * @ingroup geometry-kit
 *
 * The corner treatments — the wrappers that round any shape's corners or
 * bend its outline with a shaper, and the shapes a frame is actually cut
 * to: chamfered and notched.
 */

#include <concepts>
#include <cstdint>
#include <glm/vec2.hpp>
#include <type_traits>
#include <utility>

#include "sigilgeometry/path/Outline.h"
#include "sigilgeometry/path/Shaper.h"

namespace sigil::geometry::shapes {

// ---------------------------------------------------------------------------
// Wrappers — generators over generators

/** A silhouette VALUE: comparable, and answering an outline for a size
 *  through the `outline(size)` member every generator here declares.
 *
 *  The member is what separates a value from a bare closure, and the
 *  separation is load-bearing: a capture-free closure is an EMPTY class,
 *  so a compiler-written equality over it is vacuously true and would
 *  claim two different drawings are the same one. A wrapper asks for this
 *  before it agrees to compare. */
template <typename S>
concept Silhouette =
    std::equality_comparable<S> && requires(const S& s, glm::vec2 size) {
      { s.outline(size) } -> std::convertible_to<path::Outline>;
    };

/** Anything that answers an outline for a size: a silhouette value, a
 *  generator that cannot compare (an unkeyed `parametric`), or a bare
 *  callable. What a wrapper can wrap. */
template <typename S>
concept OutlineSource =
    requires(const S& s, glm::vec2 size) {
      { s.outline(size) } -> std::convertible_to<path::Outline>;
    } || std::is_invocable_r_v<path::Outline, const S&, glm::vec2>;

/** The outline @p source answers for @p size, whichever way it answers. */
template <OutlineSource S>
path::Outline outlineOf(const S& source, glm::vec2 size) {
  if constexpr (requires { source.outline(size); })
    return source.outline(size);
  else
    return source(size);
}

namespace detail {
path::Outline shape(const path::Outline& outline, const path::Shaper& shaper);
path::Outline translated(const path::Outline& outline, glm::vec2 offset);
}  // namespace detail

/** How a corner treatment cuts a corner — CSS `corner-shape`'s words. */
enum class CornerShape : uint8_t {
  /** An arc of the radius. */
  Round,
  /** A straight cut of the radius along each leg. */
  Bevel,
};

/** The dials of `cornered()` beyond the radius. */
struct CornerOptions {
  CornerShape shape = CornerShape::Round;
  bool operator==(const CornerOptions&) const = default;
};

namespace detail {
path::Outline cornerOutline(const path::Outline& outline, float radius,
                            const CornerOptions& options);
}  // namespace detail

/** Wraps any silhouette so every sharp corner is treated by @p radius —
 *  rounded, or cut straight — the corner treatment for shapes that have
 *  no box corners: `cornered(star(5), 8)`, or `star(5).cornered(8)` on a
 *  general shape. It holds the wrapped value rather than erasing it, so
 *  wrapping a generator gives a generator that compares by its
 *  parameters, and wrapping a bare callable gives something that
 *  compares to nothing — the same escape hatch the callable itself
 *  was. */
template <OutlineSource Inner>
struct Cornered {
  Inner inner;
  float radius = 0.0f;
  CornerOptions options{};
  bool operator==(const Cornered& o) const
    requires Silhouette<Inner>
  {
    return inner == o.inner && radius == o.radius && options == o.options;
  }
  path::Outline outline(glm::vec2 size) const {
    return detail::cornerOutline(outlineOf(inner, size), radius, options);
  }
};

/** @p shape with every corner treated by @p radius. */
template <OutlineSource Inner>
Cornered<Inner> cornered(Inner shape, float radius, CornerOptions options = {}) {
  return Cornered<Inner>{std::move(shape), radius, options};
}

/** @p shape with every sharp corner rounded to @p radius — `cornered` at
 *  its default. */
template <OutlineSource Inner>
Cornered<Inner> rounded(Inner shape, float radius) {
  return Cornered<Inner>{std::move(shape), radius};
}

/** Wraps any silhouette so a @p shaper bends the outline it answers — a
 *  torn edge, a wobbled ring, a hand-drawn square: `shaped(polygon(7),
 *  shapers::Jitter{3, 6, 11})`.
 *
 *  The deviation lands ONCE, where the shape is asked for, and what the
 *  consumer receives is an ordinary outline. That is the whole difference
 *  from running the same shaper as a path effect on the stroke: an effect
 *  is re-run on every paint of every mark the outline carries, and a
 *  figure whose edge is torn usually carries several — a fill, a keyline,
 *  a glow — each of which would tear the edge again, differently, at its
 *  own cost. Wrapped here they all agree, because there is only one
 *  outline.
 *
 *  It composes with `rounded()` in either order, and the order is a
 *  different picture: rounding a torn edge softens the tears, tearing a
 *  rounded one leaves the corners round and the runs between them
 *  ragged. */
template <OutlineSource Inner, path::ShaperScheme S>
struct Shaped {
  Inner inner;
  S shaper;
  bool operator==(const Shaped& o) const
    requires Silhouette<Inner>
  {
    return inner == o.inner && shaper == o.shaper;
  }
  path::Outline outline(glm::vec2 size) const {
    return detail::shape(outlineOf(inner, size), path::Shaper(shaper));
  }
};

template <typename Inner, typename S>
Shaped<Inner, S> shaped(Inner shape, S shaper) {
  return Shaped<Inner, S>{std::move(shape), std::move(shaper)};
}

/** @p shape drawn in the square of side `2 · radius` centred on
 *  @p centre — a shape placed where a figure wants it rather than in a
 *  box at the origin. */
template <OutlineSource S>
path::Outline at(const S& shape, glm::vec2 centre, float radius) {
  const path::Outline drawn = outlineOf(shape, glm::vec2(2 * radius));
  return detail::translated(drawn, centre - glm::vec2(radius));
}

// ---------------------------------------------------------------------------
// Corner geometry — the shapes a frame is actually cut to
//
// `borderRadius()` rounds, and rounding is the ONE corner treatment the kernel
// offers. The other two are the 45° CHAMFER — the cut corner that reads as
// machined metal — and the rectangular NOTCH, the bitten corner that reads
// as a stencil or a fixing lug. Both take a per-corner MASK rather than a
// single number, because a chamfer on two corners and square on the other
// two is the common case and no radius expresses it.

/** Which corners a treatment applies to. Clockwise from the top-left. */
enum class Corner : uint8_t {
  TopLeft = 1,
  TopRight = 2,
  BottomRight = 4,
  BottomLeft = 8,
  All = 15,
  /** The two on one diagonal — the asymmetric cut that reads as a tab. */
  Diagonal = 5,       // TopLeft | BottomRight
  AntiDiagonal = 10,  // TopRight | BottomLeft
};
/** The union of two corner selections. */
constexpr Corner operator|(Corner a, Corner b) {
  // the type is a bit set; any union of enumerators is a valid value
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  return Corner(uint8_t(a) | uint8_t(b));
}
/** Whether @p mask selects any corner @p c names. */
constexpr bool has(Corner mask, Corner c) {
  return (uint8_t(mask) & uint8_t(c)) != 0;
}

/** The CHAMFERED box: each corner @p mask names replaced by a cut of
 *  @p cut px, and every corner the mask does NOT name rounded by @p radius —
 *  "rounded except where cut", which is the machined-panel corner rule
 *  and the reason the two live in one value rather than as a rounding
 *  wrapped round a chamfer. A wrapper would round the cut as well, and
 *  the cut is the whole point of it.
 *
 *  @p cutRise is the cut's vertical leg where it differs from its
 *  horizontal one. Zero — the default — is the 45° cut, whose rise IS
 *  its run; nothing is lost by spelling it that way, because a cut of no
 *  rise is a square corner and already has a spelling. A 45° cut clamps
 *  to half the SHORT side so it stays at 45° and an over-large one
 *  degenerates to a diamond rather than an inside-out path; an
 *  anisotropic one clamps each leg to its own half-side, since it was
 *  never at 45° to begin with. The radius clamps to half the short
 *  side. */
struct Chamfered {
  float cut = 0.0f;
  float cutRise = 0.0f;
  float radius = 0.0f;
  Corner mask = Corner::All;
  bool operator==(const Chamfered&) const = default;
  path::Outline outline(glm::vec2 size) const;
};

inline Chamfered chamfered(float cut, Corner mask = Corner::All) {
  return Chamfered{.cut = cut, .mask = mask};
}

/** The NOTCHED box: each corner @p mask names carries a rectangular bite
 *  @p notchWidth wide and @p depth deep — the stencil corner, the fixing
 *  lug. Both are clamped to 0.45 of the shorter side. */
struct Notched {
  float notchWidth = 0.0f;
  float depth = 0.0f;
  Corner mask = Corner::All;
  bool operator==(const Notched&) const = default;
  path::Outline outline(glm::vec2 size) const;
};

inline Notched notched(float notchWidth, float depth,
                       Corner mask = Corner::All) {
  return Notched{notchWidth, depth, mask};
}

}  // namespace sigil::geometry::shapes

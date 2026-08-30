#pragma once

/** @file
 * SigilCompose corner treatments — the wrapper that rounds any shape's
 * corners, and the shapes a frame is actually cut to: chamfered and notched.
 */

#include <include/core/SkPathBuilder.h>

#include <cstdint>

#include "sigilcompose/Compose.h"
#include "sigilgeometry/path/Polyline.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose::shapes {

// ---------------------------------------------------------------------------
// Wrappers — generators over generators

/** Wraps any shape so every sharp corner rounds with a consistent
 *  radius — corners() for arbitrary silhouettes:
 *  `.shape(rounded(star(5), 8))`. Comparable whenever the wrapped shape
 *  is (a wrapped raw callable stays the escape hatch). */
struct Rounded {
  Shape inner;
  float radius = 0.0f;
  bool operator==(const Rounded&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Rounded rounded(Shape shape, float radius) {
  return Rounded{std::move(shape), radius};
}

// ---------------------------------------------------------------------------
// Corner geometry — the shapes a frame is actually cut to
//
// `corners()` rounds, and rounding is the ONE corner treatment the kernel
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
constexpr Corner operator|(Corner a, Corner b) {
  return Corner(uint8_t(a) | uint8_t(b));
}
constexpr bool has(Corner mask, Corner c) {
  return (uint8_t(mask) & uint8_t(c)) != 0;
}

/** The CHAMFERED box: each selected corner replaced by a 45° cut of @p cut
 *  px. Clamped to half the short side, so an over-large cut degenerates to
 *  a diamond rather than an inside-out path. */
struct Chamfered {
  float cut = 0.0f;
  Corner mask = Corner::All;
  bool operator==(const Chamfered&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Chamfered chamfered(float cut, Corner mask = Corner::All) {
  return Chamfered{cut, mask};
}

/** The NOTCHED box: each selected corner carries a rectangular bite @p
 *  notchWidth wide and @p depth deep — the stencil corner, the fixing lug.
 *  Both are clamped to 0.45 of the shorter side. */
struct Notched {
  float notchWidth = 0.0f;
  float depth = 0.0f;
  Corner mask = Corner::All;
  bool operator==(const Notched&) const = default;
  SkPath path(SkSize s) const;
  SkPath operator()(SkSize s) const { return path(s); }
};

inline Notched notched(float notchWidth, float depth,
                       Corner mask = Corner::All) {
  return Notched{notchWidth, depth, mask};
}

}  // namespace sigil::compose::shapes

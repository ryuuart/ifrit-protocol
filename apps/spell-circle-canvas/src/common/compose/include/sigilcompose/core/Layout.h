#pragma once

/** @file
 * SigilCompose layout values — Dim and its literals, Align, Justify, Echo,
 * Cache, the LayoutInput a custom LayoutScheme places children from, and
 * the ComponentProps and ComponentFn concepts the generic entry points are
 * constrained by.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <include/core/SkSize.h>
#include <sigilcore/cache/Policy.h>

#include <concepts>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace sigil::compose {

// ---------------------------------------------------------------------------
// Layout values (Yoga semantics, 1:1)

/** A length that may be absolute, relative to the parent, or left for
 *  layout to decide. Constructing one from a bare float gives pixels,
 *  so the common case reads as a number. */
struct Dim {
  enum class Unit : uint8_t { Px, Pct, Auto };
  Unit unit = Unit::Auto;
  float value = 0.0f;

  constexpr Dim() = default;
  constexpr Dim(float px)  // NOLINT: implicit by design
      : unit(Unit::Px), value(px) {}
  bool operator==(const Dim&) const = default;
};
constexpr Dim pct(float v) {
  Dim d;
  d.unit = Dim::Unit::Pct;
  d.value = v;
  return d;
}
constexpr Dim autoDim() { return {}; }

/** `width(50_pct)`, `basis(120_px)` — for the Dim-valued setters;
 *  exposed by `using namespace sigil::compose` (or `using namespace
 *  sigil::compose::literals`). */
inline namespace literals {
constexpr Dim operator""_px(long double v) { return Dim((float)v); }
constexpr Dim operator""_px(unsigned long long v) { return Dim((float)v); }
constexpr Dim operator""_pct(long double v) { return pct((float)v); }
constexpr Dim operator""_pct(unsigned long long v) { return pct((float)v); }
}  // namespace literals

enum class Align : uint8_t { Auto, Start, Center, End, Stretch, Baseline };
enum class Justify : uint8_t {
  Start,
  Center,
  End,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly
};

/** One misprint pass: the node's own fill shape and text re-stamped at
 *  `offset` in a flat color, UNDER the real content. Repeated echoes stack
 *  in declaration order, bottom first. This is the registration-error
 *  look — offset ink under-copies, hard-edged sticker stacks — as one call
 *  rather than duplicate sibling nodes. */
struct Echo {
  SkVector offset = {3, 3};
  SkColor4f color = {0, 0, 0, 1};
  bool operator==(const Echo&) const = default;
};

/** Cache override.
 *
 *  - **Auto** (the default) records provably-static subtrees as pictures.
 *  - **Picture** records, and never lets the library promote the node to
 *    a pixel bake.
 *  - **Texture** rasterizes the subtree once into an image. Best for dense
 *    or effect-heavy content; wasteful for sparse regions, where the blit
 *    of a mostly-empty image costs more than the few draws it replaced.
 *  - **Group** is Texture for a subtree whose children ANIMATE — see
 *    below.
 *  - **None** opts a node out entirely. A per-frame paint program that
 *    reads the clock MUST declare this: nothing can see that a
 *    `PaintProgram` sampled `elapsedSeconds`, so an undeclared one is
 *    recorded on its first frame and replayed frozen thereafter.
 *
 *  **Group** is for "many small rotated or blended pieces forming one
 *  assembly that is currently still". `Cache::Texture` bakes a node's OWN
 *  paint and refuses the moment anything below it is volatile, so a
 *  fill-less container of animated strips gets no bake at all and every
 *  strip replays its shaders every frame. `Cache::Group` bakes the
 *  container AND its children into one unrotated device-space layer, so
 *  the children's rotations, bevels and mutual compositing resolve INSIDE
 *  the bake at full precision. That is why it is pixel-safe where putting
 *  `Cache::Texture` on each child is not: per-child bakes isolate the
 *  pieces and change how they composite with each other.
 *
 *  It is held by a SUBTREE VALUE MEMO rather than by a volatility verdict.
 *  The bake is taken only while every bound transform, opacity and content
 *  scalar below the node still holds the value it held last frame, and is
 *  dropped on the frame any of them ticks. So an entrance animation plays
 *  live and the settled assembly costs one blit.
 *
 *  IT REFUSES, permanently and with one line to stderr, any subtree
 *  carrying volatility a float comparison cannot see: a live material
 *  (`uTime` or a bound uniform), an animated decoration, an animated
 *  image, a bound `fill()`, a variable-font drive, a `Cache::None`
 *  descendant, or a non-srcOver blend or backdrop filter below the root
 *  (which would resolve against the bake's transparent black). It also
 *  declines per frame while its own transform animates or its device rect
 *  is moving, because a device-pinned bake remade every frame costs more
 *  than the paint it replaces. A Group node that never reports itself as
 *  held has one of the above in it. */
enum class Cache : uint8_t { Auto, Picture, Texture, Group, None };

/** The POLICY half of a cache mode — what the settled-subtree proof reads,
 *  with the tier left behind.
 *
 *  Three of the five names above say the same thing to the proof: they ask
 *  for a bake, and they differ only in which artefact the painter makes.
 *  `Auto` leaves the decision to the proof, and `None` states volatility no
 *  declaration can see. The proof answers in those three terms and never
 *  learns what a picture, a texture or a group is. */
constexpr core::Cache cachePolicy(Cache c) {
  switch (c) {
    case Cache::Auto:
      return core::Cache::Auto;
    case Cache::None:
      return core::Cache::Never;
    case Cache::Picture:
    case Cache::Texture:
    case Cache::Group:
      break;
  }
  return core::Cache::Always;
}

// ---------------------------------------------------------------------------
// Custom layout (the SwiftUI Layout-protocol shape, C++20-ified)

/** What a custom layout sees: the container's resolved size, each child's
 *  measured size (text children measured by SigilWeave), and each child's
 *  first-baseline offset from its own top (NaN for children without one) —
 *  what baseline-rhythm schemes (layouts::BaselineGrid) snap by. */
struct LayoutInput {
  SkSize container = SkSize::MakeEmpty();
  std::vector<SkSize> childSizes;
  std::vector<float> childBaselines;  // NaN = no baseline (non-text)
};

/** A custom layout places children: one rect per child (position and
 *  size, container-relative). Runs as a bounded second layout pass. */
template <typename L>
concept LayoutScheme = requires(const L& l, const LayoutInput& in) {
  { l.place(in) } -> std::convertible_to<std::vector<SkRect>>;
};

// ---------------------------------------------------------------------------
// Concepts (readable errors at the generic entry points)

template <typename P>
concept ComponentProps = std::equality_comparable<P> && std::copyable<P>;

class Element;

template <typename F, typename P>
concept ComponentFn =
    std::invocable<F, const P&> &&
    std::convertible_to<std::invoke_result_t<F, const P&>, Element>;

}  // namespace sigil::compose

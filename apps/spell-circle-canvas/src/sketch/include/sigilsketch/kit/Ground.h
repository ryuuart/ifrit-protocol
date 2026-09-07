#pragma once

/** @file
 * WHAT PAINTS AN AREA, in one value: a flat fill, or a material.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/skia/Paint.h>

#include <optional>
#include <utility>

namespace sigil::sketch::kit {

/** THE PAINT BEHIND A SURFACE, in either of the two forms a caller has
 *  it in.
 *
 *  Most of what a sheet is set on is a colour, a gradient or a shader — a
 *  `compose::Fill`. A reconstruction whose ground is quarried stone,
 *  brushed metal or anything else generated per pixel has a MATERIAL
 *  instead, and a component that took only a Fill would turn that half of
 *  the tree away. Which of the two a caller holds is a fact about the
 *  picture rather than about the component, so it is one field.
 *
 *  It converts from either, so `Fill::color(…)`, a
 *  `material::skia::Paint` and a bare `material::Material` are each
 *  written where the ground is asked for and none of them is wrapped:
 *
 *      kit::well({.ground = Fill::color(kPlate)})
 *      kit::frame({.shell = mkit::stone({.hi = …, .lo = …})})
 *
 *  A MATERIAL IS PUT ON THE NODE THE WAY `Element::fill` PUTS ONE THERE:
 *  a static one collapses to a Fill and rides the same caching and prune
 *  path a colour does, while a live or geometry-dependent one stays whole
 *  on the node so the painter resolves it against the frame it is drawn
 *  at. */
class Ground {
 public:
  /** Paints nothing. */
  Ground() = default;
  // NOLINTNEXTLINE(google-explicit-constructor): the point is that a
  // caller writes the fill it already has where a ground is asked for.
  Ground(compose::Fill fill) : m_fill(std::move(fill)) {}
  // NOLINTNEXTLINE(google-explicit-constructor)
  Ground(material::skia::Paint paint) : m_paint(std::move(paint)) {}
  // NOLINTNEXTLINE(google-explicit-constructor)
  Ground(material::Material recipe)
      : m_paint(material::skia::Paint::recipe(std::move(recipe))) {}

  /** Nothing to paint — neither a material nor a fill that draws. */
  [[nodiscard]] bool none() const {
    return !m_paint && m_fill.kind == compose::Fill::Kind::None;
  }
  /** THE FILL FORM, and `Fill::none()` where the ground is a material.
   *  It is what a primitive below that takes a Fill is handed; the
   *  material then goes onto the node that primitive returned, which is
   *  what `paint` does. */
  [[nodiscard]] const compose::Fill& fill() const { return m_fill; }
  /** THE MATERIAL FORM, or null where the ground is a fill. */
  [[nodiscard]] const material::skia::Paint* material() const {
    return m_paint ? &*m_paint : nullptr;
  }

  /** PUT THE GROUND ON @p node, in whichever form it holds. A ground that
   *  paints nothing puts nothing, so a node handed an empty ground is
   *  indistinguishable from one never handed a ground at all. */
  void paint(compose::Element& node) const {
    if (m_paint)
      node.fill(*m_paint);
    else if (m_fill.kind != compose::Fill::Kind::None)
      node.fill(m_fill);
  }

  bool operator==(const Ground&) const = default;

 private:
  compose::Fill m_fill;
  std::optional<material::skia::Paint> m_paint;
};

}  // namespace sigil::sketch::kit

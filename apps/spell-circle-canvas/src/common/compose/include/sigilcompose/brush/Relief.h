#pragma once

/** @file
 * @ingroup compose-brush
 *
 * A material over the rounded relief of the outline being painted: the
 * relief's normals reach the surface maps a World scene lights the page by, and
 * an ordinary draw paints the material's colours flat.
 */

#include <sigilcompose/core/Shape.h>
#include <sigilmaterial/core/Material.h>

#include <memory>
#include <utility>

namespace sigil::compose {

namespace detail {
class UnitSurfaces;
}

/** THE RELIEF A MATERIAL STATES, said over an outline: the decoration that
 *  paints `material.relief(options)` over the outline it is given, so the
 *  outline is the unit the relief is cut from — the same statement a fill
 *  or an ink makes of its unit with `material::Material::relief`. Attach
 *  with `background()` for a shape, or `foreground()` and
 *  `decorationOutline(Boundary::Glyphs)` for type. The material keeps its
 *  base, layers, effects and surface; its normal is reoriented under the
 *  relief. On a page painted as a surface map (`Composer::setSurfaceMap`)
 *  it paints that map of the relieved surface; every other draw paints the
 *  material's colours flat over the outline.
 *  Invalid options or an outline too large to cut leave the contour flat. */
class Relief {
 public:
  Relief(material::Material material, material::ReliefOptions options = {})
      : m_material(std::move(material).relief(options)) {}

  bool operator==(const Relief& other) const {
    return m_material == other.m_material;
  }
  bool isRunning() const;
  bool usesWorldSpace() const;
  float bleed(glm::vec2 size) const;
  void paint(draw::Pen& pen, const PaintContext& context) const;

 private:
  material::Material m_material;
  // Paint-time scratch is excluded from the description's value equality.
  mutable std::shared_ptr<detail::UnitSurfaces> m_units;
};

/** @p material over the actual outline supplied to its decoration, its
 *  relief carried in the surface maps.
 *  Set a text node's ordinary ink transparent when relief supplies its
 *  glyph foreground, so the two paints do not overlap. */
inline Relief relief(material::Material material,
                     material::ReliefOptions options = {}) {
  return Relief(std::move(material), options);
}

}  // namespace sigil::compose

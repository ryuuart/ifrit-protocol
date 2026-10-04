#pragma once

/** @file
 * @ingroup compose-brush
 *
 * A material shaded over the rounded relief of the outline being painted.
 */

#include <sigilcompose/core/Shape.h>
#include <sigilmaterial/core/Material.h>

#include <memory>
#include <utility>

namespace sigil::compose {

namespace detail {
struct ReliefCache;
}

struct ReliefOptions {
  /** Shoulder width in logical pixels. Zero leaves the contour flat. */
  float shoulder = 2.0f;
  /** Height relative to the shoulder width: positive raises the outline,
   *  negative impresses it, and zero leaves it flat. */
  float depth = 1.0f;
  bool operator==(const ReliefOptions&) const = default;
};

/** An outline-aware Decoration: attach with `background()` for a shape,
 *  or `foreground()` and `decorationOutline(Boundary::Glyphs)` for type.
 *  The supplied material keeps its base, layers, effects and surface
 *  settings; its normal map is reoriented into the outline's generated
 *  relief, preserving the finish detail on the shoulder and interior.
 *  With no lighting in force it paints the material's colours flat.
 *  Invalid options or an outline too large to bake leave the contour flat.
 *  At most sixteen million normal pixels are retained by one brush. */
class Relief {
 public:
  Relief(material::Material material, ReliefOptions options = {})
      : m_material(std::move(material)), m_options(options) {}

  bool operator==(const Relief& other) const {
    return m_material == other.m_material && m_options == other.m_options;
  }
  bool isRunning() const;
  bool readsLighting() const;
  bool usesWorldSpace() const;
  float bleed(glm::vec2 size) const;
  void paint(draw::Pen& pen, const PaintContext& context) const;

 private:
  material::Material m_material;
  ReliefOptions m_options;
  // Paint-time scratch is excluded from the description's value equality.
  mutable std::shared_ptr<detail::ReliefCache> m_cache;
};

/** Shade @p material over the actual outline supplied to its decoration.
 *  Set a text node's ordinary ink transparent when relief supplies its
 *  glyph foreground, so the two paints do not overlap. */
inline Relief relief(material::Material material, ReliefOptions options = {}) {
  return Relief(std::move(material), options);
}

}  // namespace sigil::compose

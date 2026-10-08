#pragma once

/** @file
 * The ink a passage's glyphs are painted with: what the kernel resolves
 * for one draw, and the style per glyph the text engine answers where
 * that ink restarts on each unit of the passage.
 */

#include <include/core/SkPath.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/SurfaceMaps.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/style/PaintStyle.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace sigil::compose::detail {

class UnitSurfaces;

/** THE INK ONE DRAW OF A PASSAGE PAINTS ITS GLYPHS WITH. */
struct TextInk {
  /** A lit ink on a page painted as a surface map, resolved once its box
   *  is known: read as `map`. */
  struct Surface {
    material::skia::SurfaceMaps inputs;
    material::texture::Role map;
    /** The material where it reads the unit it is painted into, with the
     *  node's held maps per unit: each unit's maps are answered for that
     *  unit's outline and direction. */
    std::optional<material::Material> readsUnit;
    UnitSurfaces* units = nullptr;
    /** The ink over the unit square @p paintToLocal places in the node. */
    sk_sp<SkShader> shader(const material::FrameData& frame,
                           const glm::mat3& paintToLocal) const {
      return inputs.mapShader(map, frame, paintToLocal);
    }
    /** The ink over the unit square @p paintToLocal places in the node,
     *  answered for the unit @p outline covers in the node, running
     *  @p direction in the page, rasterised at @p density. */
    sk_sp<SkShader> shaderOver(const material::FrameData& frame,
                               const glm::mat3& paintToLocal,
                               const SkPath& outline, glm::vec2 direction,
                               float density) const;
  };
  /** One retained span source resolved for this draw. Its owner follows
   *  paragraph restyling; its shader belongs to the current destination. */
  struct Span {
    std::shared_ptr<const material::Material> source;
    sk_sp<SkShader> shader;
    std::optional<material::Color> color;
    std::optional<sigil::weave::Unit> unit;
    /** A lit unit resolves after its placed box is known. Prepared
     *  material inputs remain shared by the retained span. */
    std::optional<Surface> surface;
    /** The span's paint already is a surface map of its material. */
    bool surfaceMap = false;
  };
  /** The one glyph-paint override `ink(paint)` and `textStroke()` resolve
   *  to, its paint mapped onto the passage's text-metric box. Empty where
   *  the leaf states neither, and the glyphs draw in their spans' paint. */
  std::optional<sigil::weave::PaintStyle> passage;
  /** The leaf's ink paint as a shader on the unit square, where it
   *  restarts on each `unit` of the passage; null where it does not. */
  sk_sp<SkShader> unitSquare;
  std::optional<Surface> unitSurface;
  sigil::weave::Unit unit = sigil::weave::Unit::Glyph;
  /** Span inks visible beneath any whole-passage override. */
  std::vector<Span> spans;
  material::FrameData frame;
  /** Device pixels to each logical pixel a unit's relief is cut at. */
  float density = 1;

  /** A span or unit needs its own foreground for this draw. */
  [[nodiscard]] bool hasGlyphStyles() const {
    return unitSquare || unitSurface || !spans.empty();
  }
  [[nodiscard]] const sigil::weave::PaintStyle* override() const {
    return passage ? &*passage : nullptr;
  }
};

/** A STYLE PER GLYPH: one style per unit an ink restarts on, and which
 *  of them each glyph draws with, by its place in the walk
 *  `forEachPlacedGlyph` takes. A glyph naming `kOwnPaint` draws with the
 *  paint it would have drawn with anyway. */
struct GlyphInk {
  static constexpr uint32_t kOwnPaint = ~0u;
  std::vector<sigil::weave::PaintStyle> styles;
  std::vector<uint32_t> styleOfGlyph;

  /** Releases draw-owned paints while retaining the next walk's capacity. */
  void clear() {
    styles.clear();
    styleOfGlyph.clear();
  }
};

}  // namespace sigil::compose::detail

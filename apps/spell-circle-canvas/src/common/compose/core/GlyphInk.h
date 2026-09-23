#pragma once

/** @file
 * The ink a passage's glyphs are painted with: what the kernel resolves
 * for one draw, and the style per glyph the text engine answers where
 * that ink restarts on each unit of the passage.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/style/PaintStyle.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace sigil::compose::detail {

/** THE INK ONE DRAW OF A PASSAGE PAINTS ITS GLYPHS WITH. */
struct TextInk {
  /** The one glyph-paint override `ink(paint)` and `textStroke()` resolve
   *  to, its paint mapped onto the passage's text-metric box. Empty where
   *  the leaf states neither, and the glyphs draw in their spans' paint. */
  std::optional<sigil::weave::PaintStyle> passage;
  /** The leaf's ink paint as a shader on the unit square, where it
   *  restarts on each `unit` of the passage; null where it does not. */
  sk_sp<SkShader> unitSquare;
  sigil::weave::Unit unit = sigil::weave::Unit::Glyph;
  /** A span of the passage states an ink that restarts per unit, and no
   *  override hides it. */
  bool spanUnits = false;

  /** Some glyph's paint restarts on a unit, so the passage draws with a
   *  style per glyph. */
  [[nodiscard]] bool restarts() const { return unitSquare || spanUnits; }
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
};

}  // namespace sigil::compose::detail

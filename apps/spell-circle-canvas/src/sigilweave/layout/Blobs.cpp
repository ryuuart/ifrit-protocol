/** @file
 * A shaped word as something the canvas draws: the glyphs re-laid under a
 * justification fit, and the glyphs baked one transform each for a line
 * that is rotated or rides a contour.
 */

#include "Blobs.h"

#include <include/core/SkRSXform.h>

#include <cmath>
#include <vector>

#include "sigilweave/fonts/Shaper.h"

namespace sigil::weave::detail {

// extra advance between the glyphs, and a horizontal scale on the glyphs
// themselves. The identity is neither, which is a shared word blob and the
// only path a text that never asks for the other two ever takes. It rides
// on every run it shaped, because everything that reads the glyphs back
// has to apply the same numbers the blob was baked with.

/** The advance `word` takes under @p fit. */
[[nodiscard]] float advanceUnder(const GlyphFit& fit, const ShapedWord& word) {
  return fit.advanceOf(word.advance, word.glyphs.size());
}

/** Per-glyph positioned blob for a run a justified line respaced or scaled:
 *  the shared blob bakes one set of positions and this line needs another. */
sk_sp<SkTextBlob> buildFittedBlob(const ShapedWord& shapedWord,
                                  const GlyphFit& fit) {
  SkTextBlobBuilder builder;
  const SkFont font =
      makeFont(shapedWord.typeface, shapedWord.fontSize,
               shapedWord.scaleX * fit.glyphScale, shapedWord.aliased);
  const int glyphCount = static_cast<int>(shapedWord.glyphs.size());
  const auto& run = builder.allocRunPos(font, glyphCount);
  for (int glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
    run.glyphs[glyphIndex] = shapedWord.glyphs[glyphIndex];
    run.points()[glyphIndex] = {
        shapedWord.positions[glyphIndex].x() * fit.glyphScale +
            fit.letterSpacing * static_cast<float>(glyphIndex),
        shapedWord.positions[glyphIndex].y()};
  }
  return builder.make();
}

// Per-glyph RSXform blob for rotated straight intervals and path contours.
sk_sp<SkTextBlob> buildTransformedBlob(const ShapedWord& shapedWord,
                                       const LineInterval& interval,
                                       float penOffset, int rotationSteps) {
  if (shapedWord.glyphs.empty()) return nullptr;
  SkTextBlobBuilder builder;
  const SkFont font = makeFont(shapedWord.typeface, shapedWord.fontSize,
                               shapedWord.scaleX, shapedWord.aliased);
  const int glyphCount = static_cast<int>(shapedWord.glyphs.size());
  const auto& run = builder.allocRunRSXform(font, glyphCount);

  float penLocal = 0;
  for (int glyphIndex = 0; glyphIndex < glyphCount; ++glyphIndex) {
    const float advance = shapedWord.advances[glyphIndex];
    // Offsets HarfBuzz applied on top of the pen position.
    const float glyphOffsetX = shapedWord.positions[glyphIndex].x() - penLocal;
    const float glyphOffsetY = shapedWord.positions[glyphIndex].y();

    // The interval owns the pen→placement mapping, and it is the SAME
    // function a caller re-placing these glyphs at draw time reads, so the
    // baked blob and a live re-placement can never disagree.
    SkPoint position;
    SkVector tangent;
    interval.placeAt(penOffset + penLocal + advance * 0.5f, 0.0f, rotationSteps,
                     &position, &tangent);

    // Anchor the glyph's advance-center on the baseline point `pos`,
    // rotated to the local tangent. Center in glyph-local coordinates:
    const float glyphCenterX = advance * 0.5f - glyphOffsetX;
    const float glyphCenterY = -glyphOffsetY;
    run.glyphs[glyphIndex] = shapedWord.glyphs[glyphIndex];
    run.xforms()[glyphIndex] = {tangent.x(), tangent.y(),
                                position.x() - (tangent.x() * glyphCenterX -
                                                tangent.y() * glyphCenterY),
                                position.y() - (tangent.y() * glyphCenterX +
                                                tangent.x() * glyphCenterY)};
    penLocal += advance;
  }
  return builder.make();
}

}  // namespace sigil::weave::detail

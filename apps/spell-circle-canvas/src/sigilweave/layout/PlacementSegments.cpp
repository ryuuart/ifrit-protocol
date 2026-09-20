/** @file
 * ONE SHAPED SEGMENT LANDING ON A LINE: the blob it draws from — the
 * word's own shared one, a fitted one, or one baked with a transform per
 * glyph — and the run that carries it at the pen offset the line reached.
 */

#include <include/core/SkTextBlob.h>

#include <cstdint>
#include <utility>

#include "Blobs.h"
#include "ParagraphLayoutInternal.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {

namespace detail {

void emitSegment(ParagraphLayout& result, const FlatInterval& flatInterval,
                 const WordSegment& segment, uint32_t wordIndex,
                 float penOffset, const ParagraphLayoutOptions& options,
                 const GlyphFit& fit, float baselineShift) {
  const ShapedWord& shapedWord = *segment.shaped;
  if (shapedWord.glyphs.empty()) return;
  // THE RUN IS SETTLED BEFORE IT IS APPENDED, and then written straight
  // into the vector's own slot: building one beside the vector and handing
  // it over is a second set of stores and a second pass over the same
  // bytes, and the blob handle it carries is reference-counted.
  sk_sp<SkTextBlob> blob;
  SkPoint origin = {0, 0};
  bool transformed = false;
  float advance = shapedWord.advance;
  GlyphFit runFit;
  const bool straight = !flatInterval.interval.contour.valid();
  const bool horizontal = straight &&
                          flatInterval.interval.direction.x() == 1 &&
                          flatInterval.interval.direction.y() == 0 &&
                          segment.form == SegmentForm::kFlow;
  const bool verticalColumn = straight &&
                              flatInterval.interval.direction.x() == 0 &&
                              flatInterval.interval.direction.y() == 1;
  if (horizontal) {
    // Respacing and scaling are a STRAIGHT HORIZONTAL answer: a column and
    // a curve place per glyph already, and a second per-glyph rule on top
    // of those would be two placements arguing over one run.
    blob =
        fit.plain() ? wordBlob(shapedWord) : buildFittedBlob(shapedWord, fit);
    if (!fit.plain()) {
      runFit = fit;
      advance = advanceUnder(fit, shapedWord);
    }
    // A BASELINE SHIFT lifts the span off its line's baseline and changes
    // nothing else: the advances are the face's own, so the pen is where
    // it was and the shaped run is the shared one.
    origin = flatInterval.interval.origin + SkVector{penOffset, -baselineShift};
  } else if (verticalColumn && segment.form == SegmentForm::kUpright) {
    // Vertical-shaped word: positions already stack down the column.
    blob = wordBlob(shapedWord);
    origin = flatInterval.interval.origin + SkVector{0, penOffset};
  } else if (verticalColumn && segment.form == SegmentForm::kTateChuYoko) {
    // Horizontal run set upright across the column, centred on its axis;
    // penX already points at the run's baseline (see Paragraph::analyze).
    blob = wordBlob(shapedWord);
    origin = flatInterval.interval.origin +
             SkVector{-shapedWord.advance * 0.5f, penOffset};
  } else {
    // Rotated/curved: bake per-glyph transforms (kRotated Latin in a
    // vertical column rotates 90° clockwise here via the interval tangent).
    blob = buildTransformedBlob(shapedWord, flatInterval.interval, penOffset,
                                options.pathText.tangentRotationSteps);
    transformed = true;
  }
  if (!blob) return;
  PositionedRun& run = result.runs.emplace_back();
  run.blob = std::move(blob);
  run.shaped = segment.shaped.get();
  run.origin = origin;
  run.styleIndex = segment.styleIndex;
  run.wordIndex = wordIndex;
  run.lineIndex = flatInterval.sourceLineIndex;
  run.transformed = transformed;
  run.intervalIndex = flatInterval.index;
  run.penOffset = penOffset;
  run.advance = advance;
  run.fit = runFit;
}

}  // namespace detail

}  // namespace sigil::weave

/** @file
 * The bands a layout derives from its placed runs on demand: per-line
 * baseline, ascent, descent and extent for a horizontal layout, per-column
 * axis, pitch and extent for a vertical one, and where every inline
 * placeholder landed.
 */

#include <include/core/SkFontMetrics.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>

#include <optional>

#include <algorithm>
#include <vector>

#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/layout/ParagraphLayout.h"

namespace sigil::weave {

std::vector<LineMetrics> ParagraphLayout::lineMetrics(
    const Paragraph& paragraph) const {
  std::vector<LineMetrics> lines;
  // Memoized per font change: lines overwhelmingly share one (typeface,
  // size), so metric resolution runs once per style stretch, not per run.
  const SkTypeface* lastTypeface = nullptr;
  float lastFontSize = 0;
  SkFontMetrics fontMetrics{};

  for (const PositionedRun& run : runs) {
    if (run.transformed || (run.shaped && run.shaped->vertical)) continue;

    float runAscent = 0;
    float runDescent = 0;
    float runLeft = run.origin.x();
    float runRight = runLeft;
    if (run.shaped) {
      if (run.shaped->typeface.get() != lastTypeface ||
          run.shaped->fontSize != lastFontSize) {
        lastTypeface = run.shaped->typeface.get();
        lastFontSize = run.shaped->fontSize;
        makeFont(run.shaped->typeface, run.shaped->fontSize)
            .getMetrics(&fontMetrics);
      }
      runAscent = -fontMetrics.fAscent;
      runDescent = fontMetrics.fDescent;
      runRight += run.advance;
    } else if (run.placeholderIndex >= 0) {
      const Placeholder& placeholder =
          paragraph.placeholders()[static_cast<size_t>(run.placeholderIndex)];
      runAscent = std::max(0.0f, placeholder.height - placeholder.baselineDrop);
      runDescent = std::max(0.0f, placeholder.baselineDrop);
      runRight += placeholder.width;
    } else {
      continue;
    }

    // Runs arrive line by line, so the active line is nearly always the
    // last entry; fall back to a linear probe for safety (ellipsis trims
    // and bidi never break this in practice, but the contract shouldn't
    // depend on emission order).
    LineMetrics* line = nullptr;
    if (!lines.empty() && lines.back().lineIndex == run.lineIndex) {
      line = &lines.back();
    } else {
      for (LineMetrics& candidate : lines)
        if (candidate.lineIndex == run.lineIndex) {
          line = &candidate;
          break;
        }
    }
    const Word& word = paragraph.words()[run.wordIndex];
    if (!line) {
      lines.push_back({run.lineIndex, run.origin.y(), runAscent, runDescent,
                       runLeft, runRight, word.textBegin, word.whitespaceEnd});
      continue;
    }
    line->ascent = std::max(line->ascent, runAscent);
    line->descent = std::max(line->descent, runDescent);
    line->left = std::min(line->left, runLeft);
    line->right = std::max(line->right, runRight);
    line->textBegin = std::min(line->textBegin, word.textBegin);
    line->textEnd = std::max(line->textEnd, word.whitespaceEnd);
  }

  std::sort(lines.begin(), lines.end(),
            [](const LineMetrics& left, const LineMetrics& right) {
              return left.lineIndex < right.lineIndex;
            });
  return lines;
}

SkPath ParagraphLayout::glyphOutline(const Paragraph& paragraph) const {
  static_cast<void>(paragraph);
  SkPathBuilder outline;
  const SkTypeface* lastTypeface = nullptr;
  float lastFontSize = 0;
  float lastScaleX = 0;
  SkFont font;
  for (const PositionedRun& run : runs) {
    if (!run.shaped) continue;
    const ShapedWord& word = *run.shaped;
    if (word.typeface.get() != lastTypeface || word.fontSize != lastFontSize ||
        word.scaleX != lastScaleX) {
      lastTypeface = word.typeface.get();
      lastFontSize = word.fontSize;
      lastScaleX = word.scaleX;
      font = makeFont(word.typeface, word.fontSize, word.scaleX, word.aliased);
    }
    // A TRANSFORMED run carries its placement per glyph rather than in its
    // origin: the interval it landed on and the pen it started at are the
    // whole of what says where each contour goes, and they are the same two
    // the blob was baked from.
    const LineInterval* interval =
        run.transformed && run.intervalIndex >= 0 &&
                static_cast<size_t>(run.intervalIndex) < intervals.size()
            ? &intervals[static_cast<size_t>(run.intervalIndex)]
            : nullptr;
    float pen = 0;
    for (size_t glyphIndex = 0; glyphIndex < word.glyphs.size(); ++glyphIndex) {
      std::optional<SkPath> contour = font.getPath(word.glyphs[glyphIndex]);
      if (!contour) {
        pen += word.advances[glyphIndex];
        continue;
      }
      if (!interval) {
        outline.addPath(*contour,
                        run.origin.x() + word.positions[glyphIndex].x(),
                        run.origin.y() + word.positions[glyphIndex].y());
        continue;
      }
      const float advance = word.advances[glyphIndex];
      const float offsetX = word.positions[glyphIndex].x() - pen;
      const float offsetY = word.positions[glyphIndex].y();
      SkPoint position;
      SkVector tangent;
      interval->placeAt(run.penOffset + pen + advance * 0.5f, 0.0f,
                        tangentRotationSteps, &position, &tangent);
      const float centreX = advance * 0.5f - offsetX;
      const float centreY = -offsetY;
      SkMatrix place;
      place.setSinCos(tangent.y(), tangent.x());
      place.postTranslate(
          position.x() - (tangent.x() * centreX - tangent.y() * centreY),
          position.y() - (tangent.y() * centreX + tangent.x() * centreY));
      outline.addPath(contour->makeTransform(place));
      pen += advance;
    }
  }
  return outline.detach();
}

std::vector<ColumnMetrics> ParagraphLayout::columnMetrics(
    const Paragraph& paragraph) const {
  std::vector<ColumnMetrics> columns;
  // Memoized per font change, exactly as lineMetrics does it: a tate-chu-yoko
  // run is the only form whose column extent is a font metric rather than an
  // advance, and one column rarely holds more than a couple.
  const SkTypeface* lastTypeface = nullptr;
  float lastFontSize = 0;
  SkFontMetrics fontMetrics{};

  for (const PositionedRun& run : runs) {
    if (run.intervalIndex < 0 ||
        static_cast<size_t>(run.intervalIndex) >= intervals.size())
      continue;
    const LineInterval& interval =
        intervals[static_cast<size_t>(run.intervalIndex)];
    // A column is a straight interval whose pen travels straight down. Any
    // other geometry belongs to lineMetrics or to nothing.
    if (interval.contour.valid() || interval.direction.x() != 0 ||
        interval.direction.y() != 1)
      continue;

    float top = interval.origin.y() + run.penOffset;
    float bottom = top;
    if (run.shaped) {
      if (run.shaped->vertical || run.transformed) {
        // Upright and rotated runs both advance ALONG the column, so the
        // pen extent is the extent.
        bottom = top + run.shaped->advance;
      } else {
        // 縦中横: the run is shaped horizontally and set upright across the
        // column, and its pen offset lands on its BASELINE, so the column
        // extent is the run's font height around that baseline.
        if (run.shaped->typeface.get() != lastTypeface ||
            run.shaped->fontSize != lastFontSize) {
          lastTypeface = run.shaped->typeface.get();
          lastFontSize = run.shaped->fontSize;
          makeFont(run.shaped->typeface, run.shaped->fontSize)
              .getMetrics(&fontMetrics);
        }
        top += fontMetrics.fAscent;  // negative: above the baseline
        bottom += fontMetrics.fDescent;
      }
    } else if (run.placeholderIndex >= 0) {
      bottom =
          top +
          paragraph.placeholders()[static_cast<size_t>(run.placeholderIndex)]
              .width;
    } else {
      continue;
    }

    ColumnMetrics* column = nullptr;
    if (!columns.empty() && columns.back().lineIndex == run.lineIndex) {
      column = &columns.back();
    } else {
      for (ColumnMetrics& candidate : columns)
        if (candidate.lineIndex == run.lineIndex) {
          column = &candidate;
          break;
        }
    }
    const Word& word = paragraph.words()[run.wordIndex];
    if (!column) {
      columns.push_back({run.lineIndex, interval.origin.x(), linePitch, top,
                         bottom, word.textBegin, word.whitespaceEnd});
      continue;
    }
    column->top = std::min(column->top, top);
    column->bottom = std::max(column->bottom, bottom);
    column->textBegin = std::min(column->textBegin, word.textBegin);
    column->textEnd = std::max(column->textEnd, word.whitespaceEnd);
  }

  std::sort(columns.begin(), columns.end(),
            [](const ColumnMetrics& left, const ColumnMetrics& right) {
              return left.lineIndex < right.lineIndex;
            });
  return columns;
}

std::vector<ParagraphLayout::PlacedPlaceholder>
ParagraphLayout::placeholderRects(const Paragraph& paragraph) const {
  std::vector<PlacedPlaceholder> placedPlaceholders;
  for (const PositionedRun& run : runs) {
    if (run.placeholderIndex < 0) continue;
    const Placeholder& placeholder =
        paragraph.placeholders()[static_cast<size_t>(run.placeholderIndex)];
    placedPlaceholders.push_back(
        {run.placeholderIndex,
         SkRect::MakeXYWH(
             run.origin.x(),
             run.origin.y() - placeholder.height + placeholder.baselineDrop,
             placeholder.width, placeholder.height),
         run.lineIndex});
  }
  return placedPlaceholders;
}

}  // namespace sigil::weave

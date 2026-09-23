/** @file
 * The ink one draw of a passage paints its glyphs with: the one
 * glyph-paint override `ink(paint)` and `textStroke()` resolve to, mapped
 * onto the passage's text-metric box, and — where an ink restarts on each
 * unit of the passage — the paint on the unit square the text engine
 * lays on each unit's box instead.
 */

#include <include/core/SkFontMetrics.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilweave/fonts/Shaper.h>  // makeFont — the ink band's cap height

#include <algorithm>
#include <optional>

#include "ComposeRuntime.h"
#include "GlyphInk.h"
#include "PaintInternal.h"

namespace sigil::compose {

detail::TextInk Composer::Impl::textInkOf(Instance& inst,
                                          const PaintContext& paintCtx) {
  detail::TextInk ink;
  const ElementNode& node = *inst.description;
  // A span whose ink restarts per unit shows only where no override hides
  // the spans' paint, so it is looked for first and cleared below if one
  // does.
  if (node.textData)
    for (const SpanRestyle& span : node.textData->spanRestyles)
      if (textUnitOf(span.inkBox) && span.inkShader) {
        ink.spanUnits = true;
        break;
      }
  const material::skia::Paint* metricMat = inkPaintOf(inst);
  // The outline in force: the leaf's own where it states one, else the
  // strongest matched rule's.
  const RuleTextLayer* ruleText = inst.ruleText.get();
  const bool ownStroke = node.textData && (node.textData->options.set &
                                           TextOptions::kTextStroke) != 0;
  const bool ruleStroke = !ownStroke && ruleText && ruleText->statesStroke;
  const bool stroked = ruleStroke
                           ? ruleText->hasTextStroke
                           : node.textData && node.textData->hasTextStroke;
  if (!metricMat && !stroked) return ink;
  if (!inst.paragraph.has_value()) return ink;
  const sigil::weave::Paragraph& paragraph = inst.paragraph.value();

  // Chrome type: the material's unit square mapped to the text's metric
  // band — x across the widest line, y from the first line's cap top (real
  // cap height when the face reports one) to the last line's baseline.
  //
  // The override replaces the whole PaintStyle for every run, so it starts
  // as a COPY of the paragraph's own style and swaps only the foreground —
  // an ink paint supersedes the fill, not the underlays, overlays and
  // decorations around it (a chrome wordmark keeps its cast shadow and dark
  // keyline).
  sigil::weave::PaintStyle metric = paragraph.spans().empty()
                                        ? sigil::weave::PaintStyle{}
                                        : paragraph.spans().front().style.paint;
  metric.foreground.setShader(nullptr);
  bool havePaint = false;
  const auto finish = [&] {
    if (havePaint) {
      ink.passage = std::move(metric);
      ink.spanUnits = false;  // the override paints every glyph
    } else {
      ink.unitSquare.reset();
    }
  };
  // textStroke(): a stroke pass on the glyphs, UNDER the fill. It joins the
  // style's own underlays rather than replacing them, so an engraved face
  // keeps its cast shadow.
  if (stroked) {
    sigil::weave::PaintLayer outline;
    outline.paint.setAntiAlias(true);
    outline.paint.setStyle(SkPaint::kStroke_Style);
    outline.paint.setStrokeWidth(ruleStroke ? ruleText->textStrokeWidth
                                            : node.textData->textStrokeWidth);
    outline.paint.setStrokeJoin(SkPaint::kRound_Join);
    const Fill sf = resolveRef(
        ruleStroke ? ruleText->textStrokeFill : node.textData->textStrokeFill,
        paintCtx);
    if (sf.kind == Fill::Kind::Shader && sf.shaderValue)
      outline.paint.setShader(sf.shaderValue);
    else
      outline.paint.setColor4f(
          material::skia::toSkColor(sf.kind == Fill::Kind::Color
                                        ? sf.colorValue
                                        : material::Color{0, 0, 0, 1}),
          nullptr);
    metric.addUnderlay(outline);
    havePaint = true;
  }
  if (!metricMat) {
    finish();
    return ink;
  }

  // AN INK SPREAD ACROSS THE TREE is already resolved in this node's own
  // space — the slice it stands on of the box the ink was stretched over
  // — so nothing maps it onto the metric band, which is the element's
  // own reading.
  if (spreadsAcrossTree(inst.inkPaint.box)) {
    const Fill anchored = resolveInk(*metricMat, paintCtx);
    if (anchored.kind == Fill::Kind::Shader && anchored.shaderValue) {
      metric.foreground.setShader(anchored.shaderValue);
      havePaint = true;
    } else if (anchored.kind == Fill::Kind::Color) {
      metric.foreground.setColor4f(
          material::skia::toSkColor(anchored.colorValue), nullptr);
      havePaint = true;
    }
    finish();
    return ink;
  }

  // Geometry-dependent materials resolve against a UNIT box here, not the
  // node's. The local matrix below already maps the shader's [0,1]² onto
  // the metric band, so uResolution baked from the node's layout size
  // would divide a second time and a unit-space ramp would collapse onto
  // its first stop, flat and silently. A ramp authored in [0,1]² crosses
  // the type because the band is what it is mapped onto.
  PaintContext metricCtx = paintCtx;
  metricCtx.size = {1.0f, 1.0f};
  const Fill f = (metricMat->isAnimated() || metricMat->geometryDependent())
                     ? resolveFill(*metricMat, metricCtx)
                     : toFill(*metricMat);
  // AN INK THAT RESTARTS PER UNIT keeps its paint on the unit square: the
  // text engine lays it on each unit's own box. The passage mapping below
  // still dresses the decoration bands, which span a run, not a unit.
  const std::optional<sigil::weave::Unit> unit = textUnitOf(inst.inkPaint.box);
  if (f.kind == Fill::Kind::Shader && f.shaderValue && unit) {
    ink.unitSquare = f.shaderValue;
    ink.unit = *unit;
  }
  if (f.kind == Fill::Kind::Shader && f.shaderValue && !inst.columns.empty()) {
    // A VERTICAL passage has no cap band to hang the ramp on: a column's
    // glyphs centre across its axis rather than standing on a baseline. The
    // unit square maps onto the COLUMN BLOCK instead — x across the columns,
    // y down them — so a ramp authored in [0,1]² still crosses the type,
    // reading down the page rather than across it.
    SkRect block = SkRect::MakeEmpty();
    for (const sigil::weave::ColumnMetrics& column : inst.columns)
      block.join(column.rect());
    SkMatrix map = SkMatrix::Translate(block.left(), block.top());
    map.preScale(std::max(block.width(), 1.0f), std::max(block.height(), 1.0f));
    metric.foreground.setShader(f.shaderValue->makeWithLocalMatrix(map));
    havePaint = true;
  } else if (f.kind == Fill::Kind::Shader && f.shaderValue &&
             !inst.lines.empty()) {
    // The first run that carries glyphs is the face the cap band is read
    // from — the runs in draw order, and no walk of every glyph in the
    // passage to reach the first one.
    const sigil::weave::ShapedWord* firstFont = nullptr;
    for (const sigil::weave::PositionedRun& run : inst.textLayout.runs)
      if (run.shaped) {
        firstFont = run.shaped;
        break;
      }
    float capH = 0;
    if (firstFont && firstFont->typeface) {
      SkFontMetrics fm;
      sigil::weave::makeFont(firstFont->typeface, firstFont->fontSize)
          .getMetrics(&fm);
      capH = fm.fCapHeight;
    }
    const sigil::weave::LineMetrics& first = inst.lines.front();
    if (capH <= 0) capH = first.ascent;  // face reports none — the ascent band
    float left = first.left, right = first.right;
    for (const sigil::weave::LineMetrics& line : inst.lines) {
      left = std::min(left, line.left);
      right = std::max(right, line.right);
    }
    const float top = first.baseline - capH;
    const float bottom = inst.lines.back().baseline;
    SkMatrix map = SkMatrix::Translate(left, top);
    map.preScale(std::max(right - left, 1.0f), std::max(bottom - top, 1.0f));
    metric.foreground.setShader(f.shaderValue->makeWithLocalMatrix(map));
    havePaint = true;
  } else if (f.kind == Fill::Kind::Color) {
    metric.foreground.setColor4f(material::skia::toSkColor(f.colorValue),
                                 nullptr);
    havePaint = true;
  }
  finish();
  return ink;
}

}  // namespace sigil::compose

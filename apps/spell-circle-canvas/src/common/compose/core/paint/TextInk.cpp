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
#include <utility>

#include "GlyphInk.h"
#include "PaintInternal.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

bool detail::TextState::Restyle::paints(
    const sigil::weave::Paragraph& paragraph) const {
  const auto& owner = style.paint.foregroundMaterial;
  if (!inkDeclared || !owner || folded) return false;
  const auto& spans = paragraph.spans();
  return std::any_of(spans.begin(), spans.end(), [&](const auto& span) {
    return span.style.paint.foregroundMaterial == owner;
  });
}

const material::Paint* detail::TextState::Restyle::paintUnder(
    const material::Lighting& lighting) {
  const auto& source = style.paint.foregroundMaterial;
  if (!inkDeclared || !source) return nullptr;
  if (!material::skia::isLit(*source)) {
    if (!ink) ink = material::skia::paint(*source);
    return &*ink;
  }
  const auto under = material::skia::lightingFor(*source, lighting);
  if (!under && !litInputs) {
    if (!ink) ink = material::skia::paint(*source);
    return &*ink;
  }
  if (!litInputs) litInputs.emplace(*source);
  if (!litPaint || litUnder != under) {
    litPaint = litInputs->under(under);
    litUnder = under;
  }
  return &*litPaint;
}

detail::TextInk Composer::Impl::textInkOf(Instance& inst,
                                          const PaintContext& paintCtx) {
  detail::TextInk ink;
  ink.frame = frameOf(paintCtx);
  const ElementNode& node = *inst.description;
  const material::Paint* metricMat = inkPaintOf(inst);
  // The outline in force: the leaf's own where it states one, else the
  // strongest matched rule's.
  const RuleTextLayer* ruleText = inst.ruleText.get();
  const bool ownStroke = node.textData && (node.textData->options.set &
                                           TextOptions::kTextStroke) != 0;
  const bool ruleStroke = !ownStroke && ruleText && ruleText->statesStroke;
  const bool stroked = ruleStroke
                           ? ruleText->hasTextStroke
                           : node.textData && node.textData->hasTextStroke;
  // Span ownership belongs to the retained styles, which can outlive a
  // structurally equal replacement of the description. Resolve each
  // retained source once, before the glyph walk chooses which one paints.
  if (!metricMat && !stroked && node.textData && inst.textState &&
      inst.paragraph) {
    for (detail::TextState::Restyle& retained : inst.textState->restyles) {
      if (!retained.paints(*inst.paragraph)) continue;
      detail::TextInk::Span span;
      span.source = retained.style.paint.foregroundMaterial;
      span.unit = textUnitOf(retained.inkBox);
      const auto& inherited =
          inst.lighting ? *inst.lighting : material::Lighting{};
      const auto under =
          span.unit ? material::skia::lightingFor(*span.source, inherited)
                    : material::Lighting{};
      if (under) {
        if (!retained.litInputs) retained.litInputs.emplace(*span.source);
        span.surface = detail::TextInk::Surface{*retained.litInputs, under};
      } else {
        const material::Paint* paint = retained.paintUnder(inherited);
        if (!paint) continue;
        PaintContext context = paintCtx;
        if (span.unit) context.size = {1.0f, 1.0f};
        const Fill resolved = resolveFill(*paint, context);
        if (resolved.kind == Fill::Kind::Paint)
          span.shader = material::skia::staticShader(detail::paintOf(resolved));
        else if (resolved.kind == Fill::Kind::Color)
          span.color = resolved.colorValue;
      }
      ink.spans.push_back(std::move(span));
    }
  }
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
  metric.foregroundMaterial.reset();
  bool havePaint = false;
  const auto finish = [&] {
    if (havePaint) {
      ink.passage = std::move(metric);
      ink.spans.clear();  // the override paints every glyph
    } else {
      ink.unitSquare.reset();
      ink.unitSurface.reset();
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
    const Fill sf = resolveFill(
        ruleStroke ? ruleText->textStrokeFill : node.textData->textStrokeFill,
        paintCtx);
    if (sk_sp<SkShader> shader =
            sf.kind == Fill::Kind::Paint
                ? material::skia::staticShader(detail::paintOf(sf))
                : nullptr)
      outline.paint.setShader(std::move(shader));
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
    if (sk_sp<SkShader> shader =
            anchored.kind == Fill::Kind::Paint
                ? material::skia::staticShader(detail::paintOf(anchored))
                : nullptr) {
      metric.foreground.setShader(std::move(shader));
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
  std::optional<detail::TextInk::Surface> surface;
  if (inst.litInkInputs && inst.inkPaint.surfaced) {
    const auto under = material::skia::lightingFor(
        *inst.inkPaint.surfaced,
        inst.lighting ? *inst.lighting : material::Lighting{});
    if (under) surface = detail::TextInk::Surface{*inst.litInkInputs, under};
  }
  const Fill f = surface ? Fill::none() : resolveFill(*metricMat, metricCtx);
  // AN INK THAT RESTARTS PER UNIT keeps its paint on the unit square: the
  // text engine lays it on each unit's own box. The passage mapping below
  // still dresses the decoration bands, which span a run, not a unit.
  const std::optional<sigil::weave::Unit> unit = textUnitOf(inst.inkPaint.box);
  const sk_sp<SkShader> shader =
      f.kind == Fill::Kind::Paint
          ? material::skia::staticShader(detail::paintOf(f))
          : nullptr;
  if ((shader || surface) && unit) {
    ink.unitSquare = shader;
    ink.unitSurface = surface;
    ink.unit = *unit;
  }
  const auto mapped = [&](const SkMatrix& map) {
    return surface ? surface->inputs.shader(surface->lighting, ink.frame,
                                            material::skia::toMatrix(map))
                   : shader->makeWithLocalMatrix(map);
  };
  if ((shader || surface) && !inst.columns.empty()) {
    // A VERTICAL passage has no cap band to hang the ramp on: a column's
    // glyphs centre across its axis rather than standing on a baseline. The
    // unit square maps onto the COLUMN BLOCK instead — x across the columns,
    // y down them — so a ramp authored in [0,1]² still crosses the type,
    // reading down the page rather than across it.
    SkRect block = SkRect::MakeEmpty();
    for (const sigil::weave::ColumnMetrics& column : inst.columns)
      block.join(sigil::geometry::path::toSk(column.rect()));
    SkMatrix map = SkMatrix::Translate(block.left(), block.top());
    map.preScale(std::max(block.width(), 1.0f), std::max(block.height(), 1.0f));
    metric.foreground.setShader(mapped(map));
    havePaint = true;
  } else if ((shader || surface) && !inst.lines.empty()) {
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
    metric.foreground.setShader(mapped(map));
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

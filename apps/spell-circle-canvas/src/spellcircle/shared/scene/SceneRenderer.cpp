#include "SceneRenderer.h"

#include <sigilweave/SigilWeave.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <string_view>

namespace spellcircle {

namespace {

// Components store UTF-8 std::strings; the paragraph cache accepts them as
// u8string_view without copying or transcoding.
std::u8string_view u8view(const std::string &text) {
  return {reinterpret_cast<const char8_t *>(text.data()), text.size()};
}

} // namespace

SceneRenderer::SceneRenderer() = default;
SceneRenderer::~SceneRenderer() = default;

sigil::weave::FontContext &SceneRenderer::fontContext() {
  // Created on first use, on the calling thread (its owner) — all label and
  // paragraph text below goes through it.
  if (!m_textContext)
    m_textContext = std::make_unique<sigil::weave::FontContext>(
        sigil::weave::ports::systemFontManager());
  return *m_textContext;
}

void SceneRenderer::draw(SkCanvas *skCanvas, const ResolvedScene &scene,
                         const SceneStyle &style) {
  if (!skCanvas)
    return;

  sigil::weave::FontContext &textContext = fontContext();

  const SkColor accentColor = style.accentColor;

  // The FontContext's system font manager is reused — recreating one
  // enumerates system fonts and is not per-frame cheap. A null configured
  // typeface falls back to the context default so labels never silently
  // draw nothing.
  sk_sp<SkTypeface> typeface = style.typeface;
  if (!typeface)
    typeface = textContext.defaultTypeface();
  SkFont font(typeface, style.fontSize);
  font.setSubpixel(true);
  font.setEdging(SkFont::Edging::kAntiAlias);

  SkFontMetrics fontMetrics;
  font.getMetrics(&fontMetrics);

  // ── Edges (drawn underneath everything) ─────────────────────────────────
  SkPaint edgePaint;
  edgePaint.setAntiAlias(true);
  edgePaint.setStyle(SkPaint::kStroke_Style);
  edgePaint.setStrokeWidth(style.strokeWidth);
  edgePaint.setColor(accentColor);
  for (const auto &edge : scene.edges) {
    skCanvas->drawLine(edge.first.x, edge.first.y, edge.second.x,
                       edge.second.y, edgePaint);
  }

  // ── Circles ──────────────────────────────────────────────────────────────
  // Paints hoisted out of the loop (like edgePaint above): only the fill
  // alpha varies per circle.
  SkPaint circleStrokePaint;
  circleStrokePaint.setAntiAlias(true);
  circleStrokePaint.setStyle(SkPaint::kStroke_Style);
  circleStrokePaint.setStrokeWidth(style.strokeWidth);
  circleStrokePaint.setColor(accentColor);
  SkPaint circleFillPaint;
  circleFillPaint.setAntiAlias(true);
  circleFillPaint.setStyle(SkPaint::kFill_Style);
  circleFillPaint.setColor(accentColor);
  for (const auto &circle : scene.circles) {
    // A radius-0 circle is an invisible anchor (see isAnchorOnlyCircle in
    // SceneGeometry.cpp) used only to position points/boxes/edges — nothing
    // to draw for it.
    if (circle.radius <= 0.0f)
      continue;

    const float radius = circle.radius;
    const float centerX = circle.center.x;
    const float centerY = circle.center.y;

    skCanvas->drawCircle(centerX, centerY, radius, circleStrokePaint);

    if (circle.active > 0.0f) {
      circleFillPaint.setAlphaf(circle.active);
      skCanvas->drawCircle(centerX, centerY, radius, circleFillPaint);
    }

    if (!circle.name.empty()) {
      // Curved label: glyph middles ride (radius + labelOffset), curvature-
      // compensated and centre-anchored on textStart (see
      // spellcircle::makeRingLabelInterval). Rings are origin-centred and
      // cached, so a canvas translate puts the label on the actual circle.
      const sigil::weave::LineInterval interval = makeRingLabelInterval(
          m_ringLabelGeometry, fontMetrics, radius + style.labelOffset,
          circle.textStart);
      if (interval.contour) {
        sigil::weave::Paragraph &label = m_labelParagraphs.paragraphFor(
            u8view(circle.name), typeface, style.fontSize);
        sigil::weave::LineSetFlow flow;
        flow.lines().push_back({interval});
        sigil::weave::ParagraphLayoutOptions labelOptions;
        labelOptions.alignment = sigil::weave::TextAlignment::kCenter;
        sigil::weave::ParagraphLayout labelLayout =
            sigil::weave::layoutParagraph(textContext, label, flow, labelOptions);
        sigil::weave::PaintStyle accentPaint(accentColor);
        skCanvas->save();
        skCanvas->translate(centerX, centerY);
        labelLayout.draw(skCanvas, label, &accentPaint);
        skCanvas->restore();
      }
    }
  }

  // ── Point value labels ───────────────────────────────────────────────────
  // Positioned the same way as a box (anchor pushed outward by boxDistance
  // along the ray from the canvas center), but a point isn't a box: just
  // the text, no rect/stroke/fill around it.
  for (const auto &pointLabel : scene.pointLabels) {
    const float centerX =
        pointLabel.anchor.x + pointLabel.direction.x * style.pointDistance;
    const float centerY =
        pointLabel.anchor.y + pointLabel.direction.y * style.pointDistance;
    sigil::weave::Paragraph &label = m_labelParagraphs.paragraphFor(
        u8view(pointLabel.value), typeface, style.fontSize);
    const float textWidth = label.naturalWidth(textContext);
    sigil::weave::ParagraphLayout labelLayout = sigil::weave::layoutSingleLine(
        textContext, label,
        {centerX - textWidth * 0.5f,
         centerY + centeredBaselineOffset(fontMetrics)});
    sigil::weave::PaintStyle accentPaint(accentColor);
    labelLayout.draw(skCanvas, label, &accentPaint);
  }

  // ── Boxes ────────────────────────────────────────────────────────────────
  // Box paints hoisted like the circle paints; only fill alpha varies.
  SkPaint boxClearPaint;
  boxClearPaint.setBlendMode(SkBlendMode::kClear);
  SkPaint boxFillPaint;
  boxFillPaint.setAntiAlias(true);
  boxFillPaint.setStyle(SkPaint::kFill_Style);
  boxFillPaint.setColor(accentColor);
  SkPaint boxStrokePaint;
  boxStrokePaint.setAntiAlias(true);
  boxStrokePaint.setStyle(SkPaint::kStroke_Style);
  boxStrokePaint.setStrokeWidth(style.strokeWidth);
  boxStrokePaint.setColor(accentColor);
  for (const auto &box : scene.boxes) {
    sigil::weave::Paragraph &label = m_labelParagraphs.paragraphFor(
        u8view(box.value), typeface, style.fontSize);
    const float textWidth = label.naturalWidth(textContext);
    const float resolvedBoxWidth =
        std::max(textWidth + style.boxPadding * 2.0f, style.boxWidth);
    const float resolvedBoxHeight = style.boxHeight;

    // The box's center sits on the ray from the canvas center through the
    // point, pushed outward by boxDistance beyond the point — nothing more.
    // Edge-dependent anchoring (snapping the box's near edge to the point)
    // was rejected: it makes the box jump as the ray crosses 45° diagonals.
    const float boxCenterX = box.anchor.x + box.direction.x * style.boxDistance;
    const float boxCenterY = box.anchor.y + box.direction.y * style.boxDistance;
    const float boxX = boxCenterX - resolvedBoxWidth / 2.0f;
    const float boxY = boxCenterY - resolvedBoxHeight / 2.0f;

    const SkRect boxBounds =
        SkRect::MakeXYWH(boxX, boxY, resolvedBoxWidth, resolvedBoxHeight);

    // Punch a transparent hole first so whatever was drawn underneath
    // (edges, circles) doesn't show through the box — equivalent to
    // QCanvasPainter::clearRect().
    skCanvas->drawRect(boxBounds, boxClearPaint);

    if (box.active > 0.0f) {
      boxFillPaint.setAlphaf(box.active);
      skCanvas->drawRect(boxBounds, boxFillPaint);
    }

    skCanvas->drawRect(boxBounds, boxStrokePaint);

    // TextBaseline::Top: the top of the glyphs (not the alphabetic
    // baseline) sits at (boxX + boxPadding, boxY + boxPadding).
    sigil::weave::ParagraphLayout labelLayout = sigil::weave::layoutSingleLine(
        textContext, label,
        {boxX + style.boxPadding,
         boxY + style.boxPadding - fontMetrics.fAscent});
    sigil::weave::PaintStyle textPaint;
    if (box.active > 0.0f) {
      // Punches the text out of the box fill instead of drawing it on top
      // — matches QCanvasPainter::CompositeOperation::DestinationOut.
      textPaint.foreground.setColor(SK_ColorBLACK);
      textPaint.foreground.setBlendMode(SkBlendMode::kDstOut);
    } else {
      textPaint.foreground.setColor(accentColor);
    }
    labelLayout.draw(skCanvas, label, &textPaint);
  }
}

} // namespace spellcircle

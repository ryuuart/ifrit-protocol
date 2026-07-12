#include "SkiaSceneBackend.h"
#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"
#include "SpellCircleRenderer.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkRect.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <textflow/TextFlow.h>
#include <textflowqt/TextFlowQt.h>

#include <QByteArray>
#include <QColor>
#include <QPointF>
#include <QString>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>

using textflowqt::toSkColor;
using textflowqt::toU16;

// SkiaSceneBackendImpl deliberately lives at global scope (not in an
// anonymous namespace) so that SpellCircleRenderer's
// `friend class SkiaSceneBackendImpl;` declaration names this exact type —
// a class in an anonymous namespace gets internal linkage under a
// compiler-generated unique name, and the friend declaration would instead
// silently forward-declare an unrelated, inaccessible ::SkiaSceneBackendImpl.
class SkiaSceneBackendImpl final : public CanvasSceneBackend {
public:
  explicit SkiaSceneBackendImpl(std::unique_ptr<SkiaGraphiteContext> context)
      : m_context(std::move(context)) {}

  QCanvasImage drawScene(SpellCircleRenderer &renderer, QCanvasPainter *painter,
                         QCanvasOffscreenCanvas &canvas,
                         QSize pixelSize) override {
    // All coordinates are in 0..canvasWidth / 0..canvasHeight scene space,
    // same as QCanvasPainterSceneBackend — pixelSize always matches that
    // scene size (see SpellCircleRenderer::prePaint), so the resolved
    // geometry in renderer.m_circles/m_edges/m_boxes/m_pointLabels can be
    // drawn directly with no extra scaling here.
    const auto frameStart = std::chrono::steady_clock::now();
    SkiaOffscreenSurface surface(*m_context, canvas.texture(), pixelSize);
    SkCanvas *skCanvas = surface.canvas();
    if (!skCanvas)
      return {};

    skCanvas->clear(SK_ColorTRANSPARENT);
    // Created on first frame, on the render thread (its owner) — all label
    // and paragraph text below goes through it.
    if (!m_textCtx)
      m_textCtx = std::make_unique<textflow::FontContext>(
          SkFontMgr_New_CoreText(nullptr));

    const SkColor accentColor = toSkColor(renderer.m_accentColor);
    const float strokeWidth =
        static_cast<float>(renderer.m_strokeWidth * renderer.m_scale);
    const float boxWidth =
        static_cast<float>(renderer.m_boxWidth * renderer.m_scale);
    const float boxHeight =
        static_cast<float>(renderer.m_boxHeight * renderer.m_scale);
    const float boxPadding =
        static_cast<float>(renderer.m_boxPadding * renderer.m_scale);
    const float boxDistance =
        static_cast<float>(renderer.m_boxDistance * renderer.m_scale);
    const float pointDistance =
        static_cast<float>(renderer.m_pointDistance * renderer.m_scale);
    const float labelOffset =
        static_cast<float>(renderer.m_labelOffset * renderer.m_scale);

    const float fontSize =
        static_cast<float>(renderer.m_font.pointSizeF() * renderer.m_scale);
    // The settings dialog's "Font Style" picker (e.g. "Light", "Semibold
    // Italic") sets weight/slant via QFontDatabase::font() (see
    // FontDatabase.cpp); textflowqt::toSkTypeface carries both through.
    // The FontContext's CoreText font manager is reused — recreating one
    // enumerates system fonts and is not per-frame cheap.
    sk_sp<SkTypeface> typeface =
        textflowqt::toSkTypeface(m_textCtx->fontMgr(), renderer.m_font);
    SkFont font(typeface, fontSize);
    font.setSubpixel(true);
    font.setEdging(SkFont::Edging::kAntiAlias);

    SkFontMetrics fontMetrics;
    font.getMetrics(&fontMetrics);

    // ── Edges (drawn underneath everything) ─────────────────────────────────
    SkPaint edgePaint;
    edgePaint.setAntiAlias(true);
    edgePaint.setStyle(SkPaint::kStroke_Style);
    edgePaint.setStrokeWidth(strokeWidth);
    edgePaint.setColor(accentColor);
    for (const auto &edge : renderer.m_edges) {
      skCanvas->drawLine(static_cast<float>(edge.first.x()),
                         static_cast<float>(edge.first.y()),
                         static_cast<float>(edge.second.x()),
                         static_cast<float>(edge.second.y()), edgePaint);
    }

    // ── Circles ──────────────────────────────────────────────────────────────
    for (const auto &circle : renderer.m_circles) {
      // A radius-0 circle is an invisible anchor (see isAnchorOnlyCircle in
      // SpellCircleRenderer.cpp) used only to position points/boxes/edges —
      // nothing to draw for it.
      if (circle.radius <= 0.0f)
        continue;

      const float r = circle.radius;
      const float cx = static_cast<float>(circle.center.x());
      const float cy = static_cast<float>(circle.center.y());

      SkPaint strokePaint;
      strokePaint.setAntiAlias(true);
      strokePaint.setStyle(SkPaint::kStroke_Style);
      strokePaint.setStrokeWidth(strokeWidth);
      strokePaint.setColor(accentColor);
      skCanvas->drawCircle(cx, cy, r, strokePaint);

      if (circle.active > 0.0f) {
        SkPaint fillPaint;
        fillPaint.setAntiAlias(true);
        fillPaint.setStyle(SkPaint::kFill_Style);
        fillPaint.setColor(accentColor);
        fillPaint.setAlphaf(circle.active);
        skCanvas->drawCircle(cx, cy, r, fillPaint);
      }

      if (!circle.name.isEmpty()) {
        // Curved label: glyph middles ride (r + labelOffset), curvature-
        // compensated and centre-anchored on textStart (see
        // textflow::makeRingLabel). Rings are origin-centred and cached, so
        // a canvas translate puts the label on the actual circle.
        const textflow::LineInterval interval = textflow::makeRingLabel(
            m_rings, fontMetrics, r + labelOffset, circle.textStart);
        if (interval.contour) {
          textflow::Paragraph &label =
              m_labels.get(toU16(circle.name), typeface, fontSize);
          textflow::LineSetFlow flow;
          flow.lines().push_back({interval});
          textflow::LayoutOptions labelOpts;
          labelOpts.alignment = textflow::Alignment::kCenter;
          textflow::Layout labelLayout =
              textflow::layoutParagraph(*m_textCtx, label, flow, labelOpts);
          textflow::PaintStyle accentPaint;
          accentPaint.color = accentColor;
          skCanvas->save();
          skCanvas->translate(cx, cy);
          labelLayout.draw(skCanvas, label, &accentPaint);
          skCanvas->restore();
        }
      }
    }

    // ── Point value labels ───────────────────────────────────────────────────
    // Positioned the same way as a box (anchor pushed outward by boxDistance
    // along the ray from the canvas center), but a point isn't a box: just
    // the text, no rect/stroke/fill around it.
    for (const auto &pointLabel : renderer.m_pointLabels) {
      const QPointF center =
          pointLabel.anchor + pointLabel.direction * pointDistance;
      textflow::Paragraph &label =
          m_labels.get(toU16(pointLabel.value), typeface, fontSize);
      const float textW = label.naturalWidth(*m_textCtx);
      textflow::Layout labelLayout = textflow::layoutLabel(
          *m_textCtx, label,
          {static_cast<float>(center.x()) - textW * 0.5f,
           static_cast<float>(center.y()) +
               textflow::middleBaselineOffset(fontMetrics)});
      textflow::PaintStyle accentPaint;
      accentPaint.color = accentColor;
      labelLayout.draw(skCanvas, label, &accentPaint);
    }

    // ── Boxes ────────────────────────────────────────────────────────────────
    auto drawBox = [&](const SpellCircleRenderer::ResolvedBox &box) {
      textflow::Paragraph &label =
          m_labels.get(toU16(box.value), typeface, fontSize);
      const float textW = label.naturalWidth(*m_textCtx);
      const float bw = std::max(textW + boxPadding * 2.0f, boxWidth);
      const float bh = boxHeight;

      // The box's center sits on the ray from the canvas center through the
      // point, pushed outward by boxDistance beyond the point — nothing more
      // (see the equivalent comment in QCanvasPainterSceneBackend::drawScene
      // for why edge-dependent anchoring was rejected).
      const QPointF boxCenter = box.anchor + box.direction * boxDistance;
      const float hw = bw / 2.0f;
      const float hh = bh / 2.0f;
      const float bx = static_cast<float>(boxCenter.x()) - hw;
      const float by = static_cast<float>(boxCenter.y()) - hh;

      const SkRect rect = SkRect::MakeXYWH(bx, by, bw, bh);

      // Punch a transparent hole first so whatever was drawn underneath
      // (edges, circles) doesn't show through the box — equivalent to
      // QCanvasPainter::clearRect().
      SkPaint clearPaint;
      clearPaint.setBlendMode(SkBlendMode::kClear);
      skCanvas->drawRect(rect, clearPaint);

      if (box.active > 0.0f) {
        SkPaint fillPaint;
        fillPaint.setAntiAlias(true);
        fillPaint.setStyle(SkPaint::kFill_Style);
        fillPaint.setColor(accentColor);
        fillPaint.setAlphaf(box.active);
        skCanvas->drawRect(rect, fillPaint);
      }

      SkPaint strokePaint;
      strokePaint.setAntiAlias(true);
      strokePaint.setStyle(SkPaint::kStroke_Style);
      strokePaint.setStrokeWidth(strokeWidth);
      strokePaint.setColor(accentColor);
      skCanvas->drawRect(rect, strokePaint);

      // TextBaseline::Top: the top of the glyphs (not the alphabetic
      // baseline) sits at (bx + boxPadding, by + boxPadding).
      textflow::Layout labelLayout = textflow::layoutLabel(
          *m_textCtx, label,
          {bx + boxPadding, by + boxPadding - fontMetrics.fAscent});
      textflow::PaintStyle textPaint;
      if (box.active > 0.0f) {
        // Punches the text out of the box fill instead of drawing it on top
        // — matches QCanvasPainter::CompositeOperation::DestinationOut.
        textPaint.color = SK_ColorBLACK;
        textPaint.blendMode = SkBlendMode::kDstOut;
      } else {
        textPaint.color = accentColor;
      }
      labelLayout.draw(skCanvas, label, &textPaint);
    };
    for (const auto &box : renderer.m_boxes)
      drawBox(box);

    const auto recordEnd = std::chrono::steady_clock::now();
    surface.submit();
    const auto submitEnd = std::chrono::steady_clock::now();

    const double recordUs = std::chrono::duration<double, std::micro>(
                                recordEnd - frameStart)
                                .count();
    const double submitUs =
        std::chrono::duration<double, std::micro>(submitEnd - recordEnd)
            .count();
    m_recordUsEma =
        m_recordUsEma == 0 ? recordUs : m_recordUsEma * 0.98 + recordUs * 0.02;
    m_submitUsEma =
        m_submitUsEma == 0 ? submitUs : m_submitUsEma * 0.98 + submitUs * 0.02;
    if (m_sceneFrame++ % 600 == 0)
      spdlog::info("drawScene: record {:.0f} us (ema {:.0f}), submit {:.0f} "
                   "us (ema {:.0f})",
                   recordUs, m_recordUsEma, submitUs, m_submitUsEma);

    // The offscreen canvas's blended output is premultiplied-alpha; without
    // this flag drawImage() re-applies alpha on top of already alpha-baked-in
    // color, darkening every translucent fill.
    return painter->addImage(canvas,
                             QCanvasPainter::ImageFlag::GenerateMipmaps |
                                 QCanvasPainter::ImageFlag::Premultiplied);
  }

private:
  std::unique_ptr<SkiaGraphiteContext> m_context;

  std::unique_ptr<textflow::FontContext> m_textCtx; // render-thread only
  textflow::LabelCache m_labels; // shaped once per unique (text, font, size)
  textflow::RingCache m_rings;   // measured once per label radius
  uint64_t m_sceneFrame = 0;
  double m_recordUsEma = 0;
  double m_submitUsEma = 0;
};

std::unique_ptr<CanvasSceneBackend> createSkiaSceneBackend(QRhi *rhi) {
  std::unique_ptr<SkiaGraphiteContext> context =
      SkiaGraphiteContext::create(rhi);
  if (!context)
    return nullptr;
  return std::make_unique<SkiaSceneBackendImpl>(std::move(context));
}

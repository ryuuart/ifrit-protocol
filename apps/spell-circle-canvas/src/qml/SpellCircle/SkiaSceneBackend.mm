#include "SkiaSceneBackend.h"
#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"
#include "SpellCircleRenderer.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/ports/SkFontMgr_mac_ct.h>

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QPointF>
#include <QString>

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

SkColor toSkColor(const QColor &color) {
  return SkColorSetARGB(color.alpha(), color.red(), color.green(),
                        color.blue());
}

// Vertical offset from a target y to the baseline y that centers a glyph's
// [ascent, descent] span on that target — mirrors QCanvasPainter's
// TextBaseline::Middle.
float middleBaselineOffset(const SkFontMetrics &metrics) {
  return -(metrics.fAscent + metrics.fDescent) * 0.5f;
}

// Paints `text` tangent to a circle of `radius` centered at `center`, one
// glyph at a time — the same idea as TextPathPainter (see
// TextPathPainter.cpp) but specialised to a circle instead of an arbitrary
// QPainterPath: a circle has constant curvature, so the arc-length parameter
// t maps directly to angle (phi = 2*pi*t) with no need to flatten/sample the
// curve first.
//
//   - pathOffset [0, 1]: fraction of the (offset) circle's circumference used
//     as the text run's anchor, center-aligned around it (matching
//     TextPathPainter's default PathAlignment::Center).
//   - perpOffset: signed outward radial displacement — glyphs are placed on
//     a circle of radius (radius + perpOffset), not `radius` itself, so
//     spacing stays uniform on the curve the text actually sits on.
void drawTextAlongCircle(SkCanvas *canvas, const SkFont &font, SkColor color,
                         const QPointF &center, float radius, float pathOffset,
                         float perpOffset, const QString &text) {
  if (text.isEmpty())
    return;

  const float rArc = radius + perpOffset;
  const float arcLen = 2.0f * kPi * rArc;
  if (arcLen <= 0.0f)
    return;

  QHash<QChar, float> glyphCache;
  auto glyphAdvance = [&](QChar ch) -> float {
    auto it = glyphCache.constFind(ch);
    if (it != glyphCache.constEnd())
      return it.value();
    const QByteArray utf8 = QString(ch).toUtf8();
    const float advance =
        font.measureText(utf8.constData(), utf8.size(), SkTextEncoding::kUTF8);
    glyphCache.insert(ch, advance);
    return advance;
  };

  float totalWidth = 0.0f;
  for (QChar ch : text)
    totalWidth += glyphAdvance(ch);

  const float anchor = pathOffset * arcLen;
  float arcPos = anchor - totalWidth * 0.5f; // PathAlignment::Center

  SkFontMetrics metrics;
  font.getMetrics(&metrics);
  const float baselineShift = middleBaselineOffset(metrics);

  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor(color);

  const float cx = static_cast<float>(center.x());
  const float cy = static_cast<float>(center.y());

  for (QChar ch : text) {
    const float advance = glyphAdvance(ch);
    const float charCenter = arcPos + advance * 0.5f;
    arcPos += advance;

    if (charCenter < 0.0f || charCenter > arcLen)
      continue;

    const float t = charCenter / arcLen;
    const float phi = 2.0f * kPi * t;
    const float px = cx + rArc * std::cos(phi);
    const float py = cy + rArc * std::sin(phi);
    // Tangent direction at phi (travel direction as t increases) is
    // (-sin phi, cos phi); rotating the glyph's local +x axis to align with
    // it means rotating by (phi + 90deg).
    const float angleDeg = (phi + kPi * 0.5f) * 180.0f / kPi;

    canvas->save();
    canvas->translate(px, py);
    canvas->rotate(angleDeg);
    const QByteArray utf8 = QString(ch).toUtf8();
    canvas->drawString(utf8.constData(), -advance * 0.5f, baselineShift, font,
                       paint);
    canvas->restore();
  }
}

} // namespace

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
    SkiaOffscreenSurface surface(*m_context, canvas.texture(), pixelSize);
    SkCanvas *skCanvas = surface.canvas();
    if (!skCanvas)
      return {};

    skCanvas->clear(SK_ColorTRANSPARENT);

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
    const QByteArray familyUtf8 = renderer.m_font.family().toUtf8();
    // QFont::weight() is already on the same 1-900 (Thin..Black) scale as
    // SkFontStyle's weight, so it carries through directly — the settings
    // dialog's "Font Style" picker (e.g. "Light", "Semibold Italic") sets it
    // via QFontDatabase::font(), not just a bold/normal bool (see
    // FontDatabase::font() in FontDatabase.cpp).
    const int fontWeight = static_cast<int>(renderer.m_font.weight());
    SkFontStyle::Slant fontSlant = SkFontStyle::kUpright_Slant;
    if (renderer.m_font.style() == QFont::StyleItalic)
      fontSlant = SkFontStyle::kItalic_Slant;
    else if (renderer.m_font.style() == QFont::StyleOblique)
      fontSlant = SkFontStyle::kOblique_Slant;
    const SkFontStyle fontStyle(fontWeight, SkFontStyle::kNormal_Width,
                                fontSlant);
    // Recreating a CoreText SkFontMgr is expensive (it enumerates system
    // fonts), so it's cached for the render thread's lifetime rather than
    // rebuilt every drawScene() call.
    static const sk_sp<SkFontMgr> fontMgr = SkFontMgr_New_CoreText(nullptr);
    sk_sp<SkTypeface> typeface = fontMgr->matchFamilyStyle(
        familyUtf8.isEmpty() ? nullptr : familyUtf8.constData(), fontStyle);
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
        drawTextAlongCircle(skCanvas, font, accentColor, circle.center, r,
                            circle.textStart, labelOffset, circle.name);
      }
    }

    // ── Point value labels ───────────────────────────────────────────────────
    // Positioned the same way as a box (anchor pushed outward by boxDistance
    // along the ray from the canvas center), but a point isn't a box: just
    // the text, no rect/stroke/fill around it.
    for (const auto &label : renderer.m_pointLabels) {
      const QPointF center = label.anchor + label.direction * pointDistance;
      const QByteArray utf8 = label.value.toUtf8();
      const float textW = font.measureText(utf8.constData(), utf8.size(),
                                           SkTextEncoding::kUTF8);

      SkPaint paint;
      paint.setAntiAlias(true);
      paint.setColor(accentColor);
      skCanvas->drawString(
          utf8.constData(), static_cast<float>(center.x()) - textW * 0.5f,
          static_cast<float>(center.y()) + middleBaselineOffset(fontMetrics),
          font, paint);
    }

    // ── Boxes ────────────────────────────────────────────────────────────────
    auto drawBox = [&](const SpellCircleRenderer::ResolvedBox &box) {
      const QByteArray utf8 = box.value.toUtf8();
      const float textW = font.measureText(utf8.constData(), utf8.size(),
                                           SkTextEncoding::kUTF8);
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
      const float baselineY = by + boxPadding - fontMetrics.fAscent;

      SkPaint textPaint;
      textPaint.setAntiAlias(true);
      if (box.active > 0.0f) {
        // Punches the text out of the box fill instead of drawing it on top
        // — matches QCanvasPainter::CompositeOperation::DestinationOut.
        textPaint.setBlendMode(SkBlendMode::kDstOut);
        textPaint.setColor(SK_ColorBLACK);
      } else {
        textPaint.setColor(accentColor);
      }
      skCanvas->drawString(utf8.constData(), bx + boxPadding, baselineY, font,
                           textPaint);
    };
    for (const auto &box : renderer.m_boxes)
      drawBox(box);

    surface.submit();

    // The offscreen canvas's blended output is premultiplied-alpha; without
    // this flag drawImage() re-applies alpha on top of already alpha-baked-in
    // color, darkening every translucent fill.
    return painter->addImage(canvas,
                             QCanvasPainter::ImageFlag::GenerateMipmaps |
                                 QCanvasPainter::ImageFlag::Premultiplied);
  }

private:
  std::unique_ptr<SkiaGraphiteContext> m_context;
};

std::unique_ptr<CanvasSceneBackend> createSkiaSceneBackend(QRhi *rhi) {
  std::unique_ptr<SkiaGraphiteContext> context =
      SkiaGraphiteContext::create(rhi);
  if (!context)
    return nullptr;
  return std::make_unique<SkiaSceneBackendImpl>(std::move(context));
}

#include "QCanvasPainterSceneBackend.h"
#include "SpellCircleRenderer.h"
#include <QCanvasPath>
#include <QColor>
#include <QPainterPath>

QCanvasImage
QCanvasPainterSceneBackend::drawScene(SpellCircleRenderer &renderer,
                                      QCanvasPainter *painter,
                                      QCanvasOffscreenCanvas &canvas, QSize) {
  renderer.beginCanvasPainting(canvas);

  // All coordinates are in 0..canvasWidth / 0..canvasHeight scene space; the
  // offscreen canvas is already sized (and its transform set up) to match.
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

  QFont scaledFont = renderer.m_font;
  scaledFont.setPointSizeF(renderer.m_font.pointSizeF() * renderer.m_scale);

  // ── Edges (drawn underneath everything) ──────────────────────────────────
  painter->setStrokeStyle(renderer.m_accentColor);
  painter->setLineWidth(strokeWidth);
  for (const auto &edge : renderer.m_edges) {
    painter->beginPath();
    QCanvasPath line;
    line.moveTo(edge.first.x(), edge.first.y());
    line.lineTo(edge.second.x(), edge.second.y());
    painter->addPath(line);
    painter->stroke(line);
    painter->closePath();
  }

  // ── Circles ──────────────────────────────────────────────────────────────
  for (const auto &circle : renderer.m_circles) {
    // A radius-0 circle is an invisible anchor (see isAnchorOnlyCircle in
    // SpellCircleRenderer.cpp) used only to position points/boxes/edges —
    // nothing to draw for it.
    if (circle.radius <= 0.0f)
      continue;

    const float radius = circle.radius;
    const float centerX = static_cast<float>(circle.center.x());
    const float centerY = static_cast<float>(circle.center.y());

    QCanvasPath circlePath;
    circlePath.circle(centerX, centerY, radius);

    painter->beginPath();
    painter->addPath(circlePath);
    painter->setStrokeStyle(renderer.m_accentColor);
    painter->setLineWidth(strokeWidth);
    painter->stroke(circlePath);
    if (circle.active > 0.0f) {
      painter->setGlobalAlpha(circle.active);
      painter->setFillStyle(renderer.m_accentColor);
      painter->fill(circlePath);
      painter->setGlobalAlpha(1.0f);
    }
    painter->closePath();

    if (!circle.name.isEmpty()) {
      QPainterPath textPath;
      textPath.setCachingEnabled(true);
      textPath.addEllipse(centerX - radius, centerY - radius, radius * 2.0f,
                          radius * 2.0f);

      renderer.m_curvedTextPainter.setPathOffset(circle.textStart);
      renderer.m_curvedTextPainter.setPerpendicularOffset(
          static_cast<float>(renderer.m_labelOffset * renderer.m_scale));
      renderer.m_curvedTextPainter.setFont(scaledFont);
      renderer.m_curvedTextPainter.setColor(renderer.m_accentColor);
      renderer.m_curvedTextPainter.paint(painter, textPath, circle.name);
    }
  }

  // ── Point value labels ──────────────────────────────────────────────────
  // Positioned the same way as a box (anchor pushed outward by boxDistance
  // along the ray from the canvas center), but a point isn't a box: just the
  // text, no rect/stroke/fill around it.
  painter->setTextAlign(QCanvasPainter::TextAlign::Center);
  painter->setTextBaseline(QCanvasPainter::TextBaseline::Middle);
  painter->setFillStyle(renderer.m_accentColor);
  for (const auto &label : renderer.m_pointLabels) {
    const QPointF center = label.anchor + label.direction * pointDistance;
    painter->beginPath();
    painter->fillText(label.value, static_cast<float>(center.x()),
                      static_cast<float>(center.y()));
    painter->closePath();
  }

  // ── Boxes ──────────────────────────────────────────────────────────────────
  painter->setFont(scaledFont);
  painter->setTextAlign(QCanvasPainter::TextAlign::Left);
  painter->setTextBaseline(QCanvasPainter::TextBaseline::Top);
  auto drawBox = [&](const SpellCircleRenderer::ResolvedBox &box) {
    const QRectF bounds = painter->textBoundingBox(box.value, 0.0f, 0.0f);
    const float textWidth = static_cast<float>(bounds.width());
    const float resolvedBoxWidth =
        qMax(textWidth + boxPadding * 2.0f, boxWidth);
    const float resolvedBoxHeight = boxHeight;

    // The box's center sits on the ray from the canvas center through the
    // point, pushed outward by boxDistance beyond the point — nothing more.
    // An earlier version instead aligned whichever box edge the ray leans on
    // more (picking a horizontal or vertical edge, or interpolating between
    // them via ray/rectangle intersection near the corner), but that made the
    // box's touching edge slide around its own perimeter as the point moved,
    // producing a visible extra wobble on top of the translation whenever the
    // ray swept past a corner. Anchoring just the center avoids that: the box
    // rigidly translates along the ray with no edge-dependent correction.
    const QPointF boxCenter = box.anchor + box.direction * boxDistance;
    const float halfWidth = resolvedBoxWidth / 2.0f;
    const float halfHeight = resolvedBoxHeight / 2.0f;
    const float boxX = static_cast<float>(boxCenter.x()) - halfWidth;
    const float boxY = static_cast<float>(boxCenter.y()) - halfHeight;

    QCanvasPath rectanglePath;
    QRectF boxBounds(boxX, boxY, resolvedBoxWidth, resolvedBoxHeight);
    rectanglePath.rect(boxBounds);

    painter->clearRect(boxBounds);
    painter->beginPath();
    painter->addPath(rectanglePath);
    if (box.active > 0.0f) {
      painter->setGlobalAlpha(box.active);
      painter->setFillStyle(renderer.m_accentColor);
      painter->fill();
      painter->setGlobalAlpha(1.0f);
    }
    painter->setStrokeStyle(renderer.m_accentColor);
    painter->setLineWidth(strokeWidth);
    painter->stroke();
    painter->closePath();

    painter->beginPath();
    if (box.active > 0.0f) {
      painter->setGlobalCompositeOperation(
          QCanvasPainter::CompositeOperation::DestinationOut);
      painter->setFillStyle(QColorConstants::Black);
      painter->fillText(box.value, boxX + boxPadding, boxY + boxPadding);
      painter->setGlobalCompositeOperation(
          QCanvasPainter::CompositeOperation::SourceOver);
    } else {
      painter->setFillStyle(renderer.m_accentColor);
      painter->fillText(box.value, boxX + boxPadding, boxY + boxPadding);
    }
    painter->closePath();
  };
  for (const auto &box : renderer.m_boxes)
    drawBox(box);

  // The offscreen canvas's blended output is premultiplied-alpha; without
  // this flag drawImage() re-applies alpha on top of already alpha-baked-in
  // color, darkening every translucent fill (e.g. a 0.5 fill over background
  // `bg` rendered as `255*0.5^2 + bg*0.5` instead of `255*0.5 + bg*0.5`).
  QCanvasImage image =
      painter->addImage(canvas, QCanvasPainter::ImageFlag::GenerateMipmaps |
                                    QCanvasPainter::ImageFlag::Premultiplied);
  renderer.endCanvasPainting();
  return image;
}

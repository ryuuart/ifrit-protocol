/** @file
 * The wet pigment wash.
 */

#include "Executors.h"
#include "PolygonMath.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <sigildraw/Pen.h>
#include <sigildraw/brush/Wash.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace sigil::draw::brush {

namespace {

/** How far apart the wet edge is sampled along each side. */
constexpr float kWetEdgeStep = 14.0f;
/** The most one layer may deposit, whatever the load. */
constexpr float kLayerAlphaCeiling = 0.18f;
/** Canvas area per grain of granulation at full texture, and the most
 *  grains one wash lays. */
constexpr float kGrainArea = 52.0f;
constexpr int kGrainCap = 5000;

SkColor4f withAlpha(SkColor4f color, float alpha) {
  color.fA = std::clamp(color.fA * alpha, 0.0f, 1.0f);
  return color;
}

/** The polygon's edge pushed out by @p expansion and rippled by noise. */
SkPath wetPath(Pen& pen, std::span<const SkPoint> polygon, float expansion,
               float roughness, float phase) {
  const SkPoint center = polygonCenter(polygon);
  SkPathBuilder path;
  bool first = true;
  for (size_t edge = 0; edge < polygon.size(); ++edge) {
    const SkPoint from = polygon[edge];
    const SkPoint to = polygon[(edge + 1) % polygon.size()];
    const float dx = to.fX - from.fX;
    const float dy = to.fY - from.fY;
    const float length = std::hypot(dx, dy);
    const int steps = std::max(1, (int)std::ceil(length / kWetEdgeStep));
    for (int step = 0; step < steps; ++step) {
      const float t = (float)step / (float)steps;
      SkPoint point{from.fX + dx * t, from.fY + dy * t};
      float radialX = point.fX - center.fX;
      float radialY = point.fY - center.fY;
      const float radialLength = std::hypot(radialX, radialY);
      if (radialLength > 0.0f) {
        radialX /= radialLength;
        radialY /= radialLength;
      }
      const float ripple =
          (pen.noise(point.fX * 0.014f, point.fY * 0.014f, phase) - 0.5f) *
          roughness;
      point.fX += radialX * (expansion + ripple);
      point.fY += radialY * (expansion + ripple);
      if (first) {
        path.moveTo(point);
        first = false;
      } else {
        path.lineTo(point);
      }
    }
  }
  path.close();
  return path.detach();
}

}  // namespace

void wash(Pen& pen, const Wash& pigment, std::span<const SkPoint> polygon) {
  SkCanvas* canvas = pen.canvas();
  if (!canvas || polygon.size() < 3 || !(pigment.opacity > 0.0f) ||
      pigment.layers <= 0)
    return;
  const SkRect bounds = polygonBounds(polygon);
  const SkPoint center = polygonCenter(polygon);
  const float scale = std::max(1.0f, std::min(bounds.width(), bounds.height()));
  const float bleed = std::clamp(pigment.bleed, 0.0f, 1.0f);
  const float texture = std::clamp(pigment.texture, 0.0f, 1.0f);
  const float border = std::clamp(pigment.border, 0.0f, 1.0f);
  const int layers = std::clamp(pigment.layers, 1, 96);
  const float padding = scale * (0.08f + bleed * 0.12f);

  // The whole wash is built in one layer and composited once with the
  // wash's blend, so the layers blend with each other as wet pigment and
  // only the finished wash meets the canvas.
  pen.push();
  pen.blendMode(pigment.blend);
  pen.fill(SkColor4f{1, 1, 1, 1});
  SkPaint composite = *pen.fillPaint();
  composite.setShader(nullptr);
  composite.setColor4f({1, 1, 1, 1}, nullptr);
  const SkRect layerBounds = bounds.makeOutset(padding, padding);
  canvas->saveLayer(&layerBounds, &composite);
  pen.blendMode(BLEND);

  pen.noStroke();
  const float layerAlpha = std::clamp(
      pigment.opacity / std::sqrt((float)layers), 0.002f, kLayerAlphaCeiling);
  for (int layer = 0; layer < layers; ++layer) {
    const float depth = (float)layer / (float)std::max(1, layers - 1);
    const float signedBleed =
        pigment.bleedDirection == BleedDirection::Out ? 1.0f : -1.0f;
    const float expansion =
        pen.randomGaussian(0.0f, scale * (0.004f + bleed * 0.018f)) +
        signedBleed * bleed * scale * 0.012f * (1.0f - depth);
    const float roughness = scale * (0.008f + bleed * 0.045f);
    pen.fill(withAlpha(pigment.color, layerAlpha * pen.random(0.72f, 1.18f)));
    SkPath layerPath =
        wetPath(pen, polygon, expansion, roughness, pen.random(0.0f, 1024.0f));
    if (pigment.bleedAngle) {
      const float travel = bleed * scale * 0.018f * (1.0f - depth);
      layerPath = layerPath.makeTransform(
          SkMatrix::Translate(std::cos(*pigment.bleedAngle) * travel,
                              std::sin(*pigment.bleedAngle) * travel));
    }
    pen.shape(layerPath);
    if (layer % 3 == 0) {
      pen.fill(withAlpha(pigment.color, layerAlpha * 0.42f));
      pen.shape(wetPath(pen, polygon,
                        expansion - scale * pen.random(0.01f, 0.05f),
                        roughness * 1.35f, pen.random(0.0f, 1024.0f)));
    }
  }

  if (texture > 0.0f) {
    // Blooms: pale ellipses lifted out of the body.
    pen.push();
    pen.clip([&] { pen.shape(polygonPath(polygon)); });
    pen.noStroke();
    pen.fill(SkColor4f{1, 1, 1, texture * 15.0f / 255.0f});
    pen.blendMode(REMOVE);
    const int blooms =
        pigment.scatter ? 80 + (int)std::round(texture * 170.0f) : 0;
    const float minimumRadius = scale * 0.025f;
    const float maximumRadius = scale * (0.16f + texture * 0.22f);
    for (int bloom = 0; bloom < blooms; ++bloom) {
      const float x = pen.randomGaussian(center.fX, bounds.width() / 2.8f);
      const float y = pen.randomGaussian(center.fY, bounds.height() / 2.8f);
      const float radius = pen.random(minimumRadius, maximumRadius);
      pen.ellipse(x, y, radius * pen.random(0.65f, 1.5f),
                  radius * pen.random(0.65f, 1.5f));
    }
    pen.pop();

    // Granulation: dots of pigment settled across the body, as one batch.
    pen.push();
    pen.clip([&] { pen.shape(polygonPath(polygon)); });
    const int grains = std::clamp(
        (int)(bounds.width() * bounds.height() * texture / kGrainArea), 0,
        kGrainCap);
    const SkColor grainColor =
        withAlpha(pigment.color, pigment.opacity * 0.08f).toSkColor();
    std::vector<Stamp> grainStamps;
    grainStamps.reserve((size_t)grains);
    for (int grain = 0; grain < grains; ++grain) {
      const float diameter = pen.random(0.25f, 1.4f);
      grainStamps.push_back({{pen.random(bounds.left(), bounds.right()),
                              pen.random(bounds.top(), bounds.bottom())},
                             diameter,
                             diameter,
                             0.0f,
                             grainColor});
    }
    drawStamps(pen, BLEND, grainStamps);
    pen.pop();
  }

  if (border > 0.0f) {
    pen.noFill();
    for (int edge = 0; edge < 3; ++edge) {
      pen.stroke(withAlpha(pigment.color,
                           pigment.opacity * border * pen.random(0.12f, 0.24f)));
      pen.strokeWeight(scale * pen.random(0.002f, 0.008f));
      pen.shape(wetPath(pen, polygon, pen.random(-1.0f, 1.5f), scale * 0.025f,
                        pen.random(0.0f, 1024.0f)));
    }
  }
  canvas->restore();
  pen.pop();
}

}  // namespace sigil::draw::brush

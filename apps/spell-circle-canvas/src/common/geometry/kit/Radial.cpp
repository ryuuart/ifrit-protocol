/** @file
 * The general shapes' bodies: the radial arrangement's vertices in a box,
 * the ellipse and every cut of it, and an outline fitted to a box.
 */

#include "sigilgeometry/kit/Radial.h"

#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>

#include "sigilgeometry/path/Numeric.h"
#include "sigilgeometry/advanced/Skia.h"

namespace sigil::geometry::shapes {

std::vector<glm::vec2> Radial::points(glm::vec2 size) const {
  return path::radialPoints(count, options, size);
}

namespace {

/** The box an ellipse is inscribed in, after `uniform` and `inset`. */
SkRect boxOf(const EllipseOptions& options, glm::vec2 size) {
  SkRect box = SkRect::MakeWH(size.x, size.y);
  if (options.uniform) {
    const float side = std::min(size.x, size.y);
    box = SkRect::MakeXYWH((size.x - side) * 0.5f, (size.y - side) * 0.5f,
                           side, side);
  }
  box.inset(options.inset, options.inset);
  return box;
}

SkPath superellipse(const SkRect& box, float exponent) {
  const float e = std::max(exponent, 0.5f);
  const float cx = box.centerX(), cy = box.centerY();
  const float rx = box.width() * 0.5f, ry = box.height() * 0.5f;
  constexpr int kSegments = 96;
  SkPathBuilder b;
  for (int i = 0; i < kSegments; ++i) {
    const float t = (float)i * (path::kTau / kSegments);
    const float c = std::cos(t), s = std::sin(t);
    const float x = std::copysign(std::pow(std::abs(c), 2.0f / e), c);
    const float y = std::copysign(std::pow(std::abs(s), 2.0f / e), s);
    const SkPoint p{cx + rx * x, cy + ry * y};
    if (i == 0)
      b.moveTo(p);
    else
      b.lineTo(p);
  }
  b.close();
  return b.detach();
}

SkPath ringOf(const SkRect& outer, const EllipseOptions& options) {
  SkRect inner = outer;
  if (options.thickness > 0) {
    inner.inset(options.thickness, options.thickness);
  } else {
    const float r = std::clamp(options.inner, 0.0f, 0.999f);
    inner.inset(outer.width() * 0.5f * (1 - r), outer.height() * 0.5f * (1 - r));
  }
  SkPathBuilder b;
  b.setFillType(SkPathFillType::kEvenOdd);
  b.addOval(outer);
  // A hole with nothing left of it is no hole: an empty oval adds no
  // contour, and what stands is the disc.
  if ((options.inner > 0 || options.thickness > 0) && !inner.isEmpty())
    b.addOval(inner);
  if (options.dot > 0)
    b.addCircle(outer.centerX(), outer.centerY(), options.dot);
  return b.detach();
}

SkPath slice(const SkRect& outerBox, const EllipseOptions& options) {
  const float cx = outerBox.centerX(), cy = outerBox.centerY();
  // An arc swallows a full turn, so an unclamped sweep of 360 with a hole
  // — a gauge's annular TRACK — would draw nothing at all.
  const float sweep = std::clamp(options.sweepDegrees, -359.99f, 359.99f);
  const float inner = std::clamp(options.inner, 0.0f, 0.999f);
  SkPathBuilder b;
  if (inner <= 0.0f) {
    if (options.close == Close::Pie) b.moveTo(cx, cy);
    b.arcTo(outerBox, options.fromDegrees, sweep,
            options.close != Close::Pie);
    b.close();
    return b.detach();
  }
  const SkRect innerBox = SkRect::MakeXYWH(
      cx - outerBox.width() * 0.5f * inner, cy - outerBox.height() * 0.5f * inner,
      outerBox.width() * inner, outerBox.height() * inner);
  b.arcTo(outerBox, options.fromDegrees, sweep, true);
  b.arcTo(innerBox, options.fromDegrees + sweep, -sweep, false);
  b.close();
  return b.detach();
}

}  // namespace

path::Outline Ellipse::outline(glm::vec2 size) const {
  const SkRect box = boxOf(options, size);
  if (options.exponent != 2.0f)
    return path::fromSk(superellipse(box, options.exponent));
  if (options.close == Close::Open) {
    SkPathBuilder b;
    b.addArc(box, options.fromDegrees, std::min(options.sweepDegrees, 359.9f));
    return path::fromSk(b.detach());
  }
  if (std::abs(options.sweepDegrees) < 360.0f)
    return path::fromSk(slice(box, options));
  if (options.inner > 0 || options.thickness > 0 || options.dot > 0)
    return path::fromSk(ringOf(box, options));
  SkPathBuilder b;
  b.addOval(box, path::toSk(options.winding), options.start);
  return path::fromSk(b.detach());
}

path::Outline Fitted::outline(glm::vec2 size) const {
  const SkPath drawn = path::toSk(source);
  // The control-point bounds, which are what a vector tool's artboard
  // fits: a curve's handles count, so the figure lands where it was
  // traced.
  const SkRect from = drawn.getBounds();
  if (from.isEmpty() || size.x <= 0 || size.y <= 0) return source;
  const SkRect box = SkRect::MakeWH(size.x, size.y);
  const SkMatrix map =
      options.preserveAspect
          ? SkMatrix::RectToRect(from, box, SkMatrix::kCenter_ScaleToFit)
          : SkMatrix::RectToRect(from, box);
  return path::fromSk(drawn.makeTransform(map));
}

}  // namespace sigil::geometry::shapes

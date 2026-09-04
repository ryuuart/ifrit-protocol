/** @file
 * Natural-media interiors for polygonal shapes.
 */

#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRect.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/kit/Shape.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace sigil::draw::brush {

namespace {

SkPath polygonPath(std::span<const SkPoint> polygon) {
  SkPathBuilder path;
  if (polygon.empty()) return path.detach();
  path.moveTo(polygon.front());
  for (size_t i = 1; i < polygon.size(); ++i) path.lineTo(polygon[i]);
  path.close();
  return path.detach();
}

SkRect polygonBounds(std::span<const SkPoint> polygon) {
  float left = polygon.front().fX;
  float top = polygon.front().fY;
  float right = left;
  float bottom = top;
  for (SkPoint point : polygon) {
    left = std::min(left, point.fX);
    top = std::min(top, point.fY);
    right = std::max(right, point.fX);
    bottom = std::max(bottom, point.fY);
  }
  return SkRect::MakeLTRB(left, top, right, bottom);
}

SkPoint polygonCenter(std::span<const SkPoint> polygon) {
  SkPoint center{0, 0};
  for (SkPoint point : polygon) {
    center.fX += point.fX;
    center.fY += point.fY;
  }
  const float count = (float)polygon.size();
  return {center.fX / count, center.fY / count};
}

SkColor4f withAlpha(SkColor4f color, float alpha) {
  color.fA = std::clamp(color.fA * alpha, 0.0f, 1.0f);
  return color;
}

struct HatchSegment {
  SkPoint from;
  SkPoint to;
  bool connector = false;
};

std::vector<HatchSegment> hatchLines(
    Pen& pen, std::span<const std::span<const SkPoint>> contours,
    const Hatch& style) {
  struct Edge {
    float x1;
    float y1;
    float x2;
    float y2;
  };

  const float cosine = std::cos(style.angle);
  const float sine = std::sin(style.angle);
  float minimumY = std::numeric_limits<float>::infinity();
  float maximumY = -std::numeric_limits<float>::infinity();
  std::vector<Edge> edges;
  for (const std::span<const SkPoint> contour : contours) {
    if (contour.size() < 3) continue;
    std::vector<SkPoint> rotated;
    rotated.reserve(contour.size());
    for (const SkPoint point : contour) {
      const SkPoint transformed{point.fX * cosine - point.fY * sine,
                                point.fX * sine + point.fY * cosine};
      rotated.push_back(transformed);
      minimumY = std::min(minimumY, transformed.fY);
      maximumY = std::max(maximumY, transformed.fY);
    }
    for (size_t index = 0; index < rotated.size(); ++index) {
      const SkPoint from = rotated[index];
      const SkPoint to = rotated[(index + 1) % rotated.size()];
      if (from.fY != to.fY) edges.push_back({from.fX, from.fY, to.fX, to.fY});
    }
  }
  if (edges.empty() || !std::isfinite(minimumY) || !std::isfinite(maximumY))
    return {};

  std::vector<HatchSegment> segments;
  std::vector<float> crossings;
  float scanY = minimumY + style.spacing * 0.5f;
  float step = style.spacing;
  const float gradient = std::clamp(style.gradient, -1.0f, 1.0f);
  const float stepScale = gradient >= 0.0f ? 1.0f + gradient * 0.1f
                                           : 1.0f / (1.0f - gradient * 0.1f);
  int lanes = 0;
  while (scanY < maximumY && lanes++ < 10000) {
    crossings.clear();
    for (const Edge& edge : edges) {
      if ((edge.y1 <= scanY) == (edge.y2 <= scanY)) continue;
      crossings.push_back(edge.x1 + (scanY - edge.y1) / (edge.y2 - edge.y1) *
                                        (edge.x2 - edge.x1));
    }
    std::ranges::sort(crossings);
    for (size_t index = 0; index + 1 < crossings.size(); index += 2) {
      const float fromX = crossings[index];
      const float toX = crossings[index + 1];
      segments.push_back({
          {fromX * cosine + scanY * sine, -fromX * sine + scanY * cosine},
          {toX * cosine + scanY * sine, -toX * sine + scanY * cosine},
      });
    }
    scanY += step;
    step = std::max(0.125f, step * stepScale);
  }

  const float jitter = std::max(0.0f, style.jitter) * style.spacing * 2.0f;
  for (HatchSegment& segment : segments) {
    if (jitter > 0.0f) {
      segment.from.fX += pen.random(-jitter, jitter);
      segment.from.fY += pen.random(-jitter, jitter);
      segment.to.fX += pen.random(-jitter, jitter);
      segment.to.fY += pen.random(-jitter, jitter);
    }
  }
  if (!style.continuous) return segments;

  std::vector<HatchSegment> continuous;
  continuous.reserve(segments.size() * 2);
  for (size_t index = 0; index < segments.size(); ++index) {
    HatchSegment segment = segments[index];
    if (index % 2 == 1) std::swap(segment.from, segment.to);
    if (!continuous.empty())
      continuous.push_back({continuous.back().to, segment.from, true});
    continuous.push_back(segment);
  }
  return continuous;
}

SkRect contourBounds(std::span<const std::span<const SkPoint>> contours) {
  float left = std::numeric_limits<float>::infinity();
  float top = std::numeric_limits<float>::infinity();
  float right = -std::numeric_limits<float>::infinity();
  float bottom = -std::numeric_limits<float>::infinity();
  for (const std::span<const SkPoint> contour : contours) {
    for (const SkPoint point : contour) {
      left = std::min(left, point.fX);
      top = std::min(top, point.fY);
      right = std::max(right, point.fX);
      bottom = std::max(bottom, point.fY);
    }
  }
  return SkRect::MakeLTRB(left, top, right, bottom);
}

bool pointInRing(std::span<const SkPoint> ring, SkPoint point) {
  if (ring.size() < 3) return false;
  bool inside = false;
  for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
    const SkPoint a = ring[i];
    const SkPoint b = ring[j];
    if (((a.fY > point.fY) != (b.fY > point.fY)) &&
        point.fX < (b.fX - a.fX) * (point.fY - a.fY) / (b.fY - a.fY) + a.fX)
      inside = !inside;
  }
  return inside;
}

bool pointInContours(std::span<const std::span<const SkPoint>> contours,
                     SkPoint point) {
  bool inside = false;
  for (const std::span<const SkPoint> contour : contours)
    if (contour.size() >= 3 && pointInRing(contour, point)) inside = !inside;
  return inside;
}

float positiveSweep(float start, float stop) {
  float sweep = std::fmod(stop - start, TWO_PI);
  if (sweep < 0.0f) sweep += TWO_PI;
  return sweep;
}

bool arcFits(std::span<const std::span<const SkPoint>> contours, SkPoint center,
             float radius, float start, float stop) {
  const float sweep = positiveSweep(start, stop);
  for (int sample = 1; sample < 8; ++sample) {
    const float angle = start + sweep * (float)sample / 8.0f;
    if (!pointInContours(contours, {center.fX + radius * std::cos(angle),
                                    center.fY - radius * std::sin(angle)}))
      return false;
  }
  return true;
}

std::optional<std::pair<float, float>> massArc(
    std::span<const std::span<const SkPoint>> contours, SkPoint center,
    float radius, SkPoint from, SkPoint to) {
  float start = std::atan2(center.fY - from.fY, from.fX - center.fX);
  float stop = std::atan2(center.fY - to.fY, to.fX - center.fX);
  if (positiveSweep(start, stop) > PI) std::swap(start, stop);
  if (arcFits(contours, center, radius, start, stop))
    return std::pair{start, stop};
  if (arcFits(contours, center, radius, stop, start))
    return std::pair{stop, start};
  return std::nullopt;
}

Stroke arcStroke(Pen& pen, SkPoint center, float radius, float start,
                 float stop, float wiggle) {
  const float sweep = positiveSweep(start, stop);
  const int steps = std::clamp((int)std::ceil(sweep / TWO_PI * 96.0f), 8, 96);
  const float phase = pen.random(0.0f, 2048.0f);
  Stroke result;
  result.reserve((size_t)steps + 1);
  for (int step = 0; step <= steps; ++step) {
    const float progress = (float)step / (float)steps;
    const float angle = start + sweep * progress;
    const float displacement =
        (pen.noise(progress * 4.0f, phase, 0.37f) - 0.5f) * 2.0f * wiggle;
    const float displacedRadius = std::max(0.0f, radius + displacement);
    result.push_back({{center.fX + displacedRadius * std::cos(angle),
                       center.fY - displacedRadius * std::sin(angle)},
                      1.0f});
  }
  return result;
}

std::array<HatchSegment, 2> splitSegment(Pen& pen,
                                         const HatchSegment& segment) {
  const float at = pen.random(0.35f, 0.65f);
  const SkPoint middle{
      segment.from.fX + (segment.to.fX - segment.from.fX) * at,
      segment.from.fY + (segment.to.fY - segment.from.fY) * at};
  const float dx = segment.to.fX - segment.from.fX;
  const float dy = segment.to.fY - segment.from.fY;
  const float length = std::hypot(dx, dy);
  const float gap = pen.random(0.04f, 0.10f) * length * 0.5f;
  const float gx = length > 0.0f ? dx / length * gap : 0.0f;
  const float gy = length > 0.0f ? dy / length * gap : 0.0f;
  return {{{segment.from, {middle.fX - gx, middle.fY - gy}},
           {{middle.fX + gx, middle.fY + gy}, segment.to}}};
}

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
    const int steps = std::max(1, (int)std::ceil(length / 14.0f));
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

void hatch(Pen& pen, const Brush& tool, std::span<const SkPoint> polygon,
           const Hatch& style) {
  const std::array<std::span<const SkPoint>, 1> contours{polygon};
  hatch(pen, tool, contours, style);
}

void hatch(Pen& pen, const Brush& tool,
           std::span<const std::span<const SkPoint>> contours,
           const Hatch& style) {
  if (contours.empty() || !(style.spacing > 0.0f)) return;
  const std::vector<HatchSegment> segments = hatchLines(pen, contours, style);
  Brush mark = tool;
  mark.pressure = {0.16f, 1.0f, 0.16f};
  Stroke continuous;
  for (const HatchSegment& segment : segments) {
    if (style.continuous) {
      if (continuous.empty()) continuous.push_back({segment.from, 1.0f});
      continuous.push_back({segment.to, 1.0f});
    } else {
      line(pen, mark, segment.from, segment.to);
    }
  }
  if (style.continuous && continuous.size() >= 2) paint(pen, mark, continuous);
}

void wash(Pen& pen, const Wash& pigment, std::span<const SkPoint> polygon) {
  if (polygon.size() < 3 || !(pigment.opacity > 0.0f) || pigment.layers <= 0)
    return;
  const SkRect bounds = polygonBounds(polygon);
  const float scale = std::max(1.0f, std::min(bounds.width(), bounds.height()));
  const float bleed = std::clamp(pigment.bleed, 0.0f, 1.0f);
  const float texture = std::clamp(pigment.texture, 0.0f, 1.0f);
  const float border = std::clamp(pigment.border, 0.0f, 1.0f);
  const int layers = std::clamp(pigment.layers, 1, 96);
  const float padding = scale * (0.08f + bleed * 0.12f);
  Graphics buffer(bounds.width() + padding * 2.0f,
                  bounds.height() + padding * 2.0f);
  Pen& wet = buffer.begin(pen);
  wet.clear();
  wet.randomSeed((uint64_t)(pen.random() * 4294967295.0));
  wet.noiseSeed((uint32_t)(pen.random() * 4294967295.0));
  std::vector<SkPoint> local;
  local.reserve(polygon.size());
  for (SkPoint point : polygon)
    local.push_back({point.fX - bounds.left() + padding,
                     point.fY - bounds.top() + padding});
  const SkRect localBounds = polygonBounds(local);
  const SkPoint localCenter = polygonCenter(local);

  wet.noStroke();
  const float layerAlpha =
      std::clamp(pigment.opacity / std::sqrt((float)layers), 0.002f, 0.18f);
  for (int layer = 0; layer < layers; ++layer) {
    const float depth = (float)layer / (float)std::max(1, layers - 1);
    const float signedBleed =
        pigment.bleedDirection == BleedDirection::Out ? 1.0f : -1.0f;
    const float expansion =
        wet.randomGaussian(0.0f, scale * (0.004f + bleed * 0.018f)) +
        signedBleed * bleed * scale * 0.012f * (1.0f - depth);
    const float roughness = scale * (0.008f + bleed * 0.045f);
    wet.fill(withAlpha(pigment.color, layerAlpha * wet.random(0.72f, 1.18f)));
    SkPath layerPath =
        wetPath(wet, local, expansion, roughness, wet.random(0.0f, 1024.0f));
    if (pigment.bleedAngle) {
      const float travel = bleed * scale * 0.018f * (1.0f - depth);
      layerPath = layerPath.makeTransform(
          SkMatrix::Translate(std::cos(*pigment.bleedAngle) * travel,
                              -std::sin(*pigment.bleedAngle) * travel));
    }
    wet.shape(layerPath);
    if (layer % 3 == 0) {
      wet.fill(withAlpha(pigment.color, layerAlpha * 0.42f));
      wet.shape(wetPath(wet, local,
                        expansion - scale * wet.random(0.01f, 0.05f),
                        roughness * 1.35f, wet.random(0.0f, 1024.0f)));
    }
  }

  if (texture > 0.0f) {
    wet.push();
    wet.clip([&] { wet.shape(polygonPath(local)); });
    wet.noStroke();
    wet.fill(255, texture * 15.0f);
    wet.blendMode(REMOVE);
    const int blooms =
        pigment.scatter ? 80 + (int)std::round(texture * 170.0f) : 0;
    const float minimumRadius = scale * 0.025f;
    const float maximumRadius = scale * (0.16f + texture * 0.22f);
    for (int bloom = 0; bloom < blooms; ++bloom) {
      const float x =
          wet.randomGaussian(localCenter.fX, localBounds.width() / 2.8f);
      const float y =
          wet.randomGaussian(localCenter.fY, localBounds.height() / 2.8f);
      const float radius = wet.random(minimumRadius, maximumRadius);
      wet.ellipse(x, y, radius * wet.random(0.65f, 1.5f),
                  radius * wet.random(0.65f, 1.5f));
    }
    wet.pop();

    wet.push();
    wet.clip([&] { wet.shape(polygonPath(local)); });
    wet.stroke(withAlpha(pigment.color, pigment.opacity * 0.08f));
    const int grains = std::clamp(
        (int)(localBounds.width() * localBounds.height() * texture / 52.0f), 0,
        5000);
    for (int grain = 0; grain < grains; ++grain) {
      wet.strokeWeight(wet.random(0.25f, 1.4f));
      wet.point(wet.random(localBounds.left(), localBounds.right()),
                wet.random(localBounds.top(), localBounds.bottom()));
    }
    wet.pop();
  }

  if (border > 0.0f) {
    wet.noFill();
    for (int edge = 0; edge < 3; ++edge) {
      wet.stroke(withAlpha(
          pigment.color, pigment.opacity * border * wet.random(0.12f, 0.24f)));
      wet.strokeWeight(scale * wet.random(0.002f, 0.008f));
      wet.shape(wetPath(wet, local, wet.random(-1.0f, 1.5f), scale * 0.025f,
                        wet.random(0.0f, 1024.0f)));
    }
  }
  buffer.end();

  pen.push();
  pen.blendMode(pigment.blend);
  pen.image(buffer, bounds.left() - padding, bounds.top() - padding);
  pen.pop();
}

void mass(Pen& pen, const Brush& tool, std::span<const SkPoint> polygon,
          const Mass& style) {
  const std::array<std::span<const SkPoint>, 1> contours{polygon};
  mass(pen, tool, contours, style);
}

void mass(Pen& pen, const Brush& tool,
          std::span<const std::span<const SkPoint>> contours,
          const Mass& style) {
  if (contours.empty() || !(tool.width > 0.0f)) return;
  for (const std::span<const SkPoint> contour : contours)
    if (contour.size() < 3) return;

  const float precision = std::clamp(style.precision, 0.0f, 1.0f);
  const float strength = std::clamp(style.strength, 0.0f, 1.0f);
  const float gradient = std::clamp(style.gradient, -1.0f, 1.0f);
  const int passes =
      1 + (strength > 0.33f ? 1 : 0) + (strength > 0.66f ? 1 : 0);
  const float hatchSpacing = std::max(
      0.125f, 1.6f * pen.random(tool.scatter * 0.65f, tool.scatter * 0.85f) -
                  0.4f * gradient);
  const float baseAngle = pen.random(-HALF_PI, HALF_PI);
  const bool positive = baseAngle >= 0.0f;
  const bool firstBias = pen.random() < 0.5f;
  const SkPoint pivotBias = positive
                                ? (firstBias ? SkPoint{1, 1} : SkPoint{-1, -1})
                                : (firstBias ? SkPoint{-1, 1} : SkPoint{1, -1});

  if (style.outline) {
    Brush edge = tool;
    for (const std::span<const SkPoint> contour : contours)
      for (size_t index = 0; index < contour.size(); ++index)
        line(pen, edge, contour[index], contour[(index + 1) % contour.size()]);
  }

  for (int pass = 0; pass < passes; ++pass) {
    const float maximumJitter = std::min(tool.scatter * 2.0f, 5.0f);
    const SkPoint translation =
        pass == 0 ? SkPoint{0, 0}
                  : SkPoint{pen.random(-maximumJitter, maximumJitter),
                            pen.random(-maximumJitter, maximumJitter)};
    std::vector<std::vector<SkPoint>> storage;
    std::vector<std::span<const SkPoint>> layer;
    storage.reserve(contours.size());
    layer.reserve(contours.size());
    for (const std::span<const SkPoint> contour : contours) {
      std::vector<SkPoint>& moved = storage.emplace_back();
      moved.reserve(contour.size());
      for (const SkPoint point : contour)
        moved.push_back({point.fX + translation.fX, point.fY + translation.fY});
    }
    for (const std::vector<SkPoint>& contour : storage)
      layer.push_back(contour);

    const float angleJitter = pass == 1 ? 20.0f : 15.0f;
    const float angle =
        baseAngle + (pass == 0
                         ? 0.0f
                         : pen.random(-angleJitter, angleJitter) * PI / 180.0f);
    const float spacingScale = pass == 0 ? 0.9f : (pass == 1 ? 1.0f : 0.8f);
    const float jitter =
        pass == 0 ? 2.0f - 2.0f * precision : 0.6f - 0.6f * precision;
    const Hatch hatchStyle{.spacing = hatchSpacing * spacingScale,
                           .angle = angle,
                           .jitter = jitter,
                           .gradient = gradient,
                           .continuous = layer.size() == 1};
    const std::vector<HatchSegment> segments =
        hatchLines(pen, layer, hatchStyle);
    const SkRect bounds = contourBounds(layer);
    const float size = std::hypot(bounds.width(), bounds.height());
    const float anchorDistance = size * pen.random(0.6f, 1.4f);
    const SkPoint anchor{bounds.centerX() + pivotBias.fX * anchorDistance,
                         bounds.centerY() + pivotBias.fY * anchorDistance};

    for (const HatchSegment& segment : segments) {
      const bool shouldSplit = !segment.connector && pen.random() < 0.35f;
      const std::array<HatchSegment, 2> split =
          shouldSplit ? splitSegment(pen, segment)
                      : std::array<HatchSegment, 2>{segment, segment};
      const int partCount = shouldSplit ? 2 : 1;
      for (int partIndex = 0; partIndex < partCount; ++partIndex) {
        const HatchSegment& part = partCount == 1 ? segment : split[partIndex];
        const float dx = part.to.fX - part.from.fX;
        const float dy = part.to.fY - part.from.fY;
        const float length = std::hypot(dx, dy);
        if (!(length > 0.0f)) continue;
        const SkPoint middle{(part.from.fX + part.to.fX) * 0.5f,
                             (part.from.fY + part.to.fY) * 0.5f};
        const SkPoint normal{-dy / length, dx / length};
        const float projected = (anchor.fX - middle.fX) * normal.fX +
                                (anchor.fY - middle.fY) * normal.fY;
        SkPoint center{middle.fX + normal.fX * projected,
                       middle.fY + normal.fY * projected};
        if (layer.size() > 1) {
          const float shortness =
              1.0f - std::min(1.0f, length / std::max(size * 0.42f, 1.0f));
          const float bias = 0.08f + shortness * 0.18f;
          center = {center.fX + (middle.fX - center.fX) * bias,
                    center.fY + (middle.fY - center.fY) * bias};
        }
        const float radius =
            std::hypot(center.fX - part.from.fX, center.fY - part.from.fY);
        if (!(radius > 0.0f)) continue;
        const std::optional<std::pair<float, float>> angles =
            massArc(layer, center, radius, part.from, part.to);
        if (!angles) continue;
        paint(pen, tool,
              arcStroke(pen, center, radius, angles->first, angles->second,
                        2.0f - precision));
      }
    }
  }
}

}  // namespace sigil::draw::brush

/** @file
 * The dab style, and the round tips that go down as stamps.
 */

#include "DabStyle.h"
#include "Executors.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSamplingOptions.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>
#include <sigilskia/draw/Direct.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <source_location>

namespace sigil::draw::brush {

namespace {

float dabAngle(Pen& pen, const Tool& tool, const Dab& dab) {
  const float jitter =
      tool.shape ? pen.random(-std::max(0.0f, tool.shape->angleJitter),
                              std::max(0.0f, tool.shape->angleJitter))
                 : 0.0f;
  switch (tool.rotation) {
    case Rotation::Fixed:
      return tool.angle + dab.barrelRotation + jitter;
    case Rotation::Natural:
      return tool.angle + dab.direction + dab.barrelRotation + jitter;
    case Rotation::Random:
      return tool.angle + pen.random(TWO_PI);
    case Rotation::Tilt:
      return tool.angle + dab.tiltDirection + jitter;
  }
  return tool.angle + jitter;
}

std::optional<SkBlendMode> atlasBlend(Constant mode) {
  switch (mode) {
    case ADD:
      return SkBlendMode::kPlus;
    case DARKEST:
      return SkBlendMode::kDarken;
    case LIGHTEST:
      return SkBlendMode::kLighten;
    case DIFFERENCE:
      return SkBlendMode::kDifference;
    case EXCLUSION:
      return SkBlendMode::kExclusion;
    case MULTIPLY:
      return SkBlendMode::kMultiply;
    case SCREEN:
      return SkBlendMode::kScreen;
    case REPLACE:
      return SkBlendMode::kSrc;
    case REMOVE:
      return SkBlendMode::kDstOut;
    case OVERLAY:
      return SkBlendMode::kOverlay;
    case HARD_LIGHT:
      return SkBlendMode::kHardLight;
    case SOFT_LIGHT:
      return SkBlendMode::kSoftLight;
    case DODGE:
      return SkBlendMode::kColorDodge;
    case BURN:
      return SkBlendMode::kColorBurn;
    case SUBTRACT:
      return std::nullopt;
    default:
      return SkBlendMode::kSrcOver;
  }
}

/** The one sprite every round stamp samples: a white disc. */
constexpr int kTipPixels = 32;

sk_sp<SkImage> roundTip() {
  static const sk_sp<SkImage> tip = [] {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(kTipPixels, kTipPixels, true);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorWHITE);
    canvas.drawCircle(kTipPixels * 0.5f, kTipPixels * 0.5f,
                      kTipPixels * 0.5f - 0.5f, paint);
    bitmap.setImmutable();
    return SkImages::RasterFromBitmap(bitmap);
  }();
  return tip;
}

/** The disc promoted to a texture for the pen's canvas, kept by the pen
 *  for as long as the pen lives: a promoted image belongs to one
 *  recorder, so the cache is per pen rather than per process. */
sigil::skia::draw::Promoted& promotedTip(Pen& pen) {
  static const Slot slot = Slot::at(std::source_location::current());
  return pen.retained().get<sigil::skia::draw::Promoted>(
      slot, [] { return std::make_shared<sigil::skia::draw::Promoted>(); });
}

void drawStampsDirect(Pen& pen, std::span<const Stamp> stamps) {
  SkCanvas* canvas = pen.canvas();
  if (!canvas) return;
  pen.noStroke();
  pen.ellipseMode(CENTER);
  for (const Stamp& stamp : stamps) {
    pen.fill(SkColor4f::FromColor(stamp.color));
    SkAutoCanvasRestore restore(canvas, true);
    canvas->translate(stamp.position.fX, stamp.position.fY);
    canvas->rotate(degrees(stamp.angle));
    pen.ellipse(0, 0, stamp.width, stamp.height);
  }
}

}  // namespace

DabStyle styleDab(Pen& pen, const Tool& tool, const Dab& dab,
                  bool scatterPosition) {
  const float pressure = pressureAt(tool, dab);
  const float normalizedSpeed =
      dab.speed / (std::max(1.0f, tool.speedReference) + dab.speed);
  const float pressureSize =
      1.0f + std::clamp(tool.pressureSize, -1.0f, 1.0f) * (pressure - 1.0f);
  float size =
      tool.width * std::max(0.0f, pressureSize) *
      (1.0f - std::clamp(tool.speedSize, 0.0f, 1.0f) * normalizedSpeed);
  size *= std::max(0.0f, 1.0f + tool.tiltSize * dab.tilt);
  size *= 1.0f + pen.random(-std::max(0.0f, tool.sizeJitter),
                            std::max(0.0f, tool.sizeJitter));
  if (tool.dynamics.size)
    size *= tool.dynamics.size->at(dab, pressure, tool.speedReference);
  float opacity =
      1.0f - std::clamp(tool.speedOpacity, 0.0f, 1.0f) * normalizedSpeed;
  opacity *=
      std::max(0.0f, 1.0f + std::clamp(tool.pressureOpacity, -1.0f, 1.0f) *
                                (pressure - 1.0f));
  opacity *= std::max(0.0f, 1.0f + tool.tiltOpacity * dab.tilt);
  opacity *= 1.0f + pen.random(-std::max(0.0f, tool.opacityJitter),
                               std::max(0.0f, tool.opacityJitter));
  // Opacity is the tool's load and flow is the one dab's; with no
  // buffer between a dab and the canvas the two multiply into the same
  // alpha, and they are separate curves so one may follow the stylus
  // while the other follows the hand.
  if (tool.dynamics.opacity)
    opacity *= tool.dynamics.opacity->at(dab, pressure, tool.speedReference);
  if (tool.dynamics.flow)
    opacity *= tool.dynamics.flow->at(dab, pressure, tool.speedReference);
  const float across =
      scatterPosition ? pen.random(-tool.scatter, tool.scatter) : 0.0f;
  const float along = pen.random(-std::max(0.0f, tool.spacingJitter),
                                 std::max(0.0f, tool.spacingJitter)) *
                      spacingOf(tool);
  const float normal = dab.direction + HALF_PI;
  const float tiltTravel = tool.tiltOffset * dab.tilt * tool.width;
  // A shape states its scatter against the stamp and throws it in both
  // axes, the way a stamped brush wanders off its own path; the tool's
  // own scatter above is canvas units across the centreline.
  const float spread =
      tool.shape ? std::max(0.0f, tool.shape->scatter) * tool.width : 0.0f;
  const float strayX = spread > 0.0f ? pen.random(-spread, spread) : 0.0f;
  const float strayY = spread > 0.0f ? pen.random(-spread, spread) : 0.0f;
  return {{dab.position.fX + std::cos(normal) * across +
               std::cos(dab.direction) * along +
               std::cos(dab.tiltDirection) * tiltTravel + strayX,
           dab.position.fY + std::sin(normal) * across +
               std::sin(dab.direction) * along +
               std::sin(dab.tiltDirection) * tiltTravel + strayY},
          std::max(0.0f, size),
          std::clamp(opacity, 0.0f, 1.0f),
          dabAngle(pen, tool, dab),
          std::max(0.01f, tool.aspect * (1.0f + tool.tiltAspect * dab.tilt))};
}

void depositNib(const Tool& tool, const DabStyle& style,
                std::vector<Stamp>& stamps) {
  stamps.push_back({style.position, style.size, style.size * style.aspect,
                    style.angle, pigment(tool, style.opacity).toSkColor()});
}

void depositDust(Pen& pen, const Tool& tool, const Dab& dab,
                  const DabStyle& style, std::vector<Stamp>& stamps) {
  const float pressure = std::max(0.1f, pressureAt(tool, dab));
  if (pen.random() >=
      std::clamp(tool.density * std::min(1.0f, pressure), 0.0f, 1.0f))
    return;

  const float sharpness = std::clamp(tool.sharpness, 0.0f, 1.0f);
  const float distribution =
      sharpness +
      (1.0f - sharpness) * std::abs(pen.randomGaussian(0.0f, 1.0f)) / pressure;
  const float perpendicular =
      tool.scatter * distribution * pen.random(-1.0f, 1.0f);
  const float longitudinal =
      tool.scatter * distribution * 0.3f * pen.random(-1.0f, 1.0f);
  const float normal = dab.direction + HALF_PI;

  const float diameter =
      std::max(0.15f, style.size * pressure * pen.random(0.85f, 1.15f));
  stamps.push_back({{style.position.fX + std::cos(normal) * perpendicular +
                         std::cos(dab.direction) * longitudinal,
                     style.position.fY + std::sin(normal) * perpendicular +
                         std::sin(dab.direction) * longitudinal},
                    diameter,
                    diameter,
                    0.0f,
                    pigment(tool, style.opacity * std::max(0.8f, pressure) *
                                      pen.random(0.75f, 1.1f))
                        .toSkColor()});
}

void depositScatter(Pen& pen, const Tool& tool, const Dab& dab,
                    const DabStyle& style, std::vector<Stamp>& stamps) {
  const float pressure = std::max(0.1f, pressureAt(tool, dab));
  const int particles = std::max(
      1, (int)std::ceil((float)std::max(1, tool.bristles) *
                        std::clamp(tool.density, 0.0f, 1.0f) / pressure));
  for (int i = 0; i < particles; ++i) {
    const float radius = std::sqrt(pen.random()) * std::max(0.0f, tool.scatter);
    const float angle = pen.random(TWO_PI);
    const SkColor color =
        pigment(tool, style.opacity * pen.random(0.45f, 1.0f)).toSkColor();
    const float diameter =
        std::max(0.18f, style.size * pen.random(0.72f, 1.18f));
    stamps.push_back({{style.position.fX + std::cos(angle) * radius,
                       style.position.fY + std::sin(angle) * radius},
                      diameter,
                      diameter,
                      0.0f,
                      color});
  }
}

void drawStamps(Pen& pen, Constant blend, std::span<const Stamp> stamps) {
  SkCanvas* canvas = pen.canvas();
  const std::optional<SkBlendMode> mode = atlasBlend(blend);
  if (!canvas || !mode) {
    drawStampsDirect(pen, stamps);
    return;
  }

  constexpr float kTipSize = (float)kTipPixels;
  constexpr float kTipCenter = kTipSize * 0.5f;
  std::vector<SkRSXform> transforms;
  std::vector<SkRect> textureRects;
  std::vector<SkColor> colors;
  std::vector<SkSize> sizes;
  transforms.reserve(stamps.size());
  textureRects.reserve(stamps.size());
  colors.reserve(stamps.size());
  sizes.reserve(stamps.size());
  for (const Stamp& stamp : stamps) {
    if (!(stamp.width > 0.0f) || !(stamp.height > 0.0f)) continue;
    transforms.push_back(SkRSXform::MakeFromRadians(
        stamp.width / kTipSize, stamp.angle, stamp.position.fX,
        stamp.position.fY, kTipCenter, kTipCenter));
    textureRects.push_back(SkRect::MakeWH(kTipSize, kTipSize));
    colors.push_back(stamp.color);
    sizes.push_back({1.0f, stamp.height / stamp.width});
  }
  if (transforms.empty()) return;
  sigil::skia::draw::drawSpriteAtlas(
      *canvas, promotedTip(pen), roundTip(), transforms.data(),
      textureRects.data(), colors.data(), transforms.size(),
      SkSamplingOptions(SkFilterMode::kLinear), *mode, sizes.data());
}

}  // namespace sigil::draw::brush

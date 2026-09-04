/** @file
 * Procedural brush deposition through a SigilDraw pen.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkImage.h>
#include <include/core/SkSamplingOptions.h>
#include <sigildraw/kit/Brush.h>
#include <sigilskia/draw/Direct.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

namespace sigil::draw::brush {

namespace {

float distance(SkPoint a, SkPoint b) {
  return std::hypot(b.fX - a.fX, b.fY - a.fY);
}

float pressureAt(const Brush& tool, const Sample& sample, float progress) {
  return std::max(0.0f, sample.pressure * tool.pressure.at(progress));
}

SkColor4f pigment(const Brush& tool, float alpha) {
  SkColor4f color = tool.color;
  color.fA = std::clamp(color.fA * tool.opacity * alpha, 0.0f, 1.0f);
  return color;
}

float dabAngle(Pen& pen, const Brush& tool, const Dab& dab) {
  switch (tool.rotation) {
    case Rotation::Fixed:
      return tool.angle + dab.barrelRotation;
    case Rotation::Natural:
      return tool.angle + dab.direction + dab.barrelRotation;
    case Rotation::Random:
      return tool.angle + pen.random(TWO_PI);
    case Rotation::Tilt:
      return tool.angle + dab.tiltDirection;
  }
  return tool.angle;
}

struct DabStyle {
  SkPoint position;
  float size;
  float opacity;
  float angle;
  float aspect;
};

struct Stamp {
  SkPoint position;
  float width;
  float height;
  float angle;
  SkColor color;
};

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

sk_sp<SkImage> roundTip() {
  static const sk_sp<SkImage> tip = [] {
    constexpr int kSize = 32;
    SkBitmap bitmap;
    bitmap.allocN32Pixels(kSize, kSize, true);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorWHITE);
    canvas.drawCircle(kSize * 0.5f, kSize * 0.5f, kSize * 0.5f - 0.5f, paint);
    bitmap.setImmutable();
    return SkImages::RasterFromBitmap(bitmap);
  }();
  return tip;
}

void drawStamp(Pen& pen, const Stamp& stamp) {
  pen.noStroke();
  pen.fill(SkColor4f::FromColor(stamp.color));
  pen.push();
  pen.translate(stamp.position.fX, stamp.position.fY);
  if (SkCanvas* canvas = pen.canvas())
    canvas->rotate(stamp.angle * 180.0f / std::numbers::pi_v<float>);
  pen.ellipseMode(CENTER);
  pen.ellipse(0, 0, stamp.width, stamp.height);
  pen.pop();
}

void drawStamps(Pen& pen, Constant blend, std::span<const Stamp> stamps) {
  SkCanvas* canvas = pen.canvas();
  const std::optional<SkBlendMode> mode = atlasBlend(blend);
  if (!canvas || !mode) {
    for (const Stamp& stamp : stamps) drawStamp(pen, stamp);
    return;
  }

  constexpr float kTipSize = 32.0f;
  constexpr float kTipCenter = kTipSize * 0.5f;
  static thread_local std::vector<SkRSXform> transforms;
  static thread_local std::vector<SkRect> textureRects;
  static thread_local std::vector<SkColor> colors;
  static thread_local std::vector<SkSize> sizes;
  transforms.clear();
  textureRects.clear();
  colors.clear();
  sizes.clear();
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
  static thread_local sigil::skia::draw::Promoted promoted;
  sigil::skia::draw::drawSpriteAtlas(
      *canvas, promoted, roundTip(), transforms.data(), textureRects.data(),
      colors.data(), transforms.size(),
      SkSamplingOptions(SkFilterMode::kLinear), *mode, sizes.data());
}

DabStyle styleDab(Pen& pen, const Brush& tool, const Dab& dab,
                  bool scatterPosition = true) {
  const float pressure = pressureAt(
      tool, {.position = dab.position, .pressure = dab.pressure}, dab.progress);
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
  float opacity =
      1.0f - std::clamp(tool.speedOpacity, 0.0f, 1.0f) * normalizedSpeed;
  opacity *=
      std::max(0.0f, 1.0f + std::clamp(tool.pressureOpacity, -1.0f, 1.0f) *
                                (pressure - 1.0f));
  opacity *= std::max(0.0f, 1.0f + tool.tiltOpacity * dab.tilt);
  opacity *= 1.0f + pen.random(-std::max(0.0f, tool.opacityJitter),
                               std::max(0.0f, tool.opacityJitter));
  const float across =
      scatterPosition ? pen.random(-tool.scatter, tool.scatter) : 0.0f;
  const float along = pen.random(-std::max(0.0f, tool.spacingJitter),
                                 std::max(0.0f, tool.spacingJitter)) *
                      tool.spacing;
  const float normal = dab.direction + HALF_PI;
  const float tiltTravel = tool.tiltOffset * dab.tilt * tool.width;
  return {{dab.position.fX + std::cos(normal) * across +
               std::cos(dab.direction) * along +
               std::cos(dab.tiltDirection) * tiltTravel,
           dab.position.fY + std::sin(normal) * across +
               std::sin(dab.direction) * along +
               std::sin(dab.tiltDirection) * tiltTravel},
          std::max(0.0f, size),
          std::clamp(opacity, 0.0f, 1.0f),
          dabAngle(pen, tool, dab),
          std::max(0.01f, tool.aspect * (1.0f + tool.tiltAspect * dab.tilt))};
}

void depositNib(const Brush& tool, const DabStyle& style,
                std::vector<Stamp>& stamps) {
  stamps.push_back({style.position, style.size, style.size * style.aspect,
                    style.angle, pigment(tool, style.opacity).toSkColor()});
}

void depositGrain(Pen& pen, const Brush& tool, const Dab& dab,
                  const DabStyle& style, std::vector<Stamp>& stamps) {
  const float pressure = std::max(
      0.1f,
      pressureAt(tool, {.position = dab.position, .pressure = dab.pressure},
                 dab.progress));
  if (pen.random() >=
      std::clamp(tool.grain * std::min(1.0f, pressure), 0.0f, 1.0f))
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

void depositFiberDab(Pen& pen, const Brush& tool, const Dab& dab,
                     const DabStyle& style) {
  const int fibers = std::max(1, tool.bristles);
  const float normal = dab.direction + HALF_PI;
  pen.noStroke();
  for (int i = 0; i < fibers; ++i) {
    if (pen.random() > std::clamp(tool.grain, 0.0f, 1.0f)) continue;
    const float lane = (((float)i + 0.5f) / (float)fibers - 0.5f) * style.size;
    const float radius =
        std::max(0.15f, style.size / (float)fibers * pen.random(0.35f, 0.9f));
    pen.fill(pigment(tool, style.opacity * pen.random(0.5f, 1.1f)));
    pen.circle(style.position.fX + std::cos(normal) * lane,
               style.position.fY + std::sin(normal) * lane, radius);
  }
}

struct FiberMark {
  SkPoint position;
  float width;
  float opacity;
};

void drawFiberRun(Pen& pen, const Brush& tool, std::span<const FiberMark> marks,
                  float fiberOpacity) {
  if (marks.empty()) return;
  if (marks.size() == 1) {
    pen.stroke(pigment(tool, marks.front().opacity * fiberOpacity));
    pen.strokeWeight(std::max(0.15f, marks.front().width));
    pen.point(marks.front().position.fX, marks.front().position.fY);
    return;
  }

  constexpr size_t kChunk = 9;
  for (size_t begin = 0; begin + 1 < marks.size();) {
    const size_t end = std::min(marks.size(), begin + kChunk);
    float width = 0.0f;
    float opacity = 0.0f;
    for (size_t index = begin; index < end; ++index) {
      width += marks[index].width;
      opacity += marks[index].opacity;
    }
    const float count = (float)(end - begin);
    pen.stroke(pigment(tool, opacity / count * fiberOpacity));
    pen.strokeWeight(std::max(0.15f, width / count));
    pen.noFill();
    pen.beginShape();
    for (size_t index = begin; index < end; ++index)
      pen.vertex(marks[index].position.fX, marks[index].position.fY);
    pen.endShape();
    if (end == marks.size()) break;
    begin = end - 1;
  }
}

void depositFibers(Pen& pen, const Brush& tool, std::span<const Dab> dabs) {
  if (dabs.empty()) return;
  std::vector<DabStyle> styles;
  styles.reserve(dabs.size());
  for (const Dab& dab : dabs) styles.push_back(styleDab(pen, tool, dab, false));
  if (dabs.size() == 1) {
    depositFiberDab(pen, tool, dabs.front(), styles.front());
    return;
  }

  pen.strokeCap(ROUND);
  pen.strokeJoin(ROUND);
  const int fibers = std::max(1, tool.bristles);
  const float grain = std::clamp(tool.grain, 0.0f, 1.0f);
  for (int fiber = 0; fiber < fibers; ++fiber) {
    if (pen.random() > 0.35f + grain * 0.65f) continue;
    const float lane = ((float)fiber + 0.5f) / (float)fibers - 0.5f +
                       pen.random(-0.48f, 0.48f) / (float)fibers;
    const float edge = std::abs(lane) * 2.0f;
    const float fiberOpacity =
        pen.random(0.52f, 1.12f) * (1.0f - edge * pen.random(0.0f, 0.22f));
    const float drySeed = pen.random(0.0f, 2048.0f);
    std::vector<FiberMark> run;
    const auto flush = [&] {
      if (!run.empty()) drawFiberRun(pen, tool, run, fiberOpacity);
      run.clear();
    };
    for (size_t index = 0; index < dabs.size(); ++index) {
      const Dab& dab = dabs[index];
      const DabStyle& style = styles[index];
      const float dryBand = pen.noise(dab.distance * 0.0065f, drySeed, 0.31f);
      const float dryThreshold = (1.0f - grain) * (0.42f + edge * 0.18f);
      if (!(style.size > 0.0f) || !(style.opacity > 0.0f) ||
          dryBand < dryThreshold || pen.random() < (1.0f - grain) * 0.025f) {
        flush();
        continue;
      }
      const float normal = dab.direction + HALF_PI;
      const float sharpness = std::clamp(tool.sharpness, 0.0f, 1.0f);
      const float distribution =
          sharpness +
          (1.0f - sharpness) * (0.35f + pen.noise(dab.distance * 0.031f,
                                                  (float)fiber * 0.23f, 0.79f) *
                                            1.3f);
      const float drift =
          (pen.noise(dab.distance * 0.018f, (float)fiber * 0.37f, 0.41f) -
           0.5f) *
          2.0f * tool.scatter * distribution * (0.55f + edge * 0.8f);
      const float offset = lane * style.size + drift;
      run.push_back({
          .position = {style.position.fX + std::cos(normal) * offset,
                       style.position.fY + std::sin(normal) * offset},
          .width = style.size / (float)fibers * pen.random(0.58f, 1.18f),
          .opacity = style.opacity,
      });
    }
    flush();
  }
}

void depositScatter(Pen& pen, const Brush& tool, const Dab& dab,
                    const DabStyle& style, std::vector<Stamp>& stamps) {
  const float pressure = std::max(
      0.1f,
      pressureAt(tool, {.position = dab.position, .pressure = dab.pressure},
                 dab.progress));
  const int particles = std::max(
      1, (int)std::ceil((float)std::max(1, tool.bristles) *
                        std::clamp(tool.grain, 0.0f, 1.0f) / pressure));
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

void depositImage(Pen& pen, const Brush& tool, const DabStyle& style) {
  if (!tool.imageTip || !pen.canvas()) return;
  pen.stroke(pigment(tool, style.opacity));
  const SkPaint* stroke = pen.strokePaint();
  if (!stroke) return;
  SkPaint paint = *stroke;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setAlphaf(1.0f);
  sk_sp<SkColorFilter> tint = SkColorFilters::Blend(
      pigment(tool, style.opacity), nullptr, SkBlendMode::kSrcIn);
  if (tool.imageMask == ImageMask::InvertedLuminance) {
    constexpr float invertedLuminance[20] = {
        0, 0, 0, 0, 1, 0,        0,        0,        0, 1,
        0, 0, 0, 0, 1, -0.2126f, -0.7152f, -0.0722f, 1, 0,
    };
    tint = SkColorFilters::Compose(tint,
                                   SkColorFilters::Matrix(invertedLuminance));
  }
  paint.setColorFilter(std::move(tint));
  SkCanvas* canvas = pen.canvas();
  SkAutoCanvasRestore restore(canvas, true);
  canvas->translate(style.position.fX, style.position.fY);
  canvas->rotate(style.angle * 180.0f / std::numbers::pi_v<float>);
  const float halfWidth = style.size * 0.5f;
  const float halfHeight = style.size * style.aspect * 0.5f;
  canvas->drawImageRect(
      tool.imageTip,
      SkRect::MakeLTRB(-halfWidth, -halfHeight, halfWidth, halfHeight),
      SkSamplingOptions(SkFilterMode::kLinear), &paint);
}

void depositCustom(Pen& pen, const Brush& tool, const Dab& dab,
                   const DabStyle& style) {
  if (!tool.customTip) return;
  pen.push();
  pen.fill(pigment(tool, style.opacity));
  pen.stroke(pigment(tool, style.opacity));
  pen.translate(style.position.fX, style.position.fY);
  if (SkCanvas* canvas = pen.canvas()) {
    canvas->rotate(style.angle * 180.0f / std::numbers::pi_v<float>);
    canvas->scale(style.size, style.size * style.aspect);
  }
  tool.customTip(pen, dab);
  pen.pop();
}

}  // namespace

float Pressure::at(float progress) const {
  progress = std::clamp(progress, 0.0f, 1.0f);
  if (curve) return std::max(0.0f, curve(progress));
  if (gaussian) {
    const float halfWidth = std::max(
        0.0001f, gaussian->width * (progress < gaussian->center ? 1.2f : 0.8f));
    const float distance = std::abs((progress - gaussian->center) / halfWidth);
    const float value =
        1.0f / (1.0f + std::pow(distance, 2.0f * gaussian->sharpness));
    return gaussian->minimum + (gaussian->maximum - gaussian->minimum) * value;
  }
  return progress < 0.5f ? start + (middle - start) * progress * 2.0f
                         : middle + (end - middle) * (progress - 0.5f) * 2.0f;
}

Pressure Pressure::gaussianProfile(float centerJitter, float widthJitter,
                                   float minimum, float maximum) {
  Pressure result;
  result.gaussian = Gaussian{.minimum = minimum,
                             .maximum = maximum,
                             .centerJitter = std::max(0.0f, centerJitter),
                             .widthJitter = std::max(0.0f, widthJitter)};
  return result;
}

Brush pencil(SkColor4f color, float width) {
  return Brush{.tip = Tip::Grain,
               .color = color,
               .width = width,
               .spacing = std::max(0.08f, width * 0.32f),
               .opacity = 0.68f,
               .scatter = 0.6f,
               .grain = 0.76f,
               .bristles = 5,
               .pressure = {0.72f, 1.0f, 0.64f},
               .sizeJitter = 0.08f,
               .opacityJitter = 0.12f,
               .spacingJitter = 0.08f,
               .noise = 0.3f};
}

Brush charcoal(SkColor4f color, float width) {
  return Brush{.tip = Tip::Grain,
               .color = color,
               .width = width,
               .spacing = std::max(0.08f, width * 0.09f),
               .opacity = 0.34f,
               .scatter = 5.5f,
               .grain = 1.35f,
               .bristles = 18,
               .pressure = {0.55f, 1.0f, 0.48f},
               .blend = MULTIPLY,
               .sizeJitter = 0.14f,
               .opacityJitter = 0.18f,
               .spacingJitter = 0.16f,
               .noise = 0.45f};
}

Brush marker(SkColor4f color, float width) {
  return Brush{.tip = Tip::Nib,
               .color = color,
               .width = width,
               .spacing = std::max(0.04f, width * 0.02f),
               .opacity = 0.46f,
               .scatter = width * 0.10f,
               .grain = 1.0f,
               .bristles = 2,
               .pressure = {0.72f, 1.05f, 0.54f},
               .sizeJitter = 0.10f,
               .opacityJitter = 0.10f,
               .spacingJitter = 0.12f,
               .noise = 0.3f};
}

Brush watercolor(SkColor4f color, float width) {
  return Brush{.tip = Tip::Fibers,
               .color = color,
               .width = width,
               .spacing = 1.1f,
               .opacity = 0.11f,
               .scatter = 10.0f,
               .grain = 0.86f,
               .bristles = 34,
               .pressure = {0.35f, 1.0f, 0.22f},
               .blend = MULTIPLY};
}

Brush spray(SkColor4f color, float width) {
  return Brush{.tip = Tip::Scatter,
               .color = color,
               .width = width * 0.12f,
               .spacing = 2.2f,
               .opacity = 0.38f,
               .scatter = width,
               .grain = 0.82f,
               .bristles = 12,
               .pressure = {0.5f, 1.0f, 0.38f}};
}

Brush prepareStroke(Pen& pen, const Brush& tool) {
  Brush strokeTool = tool;
  if (strokeTool.pressure.gaussian) {
    Pressure::Gaussian& profile = *strokeTool.pressure.gaussian;
    profile.center += pen.random(-profile.centerJitter, profile.centerJitter);
    profile.width *= 1.0f - profile.widthJitter * pen.random(1.0f, 1.5f);
    profile.width = std::max(0.025f, profile.width);
  } else if (strokeTool.pressure.variation) {
    const Pressure source = strokeTool.pressure;
    const Pressure::Variation variation = *source.variation;
    const float offset = pen.random(-variation.offset, variation.offset);
    const float scale =
        pen.random(1.0f - variation.scale, 1.0f + variation.scale);
    const float warp = pen.random(-variation.warp, variation.warp);
    const float tilt = pen.random(-variation.tilt, variation.tilt);
    strokeTool.pressure.curve = [source, offset, scale, warp,
                                 tilt](float progress) {
      const float at =
          std::clamp(0.5f + (progress - 0.5f + warp) * scale, 0.0f, 1.0f);
      return source.at(at) + offset + tilt * (progress - 0.5f);
    };
    strokeTool.pressure.variation.reset();
  }
  const float noise = std::clamp(strokeTool.noise, 0.0f, 1.0f);
  if (noise > 0.0f)
    strokeTool.opacity =
        std::max(0.0f, strokeTool.opacity *
                           (1.0f + pen.randomGaussian(0.0f, noise * 0.1f)));
  return strokeTool;
}

void paint(Pen& pen, const Brush& tool, std::span<const Sample> stroke) {
  if (stroke.size() < 2 || !(tool.width > 0.0f) || !(tool.opacity > 0.0f))
    return;
  const Brush strokeTool = prepareStroke(pen, tool);
  std::vector<Input> input;
  input.reserve(stroke.size());
  float travelled = 0.0f;
  for (size_t i = 0; i < stroke.size(); ++i) {
    if (i > 0)
      travelled += distance(stroke[i - 1].position, stroke[i].position);
    input.push_back({.position = stroke[i].position,
                     .pressure = stroke[i].pressure,
                     .seconds = travelled / 600.0});
  }
  deposit(pen, strokeTool, dabs(input, strokeTool.spacing));
}

void deposit(Pen& pen, const Brush& tool, std::span<const Dab> dabs,
             DepositOptions options) {
  if (!(tool.width > 0.0f) || !(tool.opacity > 0.0f)) return;
  pen.push();
  pen.blendMode(tool.blend);
  if (tool.tip == Tip::Fibers) {
    depositFibers(pen, tool, dabs);
    pen.pop();
    return;
  }
  static thread_local std::vector<Stamp> stamps;
  stamps.clear();
  if (tool.tip == Tip::Grain || tool.tip == Tip::Nib ||
      tool.tip == Tip::Scatter)
    stamps.reserve(dabs.size() * (tool.tip == Tip::Scatter
                                      ? (size_t)std::max(1, tool.bristles)
                                      : 1u));
  for (size_t index = 0; index < dabs.size(); ++index) {
    const Dab& dab = dabs[index];
    const DabStyle style = styleDab(
        pen, tool, dab, tool.tip != Tip::Scatter && tool.tip != Tip::Grain);
    if (!(style.size > 0.0f) || !(style.opacity > 0.0f)) continue;
    const auto put = [&](const DabStyle& mark) {
      switch (tool.tip) {
        case Tip::Grain:
          depositGrain(pen, tool, dab, mark, stamps);
          break;
        case Tip::Nib:
          depositNib(tool, mark, stamps);
          break;
        case Tip::Fibers:
          break;
        case Tip::Scatter:
          depositScatter(pen, tool, dab, mark, stamps);
          break;
        case Tip::Image:
          depositImage(pen, tool, mark);
          break;
        case Tip::Custom:
          depositCustom(pen, tool, dab, mark);
          break;
      }
    };
    put(style);
    const bool endpoint = (options.start && index == 0) ||
                          (options.end && index + 1 == dabs.size());
    if (tool.markerTip && endpoint &&
        (tool.tip == Tip::Nib || tool.tip == Tip::Image ||
         tool.tip == Tip::Custom)) {
      const int layers = tool.tip == Tip::Nib ? 9 : 4;
      const float buildup = tool.tip == Tip::Nib ? 8.0f : 2.0f;
      for (int layer = 1; layer <= layers; ++layer) {
        DabStyle endpoint = styleDab(pen, tool, dab, true);
        endpoint.size *= (float)layer * 0.1f;
        endpoint.opacity = std::min(1.0f, endpoint.opacity * buildup);
        put(endpoint);
      }
    }
  }
  drawStamps(pen, tool.blend, stamps);
  pen.pop();
}

void line(Pen& pen, const Brush& tool, SkPoint from, SkPoint to,
          float startPressure, float endPressure) {
  paint(pen, tool, segment(from, to, tool.spacing, startPressure, endPressure));
}

void spline(Pen& pen, const Brush& tool, std::span<const Sample> controls,
            float curvature) {
  paint(pen, tool, brush::spline(controls, tool.spacing, curvature));
}

}  // namespace sigil::draw::brush

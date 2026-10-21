/** @file
 * The pieces of the pen more than one of its files reads.
 */

#include "PenInternal.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilshaders/Draw.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::detail {

/** p5's SQUARE ends the stroke at the point and PROJECT carries it
 *  half a weight past, which are Skia's butt and square caps. */
geometry::path::Cap capOf(Constant cap) {
  switch (cap) {
    case SQUARE:
      return geometry::path::Cap::Butt;
    case PROJECT:
      return geometry::path::Cap::Square;
    default:
      return geometry::path::Cap::Round;
  }
}

/** HOW AN IMAGE IS SAMPLED under the pen's smoothing. Smoothing off is
 *  not only about the edges of shapes: it is what a blown-up pixel
 *  source needs, one source texel per destination block with nothing
 *  blended across the boundary, so the picture is blocks and not a
 *  blur. Mipmaps go with it — a mipmap IS a blend of neighbours. */
SkSamplingOptions samplingFor(bool smooth) {
  return smooth
             ? SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear)
             : SkSamplingOptions(SkFilterMode::kNearest, SkMipmapMode::kNone);
}

/** SUBTRACT: the source's colour taken out of the canvas's. No blend
 *  mode does it, so it is a blender of two lines, built once.
 *
 *  THE CANVAS KEEPS ITS OWN ALPHA. Subtracting alpha as well would turn
 *  an opaque ground transparent instead of dark, which is the opposite
 *  of what taking light away means; and the channels floor at nothing,
 *  which keeps each of them under the alpha a premultiplied colour has
 *  to stay under. */
sk_sp<SkBlender> subtractBlender() {
  static const sk_sp<SkBlender> blender = [] {
    SkRuntimeEffect::Result made = SkRuntimeEffect::MakeForBlender(
        SkString(shaderSource("Subtract.sksl")));
    return made.effect ? made.effect->makeBlender(nullptr) : sk_sp<SkBlender>();
  }();
  return blender;
}

sk_sp<SkBlender> fadeBlender() {
  static const sk_sp<SkBlender> blender = [] {
    SkRuntimeEffect::Result made =
        SkRuntimeEffect::MakeForBlender(SkString(shaderSource("Fade.sksl")));
    return made.effect ? made.effect->makeBlender(nullptr) : sk_sp<SkBlender>();
  }();
  return blender;
}

namespace {

/** Where one 8-bit channel at @p level goes under the fade toward
 *  @p target, in the same arithmetic the blender does: the plain blend's
 *  move, or one step if that is less, never past the target. */
uint8_t fadedLevel(int level, float target, float alpha) {
  const float toward = target - (float)level;
  const float least = std::min(std::fabs(toward), 1.0f);
  const float move = std::max(std::fabs(alpha * toward), least);
  const float moved = (float)level + std::copysign(move, toward);
  return (uint8_t)std::clamp(std::lround(moved), 0L, 255L);
}

}  // namespace

bool fadeInPlace(SkCanvas& canvas, const SkRect& ground,
                 const material::Color& colour) {
  SkImageInfo info;
  size_t rowBytes = 0;
  SkIPoint origin;
  void* pixels = canvas.accessTopLayerPixels(&info, &rowBytes, &origin);
  if (!pixels || info.colorType() != kN32_SkColorType ||
      info.alphaType() != kPremul_SkAlphaType)
    return false;
  // Colours are laid as given on a canvas with no colour space or an sRGB
  // one; any other converts them, which the blender does and a table
  // would have to repeat.
  if (info.colorSpace() && !info.colorSpace()->isSRGB()) return false;
  // Every pixel at full coverage: the clip a rectangle holding the whole
  // layer, and the ground, carried to the device, holding it too.
  const SkIRect layer = SkIRect::MakeXYWH(origin.x(), origin.y(),
                                          info.width(), info.height());
  if (!canvas.isClipRect() || !canvas.getDeviceClipBounds().contains(layer))
    return false;
  const SkMatrix toDevice = canvas.getTotalMatrix();
  if (!toDevice.rectStaysRect() ||
      !toDevice.mapRect(ground).contains(SkRect::Make(layer)))
    return false;

  const float alpha = std::clamp(colour.a, 0.0f, 1.0f);
  const auto level = [](float channel) {
    return std::clamp(channel, 0.0f, 1.0f) * 255.0f;
  };
  // The ground is opaque, so premultiplied it is its own colour.
  uint8_t red[256], green[256], blue[256], opacity[256];
  for (int value = 0; value < 256; ++value) {
    red[value] = fadedLevel(value, level(colour.r), alpha);
    green[value] = fadedLevel(value, level(colour.g), alpha);
    blue[value] = fadedLevel(value, level(colour.b), alpha);
    opacity[value] = fadedLevel(value, 255.0f, alpha);
  }
  // An image taken of the surface before this keeps what it showed: the
  // surface copies its pixels away from the image first, so they are
  // asked for again afterwards.
  if (SkSurface* surface = canvas.getSurface()) {
    surface->notifyContentWillChange(SkSurface::kRetain_ContentChangeMode);
    pixels = canvas.accessTopLayerPixels(&info, &rowBytes, &origin);
    if (!pixels) return false;
  }
  for (int row = 0; row < info.height(); ++row) {
    uint32_t* pixel =
        reinterpret_cast<uint32_t*>(static_cast<char*>(pixels) + row * rowBytes);
    for (int column = 0; column < info.width(); ++column) {
      const uint32_t value = pixel[column];
      const auto channel = [&](int shift) { return (value >> shift) & 0xFF; };
      const uint32_t a = opacity[channel(SK_A32_SHIFT)];
      // A premultiplied channel never stands above its alpha.
      const auto under = [&](const uint8_t* table, int shift) {
        return std::min<uint32_t>(table[channel(shift)], a) << shift;
      };
      pixel[column] = (a << SK_A32_SHIFT) | under(red, SK_R32_SHIFT) |
                      under(green, SK_G32_SHIFT) | under(blue, SK_B32_SHIFT);
    }
  }
  return true;
}

/** p5's blend words as Skia spells them. All but one are a blend mode,
 *  and setting a mode drops any blender the paint carried, so a paint
 *  that was SUBTRACT is plain again the moment another word is set. */
void setBlend(SkPaint& paint, Constant mode) {
  switch (mode) {
    case ADD:
      paint.setBlendMode(SkBlendMode::kPlus);
      return;
    case DARKEST:
      paint.setBlendMode(SkBlendMode::kDarken);
      return;
    case LIGHTEST:
      paint.setBlendMode(SkBlendMode::kLighten);
      return;
    case DIFFERENCE:
      paint.setBlendMode(SkBlendMode::kDifference);
      return;
    case EXCLUSION:
      paint.setBlendMode(SkBlendMode::kExclusion);
      return;
    case MULTIPLY:
      paint.setBlendMode(SkBlendMode::kMultiply);
      return;
    case SCREEN:
      paint.setBlendMode(SkBlendMode::kScreen);
      return;
    case REPLACE:
      paint.setBlendMode(SkBlendMode::kSrc);
      return;
    case REMOVE:
      paint.setBlendMode(SkBlendMode::kDstOut);
      return;
    case OVERLAY:
      paint.setBlendMode(SkBlendMode::kOverlay);
      return;
    case HARD_LIGHT:
      paint.setBlendMode(SkBlendMode::kHardLight);
      return;
    case SOFT_LIGHT:
      paint.setBlendMode(SkBlendMode::kSoftLight);
      return;
    case DODGE:
      paint.setBlendMode(SkBlendMode::kColorDodge);
      return;
    case BURN:
      paint.setBlendMode(SkBlendMode::kColorBurn);
      return;
    case SUBTRACT:
      paint.setBlender(subtractBlender());
      return;
    default:
      paint.setBlendMode(SkBlendMode::kSrcOver);
      return;
  }
}

geometry::path::Join joinOf(Constant join) {
  switch (join) {
    case BEVEL:
      return geometry::path::Join::Bevel;
    case ROUND:
      return geometry::path::Join::Round;
    default:
      return geometry::path::Join::Miter;
  }
}

/** p5's arc angles made drawable: both brought into [0, 2π), each
 *  corrected from the geometric angle a sketch means to the parametric
 *  one an ellipse is traced by, and the stop carried past the start so
 *  the sweep is clockwise. @p samePoint says the two meet, which draws
 *  the whole ellipse.
 *
 *  In double, and through atan2: the correction is tan(t) = (w/h) tan(a)
 *  taken in a's own quadrant, and a float tangent at a right angle lands
 *  on either side of its pole, which would flip a quarter arc into its
 *  three-quarter complement. */
void normalizeArc(float& startOut, float& stopOut, float w, float h,
                  bool& samePoint) {
  constexpr double kEpsilon = 0.00001;
  constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
  double start = startOut;
  double stop = stopOut;
  start = start - kTwoPi * std::floor(start / kTwoPi);
  stop = stop - kTwoPi * std::floor(stop / kTwoPi);
  const double separation =
      std::min(std::fabs(start - stop), kTwoPi - std::fabs(start - stop));
  samePoint = separation < kEpsilon;
  auto correct = [&](double angle) {
    double t =
        std::atan2((double)w * std::sin(angle), (double)h * std::cos(angle));
    if (t < 0.0) t += kTwoPi;
    return t;
  };
  start = correct(start);
  stop = correct(stop);
  if (start > stop) stop += kTwoPi;
  startOut = (float)start;
  stopOut = (float)stop;
}

/** One material onto one SkPaint: a solid is a colour, anything else a
 *  shader. Returns whether the shader has to be resolved again on every
 *  draw, which a live or box-dependent material does. */
bool resolve(const material::Paint& material, SkPaint& paint,
             const material::FrameData& frame) {
  if (material.isSolid()) {
    paint.setShader(nullptr);
    paint.setColor4f(sigil::material::skia::toSkColor(material.solidColor()),
                     nullptr);
    return false;
  }
  if (material.isNone()) {
    paint.setShader(nullptr);
    paint.setColor4f({0, 0, 0, 0}, nullptr);
    return false;
  }
  paint.setColor4f({0, 0, 0, 1}, nullptr);
  const bool live = material.isRunning() || material.geometryDependent();
  paint.setShader(material::skia::shader(material, frame));
  return live;
}

bool fittable(const SkRect* box) {
  return box && box->width() > 0.0f && box->height() > 0.0f;
}

float sizePx(const weave::Type& type) {
  return type.size.value_or(weave::Length{12.0f}).value;
}

}  // namespace sigil::draw::detail

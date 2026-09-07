/** @file
 * The pieces of the pen more than one of its files reads.
 */

#include "PenInternal.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilshaders/Draw.h>

#include <algorithm>
#include <cmath>

namespace sigil::draw::detail {

/** p5's SQUARE ends the stroke at the point and PROJECT carries it
 *  half a weight past, which are Skia's butt and square caps. */
SkPaint::Cap capOf(Constant cap) {
  switch (cap) {
    case SQUARE:
      return SkPaint::kButt_Cap;
    case PROJECT:
      return SkPaint::kSquare_Cap;
    default:
      return SkPaint::kRound_Cap;
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

SkPaint::Join joinOf(Constant join) {
  switch (join) {
    case BEVEL:
      return SkPaint::kBevel_Join;
    case ROUND:
      return SkPaint::kRound_Join;
    default:
      return SkPaint::kMiter_Join;
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
bool resolve(const material::skia::Paint& material, SkPaint& paint,
             const material::skia::PaintFrame& frame) {
  if (material.isSolid()) {
    paint.setShader(nullptr);
    paint.setColor4f(material.solidColor(), nullptr);
    return false;
  }
  if (material.isNone()) {
    paint.setShader(nullptr);
    paint.setColor4f({0, 0, 0, 0}, nullptr);
    return false;
  }
  paint.setColor4f({0, 0, 0, 1}, nullptr);
  const bool live = material.isAnimated() || material.geometryDependent();
  paint.setShader(material.shaderFor(frame));
  return live;
}

bool fittable(const SkRect* box) {
  return box && box->width() > 0.0f && box->height() > 0.0f;
}

}  // namespace sigil::draw::detail

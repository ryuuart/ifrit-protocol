#include "textflow/PaintShaders.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkM44.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkString.h>
#include <include/core/SkTileMode.h>
#include <include/effects/SkRuntimeEffect.h>

#include <algorithm>
#include <cmath>

namespace textflow::PaintShaders {

namespace {

sk_sp<SkRuntimeEffect> compileEffect(const char *source) {
  SkRuntimeEffect::Result result =
      SkRuntimeEffect::MakeForShader(SkString(source));
  return std::move(result.effect);
}

void setCommonUniforms(SkRuntimeShaderBuilder &builder, const SkRect &bounds,
                       float timeSeconds) {
  builder.uniform("origin") = SkV2{bounds.left(), bounds.top()};
  builder.uniform("extent") =
      SkV2{std::max(1.0f, bounds.width()), std::max(1.0f, bounds.height())};
  builder.uniform("time") = timeSeconds;
  builder.uniform("motion") =
      SkV2{std::sin(timeSeconds * 0.83f), std::cos(timeSeconds * 0.61f)};
}

const sk_sp<SkRuntimeEffect> &waterEffect() {
  static const sk_sp<SkRuntimeEffect> effect = compileEffect(R"(
    uniform float2 origin;
    uniform float2 extent;
    uniform float time;
    uniform float2 motion;

    half4 main(float2 point) {
      float2 uv = (point - origin) / extent;
      float waveX = sin(uv.y * 18.0 - time * 2.1 + motion.x);
      float waveY = cos(uv.x * 15.0 + time * 1.6 + motion.y);
      float2 warped = uv + float2(waveX, waveY) * 0.035;

      half3 deep = half3(0.015, 0.10, 0.30);
      half3 mid = half3(0.02, 0.42, 0.68);
      half3 aqua = half3(0.08, 0.82, 0.80);
      half3 color = mix(deep, mid, half(smoothstep(-0.05, 0.72, warped.y)));
      color = mix(color, aqua,
                  half(0.30 + 0.22 * sin(warped.x * 8.0 - time * 0.7)));

      float crossing = sin((warped.x + warped.y) * 34.0 + time * 2.0) *
                       cos((warped.x - warped.y) * 27.0 - time * 1.4);
      float caustic = pow(max(0.0, crossing), 7.0);
      color += half3(0.30, 0.48, 0.40) * half(caustic);
      return half4(color, 1.0);
    }
  )");
  return effect;
}

const sk_sp<SkRuntimeEffect> &meshEffect() {
  static const sk_sp<SkRuntimeEffect> effect = compileEffect(R"(
    uniform float2 origin;
    uniform float2 extent;
    uniform float time;
    uniform float2 motion;

    half4 main(float2 point) {
      float2 uv = (point - origin) / extent;
      float2 q = uv;
      q.x += (q.y - 0.5) * (0.11 * motion.x) +
             q.y * (1.0 - q.y) * (0.08 * motion.y);
      q.y += (q.x - 0.5) * (0.09 * motion.y) -
             q.x * (1.0 - q.x) * (0.06 * motion.x);
      float sx = smoothstep(-0.12, 1.12, q.x);
      float sy = smoothstep(-0.12, 1.12, q.y);

      half3 top = mix(half3(0.96, 0.20, 0.42),
                      half3(0.98, 0.67, 0.18), half(sx));
      half3 bottom = mix(half3(0.27, 0.12, 0.70),
                         half3(0.02, 0.78, 0.76), half(sx));
      half3 color = mix(top, bottom, half(sy));

      float2 center = float2(0.52 + 0.18 * motion.x,
                             0.48 + 0.16 * motion.y);
      float bloom = 1.0 - smoothstep(0.0, 0.62, length(q - center));
      color = mix(color, half3(0.53, 0.30, 0.94), half(bloom * 0.34));
      return half4(color, 1.0);
    }
  )");
  return effect;
}

const sk_sp<SkPicture> &starPicture() {
  static const sk_sp<SkPicture> picture = [] {
    constexpr float kTileSize = 48.0f;
    SkPictureRecorder recorder;
    SkCanvas *canvas = recorder.beginRecording(kTileSize, kTileSize);
    SkPaint paint;
    paint.setAntiAlias(true);

    struct Star {
      SkPoint center;
      float radius;
      SkColor color;
    };
    constexpr Star stars[] = {{{5, 8}, 1.15f, 0xE6FFFFFF},
                              {{19, 4}, 0.70f, 0xBFD6E9FF},
                              {{34, 13}, 1.00f, 0xE6FFE8A3},
                              {{12, 27}, 0.80f, 0xCCD6E9FF},
                              {{42, 31}, 1.25f, 0xF2FFFFFF},
                              {{25, 41}, 0.65f, 0xBFFFE8A3},
                              {{3, 44}, 0.55f, 0x99D6E9FF}};
    for (const Star &star : stars) {
      paint.setStyle(SkPaint::kFill_Style);
      paint.setColor(star.color);
      canvas->drawCircle(star.center, star.radius, paint);
      if (star.radius < 1.0f)
        continue;
      paint.setStyle(SkPaint::kStroke_Style);
      paint.setStrokeWidth(0.55f);
      canvas->drawLine({star.center.x() - star.radius * 2.2f, star.center.y()},
                       {star.center.x() + star.radius * 2.2f, star.center.y()},
                       paint);
      canvas->drawLine({star.center.x(), star.center.y() - star.radius * 2.2f},
                       {star.center.x(), star.center.y() + star.radius * 2.2f},
                       paint);
    }
    return recorder.finishRecordingAsPicture();
  }();
  return picture;
}

sk_sp<SkShader> makeShader(const sk_sp<SkRuntimeEffect> &effect,
                           const SkRect &bounds, float timeSeconds) {
  if (!effect)
    return nullptr;
  SkRuntimeShaderBuilder builder(effect);
  setCommonUniforms(builder, bounds, timeSeconds);
  return builder.makeShader();
}

} // namespace

sk_sp<SkShader> water(const SkRect &bounds, float timeSeconds) {
  return makeShader(waterEffect(), bounds, timeSeconds);
}

sk_sp<SkShader> meshGradient(const SkRect &bounds, float timeSeconds) {
  return makeShader(meshEffect(), bounds, timeSeconds);
}

sk_sp<SkShader> starField(const SkRect &bounds, float timeSeconds) {
  const float pulse = 1.0f + 0.035f * std::sin(timeSeconds * 1.7f);
  SkMatrix local = SkMatrix::Scale(pulse, pulse);
  local.postTranslate(bounds.left() + std::fmod(timeSeconds * 3.0f, 48.0f),
                      bounds.top() + std::fmod(timeSeconds * 1.3f, 48.0f));
  return starPicture()->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat,
                                   SkFilterMode::kLinear, &local, nullptr);
}

} // namespace textflow::PaintShaders

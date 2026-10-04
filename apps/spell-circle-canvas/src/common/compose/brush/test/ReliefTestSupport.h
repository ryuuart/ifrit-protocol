#pragma once
// The fixtures the relief and lit-stroke suites share: a brightness
// reading, a grey lit finish, a picture that counts its reads, two runtime
// shaders a binding drives, a root-anchored unlit gradient, and the cache
// state a held panel reports. Each is in an anonymous namespace, so every
// including translation unit gets its own copy and nothing here is shared
// at link time.

#include <include/core/SkString.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmedia/advanced/Skia.h>

#include <memory>
#include <stdexcept>

#include "support/Host.h"

namespace {

float brightness(SkColor colour) {
  return static_cast<float>(SkColorGetR(colour) + SkColorGetG(colour) +
                            SkColorGetB(colour));
}

material::Material greySurface() {
  return material::from(material::Color{0.6f, 0.6f, 0.6f, 1})
      .surface({.roughness = 0.8f});
}

struct CountedPicture {
  std::shared_ptr<int> reads;
  sk_sp<SkImage> image;
  sigil::media::Frame frameAt(std::chrono::duration<double>) const {
    ++*reads;
    sigil::media::Frame frame;
    frame.image = image;
    return frame;
  }
  bool isRunning() const { return false; }
  SkISize size() const { return image->dimensions(); }
  bool operator==(const CountedPicture& other) const {
    return reads == other.reads;
  }
};

sk_sp<SkImage> greyPicture(SkColor colour = SkColorSetRGB(150, 150, 150)) {
  SkBitmap pixels;
  pixels.allocN32Pixels(128, 100, true);
  pixels.eraseColor(colour);
  pixels.setImmutable();
  return pixels.asImage();
}

const sk_sp<SkRuntimeEffect>& timeColour() {
  static const sk_sp<SkRuntimeEffect> effect =
      SkRuntimeEffect::MakeForShader(
          SkString("uniform float uTime;"
                   "half4 main(float2 p) {"
                   "return half4(1-clamp(uTime,0,1),0,clamp(uTime,0,1),1); }"))
          .effect;
  return effect;
}

const sk_sp<SkRuntimeEffect>& scalarMap() {
  static const sk_sp<SkRuntimeEffect> effect =
      SkRuntimeEffect::MakeForShader(
          SkString(
              "uniform float value;"
              "half4 main(float2 p) { return half4(value,value,value,1); }"))
          .effect;
  return effect;
}

material::Material rootGradient() {
  material::Paint paint = material::Paint::linearGradient(
      {0, 0}, {320, 0}, {{0, {1, 0, 0, 1}}, {1, {0, 0, 1, 1}}},
      {.units = material::GradientUnits::Pixels});
  paint.worldSpace();
  return material::skia::base(std::move(paint)).surface({.unlit = true});
}

Composer::CacheState panelCacheState(const Host& host) {
  for (const auto& row : host.composer.profile())
    if (row.label.starts_with("panel (")) return row.cacheState;
  throw std::logic_error("Expected a profile row for panel");
}

}  // namespace

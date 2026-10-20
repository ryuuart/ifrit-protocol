#pragma once

/** @file
 * What the surface scene suites share: the page size every case draws
 * at, a lit plate and a lit glyph ink, a picture that counts the times it
 * is read, and the readings a case makes of a scene's maps — one map's
 * pixels, one channel of the packed occlusion-roughness-metallic map, and
 * a colour held to four numbers within a tolerance. Each is in an
 * anonymous namespace, so every including translation unit gets its own
 * copy and nothing here is shared at link time.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilcompose/testing/Scene.h>
#include <sigilcompose/texture/SurfaceScene.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/PixelSource.h>

#include <chrono>
#include <memory>

#include "support/Host.h"

namespace {

using sigil::material::texture::Role;

constexpr int kWidth = 128;
constexpr int kHeight = 112;

inline material::Material plateFinish(material::Color colour) {
  return material::from(colour).surface({.metallic = .75f, .roughness = .25f});
}

inline material::Material glyphFinish() {
  // A constant tilted normal: (0.5, 0, 0.8) normalised, green up.
  return material::from(material::Color{.2f, .6f, .4f, 1})
      .surface({.roughness = .9f,
                .normal = material::from(material::Color{.75f, .5f, .9f, 1})});
}

/** A picture that counts the times it is read. */
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

inline SkBitmap pixelsOf(const SurfaceScene& scene, Role role) {
  SkBitmap pixels;
  const TextureScene* map = scene.scene(role);
  if (!map || !map->image()) return pixels;
  pixels.allocN32Pixels(map->image()->width(), map->image()->height());
  map->image()->readPixels(nullptr, pixels.pixmap(), 0, 0);
  return pixels;
}

/** One channel of the packed occlusion-roughness-metallic map, as the
 *  grey map that scalar would have been on its own: 0 occlusion, 1
 *  roughness, 2 metallic. */
inline SkBitmap channelOf(const SurfaceScene& scene, int channel) {
  const SkBitmap packed = pixelsOf(scene, Role::Packed);
  SkBitmap grey;
  if (packed.empty()) return grey;
  grey.allocN32Pixels(packed.width(), packed.height());
  for (int y = 0; y < packed.height(); ++y)
    for (int x = 0; x < packed.width(); ++x) {
      const SkColor4f c = packed.getColor4f(x, y);
      const float v = channel == 0 ? c.fR : channel == 1 ? c.fG : c.fB;
      grey.erase(SkColor4f{v, v, v, c.fA}.toSkColor(),
                 SkIRect::MakeXYWH(x, y, 1, 1));
    }
  return grey;
}

inline void expectNear(const SkColor4f& actual, float red, float green,
                       float blue, float alpha = 1, float tolerance = .012f) {
  EXPECT_NEAR(actual.fR, red, tolerance);
  EXPECT_NEAR(actual.fG, green, tolerance);
  EXPECT_NEAR(actual.fB, blue, tolerance);
  EXPECT_NEAR(actual.fA, alpha, tolerance);
}

}  // namespace

#pragma once

/** @file
 * @ingroup compose-texture
 *
 * A retained Compose scene sampled through an ordinary material::Texture.
 */

#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Paint.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/texture/Texture.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

namespace sigil::weave {
class FontContext;
}

namespace sigil::core::hardware {
class GpuDevice;
}  // namespace sigil::core::hardware

namespace sigil::skia {
class GraphiteContext;
}  // namespace sigil::skia

namespace sigil::compose {

class Composer;

/** Owns a composer and an offscreen surface. Texture values keep the scene
 *  alive; fonts and any adopted device/context must outlive those values. */
class TextureScene : public std::enable_shared_from_this<TextureScene> {
 public:
  /** An N32 premultiplied raster scene; dimensions are at least one pixel. */
  static std::shared_ptr<TextureScene> make(SkISize size,
                                            sigil::weave::FontContext& fonts,
                                            material::Color background = {
                                                0, 0, 0, 0});
  /** A premultiplied RGBA/BGRA byte, RGBA F16 or RGBA F32 raster scene,
   *  retaining @p info's color space. Null for an unsupported format,
   *  nonpositive dimensions or failed allocation. Cleared to @p background
   *  before each paint. */
  static std::shared_ptr<TextureScene> make(SkImageInfo info,
                                            sigil::weave::FontContext& fonts,
                                            material::Color background = {
                                                0, 0, 0, 0});
  ~TextureScene();

  TextureScene(const TextureScene&) = delete;
  TextureScene& operator=(const TextureScene&) = delete;

  /** Adopt a device surface of the scene's format and color space. The
   *  context must use that device. F32 remains raster-only. Failure keeps
   *  the current surface and pixels; success drops pixels and cache contents
   *  so the next render paints the retained tree on the new surface. */
  bool useDevice(sigil::core::hardware::GpuDevice& device,
                 sigil::skia::GraphiteContext& context);

  /** Reconcile @p root and paint when needed. The scene clock advances to
   *  @p seconds; earlier or nonfinite readings leave it unchanged. */
  void render(const Element& root, double seconds = 0.0);

  /** A sampling value retaining this scene and its current revision. */
  material::Texture texture() const;

  /** How many times this scene has painted. */
  uint64_t revision() const;
  SkISize size() const;
  /** The latest image; null before a paint on the current surface. Device
   *  images belong to their Graphite context and are not portable raster
   *  readbacks. */
  sk_sp<SkImage> image() const;
  /** Where those pixels stand when they stand on a device; empty on the
   *  raster surface or before the first paint on the current surface. */
  material::DeviceImage deviceImage() const;
  /** Whether the retained tree needs another render. */
  bool isRunning() const;

  /** The retained composer's statistics and queries. */
  const Composer& composer() const;
  /** Set the retained composer's texture-promotion policy. */
  void setAutoTexturePromotion(PromotionPolicy policy);
  /** Paint the tree as one map of its surface (`Composer::setSurfaceMap`),
   *  cleared to that map's ground in place of the background; none paints
   *  it as it is seen, over the background again. The next render paints. */
  void setSurfaceMap(std::optional<material::texture::Role> role);

 private:
  TextureScene();
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

/** Reads the scene's latest image and compares by scene plus captured
 *  revision. It does not preserve historical pixels. */
class SceneSource {
 public:
  SceneSource(std::shared_ptr<const TextureScene> scene, uint64_t revision)
      : m_scene(std::move(scene)), m_revision(revision) {}

  sigil::media::Frame frameAt(std::chrono::duration<double> time) const;
  bool isRunning() const { return m_scene && m_scene->isRunning(); }
  SkISize size() const {
    return m_scene ? m_scene->size() : SkISize::MakeEmpty();
  }
  const TextureScene* scene() const { return m_scene.get(); }
  uint64_t revision() const { return m_revision; }

  bool operator==(const SceneSource& other) const {
    return m_scene == other.m_scene && m_revision == other.m_revision;
  }

 private:
  std::shared_ptr<const TextureScene> m_scene;
  uint64_t m_revision = 0;
};

/** Render one tree into an N32 scene held by the returned texture. */
material::Texture texture(const Element& root, SkISize size,
                          sigil::weave::FontContext& fonts,
                          material::Color background = {0, 0, 0, 0});
/** Render one tree in @p info's format and color space. An invalid scene
 *  allocation returns an empty texture. */
material::Texture texture(const Element& root, SkImageInfo info,
                          sigil::weave::FontContext& fonts,
                          material::Color background = {0, 0, 0, 0});

}  // namespace sigil::compose

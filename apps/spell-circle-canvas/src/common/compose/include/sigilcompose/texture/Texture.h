#pragma once

/** @file
 * A compose scene as a `material::Texture` — the value a 3D surface, a
 * pattern or anything else that samples an image holds in a slot.
 *
 * There is no panel, no card and no "is this a scene" branch anywhere
 * downstream: what comes out of here is an ordinary texture, so every
 * sampling dial applies to it — the tiling per axis, the uv matrix, the
 * region cut from it, the filter — and an `Atlas` can be cut from it
 * like any other sheet.
 *
 * The scene behind the value keeps a composer, so the tree it is handed
 * is RECONCILED rather than rebuilt: a description that did not change
 * costs a comparison. It paints only when that reconcile (or a
 * transition still in flight) actually moved something, and the version
 * it hands the texture counts the paints — so a consumer's material
 * compares EQUAL across a frame in which nothing was painted, and
 * unequal the frame something was.
 */

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilmaterial/texture/Texture.h>

#include <cstdint>
#include <memory>

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

/** A COMPOSE SCENE THAT IS A TEXTURE: the composer, the surface its
 *  tree lands in, and the count of how many times that surface has been
 *  painted.
 *
 *  Hold one for as long as the picture is wanted and hand it a tree
 *  whenever the description changed. It is not copyable — it owns a
 *  retained tree and a surface — and it is reached through a shared
 *  pointer, because the texture values it hands out name it and must
 *  keep it standing.
 *
 *  TWO SURFACES ARE POSSIBLE and the choice is `useDevice`. Without one
 *  the scene paints into a raster surface, which is always available and
 *  is what a host with no device (a test, a plate, a machine with no
 *  GPU) reads. With one it paints into a texture on that device through
 *  Graphite, and a renderer standing on the SAME device binds those
 *  pixels rather than uploading a copy of them. */
class TextureScene : public std::enable_shared_from_this<TextureScene> {
 public:
  /** A scene @p size pixels across, shaped by @p fonts — which is held,
   *  not copied, and must outlive the scene — cleared to @p background
   *  before each paint. */
  static std::shared_ptr<TextureScene> make(SkISize size,
                                            sigil::weave::FontContext& fonts,
                                            SkColor4f background = {0, 0, 0,
                                                                    0});
  ~TextureScene();

  TextureScene(const TextureScene&) = delete;
  TextureScene& operator=(const TextureScene&) = delete;

  /** ZERO COPY: the scene paints into a texture @p device names, wrapped
   *  by Graphite on @p context, so a renderer on that same device
   *  samples what compose painted with nothing crossing between them.
   *
   *  Answers false — and keeps the raster surface, so the scene still
   *  draws — when the device refuses the texture or the wrap fails.
   *  Whatever the scene has painted so far is dropped, because the
   *  pixels are somewhere else now. */
  bool useDevice(sigil::core::hardware::GpuDevice& device,
                 sigil::skia::GraphiteContext& context);

  /** Reconciles @p root against the tree this scene already holds, at
   *  scene time @p seconds, and paints when anything moved.
   *
   *  @p seconds is the scene's OWN clock, in seconds from whenever it
   *  started, and it is what every time-reading material and every
   *  transition in the tree is stepped to — so a caller stepping it in
   *  fixed increments gets a picture that is a function of the number of
   *  steps and never of how fast the machine ran. */
  void render(const Element& root, double seconds = 0.0);

  /** THE VALUE a slot holds. Every sampling dial rides the copy: tile
   *  it, place it with `uv()`, cut a region out of it. Two values taken
   *  either side of a frame that painted nothing compare equal. */
  material::Texture texture() const;

  /** How many times this scene has painted. */
  uint64_t version() const;
  SkISize size() const;
  /** What the last paint left, whether it stands in host memory or on a
   *  device; null before the first one. */
  sk_sp<SkImage> image() const;
  /** Where those pixels stand when they stand on a device; empty on the
   *  raster surface. */
  material::DeviceImage deviceImage() const;
  /** Whether the next render could paint a different picture: the tree
   *  is dirty, or a transition is still running. A scene that answers
   *  false has settled, and a consumer holding its texture may cache
   *  whatever it made of it. */
  bool active() const;

  /** The composer behind the scene — its stats and its queries, for a
   *  caller verifying what a frame cost. */
  const Composer& composer() const;

 private:
  TextureScene();
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

/** THE SOURCE a scene's texture carries: the scene, and the version it
 *  had when the value was taken.
 *
 *  Two sources are equal when they name one scene that has painted the
 *  same number of times — which is what makes a material holding a
 *  scene texture prune across a still frame and patch across a painted
 *  one. */
class SceneSource {
 public:
  SceneSource(std::shared_ptr<const TextureScene> scene, uint64_t version)
      : m_scene(std::move(scene)), m_version(version) {}

  sk_sp<SkImage> image() const { return m_scene ? m_scene->image() : nullptr; }
  bool animated() const { return m_scene && m_scene->active(); }
  material::DeviceImage deviceImage() const {
    return m_scene ? m_scene->deviceImage() : material::DeviceImage{};
  }
  const TextureScene* scene() const { return m_scene.get(); }
  uint64_t version() const { return m_version; }

  bool operator==(const SceneSource& other) const {
    return m_scene == other.m_scene && m_version == other.m_version;
  }

 private:
  std::shared_ptr<const TextureScene> m_scene;
  uint64_t m_version = 0;
};

/** ONE TREE, ONE TEXTURE: a scene of its own, rendered once and held by
 *  the value. The scene lives as long as a copy of the texture does, and
 *  nothing can hand it a second tree — which is the whole difference
 *  from holding a `TextureScene`, and the right shape for a picture that
 *  is described once. */
material::Texture texture(const Element& root, SkISize size,
                          sigil::weave::FontContext& fonts,
                          SkColor4f background = {0, 0, 0, 0});

}  // namespace sigil::compose

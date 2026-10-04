#pragma once

/** @file
 * @ingroup compose-texture
 *
 * A retained Compose scene rendered as THE MAPS OF A SURFACE — base
 * colour, normal, roughness, metallic, occlusion and emissive — for a
 * renderer that lights the page itself.
 */

#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmaterial/texture/TextureSet.h>

#include <cstdint>
#include <map>
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

/** ONE TREE, PAINTED AS EACH MAP OF ITS SURFACE. Every lit fill, ink,
 *  stroke and relief paints the map its material states
 *  (`material::skia::asMap`) at the coverage it paints; content that takes
 *  no light is its own colour — its colour in the emissive map, a black
 *  base, a normal facing the viewer, roughness and occlusion one, metallic
 *  zero. Where nothing paints, each map reads that same ground, with a
 *  transparent base colour. Normals are encoded green up the picture.
 *
 *  `maps()` hands the set to `material::surface::program`, which dresses a
 *  surface a lit renderer shades, so the page is lit by the renderer's own
 *  lights and environment rather than by Compose. Each map is one
 *  `TextureScene` over the same tree with its own composer and caches, so
 *  nothing held for one map is replayed for another; a render costs one
 *  reconcile and, while the tree moves, one paint per map.
 *  Fonts, and any adopted device and context, must outlive the scene and
 *  its texture values. */
class SurfaceScene {
 public:
  /** An N32 premultiplied raster scene per map; dimensions are at least
   *  one pixel. */
  static std::shared_ptr<SurfaceScene> make(SkISize size,
                                            sigil::weave::FontContext& fonts);
  ~SurfaceScene();

  SurfaceScene(const SurfaceScene&) = delete;
  SurfaceScene& operator=(const SurfaceScene&) = delete;

  /** Adopt device surfaces for every map; see `TextureScene::useDevice`.
   *  False, with every map still where it stood, when any refuses. */
  bool useDevice(sigil::core::hardware::GpuDevice& device,
                 sigil::skia::GraphiteContext& context);

  /** Reconcile @p root into every map and paint those that need it. The
   *  scene clock advances to @p seconds; earlier or nonfinite readings
   *  leave it unchanged. */
  void render(const Element& root, double seconds = 0.0);

  /** The maps as a texture set, one texture per role, green-up normals:
   *  what `material::surface::program(maps, parameters)` dresses a
   *  surface with. */
  material::texture::TextureMaps maps() const;
  /** The texture for @p role; empty for a role this scene paints none for. */
  material::Texture texture(material::texture::Role role) const;

  /** How many renders painted anything; stands while the tree is still. */
  uint64_t revision() const;
  SkISize size() const;
  /** Whether the retained tree needs another render. */
  bool isRunning() const;
  /** The scene that paints @p role, for its composer's statistics and
   *  queries; null for a role this scene paints none for. */
  const TextureScene* scene(material::texture::Role role) const;

 private:
  SurfaceScene();
  std::map<material::texture::Role, std::shared_ptr<TextureScene>> m_scenes;
  uint64_t m_revision = 0;
};

}  // namespace sigil::compose

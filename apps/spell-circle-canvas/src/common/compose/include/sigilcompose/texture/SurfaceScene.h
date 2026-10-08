#pragma once

/** @file
 * @ingroup compose-texture
 *
 * A retained Compose scene rendered as THE MAPS OF A SURFACE — base
 * colour, normal, occlusion-roughness-metallic packed in one, emissive and
 * the clear coat — for a renderer that lights the page itself.
 */

#include <include/core/SkSize.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilmaterial/core/Material.h>
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
 *  (`material::skia::asMap`) at the coverage it paints; content that states
 *  no surface is a stock surface — its colour in the base colour map, a
 *  normal facing the viewer, roughness one half, occlusion one, metallic,
 *  emission and the coat zero — so it takes the scene's light, and only a
 *  material that states emission glows. Where nothing paints, each map
 *  reads a transparent base colour, a normal facing the viewer, roughness
 *  and occlusion one and metallic, emission and the coat zero. Normals are
 * encoded green up the picture; occlusion, roughness and metallic share the
 * `Packed` map in red, green and blue, and the coat's weight and roughness
 * share the `Clearcoat` map in red and green.
 *
 *  THE OPTIONAL LOBES COST NOTHING UNTIL STATED. A page whose materials
 *  state no sheen, brushing, film or specular paints those five maps only;
 *  the first render that paints a material stating one adds the maps that
 *  lobe needs — `Sheen` and `SheenSpecular` for a sheen, `SpecularColor`
 *  and `SheenSpecular` for a specular, `Anisotropy`, `Iridescence` — paints
 *  them in that same render, and keeps them from then on. A coat's own
 *  normal is the same: the first material stating one adds the
 *  `ClearcoatNormal` map.
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
   *  surface with. The lobe maps are there once a render has added them. */
  material::texture::TextureMaps maps() const;
  /** The texture for @p role; empty for a role this scene paints none for. */
  material::Texture texture(material::texture::Role role) const;

  /** The optional lobes the painted materials have stated so far. */
  material::Lobes lobes() const;

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
  /** A scene painting @p role, made and adopted onto the device the
   *  others stand on; true when one is there. */
  bool add(material::texture::Role role);
  std::map<material::texture::Role, std::shared_ptr<TextureScene>> m_scenes;
  uint64_t m_revision = 0;
  SkISize m_size = SkISize::MakeEmpty();
  sigil::weave::FontContext* m_fonts = nullptr;
  sigil::core::hardware::GpuDevice* m_device = nullptr;
  sigil::skia::GraphiteContext* m_context = nullptr;
  material::Lobes m_lobes;
  bool m_clearcoatNormal = false;
};

}  // namespace sigil::compose

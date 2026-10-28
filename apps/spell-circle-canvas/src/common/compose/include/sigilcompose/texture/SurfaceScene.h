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
#include <sigilcompose/texture/SurfaceRange.h>
#include <sigilcompose/texture/Texture.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmaterial/texture/TextureSet.h>

#include <cstdint>
#include <map>
#include <memory>
#include <tuple>

namespace sigil::weave {
class FontContext;
}

namespace sigil::gpu {
class GpuDevice;
}  // namespace sigil::gpu

namespace sigil::skia {
class GraphiteContext;
}  // namespace sigil::skia

namespace sigil::compose {

/** ONE TREE, PAINTED AS EACH MAP OF ITS SURFACE. Every lit fill, ink,
 *  stroke and relief paints the map its material states
 *  (`material::skia::asMap`) at the coverage it paints; content that states
 *  no surface is a stock surface — its colour in the base colour map, a
 *  normal facing the viewer, roughness one half, occlusion one, metallic,
 *  emission and the coat zero — so it takes the scene's light; only a
 *  material that states emission glows, and one stated unlit is its own
 *  colour in the emissive map. Where nothing paints, each map
 *  reads a transparent base colour, a normal facing the viewer, roughness
 *  and occlusion one and metallic, emission and the coat zero. Normals are
 * encoded green up the picture; occlusion, roughness and metallic share the
 * `Packed` map in red, green and blue, and the coat's weight and roughness
 * share the `Clearcoat` map in red and green.
 *
 *  THE OPTIONAL LOBES COST NOTHING UNTIL STATED. A page whose materials
 *  state no sheen, brushing, film, specular, transmission or distortion
 *  paints those five maps only; the first render that paints a material
 *  stating one adds the maps that lobe needs — `Sheen` and `SheenSpecular`
 *  for a sheen, `SpecularColor` and `SheenSpecular` for a specular,
 *  `Anisotropy`, `Iridescence`, `Transmission` with `Thickness`, `Volume`
 *  and `Attenuation` for a transmission, `Distortion` — paints them in that
 * same render, and keeps them from then on. A coat's own normal is the same:
 * the first material stating one adds the `ClearcoatNormal` map.
 *
 *  A TRANSMITTING REGION SEES THROUGH THE BODY WEARING THE PAGE. The
 *  transmission map is the weight in red, the thickness map the depth of
 *  the volume over the deepest any material on the page states, and the
 *  distortion map the displacement, red across and green up, over the
 *  strongest stated, and `maps()` states those two. Each region's own
 *  index, dispersion and attenuation are its volume and attenuation maps',
 *  so two transmitting regions on one page bend and tint by their own.
 *
 *  AN EMISSION PAST WHITE KEEPS ITS LIGHT. The emissive map's white stands
 *  for the brightest emission the page's materials have stated, one at
 *  least: a page whose brightest is four paints four as white and every
 *  other emission as its share of that light, and `maps()` states four as
 *  the strength the map is read at. A page that states nothing brighter
 *  than one paints its emission as stated. A STATED RANGE (`SurfaceRange`)
 *  fixes those readings from the first paint instead: a page whose
 *  emission moves on the clock states the brightest it will reach, and
 *  nothing below it clips or is repainted because it rose.
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
   *  one pixel. @p range states what the maps stand for at their white;
   *  what it leaves at zero is learned from the content. */
  static std::shared_ptr<SurfaceScene> make(SkISize size,
                                            sigil::weave::FontContext& fonts,
                                            SurfaceRange range = {});
  ~SurfaceScene();

  SurfaceScene(const SurfaceScene&) = delete;
  SurfaceScene& operator=(const SurfaceScene&) = delete;

  /** Adopt device surfaces for every map; see `TextureScene::useDevice`.
   *  False, with every map still where it stood, when any refuses. */
  bool useDevice(sigil::gpu::GpuDevice& device,
                 sigil::skia::GraphiteContext& context);

  /** Reconcile @p root into every map and paint those that need it. The
   *  scene clock advances to @p seconds; earlier or nonfinite readings
   *  leave it unchanged. */
  void render(const Element& root, double seconds = 0.0);

  /** The maps as a texture set, one texture per role, green-up normals:
   *  what `material::surface::program(maps, parameters)` dresses a
   *  surface with. The lobe maps are there once a render has added them. */
  material::texture::TextureMaps maps() const;
  /** THE MAPS OF ONE PART OF THE PAGE: @p region, in page pixels, read
   *  whole by whatever wears them, so one page lays its parts out and each
   *  body wears its own. Each map of the region is a cut of the page's map
   *  (`TextureScene::cut`) the scene keeps from the first time the region
   *  is asked for, copied again by every render that paints the page, on
   *  the surface the page stands on. The numbers the maps are read
   *  against are the whole page's. */
  material::texture::TextureMaps maps(material::PixelRect region);
  /** Whether @p scene is one of this page's maps or one of its cuts. */
  bool paints(const TextureScene* scene) const;
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
  /** The cuts of each region asked for, by region and role. */
  std::map<std::tuple<int, int, int, int>,
           std::map<material::texture::Role, std::shared_ptr<TextureScene>>>
      m_cuts;
  /** The numbers every set of maps is read against, beside its textures. */
  material::texture::TextureMaps readings() const;
  uint64_t m_revision = 0;
  SkISize m_size = SkISize::MakeEmpty();
  sigil::weave::FontContext* m_fonts = nullptr;
  sigil::gpu::GpuDevice* m_device = nullptr;
  sigil::skia::GraphiteContext* m_context = nullptr;
  material::Lobes m_lobes;
  bool m_clearcoatNormal = false;
  /** What the map of @p role is read against: its white, or its full
   *  swing. */
  float scaleOf(material::texture::Role role) const;
  /** The brightest emission painted so far, at least one, or the
   *  emission stated: what the emissive map's white stands for. */
  float m_emissionScale = 1;
  /** The deepest volume and the strongest distortion painted so far, or
   *  the ones stated, in scene units; zero before either says one. */
  float m_thicknessScale = 0;
  float m_distortionScale = 0;
  /** What the author stated the maps stand for. */
  SurfaceRange m_range;
};

}  // namespace sigil::compose

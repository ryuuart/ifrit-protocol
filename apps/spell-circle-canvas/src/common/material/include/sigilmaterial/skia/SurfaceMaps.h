#pragma once

/** @file
 * @ingroup material-skia
 *
 * A SURFACE'S MAPS IN 2D through Skia: a material whose `surface()`
 * states a response, read back UNSHADED one surface map at a time —
 * base colour, normal, roughness, metallic, occlusion, emissive, the
 * three scalars packed, the clear coat and its own normal — for a
 * renderer that does its own lighting. Its colour stack and channel
 * inputs are prepared once and shared by every map.
 */

#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/paint/Paint.h>
#include <sigilmaterial/texture/TextureSet.h>

#include <memory>
#include <optional>

namespace sigil::material::skia {

/** Whether @p material states a surface that takes light: one stated and
 *  not `unlit`. */
bool isLit(const Material& material);

/** Whether the colour stack, surface maps or effects depend on root
 *  coordinates. A cached consumer must repaint when its node-to-root
 *  placement changes. */
bool usesWorldSpace(const Material& material);

/** A material's prepared colour stack and surface maps. Retain one while
 *  the material stays the same. Copies share immutable inputs and the
 *  map passes made so far; live bindings and children resolve for each
 *  draw. Preparation retains sources without reading pixels. Resolution
 *  binds them to the draw's destination. */
class SurfaceMaps {
 public:
  explicit SurfaceMaps(const Material& material);

  /** The surface's map for @p role, unshaded; see `asMap(material, role)`.
   *  Holds one pass per role for as long as this value. */
  Paint asMap(texture::Role role) const;
  /** `asMap(role)` resolved for one draw whose input unit square maps into
   *  the node through @p paintToLocal. Colour and channel inputs share the
   *  prepared lowering; their frame has unit resolution and placement
   *  through that mapping. Encoded normals retain their slopes;
   *  height-derived normals receive logical-pixel sampling steps through
   *  the mapping. Projective mappings keep colours and encoded normals but
   *  flatten height-derived relief. Null for a role `asMap` answers
   *  nothing for, and for a nonfinite or noninvertible mapping. */
  sk_sp<SkShader> mapShader(texture::Role role, const FrameData& nodeFrame,
                            const glm::mat3& paintToLocal) const;

 private:
  struct Inputs;
  struct MapCache;
  std::shared_ptr<const Inputs> m_inputs;
  std::shared_ptr<MapCache> m_maps;
  Paint mapPass(texture::Role role) const;
};

/** Whether `asMap` answers a map for @p role: base colour, normal,
 *  roughness, metallic, occlusion, emissive, packed and clearcoat, the
 *  coat's own normal, and the optional lobes' sheen, sheen-and-specular,
 *  specular colour, anisotropy and iridescence. */
bool isSurfaceMapRole(texture::Role role);

/** WHAT THE SURFACES READ AS MAPS STATE BEYOND WHAT A MAP HOLDS AT WHITE,
 *  and the numbers of a volume no map holds — what a page painted as maps
 *  reads its maps against. */
struct MapReach {
  /** The brightest emission stated: the largest channel of an emission
   *  colour times its strength; zero where none glows. */
  float emission = 0;
  /** The deepest thickness a transmitting surface states, in scene units;
   *  a thickness that is a map reaches one. */
  float thickness = 0;
  /** The largest distortion strength stated with a map, in scene units. */
  float distortion = 0;
  /** The volume of the first transmitting surface heard. */
  std::optional<texture::Volume> volume;
  /** Whether a transmitting surface heard after the first states another
   *  volume, which a page carrying one volume cannot keep. */
  bool volumesDiffer = false;
};

/** WHICH OPTIONAL LOBES THE SURFACES READ AS MAPS STATE, heard while one
 *  of these stands on the thread that reads them. A painter that paints
 *  a page as a surface's maps stands one over the paint, and so learns
 *  which lobe maps the page needs without walking what it paints: every
 *  `SurfaceMaps` made on the thread while it stands adds the lobes its
 *  material states. Listeners nest; each hears what is made inside it. */
class LobeListener {
 public:
  LobeListener();
  ~LobeListener();
  LobeListener(const LobeListener&) = delete;
  LobeListener& operator=(const LobeListener&) = delete;

  /** The lobes heard so far. */
  [[nodiscard]] Lobes heard() const { return m_heard; }
  /** Whether a surface heard so far states a normal of its clear coat's
   *  own, which a page paints as a map of its own once one does. */
  [[nodiscard]] bool heardClearcoatNormal() const {
    return m_heardClearcoatNormal;
  }
  /** How far the surfaces heard so far reach past a map's white. */
  [[nodiscard]] const MapReach& heardReach() const { return m_reach; }

  /** @private what a `SurfaceMaps` reports its material's lobes through. */
  static void hear(const Lobes& lobes);
  /** @private what a `SurfaceMaps` reports a coat's own normal through. */
  static void hearClearcoatNormal();
  /** @private what a `SurfaceMaps` reports its material's reach through. */
  static void hearReach(const MapReach& reach);

 private:
  LobeListener* m_outer = nullptr;
  Lobes m_heard;
  bool m_heardClearcoatNormal = false;
  MapReach m_reach;
};

/** @p material's surface as THE MAP OF @p role that a metallic-roughness
 *  surface wears, unshaded — for a renderer that brings its own lights.
 *  Every map carries the colour stack's alpha as its coverage, and its
 *  value premultiplied by it.
 *
 *  A lit material answers its channels: the colour stack
 *  as the base colour; the normal (a map, a height-derived or blended
 *  normal, scaled by `normalScale`) encoded as `(n + 1) / 2` with green
 *  pointing UP the picture whatever `normalDirectX` the material stated,
 *  and facing the viewer where it states none; roughness, metallic and
 *  occlusion as grey, each its number times its map's red, in `[0, 1]`,
 *  with the stock numbers `SurfaceOptions` starts at (roughness one half,
 *  metallic zero, occlusion one); the emission colour times its
 *  strength and its map, black where it states none; `Packed` as those
 *  three in red, green and blue, glTF's order; `Clearcoat` as the
 *  coat's weight in red and its roughness in green; `Sheen` as the sheen's
 *  colour; `SheenSpecular` as the sheen's roughness in red and the
 *  specular strength in green; `SpecularColor` as the specular colour;
 *  `Anisotropy` as the direction the highlight stretches along, turned by
 *  `anisotropyRotation`, encoded in red and green as a normal map is, and
 *  its strength in blue; `Iridescence` as the film's weight in red and
 *  its thickness in green over `texture::kIridescenceThicknessRange`; and
 *  `ClearcoatNormal` as the coat's own normal, encoded as the normal is and
 *  facing the viewer where the surface states none.
 *
 *  A material that states no surface answers the stock `SurfaceOptions`:
 *  its colour stack is its base colour and it takes the renderer's light.
 *  Only a surface stated `unlit` is ITS OWN COLOUR: the colour stack is its
 *  emission, its base colour is black, its normal faces the viewer, its
 *  roughness and occlusion are one and its metallic zero — so a renderer
 *  that shades the set shows it as it was painted. A role outside
 *  `isSurfaceMapRole` paints nothing.
 *  The reflection weight has no map and is not read. */
Paint asMap(const Material& material, texture::Role role);

/** What a surface map of @p role reads where nothing is painted: the map
 *  of a surface that is its own colour at no coverage — a transparent
 *  black base colour, a normal and a coat normal facing the viewer,
 *  roughness and occlusion one, metallic, emission and the coat zero, no sheen,
 * a white specular at full strength, no brushing and no film. Every map but the
 * base colour is opaque, so a lit renderer reads the base colour's alpha as
 * where the surface is. Transparent black for a role `asMap` answers nothing
 * for. */
Color surfaceMapGround(texture::Role role);

/** What a surface map of @p role reads under content that states no
 *  surface — a plain colour, a gradient, an image, a layer style — painted
 *  on a page: the stock `SurfaceOptions` at the coverage the content
 *  painted. A normal and a coat normal facing the viewer, roughness one
 *  half, metallic, emission and the coat zero, occlusion one, no sheen, a
 *  white specular at full strength, no brushing and no film. The base
 *  colour is the content's own colour, so this answers transparent black
 *  for it, as for a role `asMap` answers nothing for. */
Color flatContentMap(texture::Role role);

}  // namespace sigil::material::skia

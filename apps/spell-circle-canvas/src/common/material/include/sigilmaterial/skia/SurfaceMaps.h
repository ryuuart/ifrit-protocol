#pragma once

/** @file
 * @ingroup material-skia
 *
 * A SURFACE'S MAPS IN 2D through Skia: a material whose `surface()`
 * states a response, read back UNSHADED one surface map at a time —
 * base colour, normal, roughness, metallic, occlusion, emissive — for a
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
 *  roughness, metallic, occlusion and emissive. */
bool isSurfaceMapRole(texture::Role role);

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
 *  metallic zero, occlusion one); and the emission colour times its
 *  strength and its map, black where it states none.
 *
 *  A material that states no surface, or states one `unlit`, is ITS OWN
 *  COLOUR: the colour stack is its emission, its base colour is black,
 *  its normal faces the viewer, its roughness and occlusion are one and
 *  its metallic zero — so a renderer that shades the set shows it as it
 *  was painted. A role outside `isSurfaceMapRole` paints nothing.
 *  Clearcoat and reflection weight have no map and are not read. */
Paint asMap(const Material& material, texture::Role role);

/** What a surface map of @p role reads where nothing is painted: the map
 *  of a surface that is its own colour at no coverage — a transparent
 *  black base colour, a normal facing the viewer, roughness and occlusion
 *  one, metallic and emission zero. Every map but the base colour is
 *  opaque, so a lit renderer reads the base colour's alpha as where the
 *  surface is. Transparent black for a role `asMap` answers nothing for. */
Color surfaceMapGround(texture::Role role);

}  // namespace sigil::material::skia

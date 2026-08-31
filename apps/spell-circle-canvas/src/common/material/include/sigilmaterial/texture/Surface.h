#pragma once

/** @file
 * The two textures a reflective surface is shaded from: an Environment —
 * an equirectangular panorama with the roughness blurs a surface samples
 * by reflection direction — and bevelNormals(), a normal map derived from
 * an outline's coverage. Both encode device-space normals (+y down, +z
 * toward the viewer) and produce Textures a recipe's child slots take.
 */

#include <include/core/SkImage.h>
#include <include/core/SkPath.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <sigilmaterial/texture/Texture.h>

#include <map>
#include <memory>

namespace sigil::material {

/** An equirect environment (u = azimuth, v = 0 at zenith) with cached
 *  roughness blurs. A copyable handle; copies share the blur cache. */
class Environment {
 public:
  Environment() = default;

  /** Neutral photo-studio bake: graded sky, floor bounce, three
   *  softboxes — the default for steel and gold. */
  static Environment studio(int width = 512);
  /** A chrome horizon: banded sky over dark ground with a hot sun
   *  stripe. */
  static Environment sunset(int width = 512);
  /** Wrap a loaded equirect panorama (LDR or F16/F32 HDR image). */
  static Environment fromEquirect(sk_sp<SkImage> image);

  bool valid() const { return m_base != nullptr; }
  /** The environment at @p roughness in [0,1]: 0 = mirror-sharp base,
   *  higher = progressively blurred copies (bucketed, cached). */
  sk_sp<SkImage> image(float roughness = 0) const;
  /** `image(roughness)` as a texture that wraps in azimuth and clamps at
   *  the poles — what a surface recipe's environment slot takes. */
  Texture texture(float roughness = 0) const;
  /** The panorama's pixel size; a body divides its lookup by this to
   *  read the texture in unit uv. */
  SkISize size() const;

 private:
  sk_sp<SkImage> m_base;
  std::shared_ptr<std::map<int, sk_sp<SkImage>>> m_blurs;
};

/** A rounded-bevel normal map derived from a path's coverage, placed so
 *  a shader's device xy reads the normal under it. The map covers
 *  @p bounds (device px); pixels outside the shape encode flat (0,0,1).
 *  @p bevelPx is the shoulder width; @p heightScale steepens the bevel
 *  (1 = bevel as deep as wide). */
Texture bevelNormals(const SkPath& path, SkIRect bounds, float bevelPx,
                     float heightScale = 1);

/** `bevelNormals` over the path's own bounds outset by the bevel, so the
 *  shoulder has room on every side. */
Texture bevelNormals(const SkPath& path, float bevelPx, float heightScale = 1);

}  // namespace sigil::material

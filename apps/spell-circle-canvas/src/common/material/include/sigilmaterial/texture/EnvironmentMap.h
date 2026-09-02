#pragma once

/** @file
 * The environment map: what a surface sees when it looks past the lights.
 * A panorama sampled by a normal for everything that falls on a body from
 * around it, and by a reflected view vector for what the body mirrors.
 *
 * ONE internal form — an equirectangular image, u = azimuth, v = 0 at the
 * zenith. Every source resolves to it while the value is being built: a
 * lat-long panorama, six cube faces, a cube sheet, a procedural bake, or
 * any image a caller produced. A cube source and a lat-long source
 * therefore shade identically once loaded, and a consumer has one
 * sampling path to write rather than one per source.
 *
 * Two derived readings sit beside the panorama, each cached with it:
 * `image(roughness)` is the specular side — nine wrap-aware blurs a
 * reflection picks by how rough the surface is — and `irradiance()` is
 * the diffuse side, the same panorama convolved with a cosine lobe, which
 * is the value a Lambertian body multiplies its albedo by.
 */

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkM44.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <sigilmaterial/texture/Texture.h>

#include <array>
#include <memory>
#include <vector>

namespace sigil::material {

/** The equirect convention every consumer of this value shares:
 *  `u = 0.5 + atan2(d.x, -d.z) / 2pi`, `v = acos(d.y) / pi`. So v = 0 is
 *  the zenith (+y), v = 1 the nadir, and u = 0.5 looks along -z — the
 *  direction a camera with no rotation faces. */
SkV2 equirectUv(SkV3 direction);
/** The inverse of `equirectUv`: the unit direction a panorama texel
 *  stands for. */
SkV3 equirectDirection(SkV2 uv);

/** An equirect panorama with the two prefiltered readings a lit body
 *  needs. A copyable handle; copies share one cache and compare equal, so
 *  a renderer can key a device texture on the value itself. */
class EnvironmentMap {
 public:
  /** How many roughness levels the specular side is prefiltered into.
   *  Level 0 is the panorama itself and level 8 is a mirror-less blur, so
   *  `roughness * 8` picks one. */
  static constexpr int kLevels = 9;

  EnvironmentMap() = default;

  /** Neutral photo-studio bake: graded sky, floor bounce, three
   *  softboxes — the default for steel and gold. */
  static EnvironmentMap studio(int width = 512);
  /** A chrome horizon: banded sky over dark ground with a hot sun
   *  stripe. */
  static EnvironmentMap sunset(int width = 512);
  /** Wrap a loaded equirect panorama (LDR, or F16/F32 with HDR range
   *  intact). This is the primary form a photographed sky arrives in. */
  static EnvironmentMap fromEquirect(sk_sp<SkImage> image);

  /** The six faces of a cube map, in the order every graphics API names
   *  them: +x, -x, +y, -y, +z, -z, each looking outward with +y up. */
  using Faces = std::array<sk_sp<SkImage>, 6>;
  /** Resample six cube faces into one equirect panorama. @p width is the
   *  panorama's width (height is half); 0 asks for four times a face's
   *  edge, which keeps the texel density a face had at the equator. */
  static EnvironmentMap fromFaces(const Faces& faces, int width = 0);

  /** One image holding all six faces, in whichever of the four layouts
   *  its aspect ratio names: a 4:3 horizontal cross, a 3:4 vertical
   *  cross, a 6:1 row or a 1:6 column, each in the +x -x +y -y +z -z
   *  order faces are named in.
   *
   *  A cube map reaches this library as an ordinary image because that is
   *  what the image library decodes: PNG, JPEG, WebP, AVIF, and — where
   *  the OpenImageIO backend is built — EXR, HDR, TIFF and PSD. The
   *  container formats that hold six surfaces and a mip chain in one file,
   *  DDS and KTX, decode nowhere in this tree, so a cube map is unpacked
   *  to a sheet or to six files before it gets here. */
  static EnvironmentMap fromCubeMap(sk_sp<SkImage> sheet);

  bool valid() const { return m_state != nullptr; }
  explicit operator bool() const { return valid(); }

  /** The panorama at @p roughness in [0,1]: 0 is the base image and
   *  higher values are progressively wider wrap-aware blurs, bucketed
   *  into `kLevels` and cached. */
  sk_sp<SkImage> image(float roughness = 0) const;
  /** `image(roughness)` as a texture that repeats in azimuth and clamps
   *  at the poles — the sampling an equirect panorama has to have. */
  Texture texture(float roughness = 0) const;
  /** The panorama's pixel size. */
  SkISize size() const;

  /** The prefiltered chain as a mip pyramid: `kLevels` images, level k
   *  holding the panorama at roughness k/8 at half the previous level's
   *  size. Level 0 is `prefilterSize()` wide. A blurrier level needs
   *  fewer texels, so the chain costs a third more than its base and
   *  a device binds it as one texture whose level a roughness picks. */
  std::vector<sk_sp<SkImage>> chain() const;

  /** The width level 0 of `chain()` is built at. A panorama is often
   *  larger than a reflection can show, and every level above 0 is a blur
   *  of it, so the chain is built at a bounded size rather than at the
   *  source's. */
  int prefilterSize() const;
  /** This map with `prefilterSize()` set to @p width. The panorama is
   *  untouched; only the chain changes. */
  EnvironmentMap withPrefilterSize(int width) const;

  /** This map with everything below the horizon replaced by @p color —
   *  a flat ground, which is what a photographed sky wants when its lower
   *  half is a tripod and a car park. The replacement happens in the
   *  panorama, so the blurs and the irradiance see it too. */
  EnvironmentMap withGround(SkColor4f color) const;

  /** The diffuse side: the panorama convolved with a cosine lobe, 32x16,
   *  sampled by a surface normal. The value a Lambertian body multiplies
   *  its albedo by — for a panorama of one colour it IS that colour. */
  sk_sp<SkImage> irradiance() const;
  /** One colour: the panorama's solid-angle-weighted mean. The flat
   *  fallback wherever a direction is not available. */
  SkColor4f average() const;

  /** Two maps are equal when they are the same panorama — copies of one
   *  value, sharing its cache. Building the same image twice makes two
   *  maps, which is what identity means for a value that carries a bake. */
  bool operator==(const EnvironmentMap& other) const {
    return m_state == other.m_state && m_prefilter == other.m_prefilter;
  }

 private:
  struct State;
  std::shared_ptr<State> m_state;
  int m_prefilter = 0;  ///< 0 asks for the default bound
};

}  // namespace sigil::material

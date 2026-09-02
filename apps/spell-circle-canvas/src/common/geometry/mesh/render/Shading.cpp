/** @file
 * Reading a panorama on the host: one bilinear tap, the level a
 * roughness picks, and the two sides a lit surface asks it for.
 */

#include <sigilgeometry/mesh/render/Shading.h>

#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>

#include <map>
#include <mutex>

namespace sigil::geometry::mesh::render {

namespace {

/** A panorama's pixels, read out once and kept.
 *
 *  A per-vertex shading takes one tap per level per vertex, and reading
 *  a texel out of an SkImage is a pixel-map lock and a colour-space
 *  question every time. The pixels behind an environment do not change
 *  — the value that owns them bakes once — so they are read out on
 *  first use and kept against the image's own address. */
struct Pixels {
  int w = 0;
  int h = 0;
  std::vector<float> px;
};

const Pixels& pixelsOf(const sk_sp<SkImage>& image) {
  static std::mutex lock;
  static std::map<const SkImage*, Pixels> cache;
  static const Pixels empty;
  if (!image) return empty;
  const std::lock_guard<std::mutex> held(lock);
  auto it = cache.find(image.get());
  if (it != cache.end()) return it->second;
  Pixels read;
  read.w = image->width();
  read.h = image->height();
  read.px.assign((size_t)read.w * read.h * 4, 0.0f);
  const SkImageInfo info = SkImageInfo::Make(
      read.w, read.h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  if (!image->readPixels(
          nullptr,
          SkPixmap(info, read.px.data(), (size_t)read.w * 4 * sizeof(float)), 0,
          0))
    read = Pixels{};
  return cache.emplace(image.get(), std::move(read)).first->second;
}

}  // namespace

glm::vec3 samplePanorama(const sk_sp<SkImage>& panorama, glm::vec2 uv) {
  const Pixels& p = pixelsOf(panorama);
  if (p.w <= 0 || p.h <= 0) return {0, 0, 0};
  const float fx = uv.x * (float)p.w - 0.5f;
  const float fy = uv.y * (float)p.h - 0.5f;
  const int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
  const float tx = fx - (float)x0, ty = fy - (float)y0;
  const auto wrap = [&](int x) { return ((x % p.w) + p.w) % p.w; };
  const auto clampY = [&](int y) { return std::clamp(y, 0, p.h - 1); };
  const int xa = wrap(x0), xb = wrap(x0 + 1);
  const int ya = clampY(y0), yb = clampY(y0 + 1);
  const auto at = [&](int x, int y) {
    const float* t = &p.px[((size_t)y * p.w + x) * 4];
    return glm::vec3(t[0], t[1], t[2]);
  };
  const glm::vec3 top = at(xa, ya) * (1 - tx) + at(xb, ya) * tx;
  const glm::vec3 bot = at(xa, yb) * (1 - tx) + at(xb, yb) * tx;
  return top * (1 - ty) + bot * ty;
}

glm::vec3 environmentRadiance(const Environment& environment,
                              glm::vec3 direction, float roughness) {
  if (environment.levels.empty()) return {0, 0, 0};
  const glm::vec2 uv = equirectUv(environment.orientation * direction);
  // The chain was filtered linearly in roughness, so the level is read
  // linearly out of it, and the two levels either side are mixed rather
  // than the nearer one taken — a body whose roughness ramps must not
  // step from one blur to the next.
  const float levels = (float)environment.levels.size();
  const float pick =
      std::clamp(roughness + environment.roughnessBias, 0.0f, 1.0f) *
      (levels - 1.0f);
  const int low = (int)pick;
  const int high = std::min(low + 1, (int)levels - 1);
  const float t = pick - (float)low;
  const glm::vec3 radiance = samplePanorama(environment.levels[low], uv) *
                                 (1.0f - t) +
                             samplePanorama(environment.levels[high], uv) * t;
  return radiance * environment.tint * environment.intensity *
         environment.specular;
}

glm::vec3 environmentIrradiance(const Environment& environment,
                                glm::vec3 normal) {
  if (!environment.irradiance) return {0, 0, 0};
  const glm::vec2 uv = equirectUv(environment.orientation * normal);
  return samplePanorama(environment.irradiance, uv) * environment.tint *
         environment.intensity * environment.diffuse;
}

}  // namespace sigil::geometry::mesh::render

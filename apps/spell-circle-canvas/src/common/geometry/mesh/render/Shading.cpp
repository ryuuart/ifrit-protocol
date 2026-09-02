/** @file
 * Reading a panorama on the host: one bilinear tap, the level a
 * roughness picks, and the two sides a lit surface asks it for.
 */

#include <sigilgeometry/mesh/render/Shading.h>

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>

#include <glm/gtc/matrix_inverse.hpp>

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace sigil::geometry::mesh::render {

namespace {

/** A panorama's pixels, read out once and kept.
 *
 *  A per-vertex shading takes one tap per level per vertex, and reading
 *  a texel out of an SkImage is a pixel-map lock and a colour-space
 *  question every time. The pixels behind an environment do not change
 *  — the value that owns them bakes once — so they are read out on
 *  first use and kept. */
struct Pixels {
  int w = 0;
  int h = 0;
  std::vector<float> px;
};

/** One kept read, with enough of the image beside it to recognise it.
 *
 *  The sk_sp reaching this file is BORROWED: the environment that owns
 *  the panorama can drop its last reference while the read taken from it
 *  stays, so an entry keyed on the image's address would let the
 *  allocator hand that address to the next sky and serve it the previous
 *  one's texels. The key is the image's unique id, which is never
 *  reissued, and the dimensions are re-checked on every hit so that a key
 *  that somehow repeats reads as a miss rather than as a wrong answer. A
 *  failed read is kept too — the dimensions are recorded even when no
 *  texels were — so an unreadable panorama is not re-attempted per
 *  vertex. */
struct Entry {
  int width = 0;
  int height = 0;
  Pixels pixels;
};

/** How many panoramas' texels are held before the map is dropped whole.
 *
 *  Nothing here can see a panorama go away, so an entry lives until this
 *  cap acts, and each one is width x height x four floats. The cap is a
 *  memory ceiling rather than a hit-rate tuning, and the map is cleared
 *  wholesale rather than evicted from: one lit frame reads a diffuse and
 *  a specular chain for at most two crossfading environments, a few
 *  dozen levels, so a clear costs one frame's re-read of the levels
 *  still in use and nothing after that. */
inline constexpr size_t kKeptPanoramas = 64;

const Pixels& pixelsOf(const sk_sp<SkImage>& image) {
  static std::mutex lock;
  static std::unordered_map<uint32_t, Entry> cache;
  static const Pixels empty;
  if (!image) return empty;
  const std::lock_guard<std::mutex> held(lock);
  const uint32_t id = image->uniqueID();
  const auto it = cache.find(id);
  if (it != cache.end() && it->second.width == image->width() &&
      it->second.height == image->height())
    return it->second.pixels;
  if (cache.size() >= kKeptPanoramas) cache.clear();

  Entry entry;
  entry.width = image->width();
  entry.height = image->height();
  Pixels read;
  read.w = entry.width;
  read.h = entry.height;
  read.px.assign((size_t)read.w * read.h * 4, 0.0f);
  const SkImageInfo info = SkImageInfo::Make(
      read.w, read.h, kRGBA_F32_SkColorType, kPremul_SkAlphaType);
  if (!image->readPixels(
          nullptr,
          SkPixmap(info, read.px.data(), (size_t)read.w * 4 * sizeof(float)), 0,
          0))
    read = Pixels{};
  entry.pixels = std::move(read);
  return cache.insert_or_assign(id, std::move(entry)).first->second.pixels;
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

namespace {

/** One chain, at @p uv and the level @p pick names. The chain was
 *  filtered linearly in roughness, so the level is read linearly out of
 *  it and the two either side are mixed rather than the nearer one
 *  taken — a body whose roughness ramps must not step from one blur to
 *  the next. */
glm::vec3 atLevel(const std::vector<sk_sp<SkImage>>& levels, glm::vec2 uv,
                  float pick) {
  if (levels.empty()) return {0, 0, 0};
  const int low = std::clamp((int)pick, 0, (int)levels.size() - 1);
  const int high = std::min(low + 1, (int)levels.size() - 1);
  const float t = std::clamp(pick - (float)low, 0.0f, 1.0f);
  return samplePanorama(levels[low], uv) * (1.0f - t) +
         samplePanorama(levels[high], uv) * t;
}

}  // namespace

glm::vec3 environmentRadiance(const Environment& environment,
                              glm::vec3 direction, float roughness) {
  if (environment.levels.empty()) return {0, 0, 0};
  const glm::vec2 uv = equirectUv(environment.orientation * direction);
  const float pick =
      std::clamp(roughness + environment.roughnessBias, 0.0f, 1.0f) *
      (float)(environment.levels.size() - 1);
  glm::vec3 radiance = atLevel(environment.levels, uv, pick);
  const float fade = std::clamp(environment.crossfade, 0.0f, 1.0f);
  if (fade > 0 && !environment.nextLevels.empty())
    radiance += (atLevel(environment.nextLevels, uv, pick) - radiance) * fade;
  return radiance * environment.tint * environment.intensity *
         environment.specular;
}

glm::vec3 environmentIrradiance(const Environment& environment,
                                glm::vec3 normal) {
  if (!environment.irradiance) return {0, 0, 0};
  const glm::vec2 uv = equirectUv(environment.orientation * normal);
  glm::vec3 received = samplePanorama(environment.irradiance, uv);
  const float fade = std::clamp(environment.crossfade, 0.0f, 1.0f);
  if (fade > 0 && environment.nextIrradiance)
    received +=
        (samplePanorama(environment.nextIrradiance, uv) - received) * fade;
  return received * environment.tint * environment.intensity *
         environment.diffuse;
}

void drawBackdrop(SkCanvas& canvas, const Environment& environment,
                  const glm::mat4& projection, const glm::mat4& viewMatrix,
                  SkSize viewport) {
  if (!environment.valid() || environment.backdrop <= 0) return;
  const int w = (int)viewport.width(), h = (int)viewport.height();
  if (w <= 0 || h <= 0) return;

  // The sky's own strength is not the specular dial, which belongs to
  // what a surface MIRRORS; the panorama is read through the same
  // function either way, so that dial is divided back out here.
  Environment sky = environment;
  sky.specular = 1;
  sky.roughnessBias = 0;
  const glm::mat4 rayMatrix = glm::inverse(projection);
  const glm::mat3 worldFromView =
      glm::transpose(glm::mat3(glm::inverseTranspose(glm::mat3(viewMatrix))));
  const float blur = std::clamp(environment.backdropBlur, 0.0f, 1.0f);

  SkBitmap picture;
  if (!picture.tryAllocPixels(SkImageInfo::MakeN32Premul(w, h))) return;
  for (int y = 0; y < h; ++y) {
    uint32_t* row = picture.getAddr32(0, y);
    // The camera's projection is authored for a CANVAS, whose second
    // axis runs down the picture, so a pixel's row maps straight onto
    // the clip coordinate rather than being turned over on the way.
    const float ndcY = 2.0f * ((float)y + 0.5f) / (float)h - 1.0f;
    for (int x = 0; x < w; ++x) {
      const float ndcX = 2.0f * ((float)x + 0.5f) / (float)w - 1.0f;
      const glm::vec4 onRay = rayMatrix * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
      glm::vec3 view = glm::vec3(onRay / onRay.w);
      // A view-space point in FRONT of the eye has a negative z. The
      // homogeneous divide above can land on a negative w and answer
      // the ray behind the eye instead, which puts the ground where the
      // zenith is and nothing about a smooth sky would show it.
      if (view.z > 0) view = -view;
      const glm::vec3 ray = worldFromView * view;
      // THE SKY ENDS AT THE SAME CURVE A SURFACE DOES, at the same
      // exposure: a panorama holds values far above one, and a backdrop
      // cut off at one would be a flat white disc where a body standing
      // in front of it shows a sun.
      const glm::vec3 radiance =
          toneMap(environmentRadiance(sky, ray, blur) * environment.backdrop,
                  environment.exposure);
      const SkColor4f colour{std::clamp(radiance.x, 0.0f, 1.0f),
                             std::clamp(radiance.y, 0.0f, 1.0f),
                             std::clamp(radiance.z, 0.0f, 1.0f), 1.0f};
      row[x] = colour.toSkColor();
    }
  }
  picture.setImmutable();
  canvas.drawImage(picture.asImage(), 0, 0);
}

}  // namespace sigil::geometry::mesh::render

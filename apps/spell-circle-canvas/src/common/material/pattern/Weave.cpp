/** @file
 * The weave: the sett expanded, the pivots read off it, the interlacing
 * rule, and the cloth as pixels and as a tile.
 */

#include "sigilmaterial/pattern/Weave.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilmaterial/skia/Painted.h>

#include <algorithm>
#include <numeric>

namespace sigil::material::pattern {

namespace {

/** A shade index a run names, clamped into the byte a threadcount holds. */
uint8_t shadeByte(int shade) { return (uint8_t)std::clamp(shade, 0, 255); }

/** Non-negative remainder: a window is read at negative coordinates and
 *  a cloth repeats in both directions. */
int wrap(int value, int period) {
  const int r = value % period;
  return r < 0 ? r + period : r;
}

/** N32 is kRGBA_8888 on one build and kBGRA_8888 on another, and writing
 *  an SkColor straight into getAddr32() silently swaps red and blue on
 *  one of them. */
uint32_t packPixel(const SkBitmap& bitmap, Color color) {
  const SkColor c = SkColor4f{color.r, color.g, color.b, color.a}.toSkColor();
  const uint32_t a = SkColorGetA(c), r = SkColorGetR(c), g = SkColorGetG(c),
                 b = SkColorGetB(c);
  return bitmap.colorType() == kBGRA_8888_SkColorType
             ? (a << 24u) | (r << 16u) | (g << 8u) | b
             : (a << 24u) | (b << 16u) | (g << 8u) | r;
}

int lcmOf(int a, int b) {
  if (a <= 0 || b <= 0) return std::max(1, std::max(a, b));
  return (int)std::lcm((long long)a, (long long)b);
}

}  // namespace

std::vector<uint8_t> threadcount(const std::vector<ThreadRun>& runs,
                                 Symmetry symmetry) {
  std::vector<uint8_t> out;
  const auto push = [&out](const ThreadRun& run) {
    for (int i = 0; i < run.threads; ++i) out.push_back(shadeByte(run.shade));
  };
  for (const ThreadRun& run : runs) push(run);
  if (symmetry == Symmetry::Asymmetric) return out;
  // The half sett, then its mirror image: the reflection axes are the
  // two gaps at the half's ends, so no thread is woven twice at either.
  const size_t half = out.size();
  out.reserve(half * 2);
  for (size_t i = half; i-- > 0;) out.push_back(out[i]);
  return out;
}

std::vector<int> pivots(const std::vector<uint8_t>& threads) {
  const int n = (int)threads.size();
  std::vector<int> out;
  for (int b = 0; b < n; ++b) {
    bool mirrors = true;
    for (int k = 0; k < n / 2 && mirrors; ++k)
      mirrors = threads[(size_t)wrap(b + k, n)] ==
                threads[(size_t)wrap(b - 1 - k, n)];
    if (mirrors) out.push_back(b);
  }
  return out;
}

bool warpUp(Weave weave, int end, int pick) {
  const int period = weave.period();
  if (weave.over >= period) return true;
  if (weave.over <= 0) return false;
  return wrap(end - weave.advance * pick, period) < weave.over;
}

uint8_t Cloth::shadeAt(int end, int pick) const {
  if (warp.empty() || weft.empty()) return 0;
  return warpUp(weave, end, pick) ? warp[(size_t)wrap(end, (int)warp.size())]
                                  : weft[(size_t)wrap(pick, (int)weft.size())];
}

Color Cloth::at(int end, int pick) const {
  if (warp.empty() || weft.empty()) return {0, 0, 0, 0};
  const bool up = warpUp(weave, end, pick);
  const size_t shade = up ? warp[(size_t)wrap(end, (int)warp.size())]
                          : weft[(size_t)wrap(pick, (int)weft.size())];
  if (shade >= shades.size()) return {0, 0, 0, 0};
  Color c = shades[shade];
  if (!up && rib > 0) {
    const float k = 1.0f - rib;
    c = {c.r * k, c.g * k, c.b * k, c.a};
  }
  return c;
}

glm::ivec2 clothRepeat(const Cloth& cloth) {
  if (cloth.warp.empty() || cloth.weft.empty()) return {0, 0};
  const int period = cloth.weave.period();
  const int step =
      cloth.weave.advance == 0
          ? 1
          : period / (int)std::gcd(period, std::abs(cloth.weave.advance));
  return {lcmOf((int)cloth.warp.size(), period),
          lcmOf((int)cloth.weft.size(), step)};
}

namespace {

/** The window as a raster image; null for a cloth with no threads or an
 *  empty window. */
sk_sp<SkImage> wovenImage(const Cloth& cloth, glm::ivec2 origin,
                          glm::ivec2 size) {
  if (cloth.warp.empty() || cloth.weft.empty()) return nullptr;
  if (size.x <= 0 || size.y <= 0) return nullptr;
  SkBitmap bitmap;
  if (!bitmap.tryAllocN32Pixels(size.x, size.y)) return nullptr;
  for (int y = 0; y < size.y; ++y)
    for (int x = 0; x < size.x; ++x)
      *bitmap.getAddr32(x, y) =
          packPixel(bitmap, cloth.at(origin.x + x, origin.y + y));
  bitmap.setImmutable();
  return bitmap.asImage();
}

}  // namespace

Texture clothImage(const Cloth& cloth, glm::ivec2 origin, glm::ivec2 size) {
  sk_sp<SkImage> woven = wovenImage(cloth, origin, size);
  if (!woven) return {};
  return Texture(std::move(woven)).sampling(Sampling::Nearest);
}

Tile clothTile(const Cloth& cloth, float threadPx) {
  const glm::ivec2 repeat = clothRepeat(cloth);
  const float px = std::max(threadPx, 1.0f);
  if (repeat.x <= 0 || repeat.y <= 0) return Tile::of({px, px}, nullptr);
  const sk_sp<SkImage> woven = wovenImage(cloth, {0, 0}, repeat);
  Tile tile = Tile::of(
      {(float)repeat.x * px, (float)repeat.y * px},
      skia::painted([woven](SkCanvas& canvas, SkSize size, uint32_t) {
                 if (!woven) return;
                 canvas.save();
                 canvas.scale(size.width() / (float)woven->width(),
                              size.height() / (float)woven->height());
                 canvas.drawImage(woven, 0, 0,
                                  SkSamplingOptions(SkFilterMode::kNearest));
                 canvas.restore();
               }));
  return tile.sampling(Sampling::Nearest);
}

}  // namespace sigil::material::pattern

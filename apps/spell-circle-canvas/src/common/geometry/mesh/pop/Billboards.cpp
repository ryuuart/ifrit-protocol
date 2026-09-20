/** @file
 * The billboard rasterizer: every point drawn as a camera-facing
 * sprite on an ordinary canvas, sized by its lane and by perspective,
 * painted back to front.
 *
 * A whole cloud goes down as ONE sprite-atlas batch, so the back-to-front
 * order is a property of the vertex list rather than of a sequence of
 * canvas draws, and a dense cloud costs the canvas one draw instead of
 * one per point.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkShader.h>
#include <sigilskia/draw/Direct.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "sigilgeometry/mesh/pop/Points.h"

namespace sigil::geometry::mesh::points {

void drawBillboards(SkCanvas& canvas, const Cloud& cloud,
                    const camera::Camera& camera, SkSize viewport,
                    const BillboardStyle& style) {
  const size_t n = cloud.size();
  if (n == 0) return;
  const glm::mat4 vp = camera.viewProjection(viewport);
  const glm::mat4 view = camera.view();

  struct Splat {
    SkPoint screen;
    float px;  // half-size in pixels
    float depth;
    glm::vec4 tint;
    /** The cell of the sprite this splat draws, in the unit square:
     *  {uOffset, vOffset, uScale, vScale}. The identity window is the
     *  whole image. */
    glm::vec4 window{0, 0, 1, 1};
  };
  std::vector<Splat> splats;
  splats.reserve(n);

  const std::vector<float>* sizeLane =
      style.sizeLane.empty() ? nullptr : cloud.scalarIf(style.sizeLane);
  const std::vector<glm::vec4>* tintLane =
      style.tintLane.empty() ? nullptr : cloud.colorIf(style.tintLane);
  const std::vector<glm::vec4>* textureLane =
      style.textureLane.empty() ? nullptr : cloud.colorIf(style.textureLane);

  // Pixels per world unit at distance d: focal / d * (h/2).
  const float focal = 1.0f / std::tan(camera.fovYDeg * (float)M_PI / 360.0f);
  const float halfH = viewport.height() * 0.5f;

  for (size_t i = 0; i < n; ++i) {
    const glm::vec3& p = cloud.positions[i];
    const glm::vec4 clip = vp * glm::vec4{p, 1};
    if (clip.w <= 1e-4f) continue;
    const glm::vec4 eye = view * glm::vec4{p, 1};
    Splat splat;
    splat.screen = {clip.x / clip.w, clip.y / clip.w};
    splat.depth = eye.z;
    const float size =
        style.size * (sizeLane && i < sizeLane->size() ? (*sizeLane)[i] : 1.0f);
    splat.px = style.perspective ? std::max(0.25f, size * 0.5f * focal * halfH /
                                                       std::max(-eye.z, 1e-3f))
                                 : size * 0.5f;
    glm::vec4 tint = style.tint;
    if (tintLane && i < tintLane->size()) tint *= (*tintLane)[i];
    splat.tint = tint;
    if (textureLane && i < textureLane->size()) {
      const glm::vec4& window = (*textureLane)[i];
      // A window with no extent is not a cell: an atlas operation that never
      // ran, or a lane padded with zeros, would otherwise splat a sliver
      // of one texel across every point.
      if (window.z > 0.0f && window.w > 0.0f) splat.window = window;
    }
    splats.push_back(splat);
  }
  if (style.depthSort)
    std::sort(splats.begin(), splats.end(), [](const Splat& a, const Splat& b) {
      return a.depth < b.depth;  // far first
    });

  // The default sprite: a CPU-baked soft white dot (quadratic falloff).
  static const sk_sp<SkImage> softDot = [] {
    constexpr int kSize = 64;
    SkBitmap bm;
    bm.allocPixels(SkImageInfo::MakeN32Premul(kSize, kSize));
    for (int y = 0; y < kSize; ++y) {
      uint32_t* row = bm.getAddr32(0, y);
      for (int x = 0; x < kSize; ++x) {
        const float dx = ((float)x + 0.5f) / kSize * 2 - 1;
        const float dy = ((float)y + 0.5f) / kSize * 2 - 1;
        const float r = std::sqrt(dx * dx + dy * dy);
        const float a = std::clamp(1.0f - r, 0.0f, 1.0f);  // linear edge...
        const float soft = a * a * (3 - 2 * a);            // ...smoothstepped
        const uint32_t v = (uint32_t)std::lround(soft * 255.0f);
        row[x] = (v << SK_A32_SHIFT) | (v << SK_R32_SHIFT) |
                 (v << SK_G32_SHIFT) | (v << SK_B32_SHIFT);  // premul white
      }
    }
    bm.setImmutable();
    return bm.asImage();
  }();

  const sk_sp<SkImage>& sprite = style.sprite ? style.sprite : softDot;
  const float sheetWidth = (float)sprite->width();
  const float sheetHeight = (float)sprite->height();
  const SkSamplingOptions sampling(SkFilterMode::kLinear,
                                   SkMipmapMode::kLinear);

  // ONE BATCH CARRIES THE WHOLE CLOUD. Every splat of one call shares the
  // sheet and the blend mode, and its atlas cell rides the batch per
  // sprite, so nothing here has to be grouped: the compatible set is the
  // cloud. A backend that fences on a destination-reading draw — which an
  // additive splat is — then fences once for the cloud rather than once
  // per point.
  std::vector<SkRSXform> transforms;
  std::vector<SkRect> cells;
  std::vector<SkColor> tints;
  std::vector<SkSize> sizes;
  transforms.reserve(splats.size());
  cells.reserve(splats.size());
  tints.reserve(splats.size());
  sizes.reserve(splats.size());
  bool tinted = false;
  bool anyCellNotSquare = false;

  for (const Splat& splat : splats) {
    // The window is in the unit square and the sheet is in texels.
    SkRect cell = SkRect::MakeXYWH(
        splat.window.x * sheetWidth, splat.window.y * sheetHeight,
        splat.window.z * sheetWidth, splat.window.w * sheetHeight);
    // One shader samples the WHOLE sheet, so a cell keeps its own texels
    // by pulling its coordinates half a texel in: a linear filter at a
    // cell edge would otherwise reach the neighbouring cell and bleed one
    // sprite into the next. A sheet taken whole has no neighbour to reach.
    // The inset answers the sheet's own texels and so holds while a splat
    // is near its cell's size; a splat minified far below it reads a mip
    // level of the whole sheet, which has averaged across the boundaries
    // already. A cell thinner than the inset is no cell at all, dropped.
    if (splat.window != glm::vec4{0, 0, 1, 1}) cell.inset(0.5f, 0.5f);
    if (cell.isEmpty()) continue;
    if (cell.width() != cell.height()) anyCellNotSquare = true;

    // A splat is a SQUARE of 2 * px whatever its cell's aspect. The
    // uniform scale answers the cell's width; the size lane answers its
    // height, which is the one thing an SkRSXform cannot carry.
    const float scale = 2.0f * splat.px / cell.width();
    transforms.push_back(SkRSXform::MakeFromRadians(
        scale, 0.0f, splat.screen.fX, splat.screen.fY, cell.width() * 0.5f,
        cell.height() * 0.5f));
    cells.push_back(cell);
    sizes.push_back({1.0f, cell.width() / cell.height()});
    // Full-color tint via modulate — works for any sprite (a white
    // sprite tints exactly).
    const SkColor tint =
        SkColor4f{splat.tint.r, splat.tint.g, splat.tint.b, splat.tint.a}
            .toSkColor();
    tinted = tinted || tint != SK_ColorWHITE;
    tints.push_back(tint);
  }
  if (transforms.empty()) return;

  skia::draw::SpriteBatch batch;
  batch.transforms = transforms;
  batch.sourceRectangles = cells;
  // A white tint modulates to identity and a square cell scales
  // uniformly, so either lane is dropped rather than carried when it
  // says nothing.
  if (tinted) batch.colors = tints;
  if (anyCellNotSquare) batch.sizes = sizes;
  skia::draw::drawSpriteAtlas(
      canvas, sprite, batch, sampling,
      style.additive ? SkBlendMode::kPlus : SkBlendMode::kSrcOver);
}

}  // namespace sigil::geometry::mesh::points

#pragma once

/** @file
 * Backend-portable forms of the two SkCanvas draws Graphite DOES NOT
 * IMPLEMENT: in this Skia, graphite::Device overrides drawImageLattice
 * and drawAtlas with empty bodies ("TODO: Implement these using per-edge
 * AA quads…"), so every such draw on a Graphite canvas silently vanishes
 * — found as invisible nine-slice frames and instance stamps in the GPU
 * gallery, pinned by compose_gpu_test's DirectPrimitiveMatrix.
 *
 * On raster canvases these call the native primitives unchanged. On
 * Graphite (canvas.recorder() != null) they decompose:
 *  - lattice → per-cell drawImageRect (NinePatch alternating bands),
 *  - atlas   → one drawVertices quad list sampling the promoted sheet.
 * Raster source images promote through a per-owner cache (Graphite also
 * performs no implicit uploads for direct image use).
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkVertices.h>
#include <include/gpu/graphite/Image.h>
#include <include/gpu/graphite/Recorder.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace sigil::compose::gpuimg {

/** One promoted texture, keyed by (source image, recorder) — hold one per
 *  owning value (a Slice, an Atlas) so re-draws reuse the upload. */
struct Promoted {
  sk_sp<SkImage> image;
  const SkImage *source = nullptr;
  const void *recorder = nullptr;
};

/** @p img ready for Graphite: the cached or freshly promoted texture
 *  (unchanged on raster canvases / already-texture images; falls back to
 *  @p img when promotion fails). */
inline sk_sp<SkImage> ready(Promoted &cache, sk_sp<SkImage> img,
                            SkCanvas &canvas) {
  skgpu::graphite::Recorder *recorder = canvas.recorder();
  if (!recorder || !img || img->isTextureBacked())
    return img;
  if (cache.image && cache.source == img.get() &&
      cache.recorder == recorder)
    return cache.image;
  sk_sp<SkImage> texture = SkImages::TextureFromImage(recorder, img.get(), {});
  if (!texture)
    return img;
  cache.image = texture;
  cache.source = img.get();
  cache.recorder = recorder;
  return texture;
}

namespace detail {

/** NinePatch band edges: divs split [0, srcLen) into alternating
 *  fixed/stretchable intervals starting FIXED. Stretch bands share the
 *  leftover destination space; when the destination is smaller than the
 *  fixed sum, fixed bands scale down proportionally (Skia's rule). */
inline void latticeEdges(const std::vector<int> &divs, float srcLen,
                         float dstLen, std::vector<float> &srcEdges,
                         std::vector<float> &dstEdges) {
  srcEdges.clear();
  dstEdges.clear();
  srcEdges.push_back(0);
  for (int d : divs)
    srcEdges.push_back((float)std::clamp(d, 0, (int)srcLen));
  srcEdges.push_back(srcLen);
  float fixedSum = 0, stretchSum = 0;
  for (size_t i = 0; i + 1 < srcEdges.size(); ++i) {
    const float len = srcEdges[i + 1] - srcEdges[i];
    (i % 2 == 1 ? stretchSum : fixedSum) += len;
  }
  float fixedScale = 1.0f, stretchScale = 0.0f;
  if (dstLen >= fixedSum) {
    stretchScale = stretchSum > 0 ? (dstLen - fixedSum) / stretchSum : 0.0f;
  } else {
    fixedScale = fixedSum > 0 ? dstLen / fixedSum : 0.0f;
  }
  dstEdges.push_back(0);
  float at = 0;
  for (size_t i = 0; i + 1 < srcEdges.size(); ++i) {
    const float len = srcEdges[i + 1] - srcEdges[i];
    at += len * (i % 2 == 1 ? stretchScale : fixedScale);
    dstEdges.push_back(at);
  }
}

} // namespace detail

/** drawImageLattice on every backend (see the file comment). Empty divs
 *  stretch the whole image (plain drawImageRect). */
inline void drawLattice(SkCanvas &canvas, Promoted &cache, sk_sp<SkImage> img,
                        const std::vector<int> &xDivs,
                        const std::vector<int> &yDivs, const SkRect &dst,
                        SkFilterMode filter) {
  if (!img)
    return;
  const SkSamplingOptions sampling(filter);
  if (xDivs.empty() && yDivs.empty()) {
    canvas.drawImageRect(img, dst, sampling);
    return;
  }
  // ALWAYS decomposed — never the native op. Cached subtrees record on a
  // recording canvas, and a recorded native lattice op would still vanish
  // when the picture replays on Graphite; recorded drawImageRects replay
  // fine (the shader path consults the ImageProvider).
  img = ready(cache, std::move(img), canvas);
  std::vector<float> sx, dx, sy, dy;
  detail::latticeEdges(xDivs, (float)img->width(), dst.width(), sx, dx);
  detail::latticeEdges(yDivs, (float)img->height(), dst.height(), sy, dy);
  for (size_t iy = 0; iy + 1 < sy.size(); ++iy)
    for (size_t ix = 0; ix + 1 < sx.size(); ++ix) {
      const SkRect src =
          SkRect::MakeLTRB(sx[ix], sy[iy], sx[ix + 1], sy[iy + 1]);
      const SkRect cell =
          SkRect::MakeLTRB(dst.left() + dx[ix], dst.top() + dy[iy],
                           dst.left() + dx[ix + 1], dst.top() + dy[iy + 1]);
      if (src.isEmpty() || cell.isEmpty())
        continue;
      canvas.drawImageRect(img, src, cell, sampling, nullptr,
                           SkCanvas::kFast_SrcRectConstraint);
    }
}

/** drawAtlas on every backend (see the file comment). @p colors may be
 *  null for the untinted path. */
inline void drawSpriteAtlas(SkCanvas &canvas, Promoted &cache,
                            sk_sp<SkImage> sheet, const SkRSXform *xforms,
                            const SkRect *tex, const SkColor *colors,
                            size_t count, const SkSamplingOptions &sampling) {
  if (!sheet || count == 0)
    return;
  // ALWAYS decomposed (see drawLattice): raster's native drawAtlas lowers
  // to the same vertices internally, and a recorded drawVertices replays
  // on Graphite where a recorded native atlas op would vanish.
  sheet = ready(cache, std::move(sheet), canvas);
  // uint16 indices cap one vertex list at 16383 sprites — chunk above it.
  constexpr size_t kMaxSprites = 16000;
  for (size_t start = 0; count - start > kMaxSprites; start += kMaxSprites)
    drawSpriteAtlas(canvas, cache, sheet, xforms + start, tex + start,
                    colors ? colors + start : nullptr, kMaxSprites, sampling);
  const size_t tail = (count - 1) % kMaxSprites + 1;
  const size_t offset = count - tail;
  xforms += offset;
  tex += offset;
  if (colors)
    colors += offset;
  count = tail;
  // Two triangles per sprite, indexed; positions from RSXform::toQuad.
  static thread_local std::vector<SkPoint> positions;
  static thread_local std::vector<SkPoint> texs;
  static thread_local std::vector<SkColor> vertexColors;
  static thread_local std::vector<uint16_t> indices;
  positions.clear();
  texs.clear();
  vertexColors.clear();
  indices.clear();
  positions.reserve(count * 4);
  texs.reserve(count * 4);
  indices.reserve(count * 6);
  if (colors)
    vertexColors.reserve(count * 4);
  for (size_t i = 0; i < count; ++i) {
    SkPoint quad[4];
    xforms[i].toQuad(tex[i].width(), tex[i].height(), quad);
    const uint16_t base = (uint16_t)positions.size();
    for (int k = 0; k < 4; ++k)
      positions.push_back(quad[k]);
    texs.push_back({tex[i].left(), tex[i].top()});
    texs.push_back({tex[i].right(), tex[i].top()});
    texs.push_back({tex[i].right(), tex[i].bottom()});
    texs.push_back({tex[i].left(), tex[i].bottom()});
    if (colors)
      for (int k = 0; k < 4; ++k)
        vertexColors.push_back(colors[i]);
    const uint16_t quadIndices[6] = {base,          (uint16_t)(base + 1),
                                     (uint16_t)(base + 2), base,
                                     (uint16_t)(base + 2), (uint16_t)(base + 3)};
    indices.insert(indices.end(), quadIndices, quadIndices + 6);
  }
  sk_sp<SkVertices> vertices = SkVertices::MakeCopy(
      SkVertices::kTriangles_VertexMode, (int)positions.size(),
      positions.data(), texs.data(), colors ? vertexColors.data() : nullptr,
      (int)indices.size(), indices.data());
  SkPaint p;
  p.setShader(
      sheet->makeShader(SkTileMode::kClamp, SkTileMode::kClamp, sampling));
  canvas.drawVertices(vertices, SkBlendMode::kModulate, p);
}

} // namespace sigil::compose::gpuimg

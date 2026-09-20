#pragma once

/** @file
 * @ingroup skia-draw
 * workaround: backend-portable forms of the two SkCanvas draws Graphite
 * DOES NOT IMPLEMENT. In this Skia, `graphite::Device` overrides
 * `drawImageLattice` and `drawAtlas` with empty bodies, so every such
 * draw on a Graphite canvas silently vanishes — as invisible nine-slice
 * frames and instance stamps. The forms here decompose on EVERY backend
 * and never call the native op, because a picture recorded on a raster
 * canvas must be able to replay on Graphite.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkSize.h>
#include <include/core/SkVertices.h>
#include <include/gpu/graphite/Image.h>
#include <include/gpu/graphite/ImageProvider.h>
#include <include/gpu/graphite/Recorder.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

/** THE CANVAS DRAWS GRAPHITE DOES NOT IMPLEMENT, put back in a form
 *  every backend performs: a nine-slice lattice as its cells, and an
 *  atlas of instance stamps as one vertex batch. Reach for these
 *  instead of `SkCanvas::drawImageLattice` and `SkCanvas::drawAtlas`
 *  anywhere a picture may be replayed on a device canvas. */
namespace sigil::skia::draw {

/** @p img ready for Graphite: the cached or freshly promoted texture,
 *  unchanged on a raster canvas and on an image already on a texture,
 *  with the recorder's image provider owning reuse. @p required is what
 *  the draw needs of the texture — a mip chain, above all.
 *  @trap A REQUIREMENT IS NEVER WORTH THE DRAW: a format that cannot
 *  carry a generated chain promotes without one, and a failed upload
 *  answers @p img rather than nothing. */
inline sk_sp<SkImage> ready(SkCanvas& canvas, sk_sp<SkImage> img,
                            SkImage::RequiredProperties required = {}) {
  skgpu::graphite::Recorder* recorder = canvas.recorder();
  if (!recorder || !img || img->isTextureBacked()) return img;
  const auto promote = [&](SkImage::RequiredProperties properties) {
    sk_sp<SkImage> texture = recorder->clientImageProvider()->findOrCreate(
        recorder, img.get(), properties);
    if (!texture)
      texture = SkImages::TextureFromImage(recorder, img.get(), properties);
    return texture;
  };
  sk_sp<SkImage> texture = promote(required);
  if (!texture && required.fMipmapped) texture = promote({});
  return texture ? std::move(texture) : std::move(img);
}

namespace detail {

/** NinePatch band edges: @p divisions split [0, @p sourceLength) into
 *  alternating fixed and stretchable intervals starting FIXED, the
 *  stretchable ones sharing what is left of @p destinationLength. An
 *  axis with no divisions is ONE STRETCHABLE BAND. @p density is source
 *  pixels per destination unit for the FIXED bands alone, so a frame
 *  drawn at twice the size it is used at declares 2 and stays sharp. */
inline void latticeEdges(const std::vector<int>& divisions, float sourceLength,
                         float destinationLength,
                         std::vector<float>& sourceEdges,
                         std::vector<float>& destinationEdges,
                         float density = 1.0f) {
  sourceEdges.clear();
  destinationEdges.clear();
  sourceEdges.push_back(0);
  for (int d : divisions)
    sourceEdges.push_back((float)std::clamp(d, 0, (int)sourceLength));
  sourceEdges.push_back(sourceLength);
  if (!(density > 0)) density = 1.0f;
  const bool undivided = divisions.empty();
  const auto stretches = [&](size_t band) {
    return undivided || band % 2 == 1;
  };
  float fixedSum = 0, stretchSum = 0;
  for (size_t i = 0; i + 1 < sourceEdges.size(); ++i) {
    const float len = sourceEdges[i + 1] - sourceEdges[i];
    (stretches(i) ? stretchSum : fixedSum) += len;
  }
  const float fixedDestinationLength = fixedSum / density;
  float fixedScale = 1.0f / density, stretchScale = 0.0f;
  if (destinationLength >= fixedDestinationLength) {
    stretchScale =
        stretchSum > 0
            ? (destinationLength - fixedDestinationLength) / stretchSum
            : 0.0f;
  } else {
    fixedScale = fixedSum > 0 ? destinationLength / fixedSum : 0.0f;
  }
  destinationEdges.push_back(0);
  float at = 0;
  for (size_t i = 0; i + 1 < sourceEdges.size(); ++i) {
    const float len = sourceEdges[i + 1] - sourceEdges[i];
    at += len * (stretches(i) ? stretchScale : fixedScale);
    destinationEdges.push_back(at);
  }
}

}  // namespace detail

/** drawImageLattice on every backend, as per-cell drawImageRect over the
 *  alternating bands. Empty divisions stretch the whole image, which is
 *  a plain drawImageRect. @p density is the source's pixels per
 *  destination unit, and scales the fixed bands alone. */
inline void drawLattice(SkCanvas& canvas, sk_sp<SkImage> img,
                        const std::vector<int>& xDivs,
                        const std::vector<int>& yDivs, const SkRect& dst,
                        SkFilterMode filter, float density = 1.0f) {
  if (!img) return;
  const SkSamplingOptions sampling(filter);
  if (xDivs.empty() && yDivs.empty()) {
    // The same picture the band split would draw — one stretchable band on
    // each axis — as the one draw it is.
    canvas.drawImageRect(img, dst, sampling);
    return;
  }
  // ALWAYS decomposed — never the native op. Cached subtrees record on a
  // recording canvas, and a recorded native lattice op would still vanish
  // when the picture replays on Graphite; recorded drawImageRects replay
  // fine (the shader path consults the ImageProvider).
  img = ready(canvas, std::move(img));
  std::vector<float> sx, dx, sy, dy;
  detail::latticeEdges(xDivs, (float)img->width(), dst.width(), sx, dx,
                       density);
  detail::latticeEdges(yDivs, (float)img->height(), dst.height(), sy, dy,
                       density);
  for (size_t iy = 0; iy + 1 < sy.size(); ++iy)
    for (size_t ix = 0; ix + 1 < sx.size(); ++ix) {
      const SkRect src =
          SkRect::MakeLTRB(sx[ix], sy[iy], sx[ix + 1], sy[iy + 1]);
      const SkRect cell =
          SkRect::MakeLTRB(dst.left() + dx[ix], dst.top() + dy[iy],
                           dst.left() + dx[ix + 1], dst.top() + dy[iy + 1]);
      if (src.isEmpty() || cell.isEmpty()) continue;
      canvas.drawImageRect(img, src, cell, sampling, nullptr,
                           SkCanvas::kFast_SrcRectConstraint);
    }
}

/** THE SPRITES ONE ATLAS DRAW LAYS DOWN, as one value: four lanes held
 *  to one length rather than four arguments and a count the caller keeps
 *  in step. `transforms` and `sourceRectangles` are the draw; `colors`
 *  and `sizes` are optional, and an EMPTY one is untinted or uniformly
 *  scaled. `sizes` is the per-sprite (x, y) MULTIPLIER on the transform's
 *  uniform scale, the lane SkRSXform cannot carry. */
struct SpriteBatch {
  /** Where each sprite lands: rotation, uniform scale and translation. */
  std::span<const SkRSXform> transforms;
  /** Which cell of the sheet each takes, in sheet pixels. */
  std::span<const SkRect> sourceRectangles;
  /** Per-sprite tint, modulated onto the sheet. Empty is untinted. */
  std::span<const SkColor> colors;
  /** Per-sprite non-uniform scale. Empty is uniform. */
  std::span<const SkSize> sizes;

  /** How many sprites the batch draws, which is how many transforms it
   *  carries. */
  size_t size() const { return transforms.size(); }
  /** Whether the batch draws nothing. */
  bool empty() const { return transforms.empty(); }

  /** Do the lanes agree? A required lane shorter than `transforms`, or a
   *  stated optional lane shorter than it, is the one thing four parallel
   *  pointers could not say — and the draw refuses rather than reading
   *  past the end of the short one. */
  bool consistent() const {
    return sourceRectangles.size() >= transforms.size() &&
           (colors.empty() || colors.size() >= transforms.size()) &&
           (sizes.empty() || sizes.size() >= transforms.size());
  }

  /** @p n sprites from @p offset, every stated lane taken along. */
  SpriteBatch slice(size_t offset, size_t n) const {
    SpriteBatch out;
    out.transforms = transforms.subspan(offset, n);
    out.sourceRectangles = sourceRectangles.subspan(offset, n);
    if (!colors.empty()) out.colors = colors.subspan(offset, n);
    if (!sizes.empty()) out.sizes = sizes.subspan(offset, n);
    return out;
  }
};

/** drawAtlas on every backend, as one drawVertices quad list sampling
 *  the promoted sheet. @p blend is how each sprite hits the DESTINATION
 *  — kPlus is the whole colour model of an additive particle system,
 *  which the element's saveLayer would flatten instead.
 *  @silent @p batch's lanes disagree: a short lane is a caller bug, and
 *  the whole batch is refused rather than read past. */
inline void drawSpriteAtlas(SkCanvas& canvas, sk_sp<SkImage> sheet,
                            const SpriteBatch& batch,
                            const SkSamplingOptions& sampling,
                            SkBlendMode blend = SkBlendMode::kSrcOver) {
  if (!sheet || batch.empty() || !batch.consistent()) return;
  // ALWAYS decomposed (see drawLattice): raster's native drawAtlas lowers
  // to the same vertices internally, and a recorded drawVertices replays
  // on Graphite where a recorded native atlas op would vanish.
  sheet = ready(canvas, std::move(sheet),
                {.fMipmapped = !sampling.useCubic &&
                               sampling.mipmap != SkMipmapMode::kNone});
  // uint16 indices cap one vertex list at 16383 sprites — chunk above it.
  constexpr size_t kMaxSprites = 16000;
  size_t count = batch.size();
  for (size_t start = 0; count - start > kMaxSprites; start += kMaxSprites)
    drawSpriteAtlas(canvas, sheet, batch.slice(start, kMaxSprites), sampling,
                    blend);
  const size_t tail = (count - 1) % kMaxSprites + 1;
  const SpriteBatch run = batch.slice(count - tail, tail);
  const SkRSXform* transforms = run.transforms.data();
  const SkRect* sourceRectangles = run.sourceRectangles.data();
  const SkColor* colors = run.colors.empty() ? nullptr : run.colors.data();
  const SkSize* sizes = run.sizes.empty() ? nullptr : run.sizes.data();
  count = tail;
  // Two triangles per sprite, indexed; positions from RSXform::toQuad.
  static thread_local std::vector<SkPoint> positions;
  static thread_local std::vector<SkPoint> textureCoordinates;
  static thread_local std::vector<SkColor> vertexColors;
  static thread_local std::vector<uint16_t> indices;
  positions.clear();
  textureCoordinates.clear();
  vertexColors.clear();
  indices.clear();
  positions.reserve(count * 4);
  textureCoordinates.reserve(count * 4);
  indices.reserve(count * 6);
  if (colors) vertexColors.reserve(count * 4);
  for (size_t i = 0; i < count; ++i) {
    SkPoint quad[4];
    if (sizes) {
      // NON-UNIFORM per-sprite scale. SkRSXform cannot express it — it
      // carries (scos, ssin) and one scale by construction — so the quad
      // is built directly here. Everything downstream is identical, which
      // is the point: the decomposed path already emits quads, so the
      // only thing that was ever missing is this branch.
      const float cos0 = transforms[i].fSCos, sin0 = transforms[i].fSSin;
      const float scale = std::sqrt(cos0 * cos0 + sin0 * sin0);
      const float c = scale > 1e-6f ? cos0 / scale : 1.0f;
      const float s0 = scale > 1e-6f ? sin0 / scale : 0.0f;
      const float hw =
          sourceRectangles[i].width() * 0.5f * scale * sizes[i].width();
      const float hh =
          sourceRectangles[i].height() * 0.5f * scale * sizes[i].height();
      // toQuad anchors at the cell centre, so recover it from the transform's
      // translation the same way RSXform does.
      const float ax = sourceRectangles[i].width() * 0.5f,
                  ay = sourceRectangles[i].height() * 0.5f;
      const float cx = transforms[i].fTx + cos0 * ax - sin0 * ay;
      const float cy = transforms[i].fTy + sin0 * ax + cos0 * ay;
      const SkPoint local[4] = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
      for (int k = 0; k < 4; ++k)
        quad[k] = {cx + local[k].fX * c - local[k].fY * s0,
                   cy + local[k].fX * s0 + local[k].fY * c};
    } else {
      transforms[i].toQuad(sourceRectangles[i].width(),
                           sourceRectangles[i].height(), quad);
    }
    const uint16_t base = (uint16_t)positions.size();
    for (const SkPoint& corner : quad) positions.push_back(corner);
    textureCoordinates.push_back(
        {sourceRectangles[i].left(), sourceRectangles[i].top()});
    textureCoordinates.push_back(
        {sourceRectangles[i].right(), sourceRectangles[i].top()});
    textureCoordinates.push_back(
        {sourceRectangles[i].right(), sourceRectangles[i].bottom()});
    textureCoordinates.push_back(
        {sourceRectangles[i].left(), sourceRectangles[i].bottom()});
    if (colors)
      for (int k = 0; k < 4; ++k) vertexColors.push_back(colors[i]);
    const uint16_t quadIndices[6] = {
        base, (uint16_t)(base + 1), (uint16_t)(base + 2),
        base, (uint16_t)(base + 2), (uint16_t)(base + 3)};
    indices.insert(indices.end(), quadIndices, quadIndices + 6);
  }
  sk_sp<SkVertices> vertices = SkVertices::MakeCopy(
      SkVertices::kTriangles_VertexMode, (int)positions.size(),
      positions.data(), textureCoordinates.data(),
      colors ? vertexColors.data() : nullptr, (int)indices.size(),
      indices.data());
  SkPaint p;
  // WITHOUT this, a sprite narrower than a pixel does not draw at all —
  // it simply covers no sample centre. A corridor of stamps whose sprite
  // width falls with depth loses its far half without antialiasing, BY
  // CONSTRUCTION of the sprite widths. Antialiasing makes a thin sprite
  // contribute its true coverage instead of nothing, which is the only
  // behaviour an accumulation can be built on.
  p.setAntiAlias(true);
  p.setBlendMode(blend);
  p.setShader(
      sheet->makeShader(SkTileMode::kClamp, SkTileMode::kClamp, sampling));
  // kModulate is the VERTEX-COLOR × shader blend (the tint lane); the
  // paint's blend mode is what reaches the destination.
  canvas.drawVertices(vertices, SkBlendMode::kModulate, p);
}

}  // namespace sigil::skia::draw

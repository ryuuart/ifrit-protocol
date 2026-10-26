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
#include <include/core/SkMatrix.h>
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

/** @p image ready for Graphite: the cached or freshly promoted texture,
 *  unchanged on a raster canvas and on an image already on a texture,
 *  with the recorder's image provider owning reuse. @p required is what
 *  the draw needs of the texture — a mip chain, above all.
 *  @trap A REQUIREMENT IS NEVER WORTH THE DRAW: a format that cannot
 *  carry a generated chain promotes without one, and a failed upload
 *  answers @p image rather than nothing. */
inline sk_sp<SkImage> ready(SkCanvas& canvas, sk_sp<SkImage> image,
                            SkImage::RequiredProperties required = {}) {
  skgpu::graphite::Recorder* recorder = canvas.recorder();
  if (!recorder || !image || image->isTextureBacked()) return image;
  const auto promote = [&](SkImage::RequiredProperties properties) {
    sk_sp<SkImage> texture = recorder->clientImageProvider()->findOrCreate(
        recorder, image.get(), properties);
    if (!texture)
      texture = SkImages::TextureFromImage(recorder, image.get(), properties);
    return texture;
  };
  sk_sp<SkImage> texture = promote(required);
  if (!texture && required.fMipmapped) texture = promote({});
  return texture ? std::move(texture) : std::move(image);
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
inline void drawLattice(SkCanvas& canvas, sk_sp<SkImage> image,
                        const std::vector<int>& xDivisions,
                        const std::vector<int>& yDivisions,
                        const SkRect& destination, SkFilterMode filter,
                        float density = 1.0f) {
  if (!image) return;
  const SkSamplingOptions sampling(filter);
  if (xDivisions.empty() && yDivisions.empty()) {
    // The same picture the band split would draw — one stretchable band on
    // each axis — as the one draw it is.
    canvas.drawImageRect(image, destination, sampling);
    return;
  }
  // ALWAYS decomposed — never the native op. Cached subtrees record on a
  // recording canvas, and a recorded native lattice op would still vanish
  // when the picture replays on Graphite; recorded drawImageRects replay
  // fine (the shader path consults the ImageProvider).
  image = ready(canvas, std::move(image));
  std::vector<float> sx, dx, sy, dy;
  detail::latticeEdges(xDivisions, (float)image->width(), destination.width(),
                       sx, dx, density);
  detail::latticeEdges(yDivisions, (float)image->height(), destination.height(),
                       sy, dy, density);
  for (size_t iy = 0; iy + 1 < sy.size(); ++iy)
    for (size_t ix = 0; ix + 1 < sx.size(); ++ix) {
      const SkRect src =
          SkRect::MakeLTRB(sx[ix], sy[iy], sx[ix + 1], sy[iy + 1]);
      const SkRect cell = SkRect::MakeLTRB(
          destination.left() + dx[ix], destination.top() + dy[iy],
          destination.left() + dx[ix + 1], destination.top() + dy[iy + 1]);
      if (src.isEmpty() || cell.isEmpty()) continue;
      canvas.drawImageRect(image, src, cell, sampling, nullptr,
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

  /** @p count sprites from @p offset, every stated lane taken along. */
  SpriteBatch slice(size_t offset, size_t count) const {
    SpriteBatch out;
    out.transforms = transforms.subspan(offset, count);
    out.sourceRectangles = sourceRectangles.subspan(offset, count);
    if (!colors.empty()) out.colors = colors.subspan(offset, count);
    if (!sizes.empty()) out.sizes = sizes.subspan(offset, count);
    return out;
  }
};

/** drawAtlas on every backend, as one drawVertices quad list sampling
 *  the promoted sheet. A sprite narrower than four device pixels carries
 *  its own edge: its coverage falls from full to none across the pixel
 *  either side of its outline, and one narrower than a pixel is spread
 *  over a pixel and faded by the part of it it covers. @p blend is how
 *  each sprite hits the DESTINATION — kPlus is the whole colour model of
 *  an additive particle system, which the element's saveLayer would
 *  flatten instead.
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
  // uint16 indices cap one vertex list at 65535 vertices, and a sprite
  // carrying its own edge takes eight — chunk above that many sprites.
  constexpr size_t kMaxSprites = 8000;
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
  // A NARROW SPRITE CARRIES ITS OWN EDGE. A vertex draw ignores the
  // paint's antialiasing on every backend and lights a pixel whose centre
  // the sprite covers, so a sprite narrower than a pixel lights none and
  // a run of fine stamps breaks into dots, and one a pixel or two wide
  // lights one more or one fewer as it moves, so the run beads. A narrow
  // sprite is drawn instead as an inner quad half a pixel inside its
  // outline and an outer one half a pixel outside it, its alpha falling
  // from the inner to nothing at the outer, which lays exactly its area
  // as ink wherever it falls; one narrower than a pixel is spread over a
  // pixel and faded by the part of it it covers, and so reads its cell
  // inside itself rather than at the cell's rim. From four pixels across,
  // the pixel a sprite gains or loses is a small part of it, and it is
  // drawn as it stands. The width is the device's as the canvas stands
  // when the batch is drawn; across a perspective there is no one width
  // to read, and every sprite is drawn as it stands.
  constexpr float kNarrow = 4.0f;
  const SkMatrix canvasMatrix = canvas.getTotalMatrix();
  const bool measurable = !canvasMatrix.hasPerspective();
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
  vertexColors.reserve(count * 4);
  indices.reserve(count * 6);
  bool edged = false;
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
    const SkRect& cell = sourceRectangles[i];
    const SkColor tint = colors ? colors[i] : SK_ColorWHITE;
    const uint16_t base = (uint16_t)positions.size();
    float across = 0.0f, down = 0.0f;
    if (measurable) {
      const SkPoint corner = canvasMatrix.mapPoint(quad[0]);
      across = SkPoint::Distance(corner, canvasMatrix.mapPoint(quad[1]));
      down = SkPoint::Distance(corner, canvasMatrix.mapPoint(quad[3]));
    }
    if (across > 0.0f && down > 0.0f && (across < kNarrow || down < kNarrow)) {
      edged = true;
      // In the sprite's own units, where a side is one long: how far the
      // inner and the outer quad stand from its centre along each side.
      // A side narrower than a pixel is spread over one first.
      const float spreadAcross = std::max(across, 1.0f);
      const float spreadDown = std::max(down, 1.0f);
      const float inner[2] = {(spreadAcross - 1.0f) / (2.0f * across),
                              (spreadDown - 1.0f) / (2.0f * down)};
      const float outer[2] = {(spreadAcross + 1.0f) / (2.0f * across),
                              (spreadDown + 1.0f) / (2.0f * down)};
      const float fade = (across / spreadAcross) * (down / spreadDown);
      const SkPoint centre = (quad[0] + quad[2]) * 0.5f;
      const SkVector side = quad[1] - quad[0];
      const SkVector up = quad[3] - quad[0];
      const SkPoint cellCentre = {cell.centerX(), cell.centerY()};
      const SkColor full =
          SkColorSetA(tint, (U8CPU)std::lround(SkColorGetA(tint) * fade));
      const SkColor none = SkColorSetA(tint, 0);
      // Inner corners first, then outer, each in the quad's own order.
      const float signs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
      for (const float* extent : {inner, outer})
        for (const auto& sign : signs) {
          const float along = sign[0] * extent[0], over = sign[1] * extent[1];
          positions.push_back(centre + side * along + up * over);
          // The cell is read inside itself: the outer quad takes the
          // cell's own corners.
          textureCoordinates.push_back(
              {cellCentre.fX + std::clamp(along, -0.5f, 0.5f) * cell.width(),
               cellCentre.fY + std::clamp(over, -0.5f, 0.5f) * cell.height()});
          vertexColors.push_back(extent == inner ? full : none);
        }
      const auto quadOf = [&](int a, int b, int c, int d) {
        const uint16_t corner[4] = {(uint16_t)(base + a), (uint16_t)(base + b),
                                    (uint16_t)(base + c), (uint16_t)(base + d)};
        indices.insert(indices.end(), {corner[0], corner[1], corner[2],
                                       corner[0], corner[2], corner[3]});
      };
      quadOf(0, 1, 2, 3);
      for (int k = 0; k < 4; ++k)
        quadOf(4 + k, 4 + (k + 1) % 4, (k + 1) % 4, k);
      continue;
    }
    for (const SkPoint& corner : quad) positions.push_back(corner);
    textureCoordinates.push_back({cell.left(), cell.top()});
    textureCoordinates.push_back({cell.right(), cell.top()});
    textureCoordinates.push_back({cell.right(), cell.bottom()});
    textureCoordinates.push_back({cell.left(), cell.bottom()});
    for (int k = 0; k < 4; ++k) vertexColors.push_back(tint);
    const uint16_t quadIndices[6] = {
        base, (uint16_t)(base + 1), (uint16_t)(base + 2),
        base, (uint16_t)(base + 2), (uint16_t)(base + 3)};
    indices.insert(indices.end(), quadIndices, quadIndices + 6);
  }
  // A batch with neither a tint nor an edge draws its cells as they are.
  const bool tinted = colors || edged;
  sk_sp<SkVertices> vertices = SkVertices::MakeCopy(
      SkVertices::kTriangles_VertexMode, (int)positions.size(),
      positions.data(), textureCoordinates.data(),
      tinted ? vertexColors.data() : nullptr, (int)indices.size(),
      indices.data());
  SkPaint p;
  p.setBlendMode(blend);
  p.setShader(
      sheet->makeShader(SkTileMode::kClamp, SkTileMode::kClamp, sampling));
  // kModulate is the VERTEX-COLOR × shader blend (the tint lane); the
  // paint's blend mode is what reaches the destination.
  canvas.drawVertices(vertices, SkBlendMode::kModulate, p);
}

}  // namespace sigil::skia::draw

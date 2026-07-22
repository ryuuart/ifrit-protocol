#pragma once

/** @file
 * SigilCompose instances — the flyweight repeat layer (REVIEW.md §6.1):
 * a template ATLAS baked once from element trees + a user-owned SoA POOL
 * + ONE drawAtlas stamp per frame. Node-graph nodes, inventory cells,
 * confetti, tick arrays, radial menus — "thousands of things" as a leaf,
 * never N Yoga subtrees.
 *
 * The contract, same discipline as Pattern/Console:
 *  - The ATLAS is a recipe: register cells (element trees at a fixed
 *    logical size), and the sheet bakes ONCE on first stamp, oversampled
 *    so raster textures never magnify. Hold it where you hold assets.
 *  - The POOL is yours: plain struct-of-arrays (position / rotation /
 *    uniform scale / tint / frame). Mutate it directly or copy in from an
 *    EnTT view — the ECS stays on YOUR side of the seam.
 *  - Stamping is `SkCanvas::drawAtlas` + RSXform: rotation + UNIFORM
 *    scale + translation only, by design. Skew or non-uniform cells are
 *    per-node content — use real elements for those.
 *
 * Two modes, matching the kernel's two write paths:
 *  - Mode::Data (default): the element carries the pool's revision;
 *    mutate → touch() → render(). An untouched pool prunes and its
 *    cached picture replays; a touched one repaints once.
 *  - Mode::Live: a Cache::None leaf that reads the pool every frame —
 *    the particle path (mutate from a ticker steppable; nothing else to
 *    declare). Measured at ~3.7 ns/sprite on Graphite (STRESS_TESTS.md).
 *
 * Past kCullThreshold instances, stamping culls against the local clip
 * (data-level, arithmetic) before building the draw arrays — the
 * bookkeeping only pays for itself at scale, so it is gated.
 */

#include "sigilcompose/Compose.h"
#include "sigilcompose/GpuImage.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRSXform.h>
#include <include/core/SkSpan.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace sigil::compose::instancing {

// ---------------------------------------------------------------------------
// Pool — the user-owned SoA store

/** Struct-of-arrays per-instance data. The scene owns and mutates this
 *  (directly, from a ticker steppable, or copied out of an EnTT view);
 *  the element only reads it. After external mutation in Mode::Data,
 *  call touch() so the next render() sees a changed revision. */
class Pool {
public:
  /** Appends one instance; returns its index. */
  size_t add(SkPoint position, int frame = 0, float rotateRadians = 0.0f,
             float scale = 1.0f, SkColor4f tint = {1, 1, 1, 1}) {
    m_positions.push_back(position);
    m_rotations.push_back(rotateRadians);
    m_scales.push_back(scale);
    m_tints.push_back(tint);
    m_frames.push_back(frame);
    ++m_revision;
    return m_positions.size() - 1;
  }
  void clear() {
    m_positions.clear();
    m_rotations.clear();
    m_scales.clear();
    m_tints.clear();
    m_frames.clear();
    ++m_revision;
  }
  void resize(size_t n) {
    m_positions.resize(n, {0, 0});
    m_rotations.resize(n, 0.0f);
    m_scales.resize(n, 1.0f);
    m_tints.resize(n, {1, 1, 1, 1});
    m_frames.resize(n, 0);
    ++m_revision;
  }
  size_t size() const { return m_positions.size(); }

  // Bulk mutation views (the EnTT copy-in path). Mutating through these
  // does NOT bump the revision — touch() when done (Mode::Data only).
  std::span<SkPoint> positions() { return m_positions; }
  std::span<float> rotations() { return m_rotations; }
  std::span<float> scales() { return m_scales; }
  std::span<SkColor4f> tints() { return m_tints; }
  std::span<int> frames() { return m_frames; }

  std::span<const SkPoint> positions() const { return m_positions; }
  std::span<const float> rotations() const { return m_rotations; }
  std::span<const float> scales() const { return m_scales; }
  std::span<const SkColor4f> tints() const { return m_tints; }
  std::span<const int> frames() const { return m_frames; }

  /** Data changed → the next describe carries a new revision, so the
   *  reconciler repaints the leaf exactly once. */
  void touch() { ++m_revision; }
  uint64_t revision() const { return m_revision; }

private:
  std::vector<SkPoint> m_positions;
  std::vector<float> m_rotations;
  std::vector<float> m_scales;
  std::vector<SkColor4f> m_tints;
  std::vector<int> m_frames;
  uint64_t m_revision = 0;
};

// ---------------------------------------------------------------------------
// Atlas — flyweight cells baked once from element trees

/** A sprite sheet of element-tree cells. Cells register up front at a
 *  LOGICAL size; the sheet bakes lazily on first stamp (fonts come from
 *  the paint context), oversampled so stamps at scale ≤ oversample never
 *  magnify raster pixels. Re-registering cells after the bake drops it —
 *  the next stamp re-bakes (Pattern's regeneration discipline). */
class Atlas {
public:
  explicit Atlas(float oversample = 2.0f)
      : m_oversample(std::max(0.5f, oversample)) {}

  /** Registers one cell; returns its frame index for Pool::frames(). The
   *  tree is forced to exactly the logical size (Pattern's tile rule). */
  int cell(Element tree, SkSize logicalSize) {
    tree.width(logicalSize.width()).height(logicalSize.height());
    m_cells.push_back({std::move(tree), logicalSize});
    m_sheet.reset();
    return (int)m_cells.size() - 1;
  }
  int frameCount() const { return (int)m_cells.size(); }
  SkSize frameSize(int frame) const {
    return valid(frame) ? m_cells[(size_t)frame].size : SkSize{0, 0};
  }
  float oversample() const { return m_oversample; }

  /** The baked sheet (null until first ensureBaked with fonts). */
  const sk_sp<SkImage> &image() const { return m_sheet; }
  /** Baked tex rect of @p frame in sheet pixels. */
  SkRect frameTex(int frame) const {
    return valid(frame) && m_sheet ? m_tex[(size_t)frame]
                                   : SkRect::MakeEmpty();
  }

  /** Bakes the sheet if needed. Shelf-packs cells left-to-right, wrapping
   *  at kMaxSheetWidth. Returns false when there is nothing to bake. */
  bool ensureBaked(sigil::weave::FontContext &fonts) {
    if (m_sheet)
      return true;
    if (m_cells.empty())
      return false;
    // Shelf pack in baked pixels.
    m_tex.assign(m_cells.size(), SkRect::MakeEmpty());
    float penX = 0, penY = 0, shelfH = 0, sheetW = 0;
    for (size_t i = 0; i < m_cells.size(); ++i) {
      const float w = m_cells[i].size.width() * m_oversample;
      const float h = m_cells[i].size.height() * m_oversample;
      if (penX > 0 && penX + w > kMaxSheetWidth) {
        penY += shelfH;
        penX = 0;
        shelfH = 0;
      }
      m_tex[i] = SkRect::MakeXYWH(penX, penY, w, h);
      penX += w;
      shelfH = std::max(shelfH, h);
      sheetW = std::max(sheetW, penX);
    }
    const int sheetWi = std::max(1, (int)std::ceil(sheetW));
    const int sheetHi = std::max(1, (int)std::ceil(penY + shelfH));
    sk_sp<SkSurface> surface =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(sheetWi, sheetHi));
    if (!surface)
      return false;
    SkCanvas &canvas = *surface->getCanvas();
    canvas.clear(SK_ColorTRANSPARENT);
    for (size_t i = 0; i < m_cells.size(); ++i) {
      // snapshot() sizes by the ROOT'S CHILDREN, not the root's own dims —
      // the cell tree carries forced dims, so a plain shell adopts them
      // (Pattern's bake rule).
      sk_sp<SkPicture> picture =
          snapshot(box().child(m_cells[i].tree), fonts);
      if (!picture)
        continue;
      canvas.save();
      canvas.translate(m_tex[i].left(), m_tex[i].top());
      canvas.scale(m_oversample, m_oversample);
      canvas.drawPicture(picture);
      canvas.restore();
    }
    m_sheet = surface->makeImageSnapshot();
    return m_sheet != nullptr;
  }

private:
  struct Cell {
    Element tree;
    SkSize size;
  };
  bool valid(int frame) const {
    return frame >= 0 && (size_t)frame < m_cells.size();
  }
  static constexpr float kMaxSheetWidth = 2048.0f;
  float m_oversample;
  std::vector<Cell> m_cells;
  std::vector<SkRect> m_tex; // baked-pixel rects, parallel to m_cells
  sk_sp<SkImage> m_sheet;

public:
  /** drawAtlas takes the sheet DIRECTLY — Graphite hosts must stamp the
   *  promoted texture (see GpuImage.h). detail::stamp uses this. */
  gpuimg::Promoted gpuCache;
};

// ---------------------------------------------------------------------------
// The stamp

namespace detail {

inline constexpr size_t kCullThreshold = 2048;

inline void stamp(SkCanvas &canvas, const PaintContext &ctx, Atlas &atlas,
                  const Pool &pool, SkBlendMode blend) {
  if (!ctx.fonts || pool.size() == 0 || !atlas.ensureBaked(*ctx.fonts))
    return;
  const float inv = 1.0f / atlas.oversample();
  const bool cull = pool.size() >= kCullThreshold;
  const SkRect clip = cull ? canvas.getLocalClipBounds() : SkRect::MakeEmpty();

  // Scratch reuse: Live pools stamp every frame — three fresh vectors per
  // frame is measurable at scale (the ui_particles port's friction report).
  static thread_local std::vector<SkRSXform> xforms;
  static thread_local std::vector<SkRect> tex;
  static thread_local std::vector<SkColor> colors;
  xforms.clear();
  tex.clear();
  colors.clear();
  xforms.reserve(pool.size());
  tex.reserve(pool.size());
  colors.reserve(pool.size());
  bool tinted = false;

  const auto positions = pool.positions();
  const auto rotations = pool.rotations();
  const auto scales = pool.scales();
  const auto tints = pool.tints();
  const auto frames = pool.frames();
  const int frameCount = atlas.frameCount();
  for (size_t i = 0; i < positions.size(); ++i) {
    const int frame = std::clamp(frames[i], 0, frameCount - 1);
    const SkRect cellTex = atlas.frameTex(frame);
    if (cellTex.isEmpty())
      continue;
    const float scale = scales[i] * inv;
    if (cull) {
      const float reach =
          0.5f * SkPoint{cellTex.width(), cellTex.height()}.length() * scale;
      const SkPoint p = positions[i];
      if (p.fX + reach < clip.left() || p.fX - reach > clip.right() ||
          p.fY + reach < clip.top() || p.fY - reach > clip.bottom())
        continue;
    }
    xforms.push_back(SkRSXform::MakeFromRadians(
        scale, rotations[i], positions[i].fX, positions[i].fY,
        cellTex.width() * 0.5f, cellTex.height() * 0.5f));
    tex.push_back(cellTex);
    const SkColor tint = tints[i].toSkColor();
    tinted |= tint != SK_ColorWHITE;
    colors.push_back(tint);
  }
  if (xforms.empty())
    return;
  // All-white tints modulate to identity — skip the colors lane entirely
  // (the untinted path is the common one for UI sprites). drawSpriteAtlas
  // is the backend-portable form: native drawAtlas on raster, decomposed
  // to one drawVertices on Graphite (whose drawAtlas is an empty stub).
  gpuimg::drawSpriteAtlas(canvas, atlas.gpuCache, atlas.image(),
                          xforms.data(), tex.data(),
                          tinted ? colors.data() : nullptr, xforms.size(),
                          SkSamplingOptions(SkFilterMode::kLinear), blend);
}

struct DataProps {
  std::shared_ptr<Atlas> atlas;
  std::shared_ptr<const Pool> pool;
  uint64_t revision = 0;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool operator==(const DataProps &) const = default; // ptr identity + rev
};

} // namespace detail

// ---------------------------------------------------------------------------
// The component

enum class Mode {
  /** Cached: mutate → touch() → render(); untouched pools prune. */
  Data,
  /** Cache::None: the leaf reads the pool every frame (particles —
   *  mutate from a ticker steppable, keep the host redrawing). */
  Live,
};

/** The single-draw stamping leaf. It FILLS ITS PARENT (absolute, inset
 *  0) — wrap it in a sized or positioned box, and pool positions are that
 *  parent's local pixels. (A memo shell cannot carry layout props — the
 *  wrapper IS the placement API.)
 *
 *  @p blend is PER SPRITE, and that distinction is the whole point:
 *  `Element::blend()` on this leaf would flatten the field into a layer
 *  and composite it once, so overlapping particles could never accumulate.
 *  Reeves' 1982 wall of fire — the first particle system — is additive
 *  end to end; its palette is not a palette but an overlap count, red
 *  saturating at 5 particles, green at 20, blue at 111. kSrcOver cannot
 *  express any of that, and until now nothing in the chain from
 *  instances() to drawSpriteAtlas carried a blend mode at all. */
inline Element instances(std::shared_ptr<Atlas> atlas,
                         std::shared_ptr<const Pool> pool,
                         Mode mode = Mode::Data,
                         SkBlendMode blend = SkBlendMode::kSrcOver) {
  if (mode == Mode::Live) {
    return custom([atlas = std::move(atlas), pool = std::move(pool),
                   blend](SkCanvas &canvas, const PaintContext &ctx) {
             detail::stamp(canvas, ctx, *atlas, *pool, blend);
           })
        .absolute()
        .inset(0)
        .cache(Cache::None);
  }
  detail::DataProps props{atlas, pool, pool->revision(), blend};
  return memo(std::move(props), [](const detail::DataProps &p) {
    return custom([atlas = p.atlas, pool = p.pool, blend = p.blend](
                      SkCanvas &canvas, const PaintContext &ctx) {
             detail::stamp(canvas, ctx, *atlas, *pool, blend);
           })
        .absolute()
        .inset(0);
  });
}

// ---------------------------------------------------------------------------
// Placement generators — data-level, O(count) arithmetic, no Yoga

namespace place {

/** Row-major grid of cell-sized slots from @p origin. */
inline void grid(Pool &pool, size_t count, int columns, SkSize cell,
                 SkPoint origin = {0, 0}, SkSize gap = {0, 0}) {
  pool.resize(count);
  auto positions = pool.positions();
  const int cols = std::max(1, columns);
  for (size_t i = 0; i < count; ++i) {
    const int col = (int)(i % (size_t)cols), row = (int)(i / (size_t)cols);
    positions[i] = {origin.fX + cell.width() * 0.5f +
                        (float)col * (cell.width() + gap.width()),
                    origin.fY + cell.height() * 0.5f +
                        (float)row * (cell.height() + gap.height())};
  }
  pool.touch();
}

/** Evenly spaced ring; @p faceOut rotates each instance along its spoke. */
inline void ring(Pool &pool, size_t count, SkPoint center, float radius,
                 float startRadians = 0.0f, bool faceOut = false) {
  pool.resize(count);
  auto positions = pool.positions();
  auto rotations = pool.rotations();
  for (size_t i = 0; i < count; ++i) {
    const float a =
        startRadians + (float)i * 2.0f * (float)M_PI / (float)count;
    positions[i] = {center.fX + std::cos(a) * radius,
                    center.fY + std::sin(a) * radius};
    if (faceOut)
      rotations[i] = a + (float)M_PI / 2.0f;
  }
  pool.touch();
}

/** The skottie Repeater law: per-copy linear translate + linear rotate,
 *  EXPONENTIAL scale (pow(scaleStep, i)), start→end opacity lerp folded
 *  into the tint alpha. */
inline void repeat(Pool &pool, size_t count, SkPoint start, SkPoint translate,
                   float rotateStepRadians = 0.0f, float scaleStep = 1.0f,
                   float opacityFrom = 1.0f, float opacityTo = 1.0f) {
  pool.resize(count);
  auto positions = pool.positions();
  auto rotations = pool.rotations();
  auto scales = pool.scales();
  auto tints = pool.tints();
  for (size_t i = 0; i < count; ++i) {
    const float t = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
    positions[i] = {start.fX + translate.fX * (float)i,
                    start.fY + translate.fY * (float)i};
    rotations[i] = rotateStepRadians * (float)i;
    scales[i] = std::pow(scaleStep, (float)i);
    tints[i].fA = opacityFrom + (opacityTo - opacityFrom) * t;
  }
  pool.touch();
}

} // namespace place

} // namespace sigil::compose::instancing

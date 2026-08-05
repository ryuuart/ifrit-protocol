#pragma once

/** @file
 * SigilCompose instances — thousands of repeated sprites as ONE leaf: an
 * ATLAS baked once from element trees, a user-owned struct-of-arrays POOL,
 * and one atlas draw per frame. Node-graph ports, inventory cells, confetti,
 * tick arrays, radial menus — things that would otherwise be N Yoga
 * subtrees.
 *
 * The three parts:
 *  - The ATLAS is a recipe: register cells (element trees at a fixed logical
 *    size) and the sheet bakes ONCE on first stamp, oversampled so stamps at
 *    scales up to `oversample` never magnify baked pixels. Hold it wherever
 *    you hold assets; it outlives any one describe.
 *  - The POOL is yours: plain parallel arrays (position / rotation / uniform
 *    scale / tint / frame). Mutate it directly, from a ticker, or by copying
 *    out of an ECS — no registry type crosses this seam.
 *  - Stamping is one `gpuimg::drawSpriteAtlas` call with RSXform semantics:
 *    rotation, uniform scale and translation, plus two opt-in lanes —
 *    `sizes()` for per-instance NON-UNIFORM scale and `texWindows()` for a
 *    per-sprite UV window. Skew is not expressible; use real elements.
 *
 * Two modes, matching the library's two write paths:
 *  - Mode::Data (default): the element carries the pool's revision, so the
 *    sequence is mutate → commit() → render(). **Skipping commit() fails
 *    silently**: the description compares equal, the node prunes, and the
 *    cached picture of the old pool replays with no diagnostic.
 *  - Mode::Live: an uncached leaf that reads the pool every frame — the
 *    particle path. Mutate from a ticker and keep the host redrawing;
 *    there is nothing to commit.
 *
 * Past kCullThreshold instances the stamp culls each sprite against the
 * local clip arithmetically before building the draw arrays. The
 * bookkeeping costs more than it saves on small pools, which is why it is
 * gated rather than always on.
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

/** Per-instance data as parallel arrays. The caller owns and mutates this
 *  — directly, from a ticker, or by copying out of an ECS view; the
 *  element only reads it. After mutating in Mode::Data, call commit() so
 *  the next render() sees a changed revision.
 *
 *  **A position is the CELL'S CENTRE, not its top-left.** Transforms
 *  copied in from elsewhere usually carry an origin, and the difference is
 *  half a cell in each axis. */
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
    // The opt-in lanes ride along when present, each at its neutral value
    // (unit scale, whole-cell window, fully opaque). Every has*() test is a
    // length comparison against the position lane, so a lane allowed to lag
    // here would silently switch itself off — or, after a clear() and
    // re-fill, line up again and apply a previous generation's values.
    if (!m_sizes.empty())
      m_sizes.push_back({1.0f, 1.0f});
    if (!m_alphas.empty())
      m_alphas.push_back(1.0f);
    if (!m_texWindows.empty())
      m_texWindows.push_back(SkRect::MakeWH(1.0f, 1.0f));
    ++m_revision;
    return m_positions.size() - 1;
  }
  void clear() {
    m_positions.clear();
    m_rotations.clear();
    m_scales.clear();
    m_tints.clear();
    m_frames.clear();
    m_sizes.clear();
    m_alphas.clear();
    m_texWindows.clear();
    ++m_revision;
  }
  void resize(size_t n) {
    m_positions.resize(n, {0, 0});
    m_rotations.resize(n, 0.0f);
    m_scales.resize(n, 1.0f);
    m_tints.resize(n, {1, 1, 1, 1});
    m_frames.resize(n, 0);
    if (!m_sizes.empty())
      m_sizes.resize(n, {1.0f, 1.0f});
    if (!m_alphas.empty())
      m_alphas.resize(n, 1.0f);
    if (!m_texWindows.empty())
      m_texWindows.resize(n, SkRect::MakeWH(1.0f, 1.0f));
    ++m_revision;
  }
  size_t size() const { return m_positions.size(); }

  // Bulk mutation views, for filling the pool in one pass. Mutating
  // through these does NOT bump the revision — call commit() when done, or
  // a Mode::Data element prunes and replays the previous picture.
  std::span<SkPoint> positions() { return m_positions; }
  std::span<float> rotations() { return m_rotations; }
  std::span<float> scales() { return m_scales; }
  /** Per-instance tint. A tint MULTIPLIES the atlas cell's colours, which
   *  makes it wrong for exact-palette work: a cell painted in a palette's
   *  own near-white and tinted with another near-white lands a couple of
   *  units below both, off the palette, invisibly. For exact palette
   *  swaps bake atlas VARIANTS — one cell per palette — and select by
   *  frame; tint only what may legitimately be scaled. */
  std::span<SkColor4f> tints() { return m_tints; }
  std::span<int> frames() { return m_frames; }
  /** Per-instance UV WINDOW inside the sprite's cell, as fractions of that
   *  cell ({0,0,1,1} is the whole thing). Opt-in like `sizes()`: the lane
   *  does not exist until asked for, and a pool that never asks costs
   *  nothing.
   *
   *  This is what expresses artwork crawling behind a slit, a sprite
   *  scrolling within its own cell, or several variants packed side by
   *  side in one cell and selected by rect rather than by frame index. */
  std::span<SkRect> texWindows() {
    if (m_texWindows.size() != m_positions.size())
      m_texWindows.resize(m_positions.size(), SkRect::MakeWH(1.0f, 1.0f));
    return m_texWindows;
  }
  bool hasTexWindows() const {
    return m_texWindows.size() == m_positions.size();
  }

  /** Per-instance NON-UNIFORM scale, as an (x, y) multiplier on top of
   *  `scales()`. Opt-in like `texWindows()`, and for a reason: an
   *  `SkRSXform` carries a rotation and ONE scale by construction, so a
   *  pool without this lane stamps through the plain atlas path, and a
   *  pool with it takes the wider one. It is what expresses a sprite
   *  stretched along its own motion — a streak whose length tracks speed
   *  while its width does not. */
  std::span<SkSize> sizes() {
    if (m_sizes.size() != m_positions.size())
      m_sizes.resize(m_positions.size(), {1.0f, 1.0f});
    return m_sizes;
  }
  bool hasSizes() const { return m_sizes.size() == m_positions.size(); }

  /** Per-instance OPACITY, opt-in like sizes(), multiplied into the tint's
   *  alpha at stamp time. It composes with the tint rather than replacing
   *  it, so an authored colour stays authored and a fade rewrites one
   *  float per instance instead of four. */
  std::span<float> alphas() {
    if (m_alphas.size() != m_positions.size())
      m_alphas.resize(m_positions.size(), 1.0f);
    return m_alphas;
  }
  bool hasAlphas() const { return m_alphas.size() == m_positions.size(); }
  std::span<const float> alphas() const { return m_alphas; }

  std::span<const SkPoint> positions() const { return m_positions; }
  std::span<const float> rotations() const { return m_rotations; }
  std::span<const float> scales() const { return m_scales; }
  std::span<const SkColor4f> tints() const { return m_tints; }
  std::span<const int> frames() const { return m_frames; }
  std::span<const SkSize> sizes() const { return m_sizes; }
  std::span<const SkRect> texWindows() const { return m_texWindows; }

  /** PUBLISH the bulk edit: the next describe carries a new revision, so
   *  the reconciler repaints the leaf exactly once.
   *
   *  The spans handed out above are a staging area — writing through them
   *  changes nothing the element can see. In Mode::Data this call is what
   *  makes those writes visible, and omitting it is silent: the props
   *  compare equal, the node prunes, and the previous picture replays as
   *  though the pool had not moved. `add()`, `resize()` and `clear()`
   *  publish themselves; only span writes need this. */
  void commit() { ++m_revision; }
  uint64_t revision() const { return m_revision; }

private:
  std::vector<SkPoint> m_positions;
  std::vector<float> m_rotations;
  std::vector<float> m_scales;
  std::vector<SkColor4f> m_tints;
  std::vector<int> m_frames;
  std::vector<SkSize> m_sizes;  // empty unless sizes() was asked for
  std::vector<float> m_alphas;  // empty unless alphas() was asked for
  std::vector<SkRect> m_texWindows; // empty unless texWindows() was asked for
  uint64_t m_revision = 0;
};

// ---------------------------------------------------------------------------
// Atlas — flyweight cells baked once from element trees

/** A sprite sheet of element-tree cells. Cells register up front at a
 *  LOGICAL size; the sheet bakes lazily on the first stamp, which is where
 *  the font context becomes available, and is oversampled so stamps at any
 *  scale up to `oversample` never magnify baked pixels. Registering another
 *  cell after the bake drops the sheet and the next stamp bakes it again,
 *  so cells are cheap to add at setup and expensive to add per frame. */
class Atlas {
public:
  /** How stamps sample the baked sheet. Linear is right for soft sprites
   *  and wrong for a pixel grid — a tilemap, a bitmap font sheet, any
   *  deliberately blocky art — where it softens every edge; pass
   *  kNearest for those. */
  Atlas &filter(SkFilterMode mode) {
    m_filter = mode;
    return *this;
  }
  SkFilterMode filter() const { return m_filter; }

  explicit Atlas(float oversample = 2.0f)
      : m_oversample(std::max(0.5f, oversample)) {}

  /** Registers one cell; returns its frame index for Pool::frames(). The
   *  tree is forced to exactly the logical size — a cell has to be a
   *  known rectangle for the sheet to pack it and for the stamp to place
   *  it, so intrinsic sizing is not an option here. */
  int cell(Element tree, SkSize logicalSize) {
    tree.width(logicalSize.width()).height(logicalSize.height());
    m_cells.push_back({std::move(tree), logicalSize});
    m_sheet.reset();
    return (int)m_cells.size() - 1;
  }

  /** VARIANTS: several BAKES of one recipe. `make(v)` is called for
   *  v ∈ [0, count) and each result is registered as its own frame; the
   *  return is the FIRST frame index, so variant v is frame `first + v` in
   *  Pool::frames().
   *
   *  Reach for this when a variant is a RE-RENDER rather than a transform
   *  of baked pixels — a differently stroked ring, a palette whose channels
   *  do not move together, anything a single tint multiply cannot reach.
   *  When the variants ARE crops of one drawing, pack them into one cell
   *  and select with `Pool::texWindows()` instead: that costs one bake, not
   *  `count` of them. */
  int variants(int count, SkSize logicalSize,
               const std::function<Element(int)> &make) {
    int first = -1;
    for (int v = 0; v < count; ++v) {
      const int idx = cell(make(v), logicalSize);
      if (v == 0)
        first = idx;
    }
    return first;
  }
  /** The general form: each variant brings its own logical size — one
   *  recipe, several geometries. */
  int variants(int count,
               const std::function<std::pair<Element, SkSize>(int)> &make) {
    int first = -1;
    for (int v = 0; v < count; ++v) {
      auto [tree, size] = make(v);
      const int idx = cell(std::move(tree), size);
      if (v == 0)
        first = idx;
    }
    return first;
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
      // snapshot() sizes by the ROOT'S CHILDREN and ignores the root's own
      // dimensions — the cell tree already carries forced dims, so wrapping
      // it in a plain shell gives the picture exactly the cell's size.
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
  SkFilterMode m_filter = SkFilterMode::kLinear;
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
  /** The GPU promotion of the baked sheet. A raster sheet handed straight
   *  to a native atlas draw does not appear on every backend, so the stamp
   *  goes through `gpuimg::` and this cache holds whatever that promotion
   *  produced. Used by detail::stamp; not part of the atlas's identity. */
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

  // Scratch reuse: a Live pool stamps every frame, so these buffers are
  // kept and refilled rather than allocated per stamp. thread_local
  // because a canvas may be painted from any one thread at a time, and
  // sharing one static across threads would interleave the fills.
  static thread_local std::vector<SkRSXform> xforms;
  static thread_local std::vector<SkRect> tex;
  static thread_local std::vector<SkColor> colors;
  static thread_local std::vector<SkSize> sizes;
  xforms.clear();
  tex.clear();
  colors.clear();
  sizes.clear();
  xforms.reserve(pool.size());
  tex.reserve(pool.size());
  colors.reserve(pool.size());
  bool tinted = false;

  const auto positions = pool.positions();
  const auto rotations = pool.rotations();
  const auto scales = pool.scales();
  const auto tints = pool.tints();
  const auto frames = pool.frames();
  const auto poolSizes = pool.sizes(); // empty unless the lane exists
  const auto poolWindows = pool.texWindows();
  const bool windowed = poolWindows.size() == positions.size();
  const bool nonUniform = poolSizes.size() == positions.size();
  if (nonUniform)
    sizes.reserve(pool.size());
  const int frameCount = atlas.frameCount();
  for (size_t i = 0; i < positions.size(); ++i) {
    const int frame = std::clamp(frames[i], 0, frameCount - 1);
    SkRect cellTex = atlas.frameTex(frame);
    if (cellTex.isEmpty())
      continue;
    if (windowed) {
      const SkRect &w = poolWindows[i];
      cellTex = SkRect::MakeXYWH(cellTex.left() + w.left() * cellTex.width(),
                                 cellTex.top() + w.top() * cellTex.height(),
                                 w.width() * cellTex.width(),
                                 w.height() * cellTex.height());
      if (cellTex.isEmpty())
        continue;
    }
    const float scale = scales[i] * inv;
    const SkSize sizeMul =
        nonUniform ? poolSizes[i] : SkSize{1.0f, 1.0f};
    if (cull) {
      const float widest =
          std::max(std::abs(sizeMul.width()), std::abs(sizeMul.height()));
      const float reach = 0.5f *
                          SkPoint{cellTex.width(), cellTex.height()}.length() *
                          scale * std::max(widest, 1.0f);
      const SkPoint p = positions[i];
      if (p.fX + reach < clip.left() || p.fX - reach > clip.right() ||
          p.fY + reach < clip.top() || p.fY - reach > clip.bottom())
        continue;
    }
    xforms.push_back(SkRSXform::MakeFromRadians(
        scale, rotations[i], positions[i].fX, positions[i].fY,
        cellTex.width() * 0.5f, cellTex.height() * 0.5f));
    tex.push_back(cellTex);
    if (nonUniform)
      sizes.push_back(sizeMul);
    SkColor4f t = tints[i];
    if (pool.hasAlphas())
      t.fA *= pool.alphas()[i]; // the fade lane composes with the tint
    const SkColor tint = t.toSkColor();
    tinted |= tint != SK_ColorWHITE;
    colors.push_back(tint);
  }
  if (xforms.empty())
    return;
  // All-white tints modulate to identity, so the colors lane is dropped
  // entirely when nothing is tinted — the common case for UI sprites.
  // drawSpriteAtlas is used rather than a native atlas draw because it
  // decomposes to one drawVertices on every backend: the native call draws
  // nothing on some of them, including when a picture recorded elsewhere
  // replays there.
  gpuimg::drawSpriteAtlas(canvas, atlas.gpuCache, atlas.image(),
                          xforms.data(), tex.data(),
                          tinted ? colors.data() : nullptr, xforms.size(),
                          SkSamplingOptions(atlas.filter()), blend,
                          nonUniform ? sizes.data() : nullptr);
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
  /** Cached: mutate → commit() → render(). A pool mutated without
   *  commit() prunes and replays the old picture. */
  Data,
  /** Uncached: the leaf reads the pool every frame (particles — mutate
   *  from a ticker and keep the host redrawing). */
  Live,
};

/** PICK a stamped instance: the index of the topmost instance whose drawn
 *  quad contains @p point, in the same local pixels the pool's positions
 *  are in, or nullopt.
 *
 *  The library's own `hitTest` cannot see inside this leaf — the whole pool
 *  is one custom draw with no nodes in it — so this is the only way to ask
 *  which sprite is under a point. It inverts exactly what the stamp
 *  applied: position, rotation and scale, the `sizes()` and `texWindows()`
 *  lanes when present, and the frame's logical size. It iterates
 *  topmost-first, since later instances draw over earlier ones.
 *
 *  Alpha does not exempt an instance: a fully faded stamp still picks, the
 *  same way a transparent Element still hit-tests. */
inline std::optional<size_t> pick(const Pool &pool, const Atlas &atlas,
                                  SkPoint point) {
  const auto positions = pool.positions();
  const auto rotations = pool.rotations();
  const auto scales = pool.scales();
  const auto frames = pool.frames();
  const auto sizes = pool.sizes();
  const auto windows = pool.texWindows();
  const bool nonUniform = sizes.size() == positions.size();
  const bool windowed = windows.size() == positions.size();
  const int frameCount = atlas.frameCount();
  if (frameCount == 0)
    return std::nullopt;
  for (size_t n = positions.size(); n-- > 0;) {
    const int frame = std::clamp(frames[n], 0, frameCount - 1);
    SkSize half = atlas.frameSize(frame);
    if (half.isEmpty())
      continue;
    if (windowed)
      half = {half.width() * windows[n].width(),
              half.height() * windows[n].height()};
    const SkSize mul = nonUniform ? sizes[n] : SkSize{1.0f, 1.0f};
    const float sx = scales[n] * mul.width(), sy = scales[n] * mul.height();
    if (sx == 0.0f || sy == 0.0f)
      continue;
    // Inverse of the stamp's RSXform: translate to the anchor (the quad
    // centre), un-rotate, un-scale, test against the half-extents.
    const float dx = point.fX - positions[n].fX;
    const float dy = point.fY - positions[n].fY;
    const float c = std::cos(-rotations[n]), s = std::sin(-rotations[n]);
    const float lx = (dx * c - dy * s) / sx;
    const float ly = (dx * s + dy * c) / sy;
    if (std::abs(lx) <= half.width() * 0.5f &&
        std::abs(ly) <= half.height() * 0.5f)
      return n;
  }
  return std::nullopt;
}

/** The single-draw stamping leaf. It FILLS ITS PARENT (absolute, inset 0),
 *  so wrap it in a sized or positioned box and the pool's positions are
 *  that box's local pixels. The wrapper is the only placement API: this
 *  element cannot carry layout props of its own, because in Mode::Data it
 *  is produced inside a memo.
 *
 *  @p blend is PER SPRITE, and the distinction matters: `Element::blend()`
 *  on this leaf would flatten the whole field into one layer and composite
 *  that once, so overlapping sprites could never accumulate. Additive
 *  particle work depends on the accumulation — brightness there IS the
 *  overlap count — and only a per-sprite mode gives it. */
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
  pool.commit();
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
  pool.commit();
}

/** A repeated copy chain: per-copy LINEAR translate and rotate, and
 *  EXPONENTIAL scale (pow(scaleStep, i)), with an optional start→end
 *  opacity ramp.
 *
 *  Every generator here writes only the lanes its parameters speak to, and
 *  no others. The opacity ramp touches the `alphas()` lane — composing with
 *  an authored tint rather than overwriting it — and only when the two
 *  opacity arguments actually say something; `frame` is written only when
 *  it is non-negative. So a pool filled by hand and then arranged by this
 *  keeps its tints and frames. */
inline void repeat(Pool &pool, size_t count, SkPoint start, SkPoint translate,
                   float rotateStepRadians = 0.0f, float scaleStep = 1.0f,
                   float opacityFrom = 1.0f, float opacityTo = 1.0f,
                   int frame = -1) {
  pool.resize(count);
  auto positions = pool.positions();
  auto rotations = pool.rotations();
  auto scales = pool.scales();
  for (size_t i = 0; i < count; ++i) {
    positions[i] = {start.fX + translate.fX * (float)i,
                    start.fY + translate.fY * (float)i};
    rotations[i] = rotateStepRadians * (float)i;
    scales[i] = std::pow(scaleStep, (float)i);
  }
  if (opacityFrom != 1.0f || opacityTo != 1.0f) {
    auto alphas = pool.alphas();
    for (size_t i = 0; i < count; ++i) {
      const float t = count > 1 ? (float)i / (float)(count - 1) : 0.0f;
      alphas[i] = opacityFrom + (opacityTo - opacityFrom) * t;
    }
  }
  if (frame >= 0) {
    auto frames = pool.frames();
    for (size_t i = 0; i < count; ++i)
      frames[i] = frame;
  }
  pool.commit();
}

} // namespace place

} // namespace sigil::compose::instancing

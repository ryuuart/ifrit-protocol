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
 *  - Stamping is one `skia::draw::drawSpriteAtlas` call with RSXform semantics:
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
 *
 * The arithmetic placers that fill a pool — a grid, a ring, a repeat
 * chain — are the kit's, in <sigilcompose/kit/Placers.h>.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <sigilcompose/Compose.h>
#include <sigilskia/draw/Direct.h>

#include <algorithm>
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
             float scale = 1.0f, SkColor4f tint = {1, 1, 1, 1});
  void clear();
  void resize(size_t n);
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
  std::span<SkRect> texWindows();
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
  std::span<SkSize> sizes();
  bool hasSizes() const { return m_sizes.size() == m_positions.size(); }

  /** Per-instance OPACITY, opt-in like sizes(), multiplied into the tint's
   *  alpha at stamp time. It composes with the tint rather than replacing
   *  it, so an authored colour stays authored and a fade rewrites one
   *  float per instance instead of four. */
  std::span<float> alphas();
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
  std::vector<SkSize> m_sizes;       // empty unless sizes() was asked for
  std::vector<float> m_alphas;       // empty unless alphas() was asked for
  std::vector<SkRect> m_texWindows;  // empty unless texWindows() was asked for
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
  Atlas& filter(SkFilterMode mode) {
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
  int cell(Element tree, SkSize logicalSize);

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
               const std::function<Element(int)>& make);
  /** The general form: each variant brings its own logical size — one
   *  recipe, several geometries. */
  int variants(int count,
               const std::function<std::pair<Element, SkSize>(int)>& make);
  int frameCount() const { return (int)m_cells.size(); }
  SkSize frameSize(int frame) const {
    return valid(frame) ? m_cells[(size_t)frame].size : SkSize{0, 0};
  }
  float oversample() const { return m_oversample; }

  /** The baked sheet (null until first ensureBaked with fonts). */
  const sk_sp<SkImage>& image() const { return m_sheet; }
  /** Baked tex rect of @p frame in sheet pixels. */
  SkRect frameTex(int frame) const {
    return valid(frame) && m_sheet ? m_tex[(size_t)frame] : SkRect::MakeEmpty();
  }

  /** Bakes the sheet if needed. Shelf-packs cells left-to-right, wrapping
   *  at kMaxSheetWidth. Returns false when there is nothing to bake. */
  bool ensureBaked(sigil::weave::FontContext& fonts);

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
  std::vector<SkRect> m_tex;  // baked-pixel rects, parallel to m_cells
  sk_sp<SkImage> m_sheet;

 public:
  /** The GPU promotion of the baked sheet. A raster sheet handed straight
   *  to a native atlas draw does not appear on every backend, so the stamp
   *  goes through `skia::draw::` and this cache holds whatever that promotion
   *  produced. Used by detail::stamp; not part of the atlas's identity. */
  skia::draw::Promoted gpuCache;
};

// ---------------------------------------------------------------------------
// The stamp

namespace detail {

inline constexpr size_t kCullThreshold = 2048;

void stamp(SkCanvas& canvas, const PaintContext& ctx, Atlas& atlas,
           const Pool& pool, SkBlendMode blend);

struct DataProps {
  std::shared_ptr<Atlas> atlas;
  std::shared_ptr<const Pool> pool;
  uint64_t revision = 0;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  bool operator==(const DataProps&) const = default;  // ptr identity + rev
};

}  // namespace detail

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
std::optional<size_t> pick(const Pool& pool, const Atlas& atlas, SkPoint point);

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
Element instances(std::shared_ptr<Atlas> atlas,
                  std::shared_ptr<const Pool> pool, Mode mode = Mode::Data,
                  SkBlendMode blend = SkBlendMode::kSrcOver);

}  // namespace sigil::compose::instancing

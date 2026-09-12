#pragma once

/** @file
 * SigilCompose paint values — Fill, Corners, the PaintContext a paint
 * program is handed, the instance-side StampCache, and the three lines
 * that put SigilMaterial's paint on a node. These are the
 * comparable values the paint stage reads. The recipe-backed material a
 * node wears beside a Fill is SigilMaterial's own
 * `sigil::material::Material`, from <sigilmaterial/core/Material.h>,
 * which the three lines below carry onto a node.
 */

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/core/SkSize.h>
#include <include/core/SkTypes.h>
#include <include/effects/SkGradient.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class SkCanvas;
class SkImageFilter;
class SkRuntimeEffect;

namespace sigil::weave {
class FontContext;
}

namespace sigil::compose {

namespace detail {
struct ElementNode;
}  // namespace detail

// ---------------------------------------------------------------------------
// Paint values

/** A paint slot: nothing, a color, or anything Skia can shade (gradient
 *  helpers live in util, SkSL via SkRuntimeEffect works here). */
struct Fill {
  enum class Kind : uint8_t { None, Color, Shader };

  static Fill color(SkColor4f c) { return {Kind::Color, c, nullptr}; }
  static Fill shader(sk_sp<SkShader> s);
  static Fill none() { return {}; }

  Kind kind = Kind::None;
  SkColor4f colorValue = {0, 0, 0, 0};
  sk_sp<SkShader> shaderValue;

  bool operator==(const Fill& o) const {
    return kind == o.kind && colorValue == o.colorValue &&
           shaderValue == o.shaderValue;
  }
};

// Colour — source palettes arrive as lists of hex integers.

/** `0xRRGGBB` (+ alpha) as an SkColor4f, sRGB byte values divided by 255 —
 *  the spelling a source palette is written in, one hex integer per colour,
 *  each one written where it is used.
 *
 *  Not `rgb`, because `rgb(0xRRGGBB)` reads as "three arguments" when
 *  there is only one. constexpr, so palette constants stay constexpr. The
 *  arithmetic is SigilMaterial's `rgb`, which is where a colour is
 *  defined; this is the spelling and the Skia colour it answers in.
 *
 *  It is the only colour verb here. What a colour BECOMES — a different
 *  alpha, a tone off a base, a lighter edge, a mix — is SigilMaterial's
 *  vocabulary and is spelled from it: `material::skia::withAlpha`,
 *  `scale`, `lighten` and `mixLinear`, over <sigilmaterial/skia/Color.h>. */
constexpr SkColor4f hexColor(uint32_t rrggbb, float a = 1.0f) noexcept {
  return material::skia::toSkColor(material::rgb(rrggbb, a));
}

/** Corner radii, clockwise from top-left. `{r}` rounds all four; the
 *  four-value form dresses each corner independently. For shapes whose
 *  corners aren't box corners (stars, polygons, custom outlines), use
 *  shapes::rounded() around the outline generator instead. */
struct Corners {
  float topLeft = 0.0f, topRight = 0.0f, bottomRight = 0.0f, bottomLeft = 0.0f;

  Corners() = default;
  Corners(float all)  // NOLINT: implicit by design (.corners({8}))
      : topLeft(all), topRight(all), bottomRight(all), bottomLeft(all) {}
  Corners(float tl, float tr, float br, float bl)
      : topLeft(tl), topRight(tr), bottomRight(br), bottomLeft(bl) {}

  bool any() const {
    return topLeft > 0 || topRight > 0 || bottomRight > 0 || bottomLeft > 0;
  }
  bool operator==(const Corners&) const = default;
};

/** WHICH SIDE OF A NODE'S PLANE IS DRAWN when a depth lane has turned it
 *  — `Element::backface`. A node is a plane, and `rotateX` or `rotateY`
 *  past a quarter turn shows the viewer its back: the same paint, mirrored.
 *  `Hidden` draws nothing then, and answers no hit, which is what the two
 *  faces of a flipping card need so the one facing away does not show
 *  through the one facing the viewer. `Visible` is the default and what a
 *  node with no depth lane always is: a plane that has not turned has no
 *  back to hide. The side is decided by the node's whole projection — its
 *  own lanes, every shared space above it and the perspective it is seen
 *  through — never by a 2D mirror, so `scaleX(-1)` stays visible. */
enum class Backface : uint8_t { Visible, Hidden };

/** The one paint-program context: custom leaves (and, in extensions,
 *  decorations and contour walks) all receive this. `elapsedSeconds` is
 *  the Ticker's FrameClock time — pause/time-scale affect it. `fonts`
 *  is the owning composer's FontContext (null only when a decoration
 *  is painted outside a composer) — what element stamps and ad-hoc
 *  SigilWeave drawing inside paint programs lay text out with. */
struct PaintContext {
  SkSize size = SkSize::MakeEmpty();
  SkPath outline;
  /** The CLOSED outline the node's shape encloses — what an Inner- or
   *  Outer-aligned stroke clips against. Empty means `outline` is that
   *  shape, which is the usual case; whatever narrows `outline` to
   *  contours BOUNDING NO AREA puts the outline it cut here, so an
   *  alignment keeps its meaning: the mark is the half of the stroke
   *  inside — or outside — the shape, along the part of the boundary
   *  that is left. Two things narrow it that way: `onEdges`, whose runs
   *  are open, and a span gate, whose revealed run is open until the
   *  reveal is complete. A decoration reads it for the clip, and for
   *  anything it classifies against the shape's own bounds — a bevel
   *  band's facing, say, which a run has no centre to be read against.
   *  What is drawn ALONG the boundary follows `outline`, which is the
   *  part of it that is shown. */
  SkPath silhouette;
  double elapsedSeconds = 0.0;
  float contentScale = 1.0f;
  /** Is the composer's Ticker running anything at all this frame, as
   *  read by a node that REPAINTS this frame (a cached node replays its
   *  recording and keeps its last-read value) — the
   *  WHOLE tree's answer, not this node's. A program that wants cheap
   *  chrome while something moves reads it; nothing in the library does.
   *  False outside a composer (a decoration painted standalone), which is
   *  the honest answer there: there is no ticker to be active. */
  bool animating = false;
  sigil::weave::FontContext* fonts = nullptr;
  /** Paths this node BORROWED from keyed elements in the derive phase, in
   *  its own local space — what `strand::from(key)` reads. Null outside a
   *  composer, or when the node borrowed nothing. Non-owning: valid for
   *  the duration of the paint call only.
   *
   *  A decoration declares what it borrows (see BorrowingDecoration
   *  below) so the element can register the keys without introspecting a
   *  type-erased value; the derive pass then resolves them on the same
   *  flat edge-store walk connectors and flowAround ride. */
  const std::vector<std::pair<std::string, SkPath>>* borrowed = nullptr;

  /** The borrowed path for `key`, or an empty path. */
  SkPath borrowedPath(const std::string& key) const {
    if (borrowed)
      for (const auto& [k, p] : *borrowed)
        if (k == key) return p;
    return SkPath();
  }

  /** The instance's stamp-bake store — null outside a composer (standalone
   *  decoration paints fall back to the brush value's own cache). Mutable
   *  through a const context on purpose: a bake is a cache write, not a
   *  paint output. */
  class StampCache* stamps = nullptr;

  /** The node→composer-root matrix, i.e. the forward accumulation of
   *  paint()'s own transform stack; the hit test walks its inverse. This
   *  is what `Material::worldSpace` anchors against. Identity outside a
   *  composer, which degrades deterministically: a world-space material
   *  resolved standalone anchors node-locally and draws the same picture
   *  as the unflagged one. Layout-derived, like `size` — never part of any
   *  prune signature. */
  SkMatrix toRoot = SkMatrix::I();
  /** The composer root's laid-out size in canvas px — what uResolution
   *  becomes for a world-space material (a canvas-unit ramp spans the
   *  canvas). Empty outside a composer; resolve falls back to `size`. */
  SkSize rootSize = SkSize::MakeEmpty();
};

using PaintProgram = std::function<void(SkCanvas&, const PaintContext&)>;

// ---------------------------------------------------------------------------
// A paint as a node's fill — the adapter between SigilMaterial's Skia
// paint value and the slot the reconciler stores.

/** The frame @p ctx supplies a paint: the node's box, the root's size and
 *  the node→root matrix a world-space paint anchors against, the clock and
 *  the device scale. Outside a composer the matrix is identity and the
 *  root size empty, which degrades a world-space paint to a node-local
 *  one rather than answering wrongly. */
material::skia::PaintFrame frameOf(const PaintContext& ctx);

/** The STATIC collapse a non-live paint stores, so it rides the fill
 *  caching and prune path unchanged. */
Fill toFill(const material::skia::Paint& paint);

/** The current-frame fill: for a live paint, the shader rebuilt from the
 *  bound Outputs and @p ctx; for a static one, exactly `toFill`. What the
 *  painter calls for a live fill. */
Fill resolveFill(const material::skia::Paint& paint, const PaintContext& ctx);

/** The INSTANCE-SIDE bake store for stamped brushes: tile bakes live with
 *  the NODE, not inside the brush value. A brush value constructed fresh
 *  by every describe would otherwise re-rasterize its art each time — the
 *  one place where re-describing costs raster work rather than a diff.
 *  Keeping the bake on the instance means the rebuilt value finds it.
 *
 *  Keyed on the art Element's node WITH A WEAK GUARD, and the guard is
 *  load-bearing: a plain map on the raw pointer would let a freed node's
 *  recycled address silently inherit the wrong art's bake. Locking the
 *  weak handle and comparing identity makes that impossible — a recycled
 *  key fails the check and re-bakes. Entries carry either a picture
 *  (pattern and scatter tiles) or a rastered image plus its logical size;
 *  each consumer reads only its own kind. */
class StampCache {
 public:
  /** One bake. A consumer stores either a recorded picture or a
   *  rastered image with the logical size it was baked at, and reads
   *  back only the kind it wrote. */
  struct Entry {
    sk_sp<SkPicture> picture;
    sk_sp<SkImage> image;
    SkSize artSize{0, 0};
  };
  /** The entry for `key`, or null — never a recycled address's entry. */
  const Entry* get(const std::shared_ptr<const void>& key) const {
    for (const Row& row : m_entries) {
      if (row.address != key.get()) continue;
      if (row.owner.lock() != key)
        return nullptr;  // the address was recycled: not this art's bake
      return &row.entry;
    }
    return nullptr;
  }
  void put(const std::shared_ptr<const void>& key, Entry entry) {
    // At this size a scan beats a hash, which is why the store is a list
    // and this header needs no map. A key already here is replaced in
    // place: re-baking one art must not cost the other bakes their
    // entries.
    for (Row& row : m_entries)
      if (row.address == key.get()) {
        row.owner = key;
        row.entry = std::move(entry);
        return;
      }
    // A node's stamp arts are few; a store that runs past its capacity
    // means keys churn every frame, and keeping stale bakes alive would
    // pin their nodes' memory. Only a new key can push it there.
    if (m_entries.size() >= kCapacity) m_entries.clear();
    m_entries.push_back({key.get(), key, std::move(entry)});
  }

 private:
  /** How many bakes one node may hold at once. */
  static constexpr size_t kCapacity = 16;
  struct Row {
    const void* address = nullptr;
    std::weak_ptr<const void> owner;
    Entry entry;
  };
  std::vector<Row> m_entries;
};

// ---------------------------------------------------------------------------
// Gradient Fills — the flat-value spelling, one line over Fill::shader.

/** Linear gradient Fill — one line over Fill::shader + SkShaders. */
inline Fill linearGradient(SkPoint from, SkPoint to,
                           std::vector<SkColor4f> colors,
                           std::vector<float> stops = {}) {
  SkPoint pts[2] = {from, to};
  return Fill::shader(
      SkShaders::LinearGradient(pts, SkGradient({{colors.data(), colors.size()},
                                                 {stops.data(), stops.size()},
                                                 SkTileMode::kClamp},
                                                {})));
}

inline Fill radialGradient(SkPoint center, float radius,
                           std::vector<SkColor4f> colors,
                           std::vector<float> stops = {}) {
  return Fill::shader(
      SkShaders::RadialGradient(center, radius,
                                SkGradient({{colors.data(), colors.size()},
                                            {stops.data(), stops.size()},
                                            SkTileMode::kClamp},
                                           {})));
}

}  // namespace sigil::compose

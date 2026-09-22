#pragma once

/** @file
 * @ingroup compose-core
 *
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
#include <include/effects/SkGradient.h>
#include <sigilcompose/core/PaintAnchor.h>
#include <sigilcompose/core/Var.h>
#include <sigilcore/callable/Callable.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Backface.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilweave/style/Type.h>

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

class VarTable;

namespace detail {
struct ElementNode;
}  // namespace detail

// ---------------------------------------------------------------------------
// Paint values

/** A paint slot: nothing, a color, or anything Skia can shade (gradient
 *  helpers live in util, SkSL via SkRuntimeEffect works here) — or a
 *  REFERENCE to a colour the tree supplies where the fill is painted: the
 *  ink in force, or a custom property. */
struct Fill {
  /** Which of the three things a fill holds. */
  enum class Kind : uint8_t {
    None,   ///< nothing is painted
    Color,  ///< a single colour, or a reference that resolves to one
    Shader  ///< anything Skia can shade
  };
  /** Where a colour fill READS its colour from when it was written as a
   *  reference rather than a value. `None` is a value. */
  enum class Ref : uint8_t { None, CurrentInk, Var };

  static Fill color(material::Color c) { return {Kind::Color, c, nullptr}; }
  static Fill shader(sk_sp<SkShader> s);
  static Fill none() { return {}; }
  /** THE INK IN FORCE where the fill is painted — CSS's currentColor: the
   *  colour the nearest `Element::ink` set, which is also the colour text
   *  under it is set in. A mark that names no colour at all is already
   *  painted in it; this is the spelling for a slot that demands a fill.
   *  Until a paint context resolves it, it stands as the root's black, so
   *  a consumer that reads the colour with no context in hand draws that
   *  rather than nothing. */
  static Fill currentInk() {
    Fill f{Kind::Color, {0, 0, 0, 1}, nullptr};
    f.ref = Ref::CurrentInk;
    return f;
  }
  /** The colour the custom property @p reference names, as the nearest
   *  ancestor's `Element::var` set it. A name nobody set, or one holding a
   *  length, resolves to no fill and says so once. */
  static Fill var(VarRef reference) {
    Fill f{Kind::Color, {0, 0, 0, 0}, nullptr};
    f.ref = Ref::Var;
    f.varId = reference.id;
    return f;
  }
  static Fill var(std::string_view name) { return var(compose::var(name)); }

  Kind kind = Kind::None;
  material::Color colorValue = {0, 0, 0, 0};
  sk_sp<SkShader> shaderValue;
  Ref ref = Ref::None;
  uint32_t varId = 0;

  /** Whether the colour is a reference the paint context still has to
   *  resolve — see `resolveRef`. */
  [[nodiscard]] bool references() const { return ref != Ref::None; }

  bool operator==(const Fill& o) const {
    return kind == o.kind && colorValue == o.colorValue &&
           shaderValue == o.shaderValue && ref == o.ref && varId == o.varId;
  }
};

// Colour — source palettes arrive as lists of hex integers.

/** `0xRRGGBB` with alpha @p a, as a colour: sRGB byte values divided by
 *  255. The spelling a source palette is written in, one hex integer per
 *  colour. constexpr, so a palette stays a constant.
 *
 *  The only colour verb here. What a colour BECOMES — a different alpha,
 *  a tone off a base, a lighter edge, a mix — is SigilMaterial's
 *  vocabulary: `material::withAlpha`, `scale`, `lighten`, `mixLinear`. */
constexpr material::Color hexColor(uint32_t rrggbb, float a = 1.0f) noexcept {
  return material::rgb(rrggbb, a);
}

/** Corner radii, clockwise from top-left. `{r}` rounds all four; the
 *  four-value form dresses each corner independently. For shapes whose
 *  corners aren't box corners (stars, polygons, custom outlines), use
 *  shapes::rounded() around the outline generator instead. */
struct Corners {
  float topLeft = 0.0f, topRight = 0.0f, bottomRight = 0.0f, bottomLeft = 0.0f;

  Corners() = default;
  Corners(float all)  // NOLINT: implicit by design (.borderRadius({8}))
      : topLeft(all), topRight(all), bottomRight(all), bottomLeft(all) {}
  Corners(float tl, float tr, float br, float bl)
      : topLeft(tl), topRight(tr), bottomRight(br), bottomLeft(bl) {}

  bool any() const {
    return topLeft > 0 || topRight > 0 || bottomRight > 0 || bottomLeft > 0;
  }
  bool operator==(const Corners&) const = default;
};

/** THE KEYS A HOST FEEDS THE COMPOSER — `Composer::setKey` — as a pen
 *  program reads them: whether one is down, the one most recently
 *  pressed by name and code, and every code held. */
struct KeyState {
  bool pressed = false;
  std::string key;
  int keyCode = 0;
  std::vector<int> down;
};

/** WHAT DECIDES A TEXTURE PROMOTION in a composer — never what a
 *  promotion is allowed to do. A paint context carries the policy the
 *  composer painting it runs under, so a program that keeps a composer of
 *  its own runs it under the same rule. */
enum class PromotionPolicy : uint8_t {
  Off,     ///< nothing is promoted, and standing bakes are dropped
  ByCost,  ///< baked after several consecutive expensive frames
  Eager,   ///< every eligible node, from its first frame
};

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
   *  flat edge-store walk connectors and contentFlowAround ride. */
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

  /** THE INK IN FORCE at this node — the colour the nearest
   *  `Element::ink` set, which every mark that names no colour is painted
   *  in and `Fill::currentInk()` resolves to. Black outside a composer,
   *  which is the root's own default. */
  material::Color ink = {0, 0, 0, 1};
  /** THE INK IN FORCE AS A PAINT, where `Element::ink` was given a ramp,
   *  a sprite, a recipe or SkSL rather than a colour. Null is the
   *  ordinary case, where `ink` above is the whole of it; valid for the
   *  duration of the paint call. */
  const material::skia::Paint* inkPaint = nullptr;
  /** THE BOX THAT PAINT IS ANCHORED TO: its extent, and this node's own
   *  space mapped into it. An EMPTY extent is the own-box case — the box
   *  being painted is the box the paint maps onto — and is what a paint
   *  anchored to a declaring box or to the canvas replaces. */
  SkSize inkAnchorSize = SkSize::MakeEmpty();
  SkMatrix inkAnchorToRoot = SkMatrix::I();
  /** THE FONT IN FORCE at this node, every field resolved — what a pen
   *  program hosted here sets its text in, and what a guest tree painted
   *  from it inherits. The initial values outside a composer. */
  sigil::weave::Type font;
  /** The custom properties in force here, or null outside a composer and
   *  where no ancestor set one. */
  const VarTable* vars = nullptr;

  /** WHERE THE POINTER STANDS, in this node's own box, and whether its
   *  button is down — what the host fed `Composer::setPointer`, mapped
   *  through the node's transform. The origin with the button up where
   *  nobody points. */
  struct Pointer {
    SkPoint at = {0, 0};
    bool pressed = false;
  };
  Pointer pointer;
  /** The keys the host fed `Composer::setKey`, or null outside a composer.
   *  Valid for the duration of the paint call. */
  const KeyState* keys = nullptr;
  /** The composer's `setBakeDensity`, device pixels per layout unit, or
   *  zero where none was declared — what a kept canvas is formed no
   *  coarser than. */
  float bakeDensity = 0.0f;
  /** THE PROMOTION POLICY THE PAINTING COMPOSER RUNS UNDER, as it stands
   *  after the host's pin and the backend's default. A program that keeps
   *  a composer of its own — a pen's retained guest — gives it this, so a
   *  deterministic capture reaches every measured decision made under it,
   *  and a run that means to test promotion reaches them too. */
  PromotionPolicy promotion = PromotionPolicy::ByCost;
};

/** A PAINT PROGRAM — a drawing on a canvas, in the frame the context
 *  above describes. Both parameters are offered and a program takes the
 *  ones it reads: `[](SkCanvas& c) {…}` is a paint program, so is
 *  `[] {…}`, and so is `[] {…}`.
 *  Incomparable, like every callable — see `Decoration::operator==` for
 *  what that costs a node that carries one. */
using PaintProgram = core::Callable<void(SkCanvas&, const PaintContext&)>;

/** A fill written as a REFERENCE — the ink in force, or a custom property —
 *  resolved against @p ctx into the colour it names; a fill written as a
 *  value passes through unchanged. A property nobody set, or one that holds
 *  a length, resolves to no fill, and the miss is reported once per name.
 *  Every consumer that reads a `Fill` with a context in hand resolves it
 *  through here first; one that reads the colour without a context sees the
 *  root's black for the ink and nothing for a property. */
[[nodiscard]] Fill resolveRef(const Fill& fill, const PaintContext& ctx);

/** THE INK'S PAINT AS A FILL at the node @p ctx describes. An own-box ink
 *  maps the paint's unit square onto that node's box, which is what every
 *  other paint on a node does; an anchored one maps it onto the box the
 *  context names and hands back this node's slice of it. */
[[nodiscard]] Fill resolveInk(const material::skia::Paint& paint,
                              const PaintContext& ctx);

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

namespace detail {
/** The ramp's colours as the shader builder takes them. A colour is four
 *  straight sRGB floats either side, so this is a copy and nothing more.
 *  Not a name a caller spells: the two gradient lines below are inline,
 *  so it stands in the header and nowhere else. */
inline std::vector<SkColor4f> rampColors(
    const std::vector<material::Color>& colors) {
  std::vector<SkColor4f> out;
  out.reserve(colors.size());
  for (const material::Color& c : colors)
    out.push_back(material::skia::toSkColor(c));
  return out;
}
}  // namespace detail

/** Linear gradient Fill — one line over Fill::shader + SkShaders. */
inline Fill linearGradient(SkPoint from, SkPoint to,
                           std::vector<material::Color> colors,
                           std::vector<float> stops = {}) {
  SkPoint pts[2] = {from, to};
  const std::vector<SkColor4f> ramp = detail::rampColors(colors);
  return Fill::shader(
      SkShaders::LinearGradient(pts, SkGradient({{ramp.data(), ramp.size()},
                                                 {stops.data(), stops.size()},
                                                 SkTileMode::kClamp},
                                                {})));
}

/** A radial ramp out of @p center to @p radius, in the node's local
 *  space. @p stops are positions in [0,1], one per colour; an empty
 *  list spaces them evenly. Clamped past the radius. */
inline Fill radialGradient(SkPoint center, float radius,
                           std::vector<material::Color> colors,
                           std::vector<float> stops = {}) {
  const std::vector<SkColor4f> ramp = detail::rampColors(colors);
  return Fill::shader(
      SkShaders::RadialGradient(center, radius,
                                SkGradient({{ramp.data(), ramp.size()},
                                            {stops.data(), stops.size()},
                                            SkTileMode::kClamp},
                                           {})));
}

}  // namespace sigil::compose

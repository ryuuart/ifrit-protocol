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

#include <glm/vec2.hpp>
#include <sigilgeometry/path/Outline.h>
#include <sigilgeometry/path/Transform.h>
#include <sigilcompose/core/PaintBox.h>
#include <sigilcompose/core/Var.h>
#include <sigilcore/callable/Callable.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Backface.h>
#include <sigilmaterial/core/FrameData.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/paint/Paint.h>
#include <sigilweave/style/Type.h>

#include <algorithm>
#include <concepts>
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


namespace sigil::weave {
class FontContext;
}

namespace sigil::draw {
class Pen;
}

namespace sigil::compose {

class VarTable;

struct Fill;

namespace detail {
struct ElementNode;
/** A fill's material and the executor's lowering of it, held together. */
struct FillMaterial;
/** The paint the executor lowered @p fill's material to — what the
 *  painter draws with; a paint of nothing for a fill that holds none. */
const material::Paint& paintOf(const Fill& fill);
}  // namespace detail

// ---------------------------------------------------------------------------
// Paint values

/** WHAT A SLOT IS PAINTED WITH: nothing, a colour, a `material::Material`
 *  — a gradient, a recipe, layers, an image, a program — or a REFERENCE
 *  to a colour the tree supplies where the fill is painted: the ink in
 *  force, or a custom property. The references are the part only a
 *  cascade can mean; everything else is the material's.
 *
 *  A material converts to a fill implicitly, so a component declares one
 *  `Fill` property and its caller writes whichever it holds; the paint an
 *  executor lowers a material to converts as well, as that material's
 *  base. It compares as its lowered paint does, by recipe: the same
 *  gradient described again is the same fill. */
struct Fill {
  /** Which of the three things a fill holds. */
  enum class Kind : uint8_t {
    None,   ///< nothing is painted
    Color,  ///< a single colour, or a reference that resolves to one
    Paint   ///< a material
  };
  /** Where a colour fill READS its colour from when it was written as a
   *  reference rather than a value. `None` is a value. */
  enum class Ref : uint8_t { None, CurrentInk, Var };

  Fill() = default;
  /** @p paint as a fill: the material whose base it is. A paint of
   *  nothing is no fill; a flat paint stays a paint, which the ink tells
   *  apart from a colour: a colour is the inherited ink lane, and a paint
   *  is a paint. */
  // NOLINTNEXTLINE(google-explicit-constructor)
  Fill(material::Paint paint);
  /** A material recipe, as the paint that wears it. */
  template <class Recipe>
    requires(std::convertible_to<Recipe, material::Material> &&
             !std::same_as<std::decay_t<Recipe>, material::Color>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  Fill(Recipe&& recipe)
      : Fill(fromMaterial(material::Material(std::forward<Recipe>(recipe)))) {}

  /** A material as a fill: a flat colour stays the colour it is, anything
   *  else is the paint it lowers to. Its effects and surface are not part
   *  of a fill; `fill(material)` places them. */
  static Fill fromMaterial(const material::Material& material);

  static Fill color(material::Color c) {
    Fill f;
    f.kind = Kind::Color;
    f.colorValue = c;
    return f;
  }
  static Fill none() { return {}; }
  /** THE INK IN FORCE where the fill is painted — CSS's currentColor: the
   *  colour the nearest `Element::ink` set, which is also the colour text
   *  under it is set in. A mark that names no colour at all is already
   *  painted in it; this is the spelling for a slot that demands a fill.
   *  Until a paint context resolves it, it stands as the root's black, so
   *  a consumer that reads the colour with no context in hand draws that
   *  rather than nothing. */
  static Fill currentInk() {
    Fill f = color({0, 0, 0, 1});
    f.ref = Ref::CurrentInk;
    return f;
  }
  /** The colour the custom property @p reference names, as the nearest
   *  ancestor's `Element::var` set it. A name nobody set, or one holding a
   *  length, resolves to no fill and says so once. */
  static Fill var(VarRef reference) {
    Fill f = color({0, 0, 0, 0});
    f.ref = Ref::Var;
    f.varId = reference.id;
    return f;
  }
  static Fill var(std::string_view name) { return var(compose::var(name)); }

  Kind kind = Kind::None;
  material::Color colorValue = {0, 0, 0, 0};
  Ref ref = Ref::None;
  uint32_t varId = 0;

  /** The material, when `kind` is `Paint`; null otherwise. */
  [[nodiscard]] const material::Material* material() const;

  /** Whether the colour is a reference the paint context still has to
   *  resolve — see `resolveRef`. */
  [[nodiscard]] bool references() const { return ref != Ref::None; }
  /** Does this fill need a frame to paint — a live paint, or one that
   *  reads the box it lands on? A slot that stores a fill without a frame
   *  cannot hold one of these as it stands. */
  [[nodiscard]] bool needsFrame() const;

  /** A paint compares by its recipe, so the same paint described again is
   *  an equal fill. */
  bool operator==(const Fill& o) const;

 private:
  friend const material::Paint& detail::paintOf(const Fill& fill);
  // The material and the paint it lowers to, held once and shared, so a
  // fill costs a node no more than a colour and a pointer however much a
  // material grows.
  std::shared_ptr<const detail::FillMaterial> m_paint;

  /** FIELD PIN. `operator==` above is written by hand, and a fill that
   *  compares equal when it is not lets its node prune and keep painting
   *  the old result. This decomposition stops compiling the moment a
   *  member is added or removed. */
  static void fieldPin(Fill& pinned) {
    auto& [pinnedKind, colour, reference, variable, paint] = pinned;
    static_assert(
        std::tuple_size_v<decltype(std::tie(pinnedKind, colour, reference,
                                            variable, paint))> == 5,
        "Fill gained or lost a member — rule on it in Fill::operator==, "
        "then bump this count.");
  }
};

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
 *  the engine's time — a hold and the speed affect it. `fonts`
 *  is the owning composer's FontContext (null only when a decoration
 *  is painted outside a composer) — what element stamps and ad-hoc
 *  SigilWeave drawing inside paint programs lay text out with. */
struct PaintContext {
  glm::vec2 size{0, 0};
  geometry::path::Outline outline;
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
  geometry::path::Outline silhouette;
  double elapsedSeconds = 0.0;
  float contentScale = 1.0f;
  /** Is the composer's engine running anything at all this frame, as
   *  read by a node that REPAINTS this frame (a cached node replays its
   *  recording and keeps its last-read value) — the
   *  WHOLE tree's answer, not this node's. A program that wants cheap
   *  chrome while something moves reads it; nothing in the library does.
   *  False outside a composer (a decoration painted standalone), which is
   *  the honest answer there: there is no engine to be running. */
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
  const std::vector<std::pair<std::string, geometry::path::Outline>>*
      borrowed = nullptr;

  /** The borrowed outline for `key`, or an empty one. */
  geometry::path::Outline borrowedPath(const std::string& key) const {
    if (borrowed)
      for (const auto& [name, outline] : *borrowed)
        if (name == key) return outline;
    return {};
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
  geometry::path::Transform toRoot;
  /** The composer root's laid-out size in canvas px — what uResolution
   *  becomes for a world-space material (a canvas-unit ramp spans the
   *  canvas). Empty outside a composer; resolve falls back to `size`. */
  glm::vec2 rootSize{0, 0};

  /** THE INK IN FORCE at this node — the colour the nearest
   *  `Element::ink` set, which every mark that names no colour is painted
   *  in and `Fill::currentInk()` resolves to. Black outside a composer,
   *  which is the root's own default. */
  material::Color ink = {0, 0, 0, 1};
  /** THE INK IN FORCE AS A PAINT, where `Element::ink` was given a ramp,
   *  a sprite, a recipe or SkSL rather than a colour. Null is the
   *  ordinary case, where `ink` above is the whole of it; valid for the
   *  duration of the paint call. */
  const material::Paint* inkPaint = nullptr;
  /** THE BOX THAT PAINT IS ANCHORED TO: its extent, and this node's own
   *  space mapped into it. An EMPTY extent is the own-box case — the box
   *  being painted is the box the paint maps onto — and is what a paint
   *  anchored to a declaring box or to the canvas replaces. */
  glm::vec2 inkAnchorSize{0, 0};
  geometry::path::Transform inkAnchorToRoot;
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
    glm::vec2 at{0, 0};
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

/** A PAINT PROGRAM — a drawing made with the draw executor's pen, in the
 *  frame the context above describes. Both parameters are offered and a
 *  program takes the ones it reads: `[](draw::Pen& pen) {…}` is a paint
 *  program, so is `[](draw::Pen& pen, const PaintContext& ctx) {…}`, and
 *  so is `[] {…}`. The pen's verbs are p5's; a program that draws past
 *  them takes the pen's canvas, which is the one door to the renderer
 *  this library opens.
 *  Incomparable, like every callable — see `Decoration::operator==` for
 *  what that costs a node that carries one. */
using PaintProgram =
    core::Callable<void(draw::Pen&, const PaintContext&)>;

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
[[nodiscard]] Fill resolveInk(const material::Paint& paint,
                              const PaintContext& ctx);

// ---------------------------------------------------------------------------
// A paint as a node's fill — the adapter between SigilMaterial's Skia
// paint value and the slot the reconciler stores.

/** The frame @p ctx supplies a paint: the node's box, the root's size and
 *  the node→root matrix a world-space paint anchors against, the clock and
 *  the device scale. Outside a composer the matrix is identity and the
 *  root size empty, which degrades a world-space paint to a node-local
 *  one rather than answering wrongly. */
material::FrameData frameOf(const PaintContext& ctx);

/** The STATIC collapse a non-live paint stores, so it rides the fill
 *  caching and prune path unchanged: a flat paint is its colour, a static
 *  one is itself, and one that needs a frame is no fill. */
Fill toFill(const material::Paint& paint);

/** The current-frame fill: a flat paint as its colour, and any other as a
 *  paint whose shader was built against @p ctx — for a live paint from
 *  the live values, for a geometry-dependent one from the node's box. What
 *  the painter calls for a live fill. */
Fill resolveFill(const material::Paint& paint, const PaintContext& ctx);

/** @p fill AS IT PAINTS at the node @p ctx describes: a reference resolved
 *  to the colour it names, and a paint as `resolveFill` above resolves it.
 *  What a decoration reads a fill it was handed through. */
Fill resolveFill(const Fill& fill, const PaintContext& ctx);

}  // namespace sigil::compose

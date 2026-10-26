#pragma once

/** @file
 * @ingroup compose-brush
 *
 * SigilCompose decoration primitives — the concrete treatments over the
 * kernel's Decoration seam. Each is a thin value struct over machinery the
 * renderer already ships, and there are deliberately few of them:
 *
 *  - PathFormat: format any stroke along the node's outline — width and
 *    ink plus an optional dash pattern, a stamped path repeated along
 *    the contour (vines, chains), or the renderer's own path effect.
 *  - Slice: map an image onto the box through a lattice, of which
 *    nine-slice is the 3×3 case.
 *  - ContourWalk: walk the outline by arc length and run a draw program at
 *    each sample, with the canvas pre-positioned at the sample and +x
 *    along the tangent — the general procedural border.
 *
 * `Wash` and `Border` are built OVER those primitives rather than beside
 * them: they add vocabulary, not a wider seam, and anything they do can be
 * done by hand with the three above.
 *
 * All are DecorationScheme values, attached with `.background()` or
 * `.foreground()`. **Which one you pick is a contract, not a hint.** A
 * background paints BENEATH the node's fill, so an opaque fill covers it
 * completely — a background wash or border on a filled box is invisible.
 * A foreground paints over the fill, the leaf content and the children.
 *
 * A decoration paints from `PaintContext::outline` and knows nothing about
 * the node it belongs to, which is why the same values also work on
 * geometry you built yourself — see `decorations::paintOn`.
 *
 * EVERY INK IS A MATERIAL. A mark that takes a `Fill` takes a material —
 * a gradient, a pattern, a program, or a colour, which converts to one —
 * or a reference to the ink in force; a mark that takes a
 * `material::Material` takes the material alone. A `material::Color`
 * converts to either implicitly where a material is asked for, and
 * `Fill::color` spells it where a fill is.
 */

#include <sigilcompose/brush/Lines.h>  // cornerBrackets, cornerGaps
#include <sigilcompose/core/Shadow.h>
#include <sigilcore/callable/Callable.h>
#include <sigilgeometry/path/Stroke.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/core/Image.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "sigilcompose/Compose.h"

namespace sigil::compose {

/** THE RENDERER'S OWN PATH EFFECT, held opaquely: built and read through
 *  `<sigilcompose/advanced/PathEffect.h>`, which a consumer includes by
 *  name. */
struct PathEffect;

/** Stroke the node's outline, formatted by data: dashes, a stamped
 *  path, or the renderer's own path effect (composable — a dash of a
 *  stamp is one effect chained by hand and handed to `effect`). */
struct PathFormat {
  /** Where the stroke sits relative to the outline (the Photoshop/Figma
   *  stroke-position control). Center straddles it; Inner clips the
   *  stroke inside the shape (borders that never fatten the silhouette);
   *  Outer clips it outside (keylines around a filled shape). Inner and
   *  Outer are meaningful on CLOSED outlines — an open rail has no
   *  inside.
   *
   *  SLICED TO CHOSEN EDGES, an alignment keeps that meaning. `onEdges`
   *  hands down runs that are open, and the clip then stands on the
   *  outline those runs were cut from: an Inner-aligned stroke paints
   *  the half of its width inside the shape ALONG THE CHOSEN EDGES and
   *  nowhere else — the same band a `kit::BevelInner` masked to those
   *  edges with sliced ends draws — and an Outer-aligned one the half
   *  outside. A centred stroke straddles the runs, as it always did.
   *
   *  REVEALED BY A SPAN, so does it. The run a span gate shows is open
   *  the same way until the reveal is complete, and the clip stands on
   *  the whole shape it was cut from: `.stroke(spans::upTo(t), stroke(w,
   *  fill, Align::Inner))` draws that fraction of the same inner band at
   *  every value of `t`, and is the unspanned stroke once `t` is 1. */
  enum class Align : uint8_t { Center, Inner, Outer };

  float width = 1.0f;
  /** THE INK IN FORCE UNLESS ONE IS NAMED — a stroke that says no colour
   *  is painted in the colour the nearest `Element::ink` set, exactly as a
   *  text leaf that names no style is set in it. */
  Fill strokeFill = Fill::currentInk();
  Align align = Align::Center;

  /** Dash on/off intervals in px (empty → solid). */
  std::vector<float> dashIntervals;
  /** How the stroke ends and how its corners meet. The defaults are
   *  butt caps and mitred joins, which end open contours square; line art
   *  built from many short open contours usually wants round for both.
   *  Distinct from `lines::Rail::join`, which shapes that rail's own
   *  OFFSET CURVE rather than this stroke on the node's outline. */
  geometry::path::Cap cap = geometry::path::Cap::Butt;
  geometry::path::Join join = geometry::path::Join::Miter;
  /** Off puts the stroke on whole pixels — the 1 px rule of an interface
   *  that was screen-shot rather than drawn, where a smoothed edge reads
   *  as a blur rather than as a line. `styles::BevelPair`, `Brackets` and
   *  `TickRail` each carry the same switch for the same reason; this is
   *  the general stroke's, so a rule beside a bracket can be as hard as
   *  the bracket is. It costs the smoothing on EVERY mark this format
   *  makes, dashes and stamps included: an axis-aligned rule wants it
   *  off, a diagonal or a curve almost never does. */
  bool antiAlias = true;
  float dashPhase = 0.0f;
  /** Bind the dash phase to a wrapping live value and the dashes MARCH — a
   *  selected route, a live link, a cut line. Like `trimPhase`, it
   *  supersedes the constant and declares the decoration animated, so the
   *  node repaints every frame without needing a re-describe.
   *
   *  An animatable rather than a bare live value, so the arithmetic that
   *  shapes the march — `bind(&secs).source(0, 3).wrap(1)`, a ping-pong,
   *  a wiggle — sits next to the phase it drives instead of in a second
   *  live value somebody has to step. What it cannot carry is its own
   *  TRANSITION: a decoration paints with a PaintContext and no instance,
   *  so there is no held motion for one to run on, and a transitioned
   *  value reads as its target. */
  std::optional<motion::Animatable<float>> dashPhaseBinding;

  /** Stamp this outline repeatedly along the contour (advance px apart),
   *  rotated to follow it — vines, chains, ornament runs. */
  geometry::path::Outline stampPath;
  float stampAdvance = 0.0f;

  /** The renderer's own path effect, from
   *  `<sigilcompose/advanced/PathEffect.h>`; overrides the dash and the
   *  stamp when set. */
  std::shared_ptr<const PathEffect> effect;

  /** Per-DECORATION trim window (fractions of arc length) — one node can
   *  carry a full static band AND a marching sliver as two strokes. Wraps
   *  like `spans::wrap` (seam-crossing windows stitch into one contour).
   *  Bind `trimPhase` to a wrapping live value and THIS stroke marches while
   *  its siblings hold still (declares the decoration animated).
   *
   *  IT COMPOSES WITH THE PASS'S OWN SPAN, which is the part people miss.
   *  A decoration receives the ALREADY-claimed run, so its own window
   *  is a fraction of the revealed part — `trimStart 0.9, trimEnd 1.0` on
   *  a second stroke is a bright sliver riding the head of a self-drawing
   *  line, and needs no second node:
   *
   *      PathFormat head = stroke(6, Fill::color(kBright));
   *      head.trimStart = 0.90f; head.trimEnd = 1.0f;
   *      box().shape(curve)
   *           .stroke(spans::upTo(&growth),
   *                   brush::layers({stroke(3, Fill::color(kBody)),
   *                                  head}));
   *
   *  So the trim window is per DECORATION, not per node: a second element
   *  duplicating the same path is never needed for this. */
  float trimStart = 0.0f, trimEnd = 1.0f;
  float trimOffset = 0.0f;
  /** Replaces `trimOffset` when set, on `dashPhaseBinding`'s terms. */
  std::optional<motion::Animatable<float>> trimPhase;

  /** Structural equality so a static stroked/dashed/stamped border prunes
   *  without memo (a renderer's path effect compares by identity). */
  bool operator==(const PathFormat&) const = default;

  /** How far the stroke's ink stands past the outline, which the
   *  recording cull, a layer and a bake grow by: nothing for an Inner
   *  stroke, which is clipped to the shape; otherwise the band's half
   *  width (its whole width outside), grown at a mitred corner to the
   *  renderer's miter limit and at a square cap to the corner of its
   *  square. */
  float bleed() const;
  /** How wide the MARK is either side of the outline — which is NOT
   *  bleed(): an Inner stroke escapes the shape by nothing while painting
   *  a mark `width` px wide inside it. Anything asking where the mark IS
   *  (a weave's crossing repair) needs this number. */
  float reach() const { return width; }
  /** A bound trim phase, a bound dash phase, or a live stroke material
   *  repaints per frame (declared volatility). */
  bool isRunning() const {
    return (trimPhase && trimPhase->isRunning()) ||
           (dashPhaseBinding && dashPhaseBinding->isRunning()) ||
           (strokeFill.material() && strokeFill.material()->isRunning());
  }
  float phase() const {
    return dashPhaseBinding ? dashPhaseBinding->value() : dashPhase;
  }
  /** Whether the stroke stands within its `bleed()` of the outline it is
   *  handed (OutlineDecoration). It does whenever nothing but the stroke
   *  is laid, since `bleed()` holds its corners and ends; a stamped path
   *  or a path effect lays ink wherever its own path puts it, so either
   *  keeps the node's box unless the stroke is Inner and clipped to the
   *  shape. */
  bool staysOnOutline() const;

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** A solid stroke of the node outline (dash/stamp via PathFormat).
 *  `align` positions it: Center (default) straddles the outline, Inner
 *  keeps it inside the silhouette, Outer outside (the keyline). */
inline PathFormat stroke(float width, Fill fill,
                         PathFormat::Align align = PathFormat::Align::Center) {
  PathFormat f;
  f.width = width;
  f.strokeFill = std::move(fill);
  f.align = align;
  return f;
}
/** A solid stroke IN THE INK IN FORCE — no colour named, so the nearest
 *  `Element::ink` paints it, and a recoloured ancestor recolours it. */
inline PathFormat stroke(float width,
                         PathFormat::Align align = PathFormat::Align::Center) {
  return stroke(width, Fill::currentInk(), align);
}

/** Image-onto-box through a lattice (per-cell stretch); nine-slice is
 *  xDivs/yDivs of size 2. Empty divs stretch the whole image.
 *
 *  Laid against the node's BOX, not the outline it is handed, so it does
 *  not declare `staysOnOutline()` and a shaped node wearing it keeps its
 *  box. */
struct Slice {
  std::shared_ptr<const sigil::media::Image> asset;
  std::vector<int> xDivs;
  std::vector<int> yDivs;
  /** How the slices sample. Linear is right for a soft frame and wrong
   *  for pixel art — a window chrome, a dialog border, a button cut from a
   *  tile sheet — where it blurs every slice boundary. */
  material::Sampling filter = material::Sampling::Linear;
  /** Source pixels per layout unit in the CORNER and EDGE bands. 1 draws the
   *  bands at their pixel count, which is right for a texture authored at the
   *  size it is used. A frame generated oversized so it stays sharp on a
   *  high-density device declares that factor here — 2 for a texture drawn at
   *  twice its on-page size — and its corners land at the width they were
   *  designed for instead of twice it. The stretchable bands absorb the rest
   *  either way, so this changes the frame's weight and nothing else. */
  float density = 1.0f;

  /** Structural equality (asset by pointer identity) so a static nine-slice
   *  frame prunes without memo. */
  bool operator==(const Slice& other) const {
    return asset == other.asset && xDivs == other.xDivs &&
           yDivs == other.yDivs && filter == other.filter &&
           density == other.density;
  }

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** One arc-length sample along the outline. */
struct PathSample {
  glm::vec2 position{0, 0};
  glm::vec2 tangent{1, 0};
  float distance = 0.0f;
  float fraction = 0.0f;  ///< 0..1 within its contour
};

/** Walk the outline at `spacing` px intervals; at every sample the
 *  canvas is translated to the sample and rotated so +x follows the
 *  tangent. The general procedural border. Its bodies state no extent
 *  of their own, so the walk states it for them in `bleedPx`, as a
 *  scatter does: a walk that declares one stands on the outline within
 *  it, and one that declares none keeps the node's box and reaches no
 *  further than it. Three bodies, composable:
 *
 *  - `draw`: a raw program per sample (per-step images, a shader, nested
 *    drawing). Set `animatedWalk` when it depends on
 *    ctx.elapsedSeconds (declared volatility).
 *  - `stamp`: a full element subtree — laid out and recorded ONCE via
 *    snapshot() (at intrinsic size, its own decorations and all), then
 *    replayed per sample centered on the contour. Recursion is closed:
 *    the stamp's decorations may walk their own contours. With
 *    `animatedWalk` the stamp re-records each paint, sampling any
 *    live values at their current values.
 *  - `stampAt`: the SEQUENCE form of `stamp` — see its own note.
 *
 *  When several are set, the sample's stamp replays first (`stampAt`'s
 *  element superseding `stamp` at that sample), then `draw` on top. */
struct ContourWalk {
  float spacing = 16.0f;
  core::Callable<void(draw::Pen&, const PathSample&, const PaintContext&)> draw;
  bool animatedWalk = false;
  /** How far a sample's ink stands from the outline: half the extent of
   *  what a body draws about its sample, measured from the path
   *  outwards. The CULL's number, which a layer and a bake grow by; a
   *  stamp wider than twice it is cut wherever the node is held. Zero
   *  declares nothing, and the walk keeps the node's box. */
  float bleedPx = 0.0f;

  std::optional<Element> stamp;

  /** Per-sample stamp SEQUENCE — ruler ticks with numbers, ribbon menus,
   *  chained ornament. Called once per sample with the sample and its
   *  running index (counting across contours); an Element returned is
   *  baked via snapshot() at intrinsic size and replayed centered at the
   *  sample exactly like `stamp`; std::nullopt falls back to `stamp`
   *  (when set), so a numbered major tick every Nth sample rides over a
   *  plain minor tick without two walks.
   *
   *  Like every raw callable in this library it is incomparable, so it
   *  keeps the decoration conservatively unequal. ContourWalk has no
   *  `operator==` at all — the raw `draw` callable makes that unavoidable
   *  — so a walk NEVER prunes structurally, and a hot describe wants the
   *  host node memoized.
   *
   *  THE BAKES ARE PER CALL AND UNCACHED, deliberately. Each returned
   *  Element is a fresh node, so there is no stable key the instance-side
   *  stamp cache could hold them under; per-index entries would churn its
   *  slots and evict the node's real brush bakes. A static walk pays the
   *  bakes once per describe; with `animatedWalk` it pays them every
   *  frame, which is the author's call to make. */
  core::Callable<std::optional<Element>(const PathSample&, size_t)> stampAt;

  bool isRunning() const { return animatedWalk; }
  float bleed() const { return std::max(bleedPx, 0.0f); }
  /** The mark's full width: every body is centred on its sample, so it
   *  spans the reserve on both sides of the path. */
  float reach() const { return bleed() * 2.0f; }
  /** A walk that declares its reserve places every body on the outline it
   *  is handed, within it; one that declares none keeps the node's box. */
  bool staysOnOutline() const { return bleedPx > 0; }

  void paint(draw::Pen& pen, const PaintContext& ctx) const;

  /** @private Replay cache, shared across the by-value copies
   *  Decoration makes; paint() is const, the bake is memoization. Its
   *  contents are the painter's. (Public to keep ContourWalk an aggregate
   *  for designated init.) */
  struct StampCache;
  std::shared_ptr<StampCache> stampCache = freshStampCache();
  /** @private An empty replay cache. */
  static std::shared_ptr<StampCache> freshStampCache();
};

/** Floods the node's OUTLINE with a Material through a blend mode — the
 *  material-valued decoration.
 *
 *  This is how a material pass goes ABOVE the children: `Element::overlay()`
 *  puts a layer over the fill but under them, and `foreground()` puts a
 *  Decoration over everything. Of the primitives, none of PathFormat, Slice
 *  or ContourWalk fills a shape with a Material, and doing it with a raw
 *  paint program costs pruning — a `std::function` is incomparable, so its
 *  node re-patches on every describe forever.
 *
 *  `Wash` is a comparable VALUE, because a Material compares structurally
 *  by recipe. A static wash prunes like any other decoration, and a wash
 *  over a live material declares itself animated so the node repaints.
 *
 *      .foreground(decorations::wash(material::noise(0.3f, {.octaves = 2,
 * .grain = true}), material::BlendMode::SoftLight, 0.35f))
 *
 *  With a `fill`, the material is a MASK and the fill is what shows
 *  through it — the one spelling of a lattice in a colour the tree
 *  supplies, since a material's own colours sit where no property reaches:
 *
 *      .foreground(decorations::wash(var("shade"),
 * material::pattern::stipple()))
 */
struct Wash {
  material::Material material;
  material::BlendMode blend = material::BlendMode::Normal;
  /** Strength, 0..1, applied as alpha on the pass. Clamped at paint; 0
   *  paints nothing at all. */
  float amount = 1.0f;
  /** THE FILL LAID THROUGH THE MATERIAL. Unset, the material is the paint.
   *  Set — a colour, the ink in force, a custom property or a paint, each
   *  read where the wash paints — the material is a mask: its alpha is
   *  where and how strongly this fill shows, so one stipple serves every
   *  colour a theme puts in the property. */
  std::optional<Fill> fill;

  bool operator==(const Wash& other) const {
    return material == other.material && blend == other.blend &&
           amount == other.amount && fill == other.fill;
  }
  bool isRunning() const {
    if (material.isRunning()) return true;
    const material::Material* shown = fill ? fill->material() : nullptr;
    return shown != nullptr && shown->isRunning();
  }
  bool usesWorldSpace() const;
  /** A wash through anything but source-over reads what is under the node
   *  — which is the point of it, and why such a node cannot be baked. */
  bool blends() const { return blend != material::BlendMode::Normal; }
  /** The wash fills the outline it is handed and nothing else. */
  bool staysOnOutline() const { return true; }

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** THE BORDER: one comparable value for a frame that is not simply a
 *  stroked rounded rect — an inset rule, a set of corner brackets, a rule
 *  that stops short of the corners, or one that thickens where it turns.
 *
 *  It FOLLOWS THE SILHOUETTE. Chamfer the node's outline and the brackets
 *  land on the chamfers with no further instruction, which is the whole
 *  advantage over four absolutely-positioned corner elements: those cost
 *  four nodes, do not move when the box resizes, and do not follow a
 *  non-rectangular shape at all.
 *
 *  For corner marks on the node's REAL boundary, prefer the span claims,
 *  which leave the rest of the boundary free for another brush:
 *
 *      .stroke(spans::corners(18), brush::solid(2, ink))  // reticle corners
 *      .stroke(spans::edges(14), brush::solid(1, ink))    // open corners
 *      .foreground(decorations::border(1, ink, 6))        // inset rule
 *
 *  Build the `Border` value directly for what those cannot say: an inset
 *  rule, or either mode as a layer inside `doubleBorder()`. */
struct Border {
  float width = 1.0f;
  Fill fill = Fill::color({1, 1, 1, 1});
  /** px INSIDE the node's outline; negative moves it outside. The second
   *  border of a double frame is the same value with a different inset. */
  float inset = 0.0f;

  /** What the rule does at the corners. */
  enum class Mode : uint8_t {
    Continuous,  ///< the whole outline — an ordinary stroke
    Bracket,     ///< ONLY within `corner` px of each corner: the four L's
    Gapped,      ///< everything EXCEPT within `corner` px: the open corner
    Weighted,    ///< continuous, but `cornerWidth` near each corner
  };
  Mode mode = Mode::Continuous;
  /** Arm length (Bracket), omission (Gapped), or the weighted run
   *  (Weighted), in px of arc length either side of the corner. */
  float corner = 18.0f;
  /** Weighted mode only: the width used near the corners. */
  float cornerWidth = 0.0f;
  /** The tangent break that counts as a corner. A gently ROUNDED corner
   *  has no hard break and therefore no corner — brackets vanish and a
   *  gapped rule runs continuous. That is correct, and it is the first
   *  thing that surprises people.
   *
   *  **A regular n-gon turns 360/n degrees per vertex, so this default
   *  finds nothing above 12 sides.** A 20-gon turns 18° and renders blank;
   *  a dodecagon turns exactly 30° and sits on the boundary. Pass roughly
   *  0.6 × the turn angle for those — 12° for a 20-gon.
   *
   *  The default is deliberately NOT adaptive. The scan steps 2 px, so an
   *  arc of radius r turns ~114/r degrees per sample — about 11° at
   *  r = 10 — and any threshold low enough to catch a 20-gon's vertices
   *  would shatter a small rounded corner into a run of false ones. The
   *  number stays yours; when a scan finds nothing the library prints the
   *  sharpest break it did see and what to pass. */
  float cornerAngleDegrees = 30.0f;

  std::vector<float> dash;
  float dashPhase = 0.0f;
  /** On `PathFormat::dashPhaseBinding`'s terms. */
  std::optional<motion::Animatable<float>> dashPhaseBinding;
  geometry::path::Cap cap = geometry::path::Cap::Butt;
  geometry::path::Join join = geometry::path::Join::Miter;

  bool operator==(const Border&) const = default;
  bool isRunning() const {
    return dashPhaseBinding && dashPhaseBinding->isRunning();
  }
  float phase() const {
    return dashPhaseBinding ? dashPhaseBinding->value() : dashPhase;
  }
  /** How far the rule's ink stands past the outline: its heaviest half
   *  width, grown at a mitred corner and a square cap as
   *  `PathFormat::bleed()` is, less the inset. */
  float bleed() const;
  /** The mark's own width (see PathFormat::reach) — independent of inset,
   *  which moves the mark rather than widening it. */
  float reach() const { return std::max(width, cornerWidth); }
  /** Whether the rule stands within its `bleed()` of the outline it is
   *  handed (OutlineDecoration): when it is laid inside the outline or on
   *  it. A negative inset strokes a mitred outward copy whose corners
   *  stand further out than the inset, so it keeps the node's box. */
  bool staysOnOutline() const;

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** THE DECORATION FACTORIES, spelled the way a call site reads:
 *  `decorations::border(...)`, `decorations::wash(...)`. Each mints one
 *  of the decoration values declared above it and nothing more — the
 *  factory exists so a node's slot verbs read as English and so a value
 *  with several fields can be started from the two or three that
 *  matter. */
namespace decorations {
inline Wash wash(const material::Material& material,
                 material::BlendMode blend = material::BlendMode::Normal,
                 float amount = 1.0f) {
  return Wash{material, blend, amount};
}

/** @p fill laid through @p mask: `wash(var("shade"), stipple())` floods the
 *  outline with the colour the property holds where the wash paints,
 *  wherever the stipple sets a cell. */
inline Wash wash(Fill fill, const material::Material& mask,
                 material::BlendMode blend = material::BlendMode::Normal,
                 float amount = 1.0f) {
  return Wash{mask, blend, amount, std::move(fill)};
}

/** A plain rule around the node's outline, `inset` px inside it. */
inline Border border(float width, Fill fill, float inset = 0.0f) {
  return Border{.width = width, .fill = std::move(fill), .inset = inset};
}

// There is no factory for the Bracket and Gapped modes. Corner marks on a
// node's own boundary are span claims —
//
//     .stroke(spans::corners(arm), brush::solid(width, fill))   // brackets
//     .stroke(spans::edges(gap),   brush::solid(width, fill))   // open corners
//
// — which run the same corner scan underneath and leave `spans::rest()`
// free to dress everything else. Where a claim cannot say it, name the mode
// on the value: `Border{.mode = Border::Mode::Bracket, …}`.

/** A border whose WEIGHT changes at the corner: `width` along the runs,
 *  `cornerWidth` within `arm` px of each turn. */
inline Border weightedCorners(float width, float cornerWidth, Fill fill,
                              float arm = 18.0f, float inset = 0.0f,
                              float angleDegrees = 30.0f) {
  return Border{.width = width,
                .fill = std::move(fill),
                .inset = inset,
                .mode = Border::Mode::Weighted,
                .corner = arm,
                .cornerWidth = cornerWidth,
                .cornerAngleDegrees = angleDegrees};
}

/** DOUBLE BORDER with independent insets — two rules as one stack, so
 *  both attach and prune together. The inner rule is often the
 *  dotted or lighter one; pass whatever you like.
 *
 *      .stroke(decorations::doubleBorder(
 *          decorations::border(1.6f, ink),
 *          decorations::border(0.8f, ink, 6)))
 */
inline DecorationStack doubleBorder(Border outer, Border inner) {
  return DecorationStack{
      {Decoration(std::move(outer)), Decoration(std::move(inner))}};
}

/** Paints a decoration against geometry you built yourself, inside a
 *  `custom()` program.
 *
 *  The whole brush vocabulary — `PathFormat` with its stroke alignment,
 *  dashes, stamps and its own trim window; `lines::Line`; `Brush`;
 *  `inset` — reads only `PaintContext::outline`. So none of it
 *  is actually restricted to a node's own shape, and geometry that
 *  changes per frame (a simulated rope, a live EQ curve, a plotted
 *  signal) can wear all of it:
 *
 *      custom([&](draw::Pen& pen, const PaintContext& ctx) {
 *        decorations::paintOn(pen, ctx, ropePath(),
 * lines::presets::cased(...));
 *      }).cache(Cache::None)
 *
 *  What live geometry inside `custom()` gives up is PRUNING, not the
 *  decoration vocabulary. */
void paintOn(draw::Pen& pen, const PaintContext& ctx,
             geometry::path::Outline outline, const Decoration& decoration);
}  // namespace decorations

}  // namespace sigil::compose

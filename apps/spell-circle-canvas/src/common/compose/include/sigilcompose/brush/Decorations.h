#pragma once

/** @file
 * SigilCompose decoration primitives — the concrete treatments over the
 * kernel's Decoration seam. Each is a thin value struct over machinery Skia
 * already ships, and there are deliberately few of them:
 *
 *  - PathFormat: format any stroke along the node's outline — width and
 *    paint plus an optional dash pattern, a stamped path repeated along
 *    the contour (vines, chains), or any custom SkPathEffect.
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
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPicture.h>
#include <sigilcompose/brush/Lines.h>  // insetOutline, cornerBrackets, cornerGaps
#include <sigilcompose/core/GpuImage.h>
#include <sigilcompose/core/Material.h>  // Wash — the material-valued decoration
#include <sigilimage/asset/ImageAsset.h>

#include <algorithm>
#include <optional>

#include "sigilcompose/Compose.h"

namespace sigil::compose {

/** Stroke the node's outline, formatted by data: dashes, a stamped
 *  path, or a custom effect (composable — dash of a stamp is legal in
 *  Skia by chaining effects yourself via `effect`). */
struct PathFormat {
  /** Where the stroke sits relative to the outline (the Photoshop/Figma
   *  stroke-position control). Center straddles it; Inner clips the
   *  stroke inside the shape (borders that never fatten the silhouette);
   *  Outer clips it outside (keylines around a filled shape). Inner and
   *  Outer are meaningful on CLOSED outlines — an open rail has no
   *  inside. */
  enum class Align : uint8_t { Center, Inner, Outer };

  float width = 1.0f;
  Fill strokeFill = Fill::color({1, 1, 1, 1});
  Align align = Align::Center;

  /** Dash on/off intervals in px (empty → solid). */
  std::vector<SkScalar> dashIntervals;
  /** A Material for the stroke, superseding `strokeFill` when set.
   *
   *  Prefer it to `strokeFill` when the same paint also fills something:
   *  a Material is authored in the unit square, compares structurally, and
   *  can carry live uniforms, where a `Fill` is node-local pixels compared
   *  by shader pointer. On an object whose surfaces are mostly strokes,
   *  using `Fill` means writing the same material twice and converting
   *  coordinates by hand in both. */
  std::optional<Material> strokeMaterial;
  /** Stroke cap and join on the paint itself. The defaults are Skia's —
   *  butt caps and mitred joins — which end open contours square; line art
   *  built from many short open contours usually wants round for both.
   *  Distinct from `lines::Rail::join`, which shapes that rail's own
   *  OFFSET CURVE rather than this stroke on the node's outline. */
  SkPaint::Cap cap = SkPaint::kButt_Cap;
  SkPaint::Join join = SkPaint::kMiter_Join;
  float dashPhase = 0.0f;
  /** Bind the dash phase to a wrapping Output and the dashes MARCH — a
   *  selected route, a live link, a cut line. Like `trimPhase`, it
   *  supersedes the constant and declares the decoration animated, so the
   *  node repaints every frame without needing a re-describe. */
  const choreograph::Output<float>* dashPhaseBinding = nullptr;

  /** Stamp this path repeatedly along the contour (advance px apart),
   *  rotated to follow it — vines, chains, ornament runs. */
  SkPath stampPath;
  float stampAdvance = 0.0f;

  /** Escape hatch: any SkPathEffect; overrides dash/stamp when set. */
  sk_sp<SkPathEffect> effect;

  /** Per-DECORATION trim window (fractions of arc length) — one node can
   *  carry a full static band AND a marching sliver as two strokes. Wraps
   *  like `spans::wrap` (seam-crossing windows stitch into one contour).
   *  Bind `trimPhase` to a wrapping Output and THIS stroke marches while
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
  const choreograph::Output<float>* trimPhase = nullptr;  // replaces offset

  /** Structural equality so a static stroked/dashed/stamped border prunes
   *  without memo (the custom SkPathEffect compares by pointer identity). */
  bool operator==(const PathFormat&) const = default;

  /** Stroke reach beyond the outline (recording cull grows by this). */
  float bleed() const {
    return align == Align::Inner   ? 0.0f
           : align == Align::Outer ? width
                                   : width * 0.5f;
  }
  /** How wide the MARK is either side of the outline — which is NOT
   *  bleed(): an Inner stroke escapes the shape by nothing while painting
   *  a mark `width` px wide inside it. Anything asking where the mark IS
   *  (a weave's crossing repair) needs this number. */
  float reach() const { return width; }
  /** A bound trim phase, a bound dash phase, or a live stroke material
   *  repaints per frame (declared volatility). */
  bool isAnimated() const {
    return trimPhase != nullptr || dashPhaseBinding != nullptr ||
           (strokeMaterial && strokeMaterial->isAnimated());
  }
  float phase() const {
    return dashPhaseBinding ? dashPhaseBinding->value() : dashPhase;
  }

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;
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

/** A soft drop shadow behind the node's outline — a value DecorationScheme
 *  (so a static shadowed node prunes without memo). Attach with .background()
 *  *before* the fill so the fill paints over it. */
struct Shadow {
  SkColor4f color = {0, 0, 0, 1};
  SkVector offset = {0, 0};
  float blur = 0;

  /** Bound offsets: when set, the Output's current value REPLACES that
   *  axis of `offset` each paint, and the decoration declares itself
   *  animated (per-frame volatility) — the hover-lift shadow slides
   *  without re-describing. `maxBind` reserves cull reach for the bound
   *  range (bleed() can't read a future value). */
  const choreograph::Output<float>* bindOffsetX = nullptr;
  const choreograph::Output<float>* bindOffsetY = nullptr;
  float maxBind = 0.0f;

  /** CSS box-shadow semantics: knock the shape's own footprint OUT of the
   *  shadow, so nothing paints under the node. A translucent node over a
   *  knocked-out shadow stays clear instead of sampling its own shadow
   *  through itself. */
  bool knockout = false;

  bool operator==(const Shadow&) const = default;
  bool isAnimated() const { return bindOffsetX || bindOffsetY; }
  /** Paint reach beyond the node's bounds; the recording's cull rect grows
   *  by this. Under-report it and a big soft shadow is clipped at the
   *  node's picture-cache bounds, which is why the bound range has to be
   *  declared through `maxBind` — bleed() cannot read a future value. */
  float bleed() const {
    return std::max({std::abs(offset.fX), std::abs(offset.fY), maxBind}) + blur;
  }

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    p.setColor4f(color, nullptr);
    if (blur > 0)
      p.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, blur * 0.5f));
    canvas.save();
    if (knockout) canvas.clipPath(ctx.outline, SkClipOp::kDifference, true);
    canvas.translate(bindOffsetX ? bindOffsetX->value() : offset.x(),
                     bindOffsetY ? bindOffsetY->value() : offset.y());
    canvas.drawPath(ctx.outline, p);
    canvas.restore();
  }
};

inline Shadow shadow(SkColor4f color, SkVector offset, float blur) {
  return Shadow{color, offset, blur};
}

/** Image-onto-box through a lattice (per-cell stretch); nine-slice is
 *  xDivs/yDivs of size 2. Empty divs stretch the whole image. */
struct Slice {
  std::shared_ptr<const sigil::image::ImageAsset> asset;
  std::vector<int> xDivs;
  std::vector<int> yDivs;
  /** Skia's native lattice draw is not implemented on every backend and
   *  silently draws NOTHING where it is not — including when a picture
   *  recorded elsewhere replays there. `gpuimg::drawLattice` decomposes
   *  the lattice itself on every backend and promotes raster sources
   *  through this cache. Excluded from equality: a cache is not part of
   *  the value. */
  std::shared_ptr<gpuimg::Promoted> gpuCache =
      std::make_shared<gpuimg::Promoted>();
  /** How the slices sample. Linear is right for a soft frame and wrong
   *  for pixel art — a window chrome, a dialog border, a button cut from a
   *  tile sheet — where it blurs every slice boundary. */
  SkFilterMode filter = SkFilterMode::kLinear;
  /** Source pixels per layout unit in the CORNER and EDGE bands. 1 draws the
   *  bands at their pixel count, which is right for a texture authored at the
   *  size it is used. A frame generated oversized so it stays sharp on a
   *  high-density device declares that factor here — 2 for a texture drawn at
   *  twice its on-page size — and its corners land at the width they were
   *  designed for instead of twice it. The stretchable bands absorb the rest
   *  either way, so this changes the frame's weight and nothing else. */
  float density = 1.0f;

  /** Structural equality (asset by pointer identity) so a static nine-slice
   *  frame prunes without memo. The promotion cache is identity-free. */
  bool operator==(const Slice& o) const {
    return asset == o.asset && xDivs == o.xDivs && yDivs == o.yDivs &&
           filter == o.filter && density == o.density;
  }

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;
};

/** One arc-length sample along the outline. */
struct PathSample {
  SkPoint position;
  SkVector tangent;
  float distance = 0.0f;
  float fraction = 0.0f;  // 0..1 within its contour
};

/** Walk the outline at `spacing` px intervals; at every sample the
 *  canvas is translated to the sample and rotated so +x follows the
 *  tangent. The general procedural border. Three bodies, composable:
 *
 *  - `draw`: a raw program per sample (per-step images, SkSL, nested
 *    drawing). Set `animatedWalk` when it depends on
 *    ctx.elapsedSeconds (declared volatility).
 *  - `stamp`: a full element subtree — laid out and recorded ONCE via
 *    snapshot() (at intrinsic size, its own decorations and all), then
 *    replayed per sample centered on the contour. Recursion is closed:
 *    the stamp's decorations may walk their own contours. With
 *    `animatedWalk` the stamp re-records each paint, sampling any
 *    bound ch::Outputs at their current values.
 *  - `stampAt`: the SEQUENCE form of `stamp` — see its own note.
 *
 *  When several are set, the sample's stamp replays first (`stampAt`'s
 *  element superseding `stamp` at that sample), then `draw` on top. */
struct ContourWalk {
  float spacing = 16.0f;
  std::function<void(SkCanvas&, const PathSample&, const PaintContext&)> draw;
  bool animatedWalk = false;

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
  std::function<std::optional<Element>(const PathSample&, size_t)> stampAt;

  bool isAnimated() const { return animatedWalk; }

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;

  /** @private Replay cache, shared across the by-value copies
   *  Decoration makes; paint() is const, the bake is memoization.
   *  (Public to keep ContourWalk an aggregate for designated init.) */
  struct StampCache {
    sk_sp<SkPicture> picture;
    const void* bakedFor = nullptr;
  };
  std::shared_ptr<StampCache> stampCache = std::make_shared<StampCache>();
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
 *      .foreground(decorations::wash(patterns::grain(0.3f, 2, 7.0f),
 *                                    SkBlendMode::kSoftLight, 0.35f))
 */
struct Wash {
  Material material;
  SkBlendMode blend = SkBlendMode::kSrcOver;
  /** Strength, 0..1, applied as alpha on the pass. Clamped at paint; 0
   *  paints nothing at all. */
  float amount = 1.0f;

  bool operator==(const Wash& o) const {
    return material == o.material && blend == o.blend && amount == o.amount;
  }
  bool isAnimated() const { return material.isAnimated(); }

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;
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
  float cornerAngleDeg = 30.0f;

  std::vector<SkScalar> dash;
  float dashPhase = 0.0f;
  const choreograph::Output<float>* dashPhaseBinding = nullptr;
  SkPaint::Cap cap = SkPaint::kButt_Cap;
  SkPaint::Join join = SkPaint::kMiter_Join;

  bool operator==(const Border&) const = default;
  bool isAnimated() const { return dashPhaseBinding != nullptr; }
  float phase() const {
    return dashPhaseBinding ? dashPhaseBinding->value() : dashPhase;
  }
  float bleed() const {
    const float heaviest = std::max(width, cornerWidth);
    return std::max(0.0f, heaviest * 0.5f - inset);
  }
  /** The mark's own width (see PathFormat::reach) — independent of inset,
   *  which moves the mark rather than widening it. */
  float reach() const { return std::max(width, cornerWidth); }

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;
};

namespace decorations {
inline Wash wash(Material material, SkBlendMode blend = SkBlendMode::kSrcOver,
                 float amount = 1.0f) {
  return Wash{std::move(material), blend, amount};
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
                              float angleDeg = 30.0f) {
  return Border{.width = width,
                .fill = std::move(fill),
                .inset = inset,
                .mode = Border::Mode::Weighted,
                .corner = arm,
                .cornerWidth = cornerWidth,
                .cornerAngleDeg = angleDeg};
}

/** DOUBLE BORDER with independent insets — two rules as one LayerStyle
 *  value, so both attach and prune together. The inner rule is often the
 *  dotted or lighter one; pass whatever you like.
 *
 *      .style(decorations::doubleBorder(
 *          decorations::border(1.6f, ink),
 *          decorations::border(0.8f, ink, 6)))
 */
inline LayerStyle doubleBorder(Border outer, Border inner) {
  return LayerStyle{
      {}, {Decoration(std::move(outer)), Decoration(std::move(inner))}};
}

/** Paints a decoration against geometry you built yourself, inside a
 *  `custom()` program.
 *
 *  The whole brush vocabulary — `PathFormat` with its stroke alignment,
 *  dashes, stamps and its own trim window; `lines::Line`; `Brush`;
 *  `shapes::inset` — reads only `PaintContext::outline`. So none of it
 *  is actually restricted to a node's own shape, and geometry that
 *  changes per frame (a simulated rope, a live EQ curve, a plotted
 *  signal) can wear all of it:
 *
 *      custom([&](SkCanvas &c, const PaintContext &ctx) {
 *        decorations::paintOn(c, ctx, ropePath(), lines::cased(...));
 *      }).cache(Cache::None)
 *
 *  What live geometry inside `custom()` gives up is PRUNING, not the
 *  decoration vocabulary. */
void paintOn(SkCanvas& canvas, const PaintContext& ctx, SkPath outline,
             const Decoration& decoration);
}  // namespace decorations

}  // namespace sigil::compose

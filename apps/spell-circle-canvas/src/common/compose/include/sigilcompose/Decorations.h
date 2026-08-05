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
#include <include/core/SkClipOp.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/effects/Sk1DPathEffect.h>
#include <include/effects/SkDashPathEffect.h>
#include <sigilimage/ImageAsset.h>

#include <cmath>
#include <optional>

#include "sigilcompose/Compose.h"
#include "sigilcompose/GpuImage.h"
#include "sigilcompose/Lines.h"     // insetOutline, cornerBrackets, cornerGaps
#include "sigilcompose/Material.h"  // Wash — the material-valued decoration

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
   *      PathFormat head = util::stroke(6, Fill::color(kBright));
   *      head.trimStart = 0.90f; head.trimEnd = 1.0f;
   *      box().shape(curve)
   *           .stroke(spans::upTo(&growth),
   *                   brush::layers({util::stroke(3, Fill::color(kBody)),
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

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    SkPaint p;
    p.setAntiAlias(true);
    p.setStyle(SkPaint::kStroke_Style);
    // Inner/Outer: clip to the shape's side and stroke DOUBLE width — the
    // visible half lands entirely on the kept side (the standard trick).
    const bool aligned = align != Align::Center;
    p.setStrokeWidth(aligned ? width * 2 : width);
    p.setStrokeCap(cap);
    p.setStrokeJoin(join);
    const Fill stroke =
        strokeMaterial ? strokeMaterial->resolve(ctx) : strokeFill;
    if (stroke.kind == Fill::Kind::Color)
      p.setColor4f(stroke.colorValue, nullptr);
    else if (stroke.kind == Fill::Kind::Shader)
      p.setShader(stroke.shaderValue);

    sk_sp<SkPathEffect> chosen = effect;
    if (!chosen && stampAdvance > 0 && !stampPath.isEmpty())
      chosen = SkPath1DPathEffect::Make(stampPath, stampAdvance, phase(),
                                        SkPath1DPathEffect::kRotate_Style);
    if (!chosen && !dashIntervals.empty())
      chosen = SkDashPathEffect::Make(
          SkSpan(dashIntervals.data(), dashIntervals.size()), phase());
    p.setPathEffect(std::move(chosen));

    // The decoration's own trim window (wrapping; the marching sliver).
    const SkPath* drawn = &ctx.outline;
    SkPath windowed;
    const float off = trimPhase ? trimPhase->value() : trimOffset;
    const float s0 = trimStart + off, e0 = trimEnd + off;
    const float span = e0 - s0;
    if (span > 0.0f && span < 1.0f) {
      const float s = s0 - std::floor(s0);
      const float e = e0 - std::floor(e0);
      SkPathBuilder window;
      SkContourMeasureIter iter(ctx.outline, false);
      while (sk_sp<SkContourMeasure> contour = iter.next()) {
        const float len = contour->length();
        if (s < e) {
          (void)contour->getSegment(s * len, e * len, &window, true);
        } else if (s > e) {
          (void)contour->getSegment(s * len, len, &window, true);
          // A closed contour has a real seam, so joining both pieces avoids
          // doubled caps there. An open route has no seam: continuing without
          // a moveTo would invent a straight chord from its end to its start.
          (void)contour->getSegment(0, e * len, &window, !contour->isClosed());
        }
      }
      windowed = window.detach();
      if (!windowed.isEmpty()) drawn = &windowed;
    } else if (span <= 0.0f) {
      return;  // empty window — nothing to stroke
    }

    if (aligned) {
      canvas.save();
      canvas.clipPath(
          ctx.outline,
          align == Align::Inner ? SkClipOp::kIntersect : SkClipOp::kDifference,
          true);
      canvas.drawPath(*drawn, p);
      canvas.restore();
    } else {
      canvas.drawPath(*drawn, p);
    }
  }
};

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

  /** Structural equality (asset by pointer identity) so a static nine-slice
   *  frame prunes without memo. The promotion cache is identity-free. */
  bool operator==(const Slice& o) const {
    return asset == o.asset && xDivs == o.xDivs && yDivs == o.yDivs &&
           filter == o.filter;
  }

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    if (!asset || asset->frames().empty()) return;
    sk_sp<SkImage> img = asset->frames().front().image;
    if (!img) return;
    const SkRect dst = SkRect::MakeWH(ctx.size.width(), ctx.size.height());
    gpuimg::drawLattice(canvas, *gpuCache, std::move(img), xDivs, yDivs, dst,
                        filter);
  }
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

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    if ((!draw && !stamp && !stampAt) || spacing <= 0) return;

    // Bake (or re-bake) the stamp element: once per description for
    // static stamps, once per paint for animated ones.
    const void* stampNode = stamp ? stamp->node().get() : nullptr;
    if (stampCache->bakedFor != stampNode) {
      stampCache->picture.reset();
      stampCache->bakedFor = stampNode;
    }
    if (stamp && ctx.fonts && (!stampCache->picture || animatedWalk))
      stampCache->picture = snapshot(*stamp, *ctx.fonts);
    const sk_sp<SkPicture>& stampPicture = stampCache->picture;

    SkContourMeasureIter iter(ctx.outline, false);
    size_t index = 0;  // runs across contours — the sequence's position
    while (sk_sp<SkContourMeasure> contour = iter.next()) {
      const float length = contour->length();
      for (float d = 0; d < length; d += spacing) {
        SkPoint pos;
        SkVector tan;
        if (!contour->getPosTan(d, &pos, &tan)) continue;
        PathSample sample{pos, tan, d, length > 0 ? d / length : 0};
        // This sample's OWN art (stampAt): baked per call, uncached — see
        // the field note. The shell box is needed because snapshot() sizes
        // by the root's CHILDREN and ignores the root's own dimensions.
        sk_sp<SkPicture> own;
        if (stampAt && ctx.fonts)
          if (std::optional<Element> e = stampAt(sample, index))
            own = snapshot(box().child(std::move(*e)), *ctx.fonts);
        const sk_sp<SkPicture>& art = own ? own : stampPicture;
        canvas.save();
        canvas.translate(pos.x(), pos.y());
        canvas.rotate(std::atan2(tan.y(), tan.x()) * 180.0f / 3.14159265f);
        if (art) {
          const SkRect cull = art->cullRect();
          canvas.save();
          canvas.translate(-cull.width() / 2, -cull.height() / 2);
          canvas.drawPicture(art);
          canvas.restore();
        }
        if (draw) draw(canvas, sample, ctx);
        canvas.restore();
        ++index;
      }
    }
  }

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

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    const float a = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
    if (a <= 0.0f) return;
    const Fill fill = material.resolve(ctx);
    SkPaint p;
    p.setAntiAlias(true);
    p.setBlendMode(blend);
    if (fill.kind == Fill::Kind::Color) {
      SkColor4f c = fill.colorValue;
      c.fA *= a;
      p.setColor4f(c, nullptr);
    } else if (fill.kind == Fill::Kind::Shader) {
      p.setShader(fill.shaderValue);
      p.setAlphaf(a);
    } else {
      return;
    }
    canvas.drawPath(ctx.outline, p);
  }
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

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    // `width` is the RUN's width, and Weighted mode has a second width for
    // the corners — so in that mode width == 0 means "corners only, no runs
    // between them", which is a real frame. The bail-out therefore tests
    // the HEAVIEST of the two widths, or weightedCorners(0, w, …) would
    // draw nothing at all.
    const float heaviest =
        mode == Mode::Weighted ? std::max(width, cornerWidth) : width;
    if (ctx.outline.isEmpty() || heaviest <= 0) return;
    const SkPath base =
        inset != 0 ? lines::insetOutline(ctx.outline, inset) : ctx.outline;

    auto strokeWith = [&](const SkPath& path, float w) {
      if (path.isEmpty() || w <= 0) return;
      SkPaint p;
      p.setAntiAlias(true);
      p.setStyle(SkPaint::kStroke_Style);
      p.setStrokeWidth(w);
      p.setStrokeCap(cap);
      p.setStrokeJoin(join);
      if (fill.kind == Fill::Kind::Color)
        p.setColor4f(fill.colorValue, nullptr);
      else if (fill.kind == Fill::Kind::Shader)
        p.setShader(fill.shaderValue);
      if (!dash.empty())
        p.setPathEffect(
            SkDashPathEffect::Make(SkSpan(dash.data(), dash.size()), phase()));
      canvas.drawPath(path, p);
    };

    switch (mode) {
      case Mode::Continuous:
        strokeWith(base, width);
        break;
      case Mode::Bracket:
        strokeWith(lines::cornerBrackets(base, corner, cornerAngleDeg), width);
        break;
      case Mode::Gapped:
        strokeWith(lines::cornerGaps(base, corner, cornerAngleDeg), width);
        break;
      case Mode::Weighted:
        // Two passes over complementary windows: the runs BETWEEN corners at
        // `width`, then the corners themselves at `cornerWidth` — a rule that
        // thickens where it turns.
        strokeWith(lines::cornerGaps(base, corner, cornerAngleDeg), width);
        strokeWith(lines::cornerBrackets(base, corner, cornerAngleDeg),
                   cornerWidth > 0 ? cornerWidth : width);
        break;
    }
  }
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
inline void paintOn(SkCanvas& canvas, const PaintContext& ctx, SkPath outline,
                    const Decoration& decoration) {
  PaintContext local = ctx;
  local.outline = std::move(outline);
  decoration.paint(canvas, local);
}
}  // namespace decorations

}  // namespace sigil::compose

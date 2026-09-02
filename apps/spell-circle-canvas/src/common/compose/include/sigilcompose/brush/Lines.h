#pragma once

/** @file
 * SigilCompose line patterns — the cartography/diagram stroke vocabulary
 * beyond dashes: parallel casings (double/triple rails, highway pairs),
 * terminal caps (arrows, dots, bars — the node-graph direction language),
 * railway ties, wave and zigzag runs. One value DecorationScheme
 * (`lines::Line`) built from pure data, so patterned connectors prune and
 * cache like any static chrome; attach with `.stroke()` to dress any
 * outline, rail, or connector route.
 *
 * Extension-point note: Skia's own seam here would be a custom
 * SkPathEffect, but the public API seals subclassing (onFilterPath lives
 * in src/). This header mirrors that contract at OUR seam instead — the
 * geometry ops run on the outline before stroking, as comparable values —
 * and PathFormat::effect stays the raw sk_sp<SkPathEffect> escape hatch
 * for effects Skia does ship (dash, corner, discrete, 1D, trim).
 *
 *   rail(stops, routers::octilinear())
 *       .stroke(lines::Line{.width = 3, .fill = ink,
 *                           .parallels = 2, .gap = 5});      // transit pair
 *   connector("a", "b").stroke(lines::arrow(2, wire, 12));   // directed edge
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <sigilmaterial/skia/Paint.h>  // material::skia::Stop — the along-arc gradient ramp

#include <optional>
#include <vector>

#include "sigilcompose/Compose.h"

namespace sigil::compose::lines {

namespace detail {}  // namespace detail

/** Convert a path into its DASHED GEOMETRY — the dash segments as real
 *  contours, rather than a paint-time effect. The building block for any
 *  construction that has to dash BEFORE it does something else to the
 *  geometry (offset each dash onto a parallel rail, stamp along the marks,
 *  measure them).
 *
 *  IT EXISTS BECAUSE THE OBVIOUS SPELLING SILENTLY DOES NOTHING. Skia's
 *  dash effect opens with
 *
 *      // we do nothing if the src wants to be filled
 *      if (kFill_Style == style || kStrokeAndFill_Style == style) return false;
 *
 *  so `filterPath` with a `SkStrokeRec(kFill_InitStyle)` returns false and
 *  leaves the destination untouched. Every other path effect used here —
 *  corner, trim, discrete — accepts a fill rec, so a fill rec is the
 *  natural thing to reach for, and the one effect that refuses it fails by
 *  leaving a SOLID path behind rather than by reporting anything. Nothing
 *  in the render says the line was meant to be dashed.
 *
 *  Hairline is the rec to use. Returns the input unchanged when the
 *  pattern is empty or Skia declines. */
SkPath dashGeometry(const SkPath& src, SkSpan<const SkScalar> intervals,
                    float phase);

/** Offset a CLOSED outline inward (positive @p px) or outward (negative),
 *  following any silhouette — a chamfered panel, a star, a blob — not just
 *  rectangles.
 *
 *  How it works: stroking the outline at width 2|px| gives the RING
 *  straddling it, so subtracting that ring shrinks the shape and unioning
 *  it grows the shape by the same amount. Returns the input unchanged if
 *  the boolean op fails. */
SkPath insetOutline(const SkPath& outline, float px);

/** CORNER BRACKETS as GEOMETRY: keep only the arc within @p arm px of each
 *  corner, so a rectangle becomes four L-shaped marks and nothing else —
 *  the reticle, the selection handle, the crop mark. It follows ANY
 *  silhouette, so chamfering the box puts the brackets on the chamfers,
 *  where four hand-placed corner elements would stay where they were put.
 *
 *  Prefer `spans::corners(arm)` when the marks belong to an element: that
 *  CLAIMS runs on the element's real boundary and leaves the rest of it
 *  free, where this returns a path that replaces the shape. Reach for this
 *  when you want the geometry itself. */
SkPath cornerBrackets(const SkPath& src, float arm, float angleDeg = 30.0f);

/** The complement: a rule that STOPS SHORT of every corner, leaving @p gap
 *  px of paper at each. The printer's open-corner box rule; also how a
 *  technical drawing keeps a frame from fighting its own dimension lines.
 *
 *  `spans::edges(gap)` is the same scan claimed on an element's own
 *  boundary; see cornerBrackets above for when to prefer which. */
SkPath cornerGaps(const SkPath& src, float gap, float angleDeg = 30.0f);

/** How a line run terminates (per contour end). Heads are FILLED with the
 *  line's own fill — solid arrowheads, station dots, buffer bars. */
enum class Cap : uint8_t { None, Arrow, Dot, Bar };

/** The patterned line: everything is data (defaulted equality — a static
 *  patterned connector prunes without memo). Compose freely: a wavy
 *  double line with arrowheads and ties is one value. */
struct Line {
  float width = 2.0f;
  Fill fill = Fill::color({1, 1, 1, 1});

  /** Parallel casing: 1 = plain, 2 = the transit pair, 3 = triple rail
   *  (odd counts keep a center line, weighted by `coreWidthFactor` — the
   *  bold-spine + light-outriders look). `gap` is CENTER-TO-CENTER spacing
   *  between adjacent lines; map styles that specify the inner CLEAR gap
   *  instead convert as gap = clear + width. Parallels follow
   *  curves exactly (offset contours via the stroke-outline construction;
   *  dashed parallels switch to per-rail offsets so dashes stay in
   *  phase). */
  int parallels = 1;
  float gap = 4.0f;
  float coreWidthFactor = 1.0f;

  /** Corner treatment for the drawn strokes AND for the parallel-offset
   *  construction. The default round join also rounds the OFFSET contour,
   *  so a crisp 45° jog in a cased wire comes out as a soft S-curve; pass
   *  `SkPaint::kMiter_Join` to keep the jog sharp. The offset rails are
   *  built from a stroke outline either way, so this exposes that join
   *  rather than adding one. */
  SkPaint::Join join = SkPaint::kRound_Join;

  /** Wave/zigzag displacement of the run itself (the y2k squiggle, the
   *  hand-drawn nerve): amplitude in px, wavelength in px along the arc.
   *  Zigzag alternates hard vertices instead of the sine. */
  float waveAmplitude = 0.0f;
  float waveLength = 24.0f;
  bool zigzag = false;

  /** Railway ties: short perpendicular ticks every `tickSpacing` px
   *  (0 for none), `tickLength` px long — the rail and blueprint idiom,
   *  which reads well at a spacing-to-length ratio near 1:1. `tickWidth`
   *  0 strokes the ties at the line's own width; map conventions often
   *  want roughly twice it. */
  float tickSpacing = 0.0f;
  float tickLength = 8.0f;
  float tickWidth = 0.0f;

  /** One-sided displacement of the whole run, **positive LEFT of travel**
   *  (the one convention — see `geometry::path::parallel`) — bus lanes beside
   * the road, half-side hachures. Same semantics as
   *  `geometry::shapers::Offset` in a Brush pipeline: reach for the
   *  shaper when several layers share one displacement, this field for a
   *  single Line. */
  float across = 0.0f;

  /** Terminal caps per contour; start is the path's first point. The
   *  convention: the arrow TIP sits AT the endpoint and the head extends
   *  BACKWARD over the run, with the body trimmed out from under Arrow and
   *  Bar heads so dashes stop cleanly instead of showing through. The apex
   *  is 60°, and a capSize around 3× the line width reads as a normal
   *  arrowhead. */
  Cap startCap = Cap::None;
  Cap endCap = Cap::None;
  float capSize = 10.0f;

  /** Mid-path repeated caps: a cap glyph every `midSpacing` px — the
   *  direction chevrons that run down a wire. */
  Cap midCap = Cap::None;
  float midSpacing = 0.0f;

  /** Dashing still composes with everything above (applied to the body
   *  strokes, never to heads or ties). */
  std::vector<SkScalar> dashIntervals;
  float dashPhase = 0.0f;
  /** Bind it and the dashes march (see PathFormat::dashPhaseBinding). */
  std::optional<motion::Animatable<float>> dashPhaseBinding;

  /** Along-arc gradient: colour as a ramp over the run's arc fraction — an
   *  energy fade, an elevation-coloured trail. Drawn as up to 48 arc chunks
   *  per contour, each solid at its own interpolated colour, with round
   *  joins hiding the seams; overrides `fill`'s colour when non-empty.
   *
   *  **It applies to a single run only.** With `parallels > 1` or a dash
   *  pattern set, this list is IGNORED and the casings paint flat. */
  std::vector<material::skia::Stop> alongStops;

  bool operator==(const Line&) const = default;

  /** A bound dash phase makes the node volatile, the same declared-
   *  volatility contract PathFormat::trimPhase uses. */
  bool isAnimated() const {
    return dashPhaseBinding && motion::isLive(nullptr, *dashPhaseBinding);
  }
  float phase() const {
    return dashPhaseBinding ? motion::resolveFloatAt(nullptr, *dashPhaseBinding)
                            : dashPhase;
  }

  /** Paint reach beyond the outline (cull growth): outer parallels, tie
   *  arms, and heads all overhang. */
  float bleed() const;

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;

 private:
  /** How much body to cut under a terminal (dashes stop under heads). */
  float trimFor(Cap cap) const;

  void applyFill(SkPaint& p) const;

  void drawCap(SkCanvas& canvas, const SkPaint& head, Cap cap, SkPoint pos,
               SkVector tan) const;
};

// ---- factory sugar ---------------------------------------------------------

/** The transit pair: two rails following the route. */
Line cased(float width, Fill fill, float gap = 5.0f);

/** Triple rail with a weighted spine (bold center, light outriders). */
Line triple(float width, Fill fill, float gap = 5.0f, float coreFactor = 1.8f);

/** Directed edge: plain body, filled arrowhead at the end. */
Line arrow(float width, Fill fill, float headSize = 10.0f);

/** Railway: body + perpendicular ties. */
Line railway(float width, Fill fill, float tieSpacing = 12.0f,
             float tieLength = 10.0f);

/** The cartographic railway: a dark line under a white dash overlay at
 *  about a third of its width, on a 50% duty cycle — the map convention,
 *  which uses no ties at all. Two decorations as one LayerStyle, so attach
 *  with `Element::style()`. */
LayerStyle railwayCarto(float scale = 1.0f,
                        SkColor4f dark = {0.439f, 0.439f, 0.439f, 1},
                        SkColor4f light = {1, 1, 1, 1});

/** The squiggle (sine) — set `zigzag` on the returned value for vertices. */
Line wavy(float width, Fill fill, float amplitude = 4.0f,
          float wavelength = 18.0f);

}  // namespace sigil::compose::lines

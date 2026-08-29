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

#include <vector>

#include "sigilcompose/Compose.h"
#include "sigilcompose/Material.h"  // Stop — the along-arc gradient ramp

namespace sigil::compose::lines {

/** Square-wave (battlement) displacement: the run holds at +amp for half a
 *  wavelength then drops to −amp for the next, with the verticals coming
 *  from doubled points at each step. Zero at both endpoints and snapped to
 *  fit, exactly like `geometry::displace` — the boxy member of the same
 *  family. */
SkPath displaceSquare(const SkPath& src, float amplitude, float wavelength);

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
   *  (the one convention — see `geometry::parallel`) — bus lanes beside the
   *  road, half-side hachures. Same semantics as
   *  `kit::brush::shapers::Offset` in a Brush pipeline: reach for the
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
  const choreograph::Output<float>* dashPhaseBinding = nullptr;

  /** Along-arc gradient: colour as a ramp over the run's arc fraction — an
   *  energy fade, an elevation-coloured trail. Drawn as up to 48 arc chunks
   *  per contour, each solid at its own interpolated colour, with round
   *  joins hiding the seams; overrides `fill`'s colour when non-empty.
   *
   *  **It applies to a single run only.** With `parallels > 1` or a dash
   *  pattern set, this list is IGNORED and the casings paint flat. */
  std::vector<Stop> alongStops;

  bool operator==(const Line&) const = default;

  /** A bound dash phase makes the node volatile, the same declared-
   *  volatility contract PathFormat::trimPhase uses. */
  bool isAnimated() const { return dashPhaseBinding != nullptr; }
  float phase() const {
    return dashPhaseBinding ? dashPhaseBinding->value() : dashPhase;
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

// ---------------------------------------------------------------------------
// N-rail strokes — a parallel rule where EVERY rail is its own line
//
// `Line::parallels` gives N rails sharing one width, one fill, one dash and
// one phase; its only per-rail knob is `coreWidthFactor`, which applies to
// the centre rail and only when `parallels` is odd. `Rails` is for
// everything that needs more than that:
//
//   heavy outer + hairline inner at two rails  (the engraver's rule)
//   solid outer + DOTTED inner                 (a road under construction)
//   unequal gaps                               (road + kerb + lane)
//   per-rail colour
//
// Neither workaround covers it. Stacked strokes with different insets only
// work on concentric shapes, because an inset is not a displacement. A
// `Brush` with a per-layer offset shaper works on any path, but each layer
// dashes ITS OWN offset curve, whose arc length differs from its
// neighbour's on every bend, so the dashes shear apart.
//
// `Rails` is built on Line's dashed-parallel construction instead:
//
//     DASH IN CENTRELINE ARC-SPACE, THEN DISPLACE THE DASHES.
//
// Every rail's pattern is measured along the SAME curve, so rails sharing
// intervals stay in register through any curvature, and a rail with a
// different pattern still registers against the centreline — which is what
// makes an inner tick fall reliably between two outer ones.

/** One rail of a `Rails` stroke: its own displacement from the route, its
 *  own width, fill, dash pattern and phase. `across` is px **LEFT of
 *  travel** — the one convention (see `geometry::parallel`), shared with
 *  `Line::across`, `strand::offset` and `Profile::across` — so a
 *  symmetric pair is {-gap/2, +gap/2}. */
struct Rail {
  float across = 0.0f;
  float width = 2.0f;
  Fill fill = Fill::color({1, 1, 1, 1});
  /** Empty → solid. Measured along the CENTRELINE, not this rail. */
  std::vector<SkScalar> dash;
  /** Added to the stroke's shared phase — the knob that slides ONE rail
   *  against its neighbours (staggered ties, a counter-dashed strand). */
  float dashPhase = 0.0f;
  SkPaint::Cap cap = SkPaint::kRound_Cap;
  SkPaint::Join join = SkPaint::kRound_Join;

  bool operator==(const Rail&) const = default;
};

/** The general parallel rule: an ordered set of `Rail`s sharing one route,
 *  one wave/zigzag displacement and one marching phase.
 *
 *      element.stroke(lines::Rails{.rails = {
 *          {.across = -4, .width = 2.4f, .fill = ink},
 *          {.across =  0, .width = 0.6f, .fill = ink, .dash = {1, 4}},
 *          {.across =  4, .width = 2.4f, .fill = ink}}});
 *
 *  A value like every other decoration: defaulted equality, so a static
 *  quad rail prunes and caches without a memo. */
struct Rails {
  std::vector<Rail> rails;

  /** Shared displacement of the route before any rail is offset — the
   *  whole set waves together (`Line::waveAmplitude` semantics). */
  float waveAmplitude = 0.0f;
  float waveLength = 24.0f;
  bool zigzag = false;

  /** Shared phase, added to every rail's own `dashPhase`. Bind it and the
   *  whole set marches in register (PathFormat::dashPhaseBinding). */
  float dashPhase = 0.0f;
  const choreograph::Output<float>* dashPhaseBinding = nullptr;

  /** Resample stride for the offset construction, px. 2 follows tight
   *  metro curves; loosen on long gentle routes. */
  float offsetStep = 2.0f;

  bool operator==(const Rails&) const = default;

  bool isAnimated() const { return dashPhaseBinding != nullptr; }
  float phase() const {
    return dashPhaseBinding ? dashPhaseBinding->value() : dashPhase;
  }

  float bleed() const;

  /** Total centre-to-centre span of the set — the number a caller needs to
   *  reserve room, and the one `Line` never exposed. */
  float span() const;

  void paint(SkCanvas& canvas, const PaintContext& ctx) const;
};

/** N identical rails, symmetric about the route — the general form of
 *  `Line::parallels`, where 2 is `cased` and 3 is `triple` with a flat
 *  spine. `gap` is centre-to-centre between neighbours. */
Rails rails(int count, float width, Fill fill, float gap = 5.0f);

/** Explicit rails, displacements and all. */
Rails rails(std::vector<Rail> set);

/** The four-rail rule, symmetric — `rails(4, …)` under a name that shows
 *  up in a completion list. */
Rails quad(float width, Fill fill, float gap = 4.0f);

/** The engraver's asymmetric parallel rule: HEAVY / hair / HEAVY — the
 *  commonest printed rule after the plain one. */
Rails heavyHairHeavy(float heavy, float hair, Fill fill, float gap = 5.0f);

/** Solid casing with a DOTTED core — the map convention for a road under
 *  construction, a proposed route, a disused rail. `dotGap` is the spacing
 *  of the core's dots; the casing stays continuous. */
Rails dottedCore(float outer, float core, Fill fill, float gap = 5.0f,
                 float dotGap = 6.0f);

/** Lattice hatching: parallel rules `spacing` px apart at `angleDeg`,
 *  `width` px each, filling the node's OUTLINE — clipped to it, so a
 *  concave shape hatches exactly rather than to its bounds. `cross` adds
 *  the perpendicular pass. A value decoration: compares, prunes and caches
 *  like any other. */
struct Hatch {
  Fill strokeFill = Fill::color({1, 1, 1, 1});
  float spacing = 6.0f;
  float width = 1.2f;
  float angleDeg = 45.0f;
  bool cross = false;
  /** Live pitch and live angle: a raw `Output<float>*`, the same
   *  convention `PathFormat::dashPhaseBinding`, `PathFormat::trimPhase`,
   *  `Line::dashPhaseBinding` and `Rails::dashPhaseBinding` already use.
   *
   *  A raw Output pointer and NOT an `Animatable`, because a decoration
   *  paints with only a `PaintContext` and has no instance against which a
   *  transition could be resolved. Binding either one makes
   *  `isAnimated()` true, which is what declares the node volatile and
   *  keeps a moiré, a tightening engraving or a rotating shade pass
   *  repainting. */
  const choreograph::Output<float>* spacingBinding = nullptr;
  const choreograph::Output<float>* angleBinding = nullptr;

  bool isAnimated() const {
    return spacingBinding != nullptr || angleBinding != nullptr;
  }
  float pitch() const {
    return spacingBinding ? spacingBinding->value() : spacing;
  }
  float angle() const {
    return angleBinding ? angleBinding->value() : angleDeg;
  }

  bool operator==(const Hatch& o) const {
    return strokeFill == o.strokeFill && spacing == o.spacing &&
           width == o.width && angleDeg == o.angleDeg && cross == o.cross &&
           spacingBinding == o.spacingBinding && angleBinding == o.angleBinding;
  }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

Hatch hatch(Fill fill, float spacing = 6.0f, float width = 1.2f,
            float angleDeg = 45.0f);

Hatch crosshatch(Fill fill, float spacing = 6.0f, float width = 1.2f,
                 float angleDeg = 45.0f);

/** RADIAL hatching: rules that fan out of a centre, rings concentric with
 *  it, or both, clipped to the node's outline.
 *
 *  `lines::hatch` is a parallel lattice at one fixed angle, which cannot
 *  describe a field engraved out of a point; approximating one from many
 *  rotated wedges costs a node per wedge for a single field.
 *
 *  `spokes` rules every 360/spokes degrees; `rings` draws circles at even
 *  radii. Set either to 0 for the other alone. `centre` is a FRACTION of
 *  the node's box, so it survives a resize. A value decoration: compares,
 *  prunes and caches like the rest. */
struct RadialHatch {
  Fill strokeFill = Fill::color({1, 1, 1, 1});
  int spokes = 48;
  int rings = 0;
  float width = 1.2f;
  /** Skip the innermost `holeFraction` of the reach — a fan out of a
   *  point crowds to solid ink at the centre otherwise. */
  float holeFraction = 0.08f;
  SkPoint centre = {0.5f, 0.5f};
  float rotateDeg = 0.0f;
  /** STATED ring radii, in px from the centre. When non-empty this list
   *  replaces the `rings` spacing entirely — one circle per entry, exactly
   *  where it says. Use it whenever the radii matter: the even spacing
   *  runs out to the bounding box's HALF-DIAGONAL, so on a circular node
   *  the outermost ring lands at R·√2, outside the shape, and is clipped
   *  away entirely. Spokes keep their own reach either way. */
  std::vector<float> radiiPx;

  bool operator==(const RadialHatch& o) const {
    return strokeFill == o.strokeFill && spokes == o.spokes &&
           rings == o.rings && width == o.width &&
           holeFraction == o.holeFraction && centre == o.centre &&
           rotateDeg == o.rotateDeg && radiiPx == o.radiiPx;
  }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

RadialHatch radialHatch(Fill fill, int spokes = 48, float width = 1.2f,
                        SkPoint centre = {0.5f, 0.5f});

/** The other half of the pair: rings only, no spokes. */
RadialHatch concentric(Fill fill, int rings = 12, float width = 1.2f,
                       SkPoint centre = {0.5f, 0.5f});

/** Rings at STATED radii, px from the centre — `concentric(ink, {60, 64})`
 *  is a two-circle band exactly where it says. The evenly-spaced form
 *  above runs out to the bounding box's half-diagonal, which on a circular
 *  node clips its outermost ring away. */
RadialHatch concentric(Fill fill, std::vector<float> radiiPx,
                       float width = 1.2f, SkPoint centre = {0.5f, 0.5f});

}  // namespace sigil::compose::lines

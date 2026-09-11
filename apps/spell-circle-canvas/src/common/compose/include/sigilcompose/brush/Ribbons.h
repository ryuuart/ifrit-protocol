#pragma once

/** @file
 * SigilCompose brushes — THE SWEPT KINDS: a band the outline carries,
 * rather than a run of stamps or a stroke.
 *
 * `brush::Ribbon` is the variable-width band — a width law swept either
 * side of the spine and filled, joined at the corners, and handed back as
 * geometry by `band()` for anything that has to measure it. `brush::Art`
 * is the same band carrying a rastered ELEMENT instead of a fill, warped
 * along the path through one drawVertices strip. Both are leaf kinds of
 * `Brush` (<sigilcompose/brush/Brushes.h>).
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <sigilgeometry/kit/Shapers.h>

#include <memory>

#include "sigilcompose/Compose.h"

namespace sigil::compose::brush {

/** The variable-width RIBBON: a filled band whose width follows a law —
 *  a linear taper by default, a calligraphic nib when `nibAngleDeg` ≥ 0
 *  (the width peaks where the path runs perpendicular to the nib), or any
 *  `Profile` on the shared width seam — joined at the corners, and handed
 *  back as geometry by `band()` for anything that has to measure it. */
struct Ribbon {
  Fill fill = Fill::color({1, 1, 1, 1});
  /** A Material for the band, superseding `fill` when set — the same
   *  door `Decoration::strokeFill` opens on a stroke, so a recipe
   *  that dresses an outline can dress the ribbon beside it without
   *  being written twice.
   *
   *  Prefer it to `fill` when the same paint also fills something else:
   *  a Material is authored in the unit square, compares structurally,
   *  and can carry live uniforms, where a `Fill` is node-local pixels
   *  compared by shader pointer. A live material makes the ribbon
   *  animated, so the node repaints without a re-describe. */
  std::optional<material::skia::Paint> fillMaterial;
  float widthStart = 10.0f, widthEnd = 2.0f;
  float nibAngleDeg = -1.0f;  ///< ≥0 → calligraphic (widthStart = full)
  float nibContrast = 0.15f;  ///< thinnest fraction at nib-aligned tangents
  float step = 3.0f;  // clamped ≥ 0.5px at paint (0 would never advance)

  /** THE WIDTH LAW, on the shared PROFILE seam.
   *
   *  A `Profile` is `float across(float along)` plus a REQUIRED
   *  `float max()` plus EQUALITY, and both of those additions are
   *  load-bearing. The declared maximum is what `bleed()` reports, so a
   *  wide ribbon cannot be silently clipped by a cull that assumed a
   *  narrow one; the equality is what lets a varying-width ribbon prune,
   *  where a bare width callable would compare unequal forever and
   *  re-record its whole band every frame.
   *
   *  `across(along)` is the FULL width at that fraction of the spine, the
   *  same value `band(spine, across(...))` reads — one vocabulary for a
   *  band's taper, a strand's displacement and a ribbon's width.
   *
   *  **A law that must not slide under a reveal is keyed in PX**: give the
   *  scheme `static constexpr bool alongIsPx = true` and `across` is
   *  handed arc-length px from the spine's start instead of a fraction.
   *  Under a span reveal such as `spans::upTo` the decoration receives the
   *  REVEALED contour, so a fraction is a fraction of what has been drawn
   *  so far and a fraction-keyed law walks along the mark as it writes;
   *  px does not move — provided the reveal is anchored at the spine's
   *  start. A window whose BEGIN moves, or one that wraps, measures px
   *  from the revealed piece's own start. See `PxKeyedProfileScheme`.
   *
   *  Default-constructed means ABSENT: the nib, then the
   *  widthStart→widthEnd taper apply. */
  geometry::path::Profile width;

  /** THE CORNER, on the OUTSIDE of a turn — bevel by default, the chord
   *  the sampled rails already cut; round adds the disc the turn sweeps
   *  out; miter carries the two rails to their meeting point, or bevels
   *  when that point is further than `miterLimit` widths away, which is
   *  Skia's own rule for a stroke.
   *
   *  It shapes the outside only, because the inside of a turn is not a
   *  choice: a band is the UNION of its cross-sections, and this one is
   *  built as one quadrilateral per sampled step, all wound the same way,
   *  so the overlap on the inside of a bend fills. A band zipped into a
   *  single left-forward, right-back contour cannot do that — its inner
   *  rail crosses itself, the crossing winds the wrong way, and the
   *  winding fill DROPS the inside of the bend. The hole opens once the
   *  band is wider than about half the leg it turns on, and it is then
   *  wider than the band itself.
   *
   *  `SkPaint::Join` rather than a word of our own, because this is the
   *  same decision a stroke makes and a caller should not have to learn a
   *  second spelling of it. */
  SkPaint::Join join = SkPaint::kBevel_Join;
  /** How far a miter may reach, in widths, before it bevels instead —
   *  Skia's default of 4, and the reason `bleed()` grows under a miter:
   *  a mitered corner is the one join that reaches past the width. */
  float miterLimit = 4.0f;

  /** Is the profile seam in use? (A default-constructed Profile compares
   *  equal to itself — see Profile::operator== — so this is the honest
   *  presence test, and a zero-width profile paints nothing either way.) */
  bool hasProfile() const { return !(width == geometry::path::Profile{}); }

  float bleed() const {
    const float w = hasProfile() ? width.max() : std::max(widthStart, widthEnd);
    // A bevel and a round join stay inside the width; a miter is allowed
    // to reach `miterLimit` of them, and a bleed that did not say so
    // would clip the one corner the caller asked to be sharp.
    return join == SkPaint::kMiter_Join ? w * std::max(miterLimit, 1.0f) : w;
  }
  bool isAnimated() const { return fillMaterial && fillMaterial->isAnimated(); }
  bool operator==(const Ribbon& o) const {
    return fill == o.fill && fillMaterial == o.fillMaterial &&
           widthStart == o.widthStart && widthEnd == o.widthEnd &&
           nibAngleDeg == o.nibAngleDeg && nibContrast == o.nibContrast &&
           step == o.step && width == o.width && join == o.join &&
           miterLimit == o.miterLimit;
  }

  /** THE BAND THIS RIBBON FILLS over @p spine — the same geometry `paint`
   *  draws, handed back.
   *
   *  Without it a study that wants to MEASURE what a ribbon drew has to
   *  transcribe the construction, and a transcription goes stale the
   *  moment the sampling changes, with the audit then measuring a band
   *  nobody draws. Pair it with `compose::test::widthAlong` to ask
   *  whether the band is the width its profile claims. */
  SkPath band(const SkPath& spine) const;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** The ART brush: ONE art cell stretched and continuously BENT along each
 *  contour. This is what the stamp and tile brushes cannot do — they break
 *  a curve into rigid segments, where this warps the art smoothly through
 *  it.
 *
 *  The art bakes once to a texture at 2× oversample, and each contour is
 *  walked into a triangle-strip ribbon (position ± normal·h per station)
 *  whose texture coordinates sweep the art from one end to the other. One
 *  drawVertices per contour. `stationPx` is the warp fidelity, one station
 *  per that many arc px: a few px follows tight curves, and a larger value
 *  is cheaper on long gentle paths.
 *
 *  THE CACHE IN THIS VALUE IS THE FALLBACK, and missing it costs more
 *  here than for the other brushes, because the bake is a rasterized
 *  texture rather than a picture. Inside a composer it lives in the
 *  INSTANCE's stamp cache, handed in through `PaintContext::stamps` and
 *  keyed on the art's node, so a brush value rebuilt by every describe
 *  still finds it. What defeats that is a NEW ART NODE each describe,
 *  since the node IS the key: keep the art Element pointer-stable. The
 *  member cache here serves standalone paints. */
struct Art {
  Element art;
  float height = 0;        ///< ribbon height (0 → the art's intrinsic)
  float stationPx = 6.0f;  ///< arc-length between strip stations
  /** How far the ribbon escapes the outline: half its height plus what
   *  the art hangs over. The CULL's number — the mark's own width is
   *  `reach()`. */
  float bleedPx = 32.0f;

  bool isAnimated() const { return false; }
  float bleed() const { return bleedPx; }
  /** The mark's full width: the ribbon is centred on the path, so it
   *  spans the reserve on both sides of it. */
  float reach() const { return bleedPx * 2.0f; }
  bool operator==(const Art& o) const {
    return art.node() == o.art.node() && height == o.height &&
           stationPx == o.stationPx && bleedPx == o.bleedPx;
  }

  /** The art's rastered strip, shared by every copy of the brush value
   *  and pinned to the art it came from. */
  struct Cache {
    sk_sp<SkImage> image;  // the 2x bake
    SkSize artSize{0, 0};  // logical art size
    std::weak_ptr<detail::ElementNode> bakedFor;
  };
  std::shared_ptr<Cache> cache = std::make_shared<Cache>();

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** Art warped along the path: the drawVertices ribbon. `height` 0 keeps
 *  the art's intrinsic height. */
Art artAlong(Element art, float height = 0, float stationPx = 6.0f);

/** A Ribbon built on the PROFILE seam — the constructor to prefer, since
 *  the profile is the half of a ribbon that shares a vocabulary with
 *  bands and strands. */
Ribbon ribbon(geometry::path::Profile width, Fill fill);

}  // namespace sigil::compose::brush

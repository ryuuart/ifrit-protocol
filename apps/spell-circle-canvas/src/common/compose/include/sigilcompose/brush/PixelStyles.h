#pragma once

/** @file
 * SigilCompose PIXEL STYLES — the hard-edged interface vocabulary a
 * bitmap-era panel is dressed with, as value decorations beside the
 * blurred ones in `LayerStyles.h`: the BEVEL PAIR (a light edge and a
 * dark edge, raised or sunken as one value), corner BRACKETS standing
 * off a box, a TICK RAIL along one of its edges, and SCANLINES over it.
 *
 * Every one is made of strokes and rectangles on the pixel lattice and
 * never of a blur or a shader, which is what the era looked like: a 1 px
 * highlight and a 1 px shadow, a reticle's four L's, a ruler nobody
 * reads, the raster of the tube the interface was shot on. Every one is
 * a VALUE with defaulted equality, so a panel wearing them prunes and
 * caches like any other static decoration.
 *
 * ATTACHMENT IS THE CONTRACT, as for every decoration: `.overlay()` is
 * the slot a bevel wants — over the fill, under the content and the
 * children — because a bevel put in `.background()` is painted and then
 * covered by the surface it was meant to sit on, and one in
 * `.foreground()` rides over the panel's own label. Brackets, rails and
 * scanlines are usually foregrounds.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <sigilcompose/core/Paint.h>
#include <sigilgeometry/kit/Corners.h>
#include <sigilgeometry/path/Edges.h>

#include <algorithm>

namespace sigil::compose::styles {

/** THE BEVEL PAIR: a light edge on the top and left of the outline and a
 *  dark edge on the bottom and right, each a stroke kept inside the
 *  silhouette — the raised panel of every interface from 1995 to 2005.
 *  SUNKEN IS NOT A SECOND DRAWING: the same two tones on the far edges,
 *  which is what a well, a trough and a pressed button have always been,
 *  and why it is one bool on one value rather than a twin type.
 *
 *  The edges are the outline's sub-contours facing each box edge, so the
 *  pair follows a chamfered panel onto its chamfers. The two vertical
 *  edges are painted first and the two horizontal ones over them, so the
 *  top-right corner belongs to the top edge and the bottom-left to the
 *  bottom — the square corner step a raised panel has, where a mitred
 *  stroke pair would split each corner on the diagonal.
 *
 *  The widths are separate because the era's panels were not symmetric:
 *  a 3 px lift over a 2 px drop is one common spelling, 1 px over 1 px
 *  the other. */
struct BevelPair {
  SkColor4f light = {1, 1, 1, 0.6f};
  SkColor4f dark = {0, 0, 0, 0.5f};
  float lightWidth = 1.0f;
  float darkWidth = 1.0f;
  /** Light on the far edges, dark on the near ones. */
  bool sunken = false;
  /** The edge classification's sampling length in px. */
  float step = 3.0f;

  bool operator==(const BevelPair&) const = default;
  /** The same pair the other way up. */
  [[nodiscard]] BevelPair inverted() const {
    BevelPair other = *this;
    other.sunken = !sunken;
    return other;
  }
  /** How wide the mark is, inside the outline. */
  float reach() const { return std::max(lightWidth, darkWidth); }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** A bevel pair in two stated tones, @p width px each. */
inline BevelPair bevelPair(SkColor4f light, SkColor4f dark, float width = 1.0f,
                           bool sunken = false) {
  return BevelPair{light, dark, width, width, sunken};
}

/** A bevel pair DERIVED from the face it sits on: the light edge is
 *  @p base lightened by @p lift (added per channel, clamped at one) and
 *  the dark edge is @p base with @p drop of its brightness taken away.
 *  Two operations rather than one because a highlight walks toward white
 *  and saturates there, while a shadow keeps the face's hue. A face near
 *  white has nowhere to lighten to and wants its tones stated
 *  outright. */
inline BevelPair bevelPair(SkColor4f base, float lift, float drop,
                           float width = 1.0f, bool sunken = false) {
  return BevelPair{lighten(base, lift), scaleRgb(base, 1.0f - drop), width,
                   width, sunken};
}

/** CORNER BRACKETS standing off a box: an L of @p arm px at each selected
 *  corner, @p gap px inside the edge — the reticle, the selection frame,
 *  the crop mark of an interface that boxes nothing.
 *
 *  They are drawn on the node's BOX, not its outline: a bracket at a gap
 *  from the edge is a box's idea. Marks that belong on a silhouette's own
 *  corners — the brackets of a chamfered frame, on its chamfers — are the
 *  span claim `spans::corners(arm)` and follow the shape. A negative gap
 *  stands the brackets outside the box, and the decoration declares that
 *  reach. */
struct Brackets {
  SkColor4f color = {1, 1, 1, 1};
  float arm = 18.0f;
  float width = 2.0f;
  float gap = 0.0f;
  geometry::shapes::Corner corners = geometry::shapes::Corner::All;
  /** Off (the default) keeps every mark on whole pixels, which is what a
   *  1 px bracket on a screen-shot interface is. */
  bool antiAlias = false;

  bool operator==(const Brackets&) const = default;
  float bleed() const { return std::max(0.0f, -gap); }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

inline Brackets brackets(
    SkColor4f color, float arm = 18.0f, float width = 2.0f, float gap = 0.0f,
    geometry::shapes::Corner corners = geometry::shapes::Corner::All) {
  return Brackets{color, arm, width, gap, corners};
}

/** A TICK RAIL along one edge of the box: a mark every @p pitch px,
 *  @p minor px long, every @p majorEvery-th one @p major px long — the
 *  ruler, the meter edge, the readout nobody reads that makes a panel
 *  denser than anything on it.
 *
 *  The ladder is linear: a mark's position is a distance along the edge,
 *  and the first stands @p phase pitches in, so a half-pitch phase (the
 *  default) keeps the end marks off the corners. Each mark occupies
 *  `[d, d + width)` across the run, which at an integer pitch and width
 *  lands on whole pixels. `edge` is a set: a rail on two edges is one
 *  value. The radial ladder is SigilGeometry's `shapes::ticks`, which
 *  states the angle convention this rail has no use for. */
struct TickRail {
  SkColor4f color = {1, 1, 1, 0.5f};
  float pitch = 8.0f;
  float minor = 4.0f;
  float major = 9.0f;
  float width = 1.0f;
  /** Every n-th mark, counting from the first, is a major one; 0 makes
   *  every mark minor. */
  int majorEvery = 4;
  /** Where the first mark stands, in pitches. */
  float phase = 0.5f;
  geometry::path::Edge edge = geometry::path::Edge::Top;
  bool antiAlias = false;

  bool operator==(const TickRail&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

inline TickRail tickRail(SkColor4f color, float pitch = 8.0f,
                         float minor = 4.0f, float major = 9.0f,
                         int majorEvery = 4,
                         geometry::path::Edge edge = geometry::path::Edge::Top) {
  return TickRail{color, pitch, minor, major, 1.0f, majorEvery, 0.5f, edge};
}

/** SCANLINES over the outline: a band @p on px tall every @p period px,
 *  in @p color through @p blend, clipped inside the shape — the raster of
 *  the tube, laid over a panel as the last thing on it.
 *
 *  Hard rows, deliberately: a 3 px period with a 1 px band is the house
 *  spelling of an interface shot off a monitor, and a raised-cosine beam
 *  is a different picture (`material::field::crtOverlay`, which also
 *  carries the tube's corner falloff). `kPlus` in a tint is the phosphor
 *  reading; source-over in a low black alpha is the print reading.
 *  @p phase slides the rows, in px. */
struct Scanlines {
  SkColor4f color = {0, 0, 0, 0.2f};
  float period = 4.0f;
  float on = 2.0f;
  float phase = 0.0f;
  SkBlendMode blend = SkBlendMode::kSrcOver;

  bool operator==(const Scanlines&) const = default;

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

inline Scanlines scanlines(SkColor4f color, float period = 4.0f,
                           float on = 2.0f,
                           SkBlendMode blend = SkBlendMode::kSrcOver) {
  return Scanlines{color, period, on, 0.0f, blend};
}

}  // namespace sigil::compose::styles

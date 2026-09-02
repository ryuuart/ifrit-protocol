#pragma once

/** @file
 * SigilCompose KIT — a node placed by the numbers a plate was measured in.
 *
 * The coordinate systems themselves are SigilGeometry's
 * (`geometry::path::Frame`, the polar one; `geometry::path::Grid`, the
 * unit map). What is here is the three ways a NODE is placed by them: a
 * disc about a centre, a pinned box at absolute coordinates, and the disc
 * a frame's own radius names.
 */

#include <include/core/SkPoint.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Layout.h>
#include <sigilgeometry/path/Frame.h>

#include <utility>

namespace sigil::compose::kit {

// ---------------------------------------------------------------------------
// Placement values — the arithmetic every polar plate spells out

/** A box of `radius` about `centre` — the polar-chart placement. Every
 *  inscribed-in-the-box generator (sector, arc, circle, star) otherwise
 *  needs `width(2r).height(2r).centerAt(c)` spelled out at its call
 *  site. */
inline Element disc(SkPoint centre, float radius) {
  return box().width(Dim(radius * 2)).height(Dim(radius * 2)).centerAt(centre);
}

/** THE PINNED BOX: a node at absolute `(x, y)` of size @p w × @p h.
 *
 *  `box().left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h))` is the
 *  four-call spelling every plate that has no layout at all repeats — a
 *  transcribed interface, an engraved plate, a pattern card. Reconstructing
 *  a reference means quoting coordinates measured off it, and there is
 *  nothing for a flexbox to decide.
 *
 *  It is a peer of `disc()` rather than a layout scheme: it decides
 *  nothing, and every number is the caller's. */
inline Element at(float x, float y, float w, float h) {
  return box().left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h));
}
/** The same, onto an element that already exists — the overload a caller
 *  reaches for when the node is built elsewhere and only its position is
 *  the plate's business. */
inline Element at(Element e, float x, float y, float w, float h) {
  e.left(Dim(x)).top(Dim(y)).width(Dim(w)).height(Dim(h));
  return e;
}

/** `disc` at @p frame: an Element sized and centred for a
 *  `shapes::circle()`/`sector()`/`arc()` outline at @p rNorm of the
 *  frame's radius. */
inline Element disc(const geometry::path::Frame& frame, float rNorm = 1.0f) {
  return disc(frame.centre, rNorm * frame.radius);
}

}  // namespace sigil::compose::kit

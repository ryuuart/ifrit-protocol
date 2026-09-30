/** @file
 * The wire a connecting operator attaches: the route between two rects or
 * through a run of stops, re-based into a figure of its own, dressed and
 * keyed.
 */

#include "sigilcompose/kit/Connect.h"

#include <sigilgeometry/advanced/Skia.h>

#include <utility>

namespace sigil::compose::connect {

namespace {

/** The routed path as a figure of its own: a box grown by @p bleed around
 *  the path, so the dress's width is not clipped, and the path held in it
 *  as the element's shape — which is what every mark then dresses. */
Element figureOf(const geometry::path::Outline& route, float bleed, std::string key,
                 const Dressing& dressing) {
  Element figure = pathFigure(route, bleed);
  if (dressing.mark) {
    // A stated run CLAIMS that part of the boundary, which is what fits a
    // mark to a reveal; the whole wire is an unqualified pass.
    if (dressing.where)
      figure.stroke(*dressing.where, *dressing.mark);
    else
      figure.foreground(*dressing.mark);
  }
  for (const Decoration& mark : dressing.style) figure.foreground(mark);
  if (dressing.gate) figure.mask(*dressing.gate);
  // The wire's shape is one open path, which the hit test answers for
  // along its line rather than inside its closure, so a keyed wire is
  // found under the pointer as the nodes it joins are.
  if (!key.empty()) figure.key(std::move(key));
  return figure;
}

}  // namespace

Element wire(const geometry::path::Rect& from, const geometry::path::Rect& to,
             const Router& router, float gap, float bleed, std::string key,
             const Dressing& dressing) {
  return figureOf(routeBetween(router, from, to, gap), bleed, std::move(key),
                  dressing);
}

Element wire(std::span<const glm::vec2> stops, const RailRouter& router,
             float gapStart, float gapEnd, float bleed, std::string key,
             const Dressing& dressing) {
  return figureOf(routeAlong(router, stops, gapStart, gapEnd), bleed,
                  std::move(key), dressing);
}

}  // namespace sigil::compose::connect

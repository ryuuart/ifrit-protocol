/** @file
 * The wire a connecting operator attaches: the route between two rects or
 * through a run of stops, re-based into a figure of its own, dressed and
 * keyed.
 */

#include "sigilcompose/kit/Connect.h"

#include <utility>

namespace sigil::compose::connect {

namespace {

/** The routed path as a figure of its own: a box grown by @p bleed around
 *  the path, so the dress's width is not clipped, and the path held in it
 *  as the element's shape — which is what every mark then dresses. */
Element figureOf(SkPath route, float bleed, std::string key,
                 const Dressing& dressing) {
  Element figure = pathFigure(std::move(route), bleed);
  if (dressing.mark) {
    // A stated run CLAIMS that part of the boundary, which is what fits a
    // mark to a reveal; the whole wire is an unqualified pass.
    if (dressing.where)
      figure.stroke(*dressing.where, *dressing.mark);
    else
      figure.foreground(*dressing.mark);
  }
  if (!dressing.style.under.empty() || !dressing.style.over.empty() ||
      !dressing.style.echoes.empty())
    figure.layerStyle(dressing.style);
  if (dressing.gate) figure.mask(*dressing.gate);
  if (!key.empty()) figure.key(std::move(key));
  // A WIRE IS TRANSPARENT TO THE HIT TEST. Its shape is one open path, and
  // containment in an open path is the region the fill's implicit close
  // encloses — the lens under an arc, the triangle inside an elbow — which
  // is nowhere near the mark anyone can see. A wire laid over a diagram
  // would answer for hits on the empty space beside it, so it answers for
  // none; the nodes it joins answer for themselves.
  figure.hitTestable(false);
  return figure;
}

}  // namespace

Element wire(const SkRect& from, const SkRect& to, const Router& router,
             float gap, float bleed, std::string key,
             const Dressing& dressing) {
  return figureOf(routeBetween(router, from, to, gap), bleed, std::move(key),
                  dressing);
}

Element wire(std::span<const SkPoint> stops, const RailRouter& router,
             float gapStart, float gapEnd, float bleed, std::string key,
             const Dressing& dressing) {
  return figureOf(routeAlong(router, stops, gapStart, gapEnd), bleed,
                  std::move(key), dressing);
}

}  // namespace sigil::compose::connect

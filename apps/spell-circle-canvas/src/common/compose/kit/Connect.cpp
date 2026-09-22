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
 *  the path, so the decoration's width is not clipped, and the path held
 *  in it as the element's shape. */
Element figureOf(SkPath route, const std::optional<Decoration>& dress,
                 float bleed, std::string key) {
  Element figure = pathFigure(std::move(route), bleed);
  if (dress) figure.foreground(*dress);
  if (!key.empty()) figure.key(std::move(key));
  return figure;
}

}  // namespace

Element wire(const SkRect& from, const SkRect& to, const Router& router,
             float gap, const std::optional<Decoration>& dress, float bleed,
             std::string key) {
  return figureOf(routeBetween(router, from, to, gap), dress, bleed,
                  std::move(key));
}

Element wire(std::span<const SkPoint> stops, const RailRouter& router,
             float gapStart, float gapEnd,
             const std::optional<Decoration>& dress, float bleed,
             std::string key) {
  return figureOf(routeAlong(router, stops, gapStart, gapEnd), dress, bleed,
                  std::move(key));
}

}  // namespace sigil::compose::connect

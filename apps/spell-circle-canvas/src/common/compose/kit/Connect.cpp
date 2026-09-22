/** @file
 * The wire a connecting operator attaches: the route between two rects,
 * re-based into a figure of its own, dressed and keyed.
 */

#include "sigilcompose/kit/Connect.h"

namespace sigil::compose::connect {

Element wire(const SkRect& from, const SkRect& to, const Router& router,
             float gap, const std::optional<Decoration>& dress, float bleed,
             std::string key) {
  Element figure = pathFigure(routeBetween(router, from, to, gap), bleed);
  if (dress) figure.foreground(*dress);
  if (!key.empty()) figure.key(key);
  return figure;
}

}  // namespace sigil::compose::connect

/** @file
 * The pinning operator: every request read off the scope's nodes, hung
 * where its tether says and, when that leaves the box it must stay in,
 * at the first fallback that fits.
 */

#include "sigilcompose/kit/Pin.h"

namespace sigil::compose::pin {

void ByLane::add(Scope& scope) const {
  for (const Scope::Node* node : scope.having(lane)) {
    const std::optional<Request> request = node->attribute<Request>(lane);
    if (!request) continue;
    // The stated tether is tried first, then each fallback in the order
    // it was written, and the first placement that lies inside its box
    // is taken; when none fits, the stated one stands, so an element
    // that fits nowhere is still hung where it was asked for.
    const auto within = [&](const Tether& tether) {
      return tether.within.isEmpty() ? scope.box : tether.within;
    };
    SkRect placed = request->where.place(node->bounds, request->size);
    if (!within(request->where).contains(placed)) {
      for (const Tether& fallback : request->where.fallbacks) {
        const SkRect tried = fallback.place(node->bounds, request->size);
        if (within(fallback).contains(tried)) {
          placed = tried;
          break;
        }
      }
    }
    Element hung = request->element;
    hung.key(node->key + "-pin").rect(placed);
    scope.attach(std::move(hung));
  }
}

}  // namespace sigil::compose::pin

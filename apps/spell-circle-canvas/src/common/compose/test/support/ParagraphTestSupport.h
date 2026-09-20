#pragma once
// What every case about a paragraph reads with: the green-ink probe an
// annotation or a decoration is found by, the type partial a restyled
// span is read back in, and the two passages long enough to wrap and to
// be worth composing.

#include "TextTestSupport.h"

namespace {

/** Any green pixel in a region — the probe a boundary assertion reads. */
bool anyGreenIn(Host& host, SkIRect region) {
  for (int y = region.top(); y < region.bottom(); ++y)
    for (int x = region.left(); x < region.right(); ++x)
      if (host.pixel(x, y) == SK_ColorGREEN) return true;
  return false;
}

/** A partial of a stated size in a stated colour, over whatever the text
 *  it lands on is set in — the probe a span restyle is read back with. */
sigil::weave::Type colouredType(float size, SkColor colour) {
  return {.size = size, .color = SkColor4f::FromColor(colour)};
}

/** A passage long enough to wrap at the measures the cases set. */
std::u8string passage() {
  return u8"One two three four five six seven eight nine ten eleven twelve.";
}

/** A passage long enough that a frame of it is worth composing — past the
 *  break position the optimizing breaker tests its floor at, so a starved
 *  block can notice it has run out. */
std::u8string longPassage() {
  std::u8string out;
  for (int i = 0; i < 30; ++i) out += passage() + u8" ";
  return out;
}

}  // namespace

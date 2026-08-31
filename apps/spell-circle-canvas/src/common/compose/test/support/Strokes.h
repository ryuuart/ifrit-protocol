#pragma once
// Stroked fixtures and the pixel rings that read them back: a straight
// open run dressed by a line style, and a fully stroked box whose boundary
// the trim and span comparisons sample.

#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <vector>

#include "Host.h"

namespace {

Element straightRun(Decoration style) {
  // A horizontal open path across the node, dressed by the line style.
  return box().child(box()
                         .absolute()
                         .inset(20, 80, 20, 80)
                         .shape([](SkSize s) {
                           SkPathBuilder b;
                           b.moveTo(0, s.height() / 2);
                           b.lineTo(s.width(), s.height() / 2);
                           return b.detach();
                         })
                         .stroke(std::move(style)));
}

/** A box whose whole boundary is stroked, for the trim/span comparisons. */
Element revealBox() { return box().rect(SkRect::MakeXYWH(20, 20, 100, 100)); }

/** A ring of samples around the stroked boundary above — enough of them
 *  that a window landing in the wrong place cannot hide. */
std::vector<SkColor> boundaryRing(Host& host) {
  std::vector<SkColor> out;
  for (int x = 15; x <= 125; x += 2) out.push_back(host.pixel(x, 20));
  for (int y = 15; y <= 125; y += 2) out.push_back(host.pixel(120, y));
  for (int x = 125; x >= 15; x -= 2) out.push_back(host.pixel(x, 120));
  for (int y = 125; y >= 15; y -= 2) out.push_back(host.pixel(20, y));
  return out;
}

size_t inkedCount(const std::vector<SkColor>& ring) {
  return (size_t)std::count_if(ring.begin(), ring.end(),
                               [](SkColor c) { return c != SK_ColorBLACK; });
}

}  // namespace

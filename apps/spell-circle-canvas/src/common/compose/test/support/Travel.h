#pragma once
// The travel scene: a frame that carries one small square along a path,
// and the pixel scans its cases read, because motion is a visual feature
// and these cases look at the frame rather than at the resolver.

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>

#include "Host.h"

namespace {

/** The centroid of every pixel of @p color, or (-1,-1) when none. Motion is
 *  a VISUAL feature: these pins scan the frame, they do not read floats out
 *  of the resolver. */
SkPoint inkCentroid(Host& host, SkColor color, int width, int height) {
  double sumX = 0, sumY = 0;
  int count = 0;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      if (host.pixel(x, y) == color) {
        sumX += x;
        sumY += y;
        ++count;
      }
  if (count == 0) return {-1, -1};
  return {(float)(sumX / count), (float)(sumY / count)};
}

/** The bounding box of everything even faintly @p color-ish — enough to
 *  ask "is this bar lying flat or standing up", which is what an
 *  orientation pin actually wants to know. */
SkIRect inkBounds(Host& host, int width, int height) {
  SkIRect box = SkIRect::MakeEmpty();
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      if (SkColorGetR(host.pixel(x, y)) > 100) {
        const SkIRect one = SkIRect::MakeXYWH(x, y, 1, 1);
        if (box.isEmpty())
          box = one;
        else
          box.join(one);
      }
  return box;
}

/** A 160x160 frame inset at (20,20) of a 200x200 canvas, carrying one small
 *  square that travels. The inscribed circle is then centre (100,100) r=80
 *  in CANVAS coordinates, so every quadrant point is on-screen. */
Element travelFrame(Element rider) {
  return box().children({box()
                             .key("frame")
                             .absolute()
                             .rect(20, 20, 160, 160)
                             .children({std::move(rider)})});
}

Element rider(MotionPath along, float size = 8) {
  return box()
      .key("dot")
      .absolute()
      .rect(0, 0, size, size)
      .fill(red())
      .travel(std::move(along));
}

}  // namespace

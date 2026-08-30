/** @file
 * The girih panel's tile program: the ground, the khatam, the outlined
 * straps and the corner fillers.
 */

#include "sigilmaterial/kit/Patterns.h"

#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>

namespace sigil::material::kit {

namespace {
SkColor4f sk(Color c) { return {c.r, c.g, c.b, c.a}; }
}  // namespace

pattern::Tile girih8(float edge, GirihPalette pal, float strapWidth) {
  const float a = std::max(edge, 4.0f);
  const float s = a * (1.0f + 1.41421356f);
  const float w = strapWidth > 0 ? strapWidth : 0.12f * a;
  return pattern::Tile::of(
      {s, s}, [a, s, w, pal](SkCanvas& c, SkSize, uint32_t) {
        SkPaint fill;
        fill.setAntiAlias(true);

        // Ground: the crosses ARE the leftover between stars + corner squares.
        fill.setColor4f(sk(pal.ground), nullptr);
        c.drawRect(SkRect::MakeWH(s, s), fill);

        // Khatam at the tile centre: two squares through the octagon's 8 edge
        // midpoints (radius = apothem = s/2, at k·45°). Winding fill = union.
        const SkPoint center{s / 2, s / 2};
        const float R = s / 2;
        auto ringPoint = [&](int k, float radius, SkPoint at) {
          const float ang = (float)k * 0.78539816f;  // 45°
          return SkPoint{at.x() + radius * std::cos(ang),
                         at.y() + radius * std::sin(ang)};
        };
        auto squarePath = [&](int k0) {
          SkPathBuilder b;
          b.moveTo(ringPoint(k0, R, center));
          for (int i = 1; i < 4; ++i)
            b.lineTo(ringPoint(k0 + 2 * i, R, center));
          b.close();
          return b.detach();
        };
        SkPath khatamA = squarePath(0), khatamB = squarePath(1);
        SkPathBuilder khatam;
        khatam.addPath(khatamA);
        khatam.addPath(khatamB);
        fill.setColor4f(sk(pal.star), nullptr);
        c.drawPath(khatam.detach(), fill);  // winding → the 8-point star union

        // Straps: outlined ribbons (dark wide pass, then the strap on top).
        auto ribbon = [&](const SkPath& path) {
          SkPaint p;
          p.setAntiAlias(true);
          p.setStyle(SkPaint::kStroke_Style);
          p.setStrokeJoin(SkPaint::kMiter_Join);
          p.setStrokeWidth(w * 1.5f);
          p.setColor4f(sk(pal.strapEdge), nullptr);
          c.drawPath(path, p);
          p.setStrokeWidth(w);
          p.setColor4f(sk(pal.strap), nullptr);
          c.drawPath(path, p);
        };
        ribbon(khatamA);
        ribbon(khatamB);

        // Corner fillers: each tile corner carries the 4.8.8 square (edge a,
        // rotated 45°); the 45° contact inscribes the square through ITS edge
        // midpoints (radius a/2 at 45°+k·90°). Drawn with wraparound copies so
        // the repeat is seamless.
        const SkPoint corners[4] = {{0, 0}, {s, 0}, {0, s}, {s, s}};
        for (const SkPoint& corner : corners) {
          SkPathBuilder b;
          for (int k = 0; k < 4; ++k) {
            const float ang = 0.78539816f + (float)k * 1.57079633f;
            const SkPoint pt{corner.x() + (a / 2) * std::cos(ang),
                             corner.y() + (a / 2) * std::sin(ang)};
            if (k == 0)
              b.moveTo(pt);
            else
              b.lineTo(pt);
          }
          b.close();
          ribbon(b.detach());
        }
      });
}

}  // namespace sigil::material::kit

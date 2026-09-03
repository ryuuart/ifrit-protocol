/** @file
 * The girih panel's tile program: the ground, the khatam, the outlined
 * straps and the corner fillers — the classic 45° panel in closed form,
 * and Hankin's rays at any other contact angle.
 */

#include "sigilmaterial/kit/Patterns.h"

#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>

namespace sigil::material::kit {

namespace {
SkColor4f sk(Color c) { return {c.r, c.g, c.b, c.a}; }

constexpr float kPi = 3.14159265f;

/** The straps' two-pass ribbon: a dark wide pass, then the strap on top. */
void ribbon(SkCanvas& c, const SkPath& path, float w, const GirihPalette& pal) {
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
}

/** THE CLASSIC PANEL, at the 45° contact: the khatam is two squares
 *  through the octagon's eight edge midpoints, whose union is the
 *  {8/2} star and whose outlines are the interlace; each corner filler
 *  is the square inscribed through its own edge midpoints. */
void classic(SkCanvas& c, float a, float s, float w, const GirihPalette& pal) {
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
    for (int i = 1; i < 4; ++i) b.lineTo(ringPoint(k0 + 2 * i, R, center));
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
  ribbon(c, khatamA, w, pal);
  ribbon(c, khatamB, w, pal);

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
    ribbon(c, b.detach(), w, pal);
  }
}

/** HANKIN'S RAYS on one regular n-gon at contact angle @p theta (radians
 *  from the edge): two rays leave every edge midpoint, and where the rays
 *  of two neighbouring edges first meet — on the bisector between them —
 *  is a vertex of the star. The star's outer vertices are the midpoints
 *  themselves, at the apothem; its inner ones stand at
 *  apothem·cos θ / cos(θ − π/n). Past that first contact a ray runs on to
 *  meet the ray of the next-but-one edge on the axis through the edge
 *  between, at apothem·cos θ / cos(θ − 2π/n) — inside the star whenever
 *  θ is at least half the turn between edges — and that continuation is
 *  the interlace: at 45° on an octagon the two meetings are collinear and
 *  the straps are the classic squares. Below that angle the rays stop at
 *  first contact and the straps are the star's own outline. */
struct Rays {
  int n;
  SkPoint centre;
  float apothem;
  float phase;  ///< the angle of the first edge midpoint
  float theta;

  float turn() const { return 2.0f * kPi / (float)n; }
  bool crossing() const { return theta >= turn() * 0.5f - 1e-4f; }
  SkPoint polar(float r, float ang) const {
    return {centre.x() + r * std::cos(ang), centre.y() + r * std::sin(ang)};
  }
  SkPoint midpoint(int k) const {
    return polar(apothem, phase + (float)k * turn());
  }
  SkPoint contact(int k) const {
    const float half = turn() * 0.5f;
    return polar(apothem * std::cos(theta) / std::cos(theta - half),
                 phase + (float)k * turn() + half);
  }
  SkPoint cross(int k) const {
    return polar(apothem * std::cos(theta) / std::cos(theta - turn()),
                 phase + (float)(k + 1) * turn());
  }
  /** The star: midpoints and first contacts alternating. */
  SkPath star() const {
    SkPathBuilder b;
    b.moveTo(midpoint(0));
    for (int k = 0; k < n; ++k) {
      b.lineTo(contact(k));
      b.lineTo(midpoint(k + 1));
    }
    b.close();
    return b.detach();
  }
  /** The strap that leaves midpoint k: to its first contact, and on
   *  through the crossing to the next-but-one midpoint where the angle
   *  allows. */
  SkPath strap(int k) const {
    SkPathBuilder b;
    b.moveTo(midpoint(k));
    b.lineTo(contact(k));
    if (crossing()) {
      b.lineTo(cross(k));
      b.lineTo(contact(k + 1));
      b.lineTo(midpoint(k + 2));
    } else {
      b.lineTo(midpoint(k + 1));
    }
    return b.detach();
  }
};

/** The panel at any contact angle: the same lattice, the octagon's star
 *  filled and its straps ribboned, the fillers' straps at the corners. */
void hankin(SkCanvas& c, float a, float s, float w, float theta,
            const GirihPalette& pal) {
  SkPaint fill;
  fill.setAntiAlias(true);
  fill.setColor4f(sk(pal.ground), nullptr);
  c.drawRect(SkRect::MakeWH(s, s), fill);

  const Rays octagon{8, {s / 2, s / 2}, s / 2, 0.0f, theta};
  fill.setColor4f(sk(pal.star), nullptr);
  c.drawPath(octagon.star(), fill);
  // The straps leave the octagon at the four midpoints on the tile's
  // edges and continue into the neighbouring tile, so the neighbours'
  // copies are drawn too and the repeat is seamless.
  for (int dy = -1; dy <= 1; ++dy)
    for (int dx = -1; dx <= 1; ++dx) {
      Rays copy = octagon;
      copy.centre = {s / 2 + (float)dx * s, s / 2 + (float)dy * s};
      for (int k = 0; k < 8; ++k) ribbon(c, copy.strap(k), w, pal);
    }

  // The corner fillers: the 4.8.8 square, its midpoints at a/2 on the
  // diagonals, one at each tile corner so the four quarters make one.
  const SkPoint corners[4] = {{0, 0}, {s, 0}, {0, s}, {s, s}};
  for (const SkPoint& corner : corners) {
    const Rays square{4, corner, a / 2, kPi / 4, theta};
    for (int k = 0; k < 4; ++k) ribbon(c, square.strap(k), w, pal);
  }
}

}  // namespace

pattern::Tile girih8(float edge, GirihPalette pal, float strapWidth,
                     float contactDeg) {
  const float a = std::max(edge, 4.0f);
  const float s = a * (1.0f + 1.41421356f);
  const float w = strapWidth > 0 ? strapWidth : 0.12f * a;
  const float theta = std::clamp(contactDeg, 5.0f, 85.0f);
  // The classic angle keeps its closed form: the two squares ARE the
  // rays at 45°, and drawing them as squares is what the panel has always
  // been.
  if (std::abs(theta - 45.0f) < 1e-3f)
    return pattern::Tile::of({s, s}, [a, s, w, pal](SkCanvas& c, SkSize,
                                                    uint32_t) {
      classic(c, a, s, w, pal);
    });
  const float radians = theta * kPi / 180.0f;
  return pattern::Tile::of(
      {s, s}, [a, s, w, radians, pal](SkCanvas& c, SkSize, uint32_t) {
        hankin(c, a, s, w, radians, pal);
      });
}

}  // namespace sigil::material::kit

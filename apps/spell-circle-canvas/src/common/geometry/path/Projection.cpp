#include "sigilgeometry/path/Projection.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>

#include "sigilgeometry/path/Numeric.h"

namespace sigil::geometry::path {
namespace {

/** An angle brought into (-180, 180]. */
float signed180(float deg) { return wrap(deg + 180.0f, 360.0f) - 180.0f; }

/** Mercator's ordinate at a latitude, in units of the scale: the integral
 *  of the secant, which runs away at the poles. Held just short of them,
 *  since a chart drawn to infinity has no bounds anybody can measure. */
float mercatorOrdinate(float latDeg) {
  const float lat = std::clamp(latDeg, -89.9999f, 89.9999f);
  return std::log(std::tan(radians(45.0f + lat * 0.5f)));
}

/** The direction of increasing longitude at a point — east, which is
 *  well defined at a pole too, where it is the direction square to that
 *  point's own meridian. */
glm::vec3 eastAt(Spherical s) {
  const float lon = radians(s.lonDeg);
  return {-std::sin(lon), std::cos(lon), 0};
}

/** Any unit vector square to @p v — the bearing a construction sets out
 *  along when the direction it would have used is not a direction. */
glm::vec3 anyPerpendicular(glm::vec3 v) {
  const glm::vec3 other =
      std::abs(v.z) < 0.9f ? glm::vec3{0, 0, 1} : glm::vec3{1, 0, 0};
  return glm::normalize(glm::cross(v, other));
}

}  // namespace

// ---------------------------------------------------------------------------
// Spherical.

glm::vec3 Spherical::direction() const {
  const float lat = radians(latDeg), lon = radians(lonDeg);
  const float cosLat = std::cos(lat);
  return {cosLat * std::cos(lon), cosLat * std::sin(lon), std::sin(lat)};
}

Spherical Spherical::of(glm::vec3 direction) {
  const float len = glm::length(direction);
  if (!(len > 0)) return {};
  const glm::vec3 v = direction / len;
  return {wrap(degrees(std::atan2(v.y, v.x)), 360.0f),
          degrees(std::asin(std::clamp(v.z, -1.0f, 1.0f)))};
}

float angleBetween(Spherical a, Spherical b) {
  const float chord = glm::length(a.direction() - b.direction());
  return degrees(2.0f * std::asin(std::clamp(chord * 0.5f, 0.0f, 1.0f)));
}

Spherical offsetFrom(Spherical from, float bearingDeg, float arcDeg) {
  const float lat = radians(from.latDeg), lon = radians(from.lonDeg);
  const float sinLat = std::sin(lat), cosLat = std::cos(lat);
  const glm::vec3 north{-sinLat * std::cos(lon), -sinLat * std::sin(lon),
                        cosLat};
  const glm::vec3 east{-std::sin(lon), std::cos(lon), 0};
  const float bearing = radians(bearingDeg), arc = radians(arcDeg);
  const glm::vec3 along = std::cos(bearing) * north + std::sin(bearing) * east;
  return Spherical::of(std::cos(arc) * from.direction() +
                       std::sin(arc) * along);
}

// ---------------------------------------------------------------------------
// Rotation.

glm::vec3 Rotation::operator()(glm::vec3 direction) const {
  return basis * direction;
}

Spherical Rotation::operator()(Spherical direction) const {
  return Spherical::of(basis * direction.direction());
}

Rotation Rotation::inverse() const { return {glm::transpose(basis)}; }

Rotation Rotation::then(const Rotation& next) const {
  return {next.basis * basis};
}

Rotation Rotation::aboutX(float deg) {
  const float c = std::cos(radians(deg)), s = std::sin(radians(deg));
  return {glm::mat3{1, 0, 0, 0, c, s, 0, -s, c}};
}

Rotation Rotation::aboutY(float deg) {
  const float c = std::cos(radians(deg)), s = std::sin(radians(deg));
  return {glm::mat3{c, 0, -s, 0, 1, 0, s, 0, c}};
}

Rotation Rotation::aboutZ(float deg) {
  const float c = std::cos(radians(deg)), s = std::sin(radians(deg));
  return {glm::mat3{c, s, 0, -s, c, 0, 0, 0, 1}};
}

Rotation Rotation::zyz(float aDeg, float bDeg, float cDeg) {
  return aboutZ(cDeg).then(aboutY(bDeg)).then(aboutZ(aDeg));
}

// ---------------------------------------------------------------------------
// Projection.

bool Projection::azimuthal() const {
  return scheme == Scheme::Stereographic || scheme == Scheme::Orthographic ||
         scheme == Scheme::AzimuthalEquidistant;
}

float Projection::radiusAt(float arcDeg) const {
  switch (scheme) {
    case Scheme::Stereographic: {
      // The far point of the sphere is infinitely far up the paper, and a
      // path carrying a point there has no bounds worth reading, so the
      // half-angle is held just short of the turn that puts it there.
      const float half = std::clamp(radians(arcDeg) * 0.5f, -1.5707f, 1.5707f);
      return 2.0f * scale * std::tan(half);
    }
    case Scheme::Orthographic:
      return scale * std::sin(radians(arcDeg));
    case Scheme::AzimuthalEquidistant:
    case Scheme::Equirectangular:
      return scale * radians(arcDeg);
    case Scheme::Mercator:
      return scale * (mercatorOrdinate(centre.latDeg + arcDeg) -
                      mercatorOrdinate(centre.latDeg));
  }
  return 0;
}

float Projection::arcAtRadius(float radius) const {
  switch (scheme) {
    case Scheme::Stereographic:
      return degrees(2.0f * std::atan(radius / (2.0f * scale)));
    case Scheme::Orthographic:
      return degrees(std::asin(std::clamp(radius / scale, -1.0f, 1.0f)));
    case Scheme::AzimuthalEquidistant:
    case Scheme::Equirectangular:
      return degrees(radius / scale);
    case Scheme::Mercator: {
      const float y = radius / scale + mercatorOrdinate(centre.latDeg);
      return degrees(2.0f * std::atan(std::exp(y))) - 90.0f - centre.latDeg;
    }
  }
  return 0;
}

float Projection::angleFrom(Spherical direction) const {
  return angleBetween(centre, direction);
}

glm::vec2 Projection::at(Spherical direction) const {
  glm::vec2 p{0, 0};
  if (azimuthal()) {
    const glm::vec3 u = centre.direction();
    const glm::vec3 east = eastAt(centre);
    const glm::vec3 north = glm::cross(u, east);
    const glm::vec3 v = direction.direction();
    const float alongEast = glm::dot(v, east), alongNorth = glm::dot(v, north);
    // atan2 of the across-the-map component against the along-the-axis one,
    // rather than an acos of the dot product, which loses its digits for
    // exactly the directions nearest the centre.
    const float arc =
        degrees(std::atan2(std::hypot(alongEast, alongNorth), glm::dot(v, u)));
    const float bearing = std::atan2(alongNorth, alongEast);
    const float r = radiusAt(arc);
    p = {r * std::cos(bearing), r * std::sin(bearing)};
  } else {
    p = {scale * radians(signed180(direction.lonDeg - centre.lonDeg)),
         radiusAt(direction.latDeg - centre.latDeg)};
  }
  if (vantage == Vantage::Outside) p.x = -p.x;
  const float roll = radians(rollDeg);
  const float c = std::cos(roll), s = std::sin(roll);
  return {p.x * c - p.y * s, p.x * s + p.y * c};
}

Spherical Projection::from(glm::vec2 point) const {
  const float roll = radians(rollDeg);
  const float c = std::cos(roll), s = std::sin(roll);
  glm::vec2 p{point.x * c + point.y * s, -point.x * s + point.y * c};
  if (vantage == Vantage::Outside) p.x = -p.x;
  if (azimuthal()) {
    const glm::vec3 u = centre.direction();
    const glm::vec3 east = eastAt(centre);
    const glm::vec3 north = glm::cross(u, east);
    const float arc = radians(arcAtRadius(glm::length(p)));
    const float bearing = std::atan2(p.y, p.x);
    return Spherical::of(
        std::cos(arc) * u +
        std::sin(arc) * (std::cos(bearing) * east + std::sin(bearing) * north));
  }
  return {wrap(centre.lonDeg + degrees(p.x / scale), 360.0f),
          std::clamp(centre.latDeg + arcAtRadius(p.y), -90.0f, 90.0f)};
}

std::optional<PlaneCircle> Projection::circleOf(Spherical pole,
                                                float arcDeg) const {
  if (scheme != Scheme::Stereographic) return std::nullopt;
  const glm::vec3 u = centre.direction();
  const glm::vec3 q = pole.direction();
  // The great circle through the map's centre and the circle's pole cuts
  // the circle at its two extremes, and those two are the ends of a
  // diameter of the image — so the image is found from the raw point
  // formula alone, with no closed form to keep in step with it.
  const glm::vec3 toward = u - glm::dot(u, q) * q;
  const float reach = glm::length(toward);
  const glm::vec3 along = reach > 1e-5f ? toward / reach : anyPerpendicular(q);
  const float arc = radians(arcDeg);
  const glm::vec3 nearEnd = std::cos(arc) * q + std::sin(arc) * along;
  const glm::vec3 farEnd = std::cos(arc) * q - std::sin(arc) * along;
  // A circle through the point the projection is taken FROM is a straight
  // line on the plane and has no centre to answer with.
  if (glm::dot(nearEnd, u) < -0.999999f || glm::dot(farEnd, u) < -0.999999f)
    return std::nullopt;
  const glm::vec2 a = at(Spherical::of(nearEnd));
  const glm::vec2 b = at(Spherical::of(farEnd));
  return PlaneCircle{(a + b) * 0.5f, glm::length(a - b) * 0.5f};
}

Projection Projection::centredOn(Spherical newCentre) const {
  Projection out = *this;
  out.centre = newCentre;
  return out;
}

std::optional<PlaneCircle> circleThrough(glm::vec2 a, glm::vec2 b,
                                         glm::vec2 c) {
  const glm::vec2 ab = b - a, ac = c - a;
  const float turn = ab.x * ac.y - ab.y * ac.x;
  // The sine of the turn at `a`, which is what the circumradius divides
  // by: three points that bend by less than this are one straight line,
  // and a projection's own families lean on that being answered rather
  // than approximated by an enormous circle.
  if (std::abs(turn) <= 1e-7f * glm::length(ab) * glm::length(ac))
    return std::nullopt;
  const float aa = glm::dot(a, a), bb = glm::dot(b, b), cc = glm::dot(c, c);
  const float d =
      2.0f * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
  const glm::vec2 centre{
      (aa * (b.y - c.y) + bb * (c.y - a.y) + cc * (a.y - b.y)) / d,
      (aa * (c.x - b.x) + bb * (a.x - c.x) + cc * (b.x - a.x)) / d};
  return PlaneCircle{centre, glm::length(a - centre)};
}

}  // namespace sigil::geometry::path

#pragma once
/** @file
 * The sphere laid onto a plane, as a value — and the rotation that turns
 * the sphere before it is laid.
 *
 * A map is two decisions and nothing else: WHERE the sphere is looked at
 * from, and WHAT LAW carries an angle out from there into a distance on
 * the paper. Written as free functions, those decisions turn into a
 * scatter of `tan(half the co-latitude)` and `log tan(45 + lat/2)`
 * expressions with the centre, the scale and the handedness spelled again
 * at every call site — and two spellings of one map disagree about where
 * a star lands. Written as a `Projection`, they are five fields set once,
 * and every reading below respects them.
 *
 *     Projection plate{.scheme = Scheme::Stereographic,
 *                      .centre = {.latDeg = 90}, .scale = 235};
 *     const glm::vec2 star = plate.at({.lonDeg = 63.4f, .latDeg = 31.8f});
 *     const Spherical back = plate.from(star);   // and the way home
 *
 * **The plane is y UP**: `+y` is the direction of increasing latitude at
 * the map's centre, which is what "north is up" means, and the unit is the
 * caller's own — pixels, millimetres of paper, radii of a tropic.
 * `Grid{.yScale = -1}` is how the answer reaches a y-down canvas, and
 * `Grid` is also where an ANISOTROPIC map belongs: a projection here is
 * isotropic, because a per-axis scale would turn a stereographic's circles
 * into ellipses and none of the laws below would hold. A chart measured at
 * one number of degrees per centimetre across and another down is an
 * isotropic projection under an anisotropic unit map.
 *
 * ## Why one value and not one function per map
 *
 * The five schemes differ in one line of arithmetic each and agree about
 * everything else — the centring, the handedness, the turn, the way back.
 * A caller comparing two projections of the same sky (which is what asking
 * "was this chart drawn on a cylinder or from a pole?" IS) needs them to
 * be the same kind of thing, held in a variable and swapped. `scheme` is
 * therefore a field, not a name.
 *
 * ## What the projection does not know
 *
 * A projection is a coordinate map, and the astronomy, cartography or
 * cartouche that stands on it is the caller's. `Rotation` covers the part
 * that IS geometry — an epoch's worth of precession, a globe turned to
 * bring a place to the middle, a chart re-poled onto the ecliptic — as a
 * rotation of the sphere; which angles to turn by is what the caller
 * knows. A measured artefact's own departures from its law (a centre that
 * is not quite the pole, an azimuth that runs a few per cent fast) belong
 * beside the artefact for the same reason.
 */

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <optional>

namespace sigil::geometry::path {

/** A direction on the unit sphere, in degrees: `lonDeg` round the equator
 *  and `latDeg` up from it. Right ascension and declination are these two
 *  under the sky's names; longitude and latitude under the ground's. */
struct Spherical {
  float lonDeg = 0;
  float latDeg = 0;

  bool operator==(const Spherical&) const = default;

  /** The unit vector: `+x` at (0, 0), `+y` at (90, 0), `+z` at the north
   *  pole. */
  glm::vec3 direction() const;

  /** The direction back as an angle pair, with `lonDeg` in [0, 360). A
   *  vector of any length is read for its direction alone. */
  static Spherical of(glm::vec3 direction);
};

/** How far apart two directions stand, in degrees — the great-circle
 *  angle, computed from the half-chord so that a pair nearly on top of
 *  one another keeps its precision where an `acos` of the dot product
 *  would lose it. */
float angleBetween(Spherical a, Spherical b);

/** The direction @p arcDeg of great circle away from @p from, setting out
 *  along @p bearingDeg measured from NORTH toward EAST.
 *
 *  The step a spherical construction is made of: the horizon point at an
 *  azimuth, the pole of the great circle a chart's own line is, a star
 *  offset from the one beside it. At a pole north is not a direction, and
 *  the bearing is then measured from `from`'s own meridian — which is what
 *  the arithmetic does anyway, continuously with everywhere else. */
Spherical offsetFrom(Spherical from, float bearingDeg, float arcDeg);

// ---------------------------------------------------------------------------
// Rotation — the sphere turned.

/** A rotation of the sphere, as a value: an orthonormal basis you apply to
 *  a direction, invert, and compose with another.
 *
 *  It is here rather than in a matrix header because it is the half of a
 *  map that is not the map: a chart of one epoch's sky drawn from another
 *  epoch's catalogue, a globe turned to bring a coast to the middle, and a
 *  plate re-poled onto the ecliptic are all one projection under one
 *  rotation. Composing them is the whole of what a caller does with it, so
 *  `then()` is the verb and matrix multiplication order is not something
 *  anybody has to remember. */
struct Rotation {
  /** The rotation itself. Identity by default, which is the sphere left
   *  where it is. */
  glm::mat3 basis{1.0f};

  bool operator==(const Rotation& other) const { return basis == other.basis; }

  glm::vec3 operator()(glm::vec3 direction) const;
  /** The same turn read in angles — the form a catalogue is held in. */
  Spherical operator()(Spherical direction) const;

  /** The turn undone. */
  Rotation inverse() const;
  /** This rotation FOLLOWED BY @p next: `a.then(b)(v) == b(a(v))`. */
  Rotation then(const Rotation& next) const;

  /** A turn of @p deg about an axis, counterclockwise seen from the
   *  positive end of that axis. */
  static Rotation aboutX(float deg);
  static Rotation aboutY(float deg);
  static Rotation aboutZ(float deg);

  /** `Rz(a) · Ry(b) · Rz(c)` — the three-angle form every rotation of the
   *  sphere can be written in, and the one an epoch-to-epoch precession
   *  and a re-poling are both published as. The caller owns which angles
   *  and which signs; this owns the composition, which is where a
   *  hand-multiplied nine-term matrix goes wrong. */
  static Rotation zyz(float aDeg, float bDeg, float cDeg);
};

// ---------------------------------------------------------------------------
// Projection.

/** The law that carries an angle from the map's centre into a distance on
 *  the paper. Three azimuthal (the map is round about its centre and the
 *  law holds at every bearing) and two cylindrical (the parallels are
 *  straight and the law holds up the map only). */
enum class Scheme {
  /** Azimuthal, CONFORMAL: `r = 2 k tan(p/2)`. Angles are true
   *  everywhere, and every circle on the sphere is a circle on the plane —
   *  which is what `circleOf()` answers and what an astrolabe is built
   *  out of. The far side of the sphere runs away to infinity. */
  Stereographic,
  /** Azimuthal: `r = k sin p`. The sphere as seen from infinitely far
   *  away — a globe drawn as a disc, with the whole far hemisphere folded
   *  onto the near one and the limb at `k`. */
  Orthographic,
  /** Azimuthal, EQUIDISTANT: `r = k p`. Distance from the centre is read
   *  straight off the paper with a ruler, at every bearing. */
  AzimuthalEquidistant,
  /** Cylindrical, EQUIDISTANT — the plate carrée: `x = k dlon`,
   *  `y = k dlat`. Both axes are rulers and nothing else is true. */
  Equirectangular,
  /** Cylindrical, CONFORMAL: `x = k dlon`, `y = k ln tan(45 + lat/2)`.
   *  The ordinate a navigator's chart lays down, and the one a chart
   *  measured off paper is tested against. */
  Mercator,
};

/** Which side of the sphere the map is drawn from. The same directions
 *  laid down from outside the sphere and from inside it are MIRROR IMAGES
 *  across the map's own north line, and neither is a mistake in the
 *  other's arithmetic: a chart of the sky drawn as it is seen from under
 *  it and the same sky engraved as it stands on a globe read the opposite
 *  ways round. */
enum class Vantage {
  /** Longitude increases counterclockwise about a north-polar centre, and
   *  to the right on a cylindrical map. */
  Inside,
  /** The mirror of that, taken about the map's north line before `roll`
   *  turns it. */
  Outside,
};

/** A circle on the PLANE: a centre and a radius. What a circle on the
 *  sphere becomes under a conformal projection, and what three points on
 *  the paper decide. (`shapes::Circle` is the silhouette generator and a
 *  different thing entirely.) */
struct PlaneCircle {
  glm::vec2 centre{0, 0};
  float radius = 0;
  bool operator==(const PlaneCircle&) const = default;
};

/** The circle through three points, absent when they stand on one line.
 *
 *  Beside `Projection::circleOf()` because it is the other way to the same
 *  answer: the closed form knows what the image of a sphere circle IS, and
 *  this is how a maker without the closed form STRIKES one — through three
 *  of its points. Where both answer, they answer the same circle. */
std::optional<PlaneCircle> circleThrough(glm::vec2 a, glm::vec2 b, glm::vec2 c);

/** The sphere laid onto a plane.
 *
 *  An aggregate, meant for designated initialisation: a positional
 *  constructor could not gain a field later without breaking every call
 *  site, and these are exactly the fields a caller wants to name.
 *
 *  Trivially copyable and comparable, so a consumer that caches drawings
 *  can prove two frames asked for the same map. */
struct Projection {
  Scheme scheme = Scheme::Stereographic;

  /** The direction at the middle of the map: the tangent point of an
   *  azimuthal projection, and the point a cylindrical one's central
   *  meridian crosses its standard parallel. */
  Spherical centre{};

  /** PLANE UNITS PER RADIAN OF ARC AT THE CENTRE — the one derivative all
   *  five schemes share there, so changing the scheme leaves the middle of
   *  the map the size it was and moves only what is far from it.
   *
   *  Further out each law has its own say, and the two worth knowing by
   *  heart are: an orthographic's limb stands at `scale`, and a polar
   *  stereographic's equator at twice it. */
  float scale = 1;

  /** The map turned counterclockwise on the paper, in degrees. Where the
   *  centre is a pole, north is not a direction and this is what says
   *  which longitude lies along `+x`. */
  float rollDeg = 0;

  Vantage vantage = Vantage::Inside;

  bool operator==(const Projection&) const = default;

  /** Whether the map is round about its centre — true of the three
   *  azimuthal schemes, and the condition under which `radiusAt()` reads
   *  at every bearing rather than up the middle alone. */
  bool azimuthal() const;

  /** Where @p direction lands on the plane. */
  glm::vec2 at(Spherical direction) const;

  /** The direction that lands at @p point — the way home.
   *
   *  On an orthographic map the far hemisphere lies on top of the near
   *  one, and what comes back is the NEAR reading; past the limb there is
   *  no direction at all and the answer is the point on the limb. */
  Spherical from(glm::vec2 point) const;

  /** How far @p direction stands from the centre, in degrees of arc. What
   *  a caller culls an orthographic's far side, or a plate's outer limit,
   *  against. */
  float angleFrom(Spherical direction) const;

  /** The plane distance this map lays down for @p arcDeg of arc from the
   *  centre, measured up the map's own north line: the RADIUS on an
   *  azimuthal projection, where the map is round and the same law holds
   *  at every bearing, and the ORDINATE on a cylindrical one, where it
   *  does not. Signed, and odd about the centre, so a southward arc comes
   *  back as a distance below it. */
  float radiusAt(float arcDeg) const;

  /** The inverse of `radiusAt()`: the arc, in degrees, that stands at that
   *  distance up the map. */
  float arcAtRadius(float radius) const;

  /** The image of the circle standing @p arcDeg of arc away from @p pole —
   *  an almucantar about a zenith, a parallel about a pole, a great circle
   *  (at 90) about the pole that defines it.
   *
   *  Only a conformal azimuthal map answers: under a stereographic every
   *  circle on the sphere is a circle on the plane, which is what lets a
   *  whole family of them be struck with a compass instead of plotted. Two
   *  cases have no circle and come back absent — a scheme that does not
   *  map circles to circles, and a circle passing through the very point
   *  the projection is taken FROM, whose image is a straight line. The
   *  second is not a degeneracy to guard against but a feature of the
   *  drawing: the meridian through the centre of an astrolabe's plate IS
   *  straight. */
  std::optional<PlaneCircle> circleOf(Spherical pole, float arcDeg) const;

  /** The same map about another centre — the sibling strip, the next
   *  panel, the same law re-poled. Saves the field-by-field restatement,
   *  which is where a scale or a handedness gets silently dropped. */
  Projection centredOn(Spherical newCentre) const;
};

}  // namespace sigil::geometry::path

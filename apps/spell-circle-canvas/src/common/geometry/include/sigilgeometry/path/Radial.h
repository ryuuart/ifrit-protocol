#pragma once
/** @file
 * @ingroup geometry-path
 *
 * THE RADIAL ARRANGEMENT: N things dealt round a centre. One set of
 * options says where the vertices stand — how many, over what sweep, at
 * which radii, growing how — and how they are joined: into one loop (a
 * polygon, a star), chord by chord (a star polygon's chords, a polygon's
 * sides as separate runs), or not at all, with a mark standing at each
 * (a tick ladder, a ring of dial segments, a ring of studs). The kit's
 * `shapes::radial` draws it as an outline over a box; `path::points`
 * with the `radial` pattern answers its vertices.
 */
#include <cstdint>
#include <functional>
#include <glm/vec2.hpp>
#include <vector>

#include "sigilgeometry/path/Frame.h"
#include "sigilgeometry/path/Outline.h"

namespace sigil::geometry::path {

/** How a radial arrangement's vertices are joined. */
enum class Connect : uint8_t {
  /** One closed ring through every vertex in turn — or, with a `skip`
   *  that is not coprime with the count, one ring per orbit the skip
   *  makes: `{6/2}` is two triangles. */
  Loop,
  /** Every vertex to the one `skip` further round, each chord its own
   *  open contour — the per-side form a text baseline walks. */
  Each,
  /** No joining: a `Mark` stands at every vertex instead. */
  None,
};

/** How the radius grows with the vertex index, on top of `radii`. */
enum class Growth : uint8_t {
  /** Every vertex at its own radius. */
  None,
  /** In proportion to the index: an Archimedean run of seeds. */
  Linear,
  /** With the square root of the index: equal area per seed, which with
   *  the golden angle as the step is phyllotaxis. */
  SquareRoot,
};

/** WHAT STANDS AT A VERTEX when the vertices are not joined. Radii are
 *  NORMALISED (1 is the arrangement's radius). */
struct Mark {
  enum class Kind : uint8_t {
    /** An open radial line from `inner` to `outer` — a tick. */
    Line,
    /** A closed stretch of the ring between `inner` and `outer`,
     *  `spanDegrees` wide, whose curved sides are the ring's own arcs — a
     *  dial segment, which fattens with radius. */
    Segment,
    /** A closed bar along the radius from `inner` to `outer`, `width` px
     *  across it — the node, the lozenge. */
    Bar,
    /** `figure`, centred on the vertex. */
    Figure,
  };
  Kind kind = Kind::Line;
  float inner = 0.92f;
  float outer = 1.0f;
  float spanDegrees = 0;
  float width = 0;
  Outline figure{};

  /** The open radial line between two radii. */
  static Mark line(float inner = 0.92f, float outer = 1.0f) {
    return {Kind::Line, inner, outer};
  }
  /** The closed stretch of ring @p spanDegrees wide between two radii. */
  static Mark segment(float spanDegrees, float inner = 0.88f,
                      float outer = 1.0f) {
    return {Kind::Segment, inner, outer, spanDegrees};
  }
  /** The closed bar @p width px across, between two radii. */
  static Mark bar(float width, float inner = 0.92f, float outer = 1.0f) {
    return {Kind::Bar, inner, outer, 0, width};
  }
  /** @p figure, drawn about its own bounds' centre, at every vertex. */
  static Mark shape(Outline figure) {
    Mark mark{Kind::Figure};
    mark.figure = std::move(figure);
    return mark;
  }
  /** A mark that draws nothing: a degenerate line. */
  bool empty() const {
    return kind == Kind::Figure ? figure.empty() : inner == outer;
  }
  bool operator==(const Mark&) const = default;
};

/** Where a radial arrangement's vertices stand and how they are joined.
 *  Angles are in the frame's convention — zero north, clockwise, unless
 *  `frame` says otherwise. */
struct RadialOptions {
  /** The normalised radius of each vertex, CYCLED: `{1}` is a polygon,
   *  `{1, 0.42f}` a star. */
  std::vector<float> radii{1.0f};
  /** The frame's degrees of vertex 0. */
  float fromDegrees = 0;
  /** The span the vertices are dealt over. */
  float sweepDegrees = 360;
  /** The degrees between one vertex and the next; zero deals the sweep
   *  evenly. 137.508 is the golden angle. */
  float stepDegrees = 0;
  /** Deal `count + 1` vertices, closing the ladder at both ends — what a
   *  0–90° scale wants and a full ring does not. */
  bool closed = false;
  /** How far round the next vertex of a chord is: 1 the sides, 2 a
   *  `{n/2}` star polygon's chords. */
  int skip = 1;
  Connect connect = Connect::Loop;
  /** What stands at each vertex when `connect` is `None`, CYCLED:
   *  `{long, short, short, short, short}` puts a long mark every fifth.
   *  Empty is `Mark::line()`. */
  std::vector<Mark> marks{};
  Growth growth = Growth::None;
  /** Px trimmed off each end of every chord (`Connect::Each` alone). */
  float inset = 0;
  /** Bows every edge of a loop INWARD along its own bisector, in units of
   *  the radius — the engraved star's narrowing arms; negative bulges. */
  float waist = 0;
  /** True measures the radius as half the SHORTER side of the box, a
   *  circle on any box; false inscribes the arrangement in the box's own
   *  ellipse. */
  bool uniform = false;
  /** Where zero is and which way the angles run; its centre and radius
   *  come from the box. */
  PolarFrame frame{};
  /** Per vertex: the mark the fields gave, overridden. A mark with
   *  `inner == outer` skips its vertex.
   *  @trap A callable has no equality, so options carrying one compare
   *  unequal to everything and a node shaped by them never prunes. */
  std::function<Mark(int index, Mark fromFields)> each;

  bool operator==(const RadialOptions& other) const;
};

/** The vertices of @p count dealt by @p options on @p frame — the frame's
 *  own centre and radius. */
std::vector<glm::vec2> radialPoints(int count, const RadialOptions& options,
                                    const PolarFrame& frame);

/** The arrangement drawn on @p frame as one outline: loops, chords or
 *  marks as the options say. */
Outline radialOutline(int count, const RadialOptions& options,
                      const PolarFrame& frame);

/** The same arrangement inscribed in a box of @p size: centred on it,
 *  its radius half the shorter side when `uniform` and the box's own
 *  half-extents otherwise. */
Outline radialOutline(int count, const RadialOptions& options, glm::vec2 size);

}  // namespace sigil::geometry::path
